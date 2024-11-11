#include <slurm/slurm.h>
#include <slurm/slurm_errno.h>

#include <eps_ema.h>
#include <eps_utils.h>

#include "src/interfaces/prep.h"

const char plugin_name[] = "EPS PrEp plugin";
const char plugin_type[] = "prep/eps";
const uint32_t plugin_version = SLURM_VERSION_NUMBER;

/********************************
 *
 * Globals
 *
 ********************************/

 DevicePtrArray devices;

 Measurements e0;
 Measurements e1;
 //TODO: Find a better name...
 Measurements res;

/********************************
 *
 * Slurm Plugin API hooks
 *
 ********************************/

extern int init(void)
{
        slurm_info("Init: %s", plugin_name);

        if (!running_in_slurmd()) return SLURM_SUCCESS;

        slurm_info("Initializing EMA...");
        int err = EMA_init(NULL);
        if (err) {
           slurm_info("EMA Initialization Error: %d", err);
           return SLURM_ERROR;
        }

        devices = EMA_get_devices();
        
        log_ema_devices();

        e0 = malloc(devices.size * sizeof(unsigned long long));
        e1 = malloc(devices.size * sizeof(unsigned long long));
        res = malloc(devices.size * sizeof(unsigned long long));

	return SLURM_SUCCESS;
}

extern void fini(void)
{
        slurm_info("Fini: %s", plugin_name);

        if (!running_in_slurmd()) return;

        slurm_info("Finalizing EMA...");
        int err = EMA_finalize();
        if (err) {
            slurm_info("Failed to finalize EMA!");
        }

        free(e0);
        free(e1);
        free(res);
}

extern void prep_p_register_callbacks(prep_callbacks_t* callbacks) {}

extern int prep_p_prolog(job_env_t* job_env, slurm_cred_t *cred)
{
        slurm_info("Prolog: %s", plugin_name);

        measure_energy(e0);

	return SLURM_SUCCESS;
}

extern int prep_p_epilog(job_env_t* job_env, slurm_cred_t *cred)
{
        slurm_info("Epilog: %s", plugin_name);

        measure_energy(e1);

        for (size_t i = 0; i < devices.size; i++)
        {
            res[i] = e1[i] - e0[i];
        }

        log_measurements(res);
        
	return SLURM_SUCCESS;
}

extern int prep_p_prolog_slurmctld(job_record_t* job_ptr, bool* async)
{
        slurm_info("Ctld_prolog: %s", plugin_name);

	return SLURM_SUCCESS;
}

extern int prep_p_epilog_slurmctld(job_record_t* job_ptr, bool* async)
{
        slurm_info("Ctld_epilog: %s", plugin_name);

	return SLURM_SUCCESS;
}

extern void prep_p_required(prep_call_type_t type, bool* required)
{
    *required = false;
    switch (type)
    {
        case PREP_PROLOG_SLURMCTLD:
            if (running_in_slurmctld())
                *required = true;
            break;
        case PREP_EPILOG_SLURMCTLD:
            if (running_in_slurmctld())
                *required = false;
            break;
        case PREP_PROLOG:
        case PREP_EPILOG:
            if (running_in_slurmd())
                *required = false;
            break;
        default:
            return;
    }
    return;
}
