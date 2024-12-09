#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <eps_efp.h>
#include <eps_utils.h>

extern const char* task_init_log_file;
const char* efp_log = "/tmp/efp.log";
int efp_log_fd;

void efp_main(pid_t pgid) {
    char msg[256];

    remove_log_file(efp_log);
    system("rm /tmp/efp_exit.beacon");
    efp_log_fd = open(efp_log, O_RDWR | O_CREAT, 0666);

    pid_t child_pid = getpid();
    pid_t parent_pid = getppid();

    sprintf(msg, "Global char: %p\n", task_init_log_file);
    log_message(msg, efp_log_fd);

    sprintf(msg, "Global int: %p\n", &test);
    log_message(msg, efp_log_fd);

    sprintf(msg, "Global char value: %s\n", task_init_log_file);
    log_message(msg, efp_log_fd);

    sprintf(msg, "Global int value: %d\n", test);
    log_message(msg, efp_log_fd);

    sprintf(msg, "EFP PID: %d\n", child_pid);
    log_message(msg, efp_log_fd);
    log_cgroup(child_pid, efp_log);

    sprintf(msg, "Parent Cgroup:\n");
    log_message(msg, efp_log_fd);
    log_cgroup(parent_pid, efp_log);

    sprintf(msg, "Moving efp to its own session...\n");
    log_message(msg, efp_log_fd);

    if (setsid() == -1) {
        sprintf(msg, "error: setsid: %s\n", strerror(errno));
        log_message(msg, efp_log_fd);
    }

    sprintf(msg, "EFP New Process SID: %d\n", getsid(child_pid));
    log_message(msg, efp_log_fd);

    //sprintf(msg, "Moving efp to its own process group...\n");
    //log_message(msg, efp_log_fd);

    //if (setpgid(0,pgid) == -1) {
    //    sprintf(msg, "error: setpgid: %s\n", strerror(errno));
    //    log_message(msg, efp_log_fd);
    //}

    sprintf(msg, "EFP New Process GID: %d\n", getpgid(child_pid));
    log_message(msg, efp_log_fd);

    // INFO: Currently if the sleep is there, you will not see
    //       the child exit log. Probably is is because the forked
    //       process appears in the process group of slurmstepd and 
    //       when it exits, it kills all spawned processes in that group.
    //       this is just my assumption by now, it is kinda hard to
    //       tell for sure...
    sprintf(msg, "Child sleeping...");
    sleep(3);
    log_message(msg, efp_log_fd);

    system("echo 0 >> /tmp/efp_exit.beacon");

    log_message(msg, task_init_log_file);
    sprintf(msg, "Child exiting success...\n");
    log_message(msg, efp_log_fd);
    exit(EXIT_SUCCESS);
}
