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

/////////////////////////////
// Internal Data Structures
/////////////////////////////
typedef unsigned long long* Measurements;

/////////////////////////////
// Logging/Utils
/////////////////////////////
extern void slurm_error (const char* format, ...);

void slurm_warn(const char* msg)
{
    slurm_info("warn: %s", msg);
}

//////////////////////////////
// EMA
//////////////////////////////

DevicePtrArray devices;

static char* ema_lib_path = "/tmp/EMA/lib/libEMA.so";
static void* ema_handle = NULL;

Measurements e1 = NULL;
Measurements e0 = NULL;

/* EMA function pointers */
static int (*ema_init_fn)(EMA_init_cb) = NULL;
static int (*ema_finalize_fn)() = NULL;
static DevicePtrArray (*ema_get_devices_fn)() = NULL;
static char* (*ema_get_device_name_fn)() = NULL;
static unsigned long long (*ema_device_get_energy_fn)() = NULL;

/* Plugin specific EMA functions */
Measurements _EMA_get_energy()
{
    slurm_info("Getting energy values...");
    if (!devices.size) return NULL;

    Measurements m = malloc(devices.size * sizeof(unsigned long long));
    for (size_t i = 0; i < devices.size; i++)
    {
        m[i] = ema_device_get_energy_fn(devices.array[i]);
    }

    return m;
}

void _EMA_log_energy(const Measurements m)
{
        for (size_t i = 0; i < devices.size; i++)
            slurm_info("%s: %lld", ema_get_device_name_fn(
                devices.array[i]), m[i]
            );
}

void _EMA_log_devices()
{
        if (!devices.size) {
            slurm_warn("No EMA devices!");
            return;
        };

        slurm_info("Num EMA devices: %ld", devices.size);
        slurm_info("EMA devices:");
        for (size_t i = 0; i < devices.size; i++)
            slurm_info("\t%s", ema_get_device_name_fn(devices.array[i]));
}

int _EMA_load_symbol(void** container, const char* name)
{
    *container = dlsym(ema_handle, name);
    char* error = dlerror();
    
    if (!(*container) || error) {
        slurm_error(error);
        return 1;
    }
    return 0;
}

int _EMA_init(const char* path)
{

    ema_handle = dlopen(path, RTLD_LAZY);

    if (!ema_handle) {
        slurm_error(dlerror());
        return 1;
    }

    int err = _EMA_load_symbol((void**)&ema_init_fn, "EMA_init");
    if (err) return err;

    err = _EMA_load_symbol((void**)&ema_finalize_fn, "EMA_finalize");
    if (err) return err;

    err = _EMA_load_symbol((void*)&ema_get_devices_fn, "EMA_get_devices");
    if (err) return err;

    err = _EMA_load_symbol((void*)&ema_get_device_name_fn, "EMA_get_device_name");
    if (err) return err;

    err = _EMA_load_symbol((void*)&ema_device_get_energy_fn, "EMA_get_energy_uj");
    if (err) return err;

    return 0;
}

void _EMA_fini()
{
    ema_init_fn = NULL;
    ema_finalize_fn = NULL;

    int err = dlclose(ema_handle);
    if (err) {
        slurm_error(dlerror());
    }
}

//////////////////////////////
// Data obtaining functions
//////////////////////////////

uint32_t get_jobid_from_env(job_env_t* job_env)
{
    return job_env->jobid;
}

uint32_t get_jobid_from_record(job_record_t* job)
{
    return job->job_id;
}

uint32_t get_userid_from_env(job_env_t* job_env)
{
    return job_env->uid;
}

//////////////////////////////
// Slurm Plugin API hooks
/////////////////////////////

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

        _EMA_fini();
}

extern void prep_p_register_callbacks(prep_callbacks_t* callbacks) {}

extern int prep_p_prolog(job_env_t* job_env, slurm_cred_t *cred)
{
        uint32_t jobid = get_jobid_from_env(job_env);
        uint32_t uid = get_userid_from_env(job_env);

        slurm_info("Prolog: %s", plugin_name);
        slurm_info("\tJob ID: %d", jobid);
        slurm_info("\tUser ID: %d", uid);

        char* ld_path = getenv("LD_LIBRARY_PATH");
        char* lib_path = getenv("LIBRARY_PATH");

        slurm_info("\tLD lib path: %s", ld_path);
        slurm_info("\tLib path: %s", lib_path);

	return SLURM_SUCCESS;
}

extern int prep_p_epilog(job_env_t* job_env, slurm_cred_t *cred)
{
        uint32_t jobid = get_jobid_from_env(job_env);

        slurm_info("Epilog: %s", plugin_name);
        slurm_info("\tJob ID: %d", jobid);

        DevicePtrArray devices = ema_get_devices_fn();

        slurm_info("Num EMA devices: %ld", devices.size);
        slurm_info("EMA devices:");
        for (size_t i = 0; i < devices.size; i++)
            slurm_info("\t%s", ema_get_device_name_fn(devices.array[i]));
        
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
