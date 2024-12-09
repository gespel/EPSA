#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <eps_sem.h>
#include <eps_efp.h>
#include <eps_utils.h>

#define SEM_NAME "/efpsem"

void efp_main(pid_t pgid, int jid) {
    char msg[256];

    char* efp_log_file_path = get_efp_log_file_path(jid);
    int log_fd = get_log_file_fd(efp_log_file_path);
    free(efp_log_file_path);

    char* sem_name = get_sem_name(jid);

    sem_t* mutex = get_efp_mutex(sem_name, 1);
    if (!mutex) {
        sprintf(msg, "error: get_efp_mutex: %s", strerror(errno));
        log_message(msg, log_fd);
        exit(EXIT_FAILURE);
    }
    free(sem_name);

    int sem_val;
    sem_getvalue(mutex, &sem_val);
    sprintf(msg, "/efpsem: %d\n", sem_val);
    log_message(msg, log_fd);

    pid_t child_pid = getpid();
    pid_t parent_pid = getppid();

    sprintf(msg, "EFP PID: %d\n", child_pid);
    log_message(msg, log_fd);

    sprintf(msg, "EFP Process SID: %d\n", getsid(child_pid));
    log_message(msg, log_fd);

    sprintf(msg, "EFP Process GID: %d\n", getpgid(child_pid));
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

    sprintf(msg, "EFP New Process GID: %d\n", getpgid(child_pid));
    log_message(msg, log_fd);

    sprintf(msg, "Child waiting...\n");
    log_message(msg, log_fd);

    sem_wait(mutex);

    sprintf(msg, "Child exiting success...\n");
    log_message(msg, log_fd);

    sem_close(mutex);
    close(log_fd);

    exit(EXIT_SUCCESS);
}
