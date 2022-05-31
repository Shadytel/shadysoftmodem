#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
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
    int delay;
} ExtModModem;

#ifdef DEBUG_LOG
FILE *logFile;
#define DLPRINTF(args...) fprintf(logFile, args)
#else
#define DLPRINTF(unsed...)
#endif

#ifdef DEBUG_SAMPLES
FILE *inSamples;
FILE *outSamples;
FILE *inResamples;
FILE *outResamples;
#define SFWRITE(args...) fwrite(args)
#else
#define SFWRITE(args...)
#endif // DEBUG_SAMPLES

static int yate_extmod_modem_start(struct modem *m)
{
    ExtModModem * t = (ExtModModem *)m->dev_data;
    t->active = 1;
    //t->delay = 0;
    t->delay = 256; // 160 sample buffer from YATE, 96 sample jitter buffer from ATA
    resamp_8khz_9k6hz_init(&t->in_resamp_state);
	resamp_9k6hz_8khz_init(&t->out_resamp_state);
    DLPRINTF("yate_extmod_modem_start, rate = %d\n", m->srate);
    return 0;
}

static int yate_extmod_modem_stop(struct modem *m)
{
    ExtModModem * t = (ExtModModem *)m->dev_data;
    t->active = 0;
    DLPRINTF("yate_extmod_modem_stop\n");
    return 0;
}

static int yate_extmod_modem_ioctl(struct modem *m, unsigned int cmd, unsigned long arg)
{
    ExtModModem * t = (ExtModModem *)m->dev_data;
    DLPRINTF("yate_extmod_modem_ioctl: cmd %x, arg %lx...\n", cmd, arg);
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
        DLPRINTF(" ... delay = %d\n", t->delay);
		return t->delay; // FIXME?
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
        fprintf(stderr, "Error creating pty: %s\n", strerror(errno));
        return errno;
    }

    tcgetattr(pty, &termios);
    cfmakeraw(&termios);
    cfsetispeed(&termios, B115200);
    cfsetospeed(&termios, B115200);

    if (tcsetattr(pty, TCSANOW, &termios)) {
        fprintf(stderr, "Error creating pty in tcsetattr: %s\n",
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
    int skipSamples;
    char buf[4096];
    samp_t inSampleBuf[sizeof(buf) / 2];
    samp_t outSampleBuf[sizeof(buf) / 2];
    fd_set in_fds;
    fd_set out_fds;
    fd_set err_fds;
    std::string in_msg;
    ExtModModem modem;
    struct termios termios;
    int child = 0;
    
#ifdef DEBUG_LOG
    logFile = fopen("/tmp/inbound_modem_dbg.log", "wt");
#endif

#ifdef DEBUG_SAMPLES
    inSamples = fopen("/tmp/inbound_modem_in.snd", "wb");
    outSamples = fopen("/tmp/inbound_modem_out.snd", "wb");
    inResamples = fopen("/tmp/inbound_modem_in_resamp.snd", "wb");
    outResamples = fopen("/tmp/inbound_modem_out_resamp.snd", "wb");
#endif

    dp_dummy_init();
    dp_sinus_init();
    prop_dp_init();
    modem_timer_init();

    init_modem(&modem);

    if (argc > 1) {
        // If a program is specified as a command line argument, then we run
        // the command with the pty name as the argument.

        int attach_pty = strstr(argv[0], "attach") >= 0;

        const char * const prgName = argv[1];
        const char * const prgArgv[3] = { prgName, modem.modem->pty_name, NULL };
        child = fork();

        if (!child) {
            if (attach_pty) {
                int pty = open(modem.modem->pty_name, O_RDWR);
                dup2(pty, 0);
                dup2(pty, 1);
                dup2(pty, 2);
            }
            // The const-ness is probably safe to case away here since we've fork()ed
            execv(argv[1], (char * const *)prgArgv);
            return 0;
        }
    }

    DLPRINTF("Modem pty is fd %d\n", modem.modem->pty);

    while (!child || !waitpid(child, NULL, WNOHANG)) {
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
        FD_SET(modem.modem->pty, &err_fds);

        if (select(modem.modem->pty + 1, &in_fds, NULL, &err_fds, NULL) <= 0) {
            break;
        }

        /*
        for (int i = 0; i < modem.modem->pty; i++) {
            if (FD_ISSET(i, &in_fds) && i != 3) {
                fprintf(logFile, "fd %d is ready to read\n", i);
            }
        }
        */

        for (int i = 0; i <= modem.modem->pty; i++) {
            if (FD_ISSET(i, &err_fds)) {
                fprintf(stderr, "fd %d is exceptional\n", i);
                break;
            }
        }

        if (FD_ISSET(0, &in_fds)) {
            len = read(0, buf, sizeof(buf));
            if (len <= 0) {
                break;
            }
            //fwrite(buf, 1, len, logFile);
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
                break;
            }
            if (modem.active) {
                if (len % 2) {
                    DLPRINTF("read an odd number of bytes!! (%d)\n", len);
                }
                SFWRITE(buf, 1, len, inSamples);

                numSamples = (sizeof(inSampleBuf) / 2) -
                    resample(&modem.in_resamp_state, (int16_t *)buf, len / 2,
                    inSampleBuf, sizeof(inSampleBuf) / 2);
                
                DLPRINTF("Received %d bytes (%d samples), resampled to %d samples (ud = %d)\n",
                    len, len / 2, numSamples, modem.modem->update_delay);
                SFWRITE(inSampleBuf, 2, numSamples, inResamples);
                
                skipSamples = 0;
                if (modem.modem->update_delay < 0) {
				    if ( -modem.modem->update_delay >= len / 2) {
					    DLPRINTF("change delay -%d...\n", len / 2);
					    modem.delay -= len / 2;
					    modem.modem->update_delay += len / 2;
					    continue;
				    }

                    DLPRINTF("change delay %d...\n", modem.modem->update_delay);
                    skipSamples = modem.modem->update_delay;
                    numSamples += skipSamples; // skipSamples is negative here
				    modem.delay += modem.modem->update_delay;
                    modem.modem->update_delay = 0;
                }

                modem_process(modem.modem, inSampleBuf - skipSamples, outSampleBuf, numSamples);

                SFWRITE(outSampleBuf, 2, numSamples, outSamples);
                
                numSamples = (sizeof(buf) / 2) - 
                    resample(&modem.out_resamp_state, outSampleBuf, numSamples,
                    (int16_t *)buf, sizeof(buf) / 2);
                
                DLPRINTF("upconverted modem samples to %d\n", numSamples);
                SFWRITE(buf, 2, numSamples, outResamples);
            } else {
                // Return silence
                memset(buf, 0, len);
                numSamples = len / 2;
            }

            DLPRINTF("Writing %d samples (%d bytes) back to YATE\n", numSamples, numSamples * 2);
            
            if (write(4, buf, numSamples * 2) != numSamples * 2) {
                fprintf(stderr, "can't write the entire outgoing buffer!\n");
                break;
            }

            if (modem.modem->update_delay > 0) {
				DLPRINTF("change delay +%d...\n", modem.modem->update_delay);
                len = modem.modem->update_delay * 2;
                if (len > sizeof(buf)) {
                    DLPRINTF("Delay change required %d bytes, only %d available\n",
                        len, sizeof(buf));
                    len = sizeof(buf);
                }
                memset(buf, 0, len);
                if (write(4, buf, len) != len) {
                    fprintf(stderr, "can't write the entire outgoing delay buffer!\n");
                    break;
                }

			    modem.delay += modem.modem->update_delay;
			    modem.modem->update_delay = 0;
			}
        }

        if (FD_ISSET(modem.modem->pty, &in_fds)) {
            tcgetattr(modem.modem->pty, &termios);
			if (memcmp(&termios, &modem.modem->termios, sizeof(termios))) {
				modem_update_termios(modem.modem, &termios);
			}
            len = modem.modem->xmit.size - modem.modem->xmit.count;
            if (len == 0) {
                continue;
            }
            if (len > sizeof(buf)) {
                len = sizeof(buf);
            }
            len = read(modem.modem->pty, buf, len);
            if (len > 0) {
                modem_write(modem.modem, buf, len);
            }
        }
    }

    kill(child, SIGHUP);

    dp_dummy_exit();
    dp_sinus_exit();
    prop_dp_exit();
}
