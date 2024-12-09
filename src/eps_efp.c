#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <eps_efp.h>
#include <eps_utils.h>

extern const char* task_init_log_file;

void efp_main(pid_t pgid, int jid) {
    char msg[256];

    char* efp_log_file_path = get_efp_log_file_path(jid);
    int log_fd = get_log_file_fd(efp_log_file_path);

    pid_t child_pid = getpid();
    pid_t parent_pid = getppid();

    sprintf(msg, "EFP PID: %d\n", child_pid);
    log_message(msg, log_fd);

    sprintf(msg, "Parent PID: %d\n", parent_pid);
    log_message(msg, log_fd);

    sprintf(msg, "Moving efp to its own session...\n");
    log_message(msg, log_fd);

    if (setsid() == -1) {
        sprintf(msg, "error: setsid: %s\n", strerror(errno));
        log_message(msg, log_fd);
    }

    sprintf(msg, "EFP New Process SID: %d\n", getsid(child_pid));
    log_message(msg, log_fd);

    //sprintf(msg, "Moving efp to its own process group...\n");
    //log_message(msg, log_fd);

    //if (setpgid(0,pgid) == -1) {
    //    sprintf(msg, "error: setpgid: %s\n", strerror(errno));
    //    log_message(msg, log_fd);
    //}

    sprintf(msg, "EFP New Process GID: %d\n", getpgid(child_pid));
    log_message(msg, log_fd);

    sprintf(msg, "Child exiting success...\n");
    log_message(msg, log_fd);

    free(efp_log_file_path);
    close(log_fd);

    exit(EXIT_SUCCESS);
}
