#include <eps_db.h>
#include <slurm/spank.h>
#include <unistd.h>

#define PLUGIN_NAME "Spank/Eps"

SPANK_PLUGIN(eps, 1)

/********************************
 *
 * Spank functions
 *
 ********************************/
int slurm_spank_init(spank_t sp, int ac, char **av) {
    slurm_info("Init: " PLUGIN_NAME);
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
