#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <eps_efp.h>
#include <eps_utils.h>

extern const char* task_init_log_file;
const char* efp_log = "/tmp/efp.log";

void efp_main(pid_t pgid) {
    char msg[256];

    remove_log_file(efp_log);
    system("rm /tmp/efp_exit.beacon");

    pid_t child_pid = getpid();
    pid_t parent_pid = getppid();

    sprintf(msg, "EFP PID: %d", child_pid);
    log_message(msg, efp_log);

    sprintf(msg, "EFP Process SID: %d", getsid(child_pid));
    log_message(msg, efp_log);

    sprintf(msg, "EFP Process GID: %d", getpgid(child_pid));
    log_message(msg, efp_log);

    sprintf(msg, "Parent PID: %d", parent_pid);
    log_message(msg, efp_log);

    sprintf(msg, "Child Cgroup:");
    log_message(msg, efp_log);
    log_cgroup(child_pid, efp_log);

    sprintf(msg, "Parent Cgroup:");
    log_message(msg, efp_log);
    log_cgroup(parent_pid, efp_log);

    sprintf(msg, "Moving efp to its own session...");
    log_message(msg, efp_log);

    if (setsid() == -1) {
        sprintf(msg, "error: setsid: %s", strerror(errno));
        log_message(msg, efp_log);
    }

    sprintf(msg, "EFP New Process SID: %d", getsid(child_pid));
    log_message(msg, efp_log);

    //sprintf(msg, "Moving efp to its own process group...");
    //log_message(msg, efp_log);

    //if (setpgid(0,pgid) == -1) {
    //    sprintf(msg, "error: setpgid: %s", strerror(errno));
    //    log_message(msg, efp_log);
    //}

    sprintf(msg, "EFP New Process GID: %d", getpgid(child_pid));
    log_message(msg, efp_log);

    // INFO: Currently if the sleep is there, you will not see
    //       the child exit log. Probably is is because the forked
    //       process appears in the process group of slurmstepd and 
    //       when it exits, it kills all spawned processes in that group.
    //       this is just my assumption by now, it is kinda hard to
    //       tell for sure...
    sprintf(msg, "Child sleeping...");
    log_message(msg, efp_log);
    sleep(3);

    system("echo 0 >> /tmp/efp_exit.beacon");

    sprintf(msg, "Child exiting success...");
    log_message(msg, efp_log);
    log_message(msg, task_init_log_file);
    exit(EXIT_SUCCESS);
}
