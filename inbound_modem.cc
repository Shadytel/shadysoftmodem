#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>

#include <iostream>
#include <string>

extern "C" {
#define delete _delete
#include "modem.h"
#undef delete

/* modem init externals : FIXME remove it */
extern int  dp_dummy_init(void);
extern void dp_dummy_exit(void);
extern int  dp_sinus_init(void);
extern void dp_sinus_exit(void);
extern int  prop_dp_init(void);
extern void prop_dp_exit(void);
extern int datafile_load_info(char *name,struct dsp_info *info);
extern int datafile_save_info(char *name,struct dsp_info *info);
extern int modem_ring_detector_start(struct modem *m);

#include "resample.h"
}

typedef struct {
    struct modem * modem;
    int active;
    ResamplerState in_resamp_state;
    ResamplerState out_resamp_state;
} ExtModModem;

FILE *logFile;
FILE *inSamples;
FILE *outSamples;
FILE *outResamples;

static int yate_extmod_modem_start(struct modem *m)
{
    ExtModModem * t = (ExtModModem *)m->dev_data;
    t->active = 1;
    resamp_8khz_9k6hz_init(&t->in_resamp_state);
	resamp_9k6hz_8khz_init(&t->out_resamp_state);
    fprintf(logFile, "yate_extmod_modem_start\n");
    return 0;
}

static int yate_extmod_modem_stop(struct modem *m)
{
    ExtModModem * t = (ExtModModem *)m->dev_data;
    t->active = 0;
    fprintf(logFile, "yate_extmod_modem_stop\n");
    return 0;
}

static int yate_extmod_modem_ioctl(struct modem *m, unsigned int cmd, unsigned long arg)
{
	ExtModModem * t = (ExtModModem *)m->dev_data;
	fprintf(logFile, "yate_extmod_modem_ioctl: cmd %x, arg %lx...\n", cmd, arg);
    switch (cmd) {
    case MDMCTL_CAPABILITIES:
        return -1;
    case MDMCTL_HOOKSTATE:
    case MDMCTL_SPEED:
    case MDMCTL_GETFMTS:
    case MDMCTL_SETFMT:
    case MDMCTL_SETFRAGMENT:
    case MDMCTL_SPEAKERVOL:
		return 0;
    case MDMCTL_CODECTYPE:
        return CODEC_UNKNOWN;
    case MDMCTL_IODELAY:
		return 0; // FIXME?
    default:
        return 0;
    }
}

struct modem_driver yate_extmod_modem_driver = {
        .name = "YATE External Module Modem Driver",
        .start = yate_extmod_modem_start,
        .stop = yate_extmod_modem_stop,
        .ioctl = yate_extmod_modem_ioctl,
};

int init_modem(ExtModModem *m) {
    struct termios termios;
    char * pty_name;
    int pty;
    
    pty = getpt();
    if (pty < 0 || grantpt(pty) < 0 || unlockpt(pty) < 0) {
        fprintf(logFile, "Error creating pty: %s\n", strerror(errno));
        return errno;
    }

    tcgetattr(pty, &termios);
    cfmakeraw(&termios);
    cfsetispeed(&termios, B115200);
    cfsetospeed(&termios, B115200);

    if (tcsetattr(pty, TCSANOW, &termios)) {
        fprintf(logFile, "Error creating pty in tcsetattr: %s\n",
            strerror(errno));
        return errno;
    }

    fcntl(pty, F_SETFL, O_NONBLOCK);

    pty_name = ptsname(pty);

    m->active = 0;
    m->modem = modem_create(&yate_extmod_modem_driver, pty_name);
    m->modem->dev_data = (void *)m;

    m->modem->pty = pty;
    m->modem->pty_name = pty_name;

    modem_update_termios(m->modem, &termios);

    return 0;
}

void process_msg(std::string & in_msg) {
    size_t id_begin_idx;
    size_t id_len;

    id_begin_idx = in_msg.find(':') + 1;
    id_len = in_msg.find(':', id_begin_idx) - id_begin_idx;
    std::cout << "%%<message:" <<
        in_msg.substr(id_begin_idx, id_len) << ":true:" << std::endl;
}

typedef int16_t samp_t;

int main(int argc, char *argv[]) {
    int len;
    int numSamples;
    char buf[4096];
    char ans[] = "ATA\n";
    samp_t inSampleBuf[sizeof(buf) / 2];
    samp_t outSampleBuf[sizeof(buf) / 2];
    fd_set in_fds;
    fd_set out_fds;
    fd_set err_fds;
    std::string in_msg;
    ExtModModem modem;

    logFile = fopen("/tmp/inbound_modem_dbg.log", "wt");
    inSamples = fopen("/tmp/inbound_modem_in.snd", "wb");
    outSamples = fopen("/tmp/inbound_modem_out.snd", "wb");
    outResamples = fopen("/tmp/inbound_modem_out_resamp.snd", "wb");

    dp_dummy_init();
	dp_sinus_init();
	prop_dp_init();
	modem_timer_init();

    init_modem(&modem);

    fprintf(logFile, "Modem pty is fd %d\n", modem.modem->pty);

    modem_write(modem.modem, ans, sizeof(ans));

    for (;;) {
        if (modem.modem->event) {
            modem_event(modem.modem);
        }

        FD_ZERO(&in_fds);
        FD_ZERO(&out_fds);
        FD_ZERO(&err_fds);

        FD_SET(0, &in_fds);
        FD_SET(3, &in_fds);
        FD_SET(modem.modem->pty, &in_fds);

        FD_SET(1, &out_fds);
        FD_SET(4, &out_fds);

        for (int i = 0; i < 5; i++) {
            FD_SET(i, &err_fds);
        }
        
        if (select(modem.modem->pty, &in_fds, NULL, &err_fds, NULL) <= 0) {
            return -1;
        }

        for (int i = 0; i < modem.modem->pty; i++) {
            if (FD_ISSET(i, &in_fds) && i != 3) {
                fprintf(logFile, "fd %d is ready to read\n", i);
            }
        }

        for (int i = 0; i < 5; i++) {
            if (FD_ISSET(i, &err_fds)) {
                fprintf(logFile, "fd %d is exceptional\n", i);
                return -2;
            }
        }

        if (FD_ISSET(0, &in_fds)) {
            len = read(0, buf, sizeof(buf));
            if (len <= 0) {
                return -3;
            }
            fwrite(buf, 1, len, logFile);
            for (int i = 0; i < len; i++) {
                in_msg.push_back(buf[i]);
                if (buf[i] == '\n') {
                    process_msg(in_msg);
                    in_msg.clear();
                }
            }
        }

        if (FD_ISSET(3, &in_fds)) {
            len = read(3, buf, sizeof(buf));
            if (len <= 0) {
                return -4;
            }
            if (modem.active) {
                if (len % 2) {
                    fprintf(logFile, "read an odd number of bytes!! (%d)\n", len);
                }
                fwrite(buf, 1, len, inSamples);
                numSamples = (sizeof(inSampleBuf) / 2) -
                    resample(&modem.in_resamp_state, (int16_t *)buf, len / 2,
                    inSampleBuf, sizeof(inSampleBuf) / 2);
                fprintf(logFile, "Received %d bytes (%d samples), resampled to %d samples\n",
                    len, len / 2, numSamples);
                fwrite(inSampleBuf, 2, numSamples, inSamples);
                modem_process(modem.modem, inSampleBuf, outSampleBuf, numSamples);
                fwrite(outSampleBuf, 2, numSamples, outSamples);
                numSamples = (sizeof(buf) / 2) - 
                    resample(&modem.out_resamp_state, outSampleBuf, numSamples,
                    (int16_t *)buf, sizeof(buf) / 2);
                fprintf(logFile, "upconverted modem samples to %d\n", numSamples);
                fwrite(buf, 2, numSamples, outResamples);
            } else {
                // Return silence
                memset(buf, 0, len);
                numSamples = len / 2;
            }
            fprintf(logFile, "Writing %d samples (%d bytes) back to YATE\n", numSamples, numSamples * 2);
            if (write(4, buf, numSamples * 2) != numSamples * 2) {
                fprintf(logFile, "can't write the entire outgoing buffer!\n");
            }
        }

        if (FD_ISSET(modem.modem->pty, &in_fds)) {
            len = read(modem.modem->pty, buf, sizeof(buf) - 1);
            buf[len] = 0;
            fprintf(logFile, "Read from pty: %s\n", buf);
            modem_write(modem.modem, buf, len);
        }
    }

    dp_dummy_exit();
	dp_sinus_exit();
	prop_dp_exit();
}
