#include <slurm/spank.h>

SPANK_PLUGIN(eps, 1)

/********************************
 *
 * Spank functions
 *
 ********************************/
int slurm_spank_init(spank_t sp, int ac, char **av) {
    return 0;
}

int slurm_spank_job_epilog(spank_t sp, int ac, char **av) {
    return 0;
}
