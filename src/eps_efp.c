#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <eps_sem.h>
#include <eps_efp.h>
#include <eps_utils.h>

#include <EMA.h>


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

    pid_t child_pid = getpid();

    sprintf(msg, "EFP PID: %d\n", child_pid);
    log_message(msg, log_fd);

    // NOTE: Is it really needed after all?
    sprintf(msg, "Moving efp to its own session...\n");
    log_message(msg, log_fd);

    if (setsid() == -1) {
        sprintf(msg, "error: setsid: %s\n", strerror(errno));
        log_message(msg, log_fd);
    }

    sprintf(msg, "EFP New Process SID: %d\n", getsid(child_pid));
    log_message(msg, log_fd);

    sprintf(msg, "Initializig EMA...\n");
    log_message(msg, log_fd);
    int err = EMA_init(NULL);

    if (err) {
        sprintf(msg, "Failed to initialize EMA: %d\n", err);
        log_message(msg, log_fd);
        exit(EXIT_FAILURE);
    }

    sprintf(msg, "Child waiting...\n");
    log_message(msg, log_fd);

    sem_wait(mutex);

    sprintf(msg, "Finalizing EMA...\n");
    log_message(msg, log_fd);
    err = EMA_finalize(NULL);
    if (err) {
        sprintf(msg, "Failed to finalize EMA: %d\n", err);
        log_message(msg, log_fd);
    }

    // INFO: Currently this palce can potentialy not be reached by the time
    //       when the srun exits, see comments in task_exit hook.
    sprintf(msg, "Child exiting success...\n");
    log_message(msg, log_fd);

    sem_close(mutex);
    close(log_fd);

    exit(EXIT_SUCCESS);
}
