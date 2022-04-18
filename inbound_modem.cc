#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <unistd.h>

#include <iostream>
#include <string>

extern "C" {
#define delete _delete
#include "modem.h"
#undef delete
}

#include "resample.h"

typedef struct {
    ResamplerState in_resamp_state;
    ResamplerState out_resamp_state;
} ExtModModem;

FILE *logFile;

static int yate_extmod_modem_start(struct modem *m)
{
    return 0;
}

static int yate_extmod_modem_stop(struct modem *m)
{
    return 0;
}

static int yate_extmod_modem_ioctl(struct modem *m, unsigned int cmd, unsigned long arg)
{
	ExtModModem * t = (ExtModModem *)m->dev_data;
	//DBG("modem_test_ioctl: cmd %x, arg %lx...\n",cmd,arg);
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

int create_pty() {
    struct termios termios;
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

int main(int argc, char *argv[]) {
    int len;
    char buf[32768];
    fd_set in_fds;
    fd_set out_fds;
    fd_set err_fds;
    std::string in_msg;

    logFile = fopen("/tmp/inbound_modem_dbg.log", "wt");

    for (;;) {
        FD_ZERO(&in_fds);
        FD_ZERO(&out_fds);
        FD_ZERO(&err_fds);

        FD_SET(0, &in_fds);
        FD_SET(3, &in_fds);

        FD_SET(1, &out_fds);
        FD_SET(4, &out_fds);

        for (int i = 0; i < 5; i++) {
            FD_SET(i, &err_fds);
        }
        
        if (select(5, &in_fds, NULL, &err_fds, NULL) <= 0) {
            return -1;
        }

        fprintf(logFile, "select() returned:\n");
        for (int i = 0; i < 5; i++) {
            if (FD_ISSET(i, &in_fds)) {
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

        // Echo audio back
        if (FD_ISSET(3, &in_fds)) {
            len = read(3, buf, sizeof(buf));
            if (len <= 0) {
                return -4;
            }
            write(4, buf, len);
        }
    }
}
