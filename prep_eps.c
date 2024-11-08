#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>

#include <slurm/slurm.h>
#include <slurm/slurm_errno.h>

#include <EMA.h>

#include "src/interfaces/prep.h"

#define P_NAME "PrEp-EPS: "

const char plugin_name[] = "EPS PrEp plugin";
const char plugin_type[] = "prep/eps";
const uint32_t plugin_version = SLURM_VERSION_NUMBER;

/********************************
 *
 * Slurm Plugin API hooks
 *
 ********************************/

extern int init(void)
{
        slurm_info("Init: %s", plugin_name);

        if (!running_in_slurmd()) return SLURM_SUCCESS;

        int err = _EMA_init(ema_lib_path);

        if (err) {
            if (ema_handle) {
                int ret = dlclose(ema_handle);
                if (ret) slurm_error(dlerror());
            }
            return SLURM_ERROR;
        }

        slurm_info("Initializing EMA...");
        err = ema_init_fn(NULL);
        if (err) {
           slurm_error("EMA Initialization Error: %d", err);
           return SLURM_ERROR;
        }

        devices = ema_get_devices_fn();

        _EMA_log_devices();

        e1 = malloc(devices.size * sizeof(unsigned long long));
        e0 = malloc(devices.size * sizeof(unsigned long long));
        res = malloc(devices.size * sizeof(unsigned long long));

	return SLURM_SUCCESS;
}

extern void fini(void)
{
        slurm_info("Fini: %s", plugin_name);

        if (!running_in_slurmd()) return;

        slurm_info("Finalizing EMA...");
        int err = ema_finalize_fn();
        if (err) {
            slurm_error("Failed to finalize EMA!");
        }

        free(e1);
        free(e0);
        free(res);

        _EMA_fini();
}

extern void prep_p_register_callbacks(prep_callbacks_t* callbacks) {}

extern int prep_p_prolog(job_env_t* job_env, slurm_cred_t *cred)
{
        slurm_info("Prolog: %s", plugin_name);

        _EMA_get_energy(e0);
        _EMA_log_energy(e0);

	return SLURM_SUCCESS;
}

extern int prep_p_epilog(job_env_t* job_env, slurm_cred_t *cred)
{
        uint32_t jobid = get_jobid_from_env(job_env);

        slurm_info("Epilog: %s", plugin_name);
        slurm_info("\tJob ID: %d", jobid);

        _EMA_get_energy(e1);

        for (size_t i = 0; i < devices.size; i++)
        {
            res[i] = e1[i] - e0[i];
        }

        _EMA_log_energy(res);

        unsigned long long total_energy = 0;

        for (size_t i = 0; i < devices.size; i++)
        {
            total_energy = total_energy + res[i];
        }

        slurm_info("Total energy: %lld", total_energy);

        
	return SLURM_SUCCESS;
}

extern int prep_p_prolog_slurmctld(job_record_t* job_ptr, bool* async)
{
        uint32_t jobid = get_jobid_from_record(job_ptr);

        slurm_info("Ctld_prolog: %s", plugin_name);
        slurm_info("\tJob ID: %d", jobid);

	return SLURM_SUCCESS;
}

extern int prep_p_epilog_slurmctld(job_record_t* job_ptr, bool* async)
{
        uint32_t jobid = get_jobid_from_record(job_ptr);

        slurm_info("Ctld_epilog: %s", plugin_name);
        slurm_info("\tJob ID: %d", jobid);

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
