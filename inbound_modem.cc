#include <iostream>
#include <stdio.h>
#include <sys/select.h>
#include <unistd.h>
#include <string>

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
    FILE *logFile;

    logFile = fopen("/tmp/inbound_modem_dbg.log", "wt");

    for (;;) {
        FD_ZERO(&in_fds);
        FD_ZERO(&out_fds);
        FD_ZERO(&err_fds);

        FD_SET(0, &in_fds);
        FD_SET(2, &in_fds);
        FD_SET(3, &in_fds);

        FD_SET(1, &out_fds);
        FD_SET(4, &out_fds);

        for (int i = 0; i < 5; i++) {
            FD_SET(i, &err_fds);
        }
        
        if (select(5, &in_fds, NULL, &err_fds, NULL) <= 0) {
            return -1;
        }

        if (FD_ISSET(0, &in_fds)) {
            len = read(0, buf, sizeof(buf));
            if (len <= 0) {
                return -2;
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

        if (FD_ISSET(2, &in_fds)) {
            len = read(2, buf, sizeof(buf));
            // Ignore errors
        }

        // Echo audio back
        if (FD_ISSET(3, &in_fds) && FD_ISSET(4, &out_fds)) {
            len = read(3, buf, sizeof(buf));
            if (len <= 0) {
                return -2;
            }
            write(4, buf, len);
        }
    }
}