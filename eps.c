#include <slurm/spank.h>
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>

#define PLUGIN_NAME "Spank/Eps"

SPANK_PLUGIN(eps, 1)

const char* global = "global";

/********************************
 *
 * Spank functions
 *
 ********************************/
int slurm_spank_init(spank_t sp, int ac, char **av) {
    slurm_info("Init: " PLUGIN_NAME);
    pid_t hook_pid = getpid();

    slurm_info("PID: %d", hook_pid);
    slurm_info("Global: %s", global);

    if (spank_context() == S_CTX_LOCAL) {
        pid_t pid = fork();

        switch(pid) {
            case -1:
                perror("fork");
                return 1;
            case 0:
                pid_t child_pid = getpid();
                pid_t parent_pid = getppid();
                slurm_info("Child PID: %d", child_pid);
                slurm_info("Parent PID: %d", parent_pid);
                slurm_info("Global: %s", global);
                slurm_info("Child exiting success...");
                exit(EXIT_SUCCESS);
            default:
                slurm_info("Child PID: %d", pid);
                slurm_info("Init hook exits...");
                return 0;
        }

    }

    return 0;
}

int slurm_spank_exit(spank_t sp, int ac, char **av) {
    slurm_info("Exit: " PLUGIN_NAME);
    return 0;
}

int slurm_spank_job_prolog (spank_t sp, int ac, char **av) {
    slurm_info("Prolog: " PLUGIN_NAME);
    return 0;
}

int slurm_spank_job_epilog (spank_t sp, int ac, char **av) {
    slurm_info("Epilog: " PLUGIN_NAME);
    return 0;
}
