#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>

#include <eps_wait.h>

int timedwaitpid(pid_t pid, int *status, int timeout_sec) {
    const int interval_ms = 100;
    int waited_ms = 0;

    while (waited_ms < timeout_sec * 1e3) {
        pid_t result = waitpid(pid, status, WNOHANG);
        if (result == pid) {
            return 0;
        } else if (result == 0) {
            struct timespec ts = {0, interval_ms * 1e6};
            nanosleep(&ts, NULL);
            waited_ms += interval_ms;
        } else {
            perror("waitpid");
            return -1;
        }
    }
    return 1;
}
