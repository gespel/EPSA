#include <slurm/spank.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <src/common/slurm_protocol_api.c>

#define PLUGIN_NAME "Spank/Eps"

SPANK_PLUGIN(eps, 1)

static int _ema_opt_process(int, const char*, int);

/* Provide a --eps option. */
struct spank_option spank_options[] =
{
    { "eps", "", "Enables EMA plugin for Slurm.", 0, 0,
        (spank_opt_cb_f) _ema_opt_process
    },
    SPANK_OPTIONS_TABLE_END
};

/* Evaluate option arguments. */
static int _ema_opt_process(int val, const char *optarg, int remote)
{
    if (optarg == NULL)
    {
        printf("args: NULL\n");
        return ESPANK_SUCCESS;
    }

    return ESPANK_ERROR;
}
/********************************
 *
 * Utility functions
 *
 ********************************/

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
    uint32_t nid, jid;
    if(spank_context() == S_CTX_REMOTE)
    {
        spank_get_item(sp, S_JOB_NODEID, &nid);
        spank_get_item(sp, S_JOB_ID, &jid);
        slurm_info("Node ID: %u", nid);
        slurm_info("Job ID: %d", jid);
    }
    return 0;
}

int slurm_spank_job_prolog (spank_t sp, int ac, char **av) {
    slurm_info("Prolog: " PLUGIN_NAME);
    slurm_spank_log("%s: %s", "slurm_spank_log", __func__);
    return 0;
}

int slurm_spank_job_epilog (spank_t sp, int ac, char **av) {
    slurm_info("Epilog: " PLUGIN_NAME);
    uint32_t nid, jid;

    spank_get_item(sp, S_JOB_NODEID, &nid);
    spank_get_item(sp, S_JOB_ID, &jid);

    slurm_info("Node ID: %u", nid);
    slurm_info("Job ID: %d", jid);

    return 0;
}
