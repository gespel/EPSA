#include <stdio.h>
#include <stdlib.h>

#include <eps_efp.h>
#include <eps_utils.h>

const char* efp_log = "/tmp/efp.log";

void efp_main() {
    char msg[256];

    remove_log_file(efp_log);

    pid_t child_pid = getpid();
    pid_t parent_pid = getppid();

    sprintf(msg, "Child PID: %d", child_pid);
    log_message(msg, efp_log);

    sprintf(msg, "Parent PID: %d", parent_pid);
    log_message(msg, efp_log);

    sprintf(msg, "Child Cgroup:");
    log_message(msg, efp_log);
    log_cgroup(child_pid, efp_log);

    sprintf(msg, "Parent Cgroup:");
    log_message(msg, efp_log);
    log_cgroup(parent_pid, efp_log);

    // INFO: Currently if the sleep is there, you will not see
    //       the child exit log. Probably is is because the forked
    //       process appears in the process group of slurmstepd and 
    //       when it exits, it kills all spawned processes in that group.
    //       this is just my assumption by now, it is kinda hard to
    //       tell for sure...
    //sprintf(msg, "Child sleeping...");
    //log_message(msg, efp_log);
    //sleep(3);


    system("echo 1 >> /tmp/efp_exit.beacon");
    sprintf(msg, "Child exiting success...");
    log_message(msg, efp_log);
    exit(EXIT_SUCCESS);
}
