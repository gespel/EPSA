#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <slurm/spank.h>

#define PLUGIN_NAME "Spank/Eps"


SPANK_PLUGIN(eps, 1)

/********************************
 *
 * Spank functions
 *
 ********************************/
int slurm_spank_job_epilog(spank_t sp, int ac, char **av) {
    return 0;
}

int slurm_spank_task_init_privileged(spank_t sp, int ac, char **av) {
    return 0;
}

int slurm_spank_task_exit(spank_t sp, int ac, char **av) {
    return 0;
}
