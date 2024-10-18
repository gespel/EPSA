#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>

#include <slurm/slurm.h>
#include <slurm/slurm_errno.h>

#include <EMA.h>

#include "src/interfaces/prep.h"

#define P_NAME "PrEp-EPS: "

extern void slurm_error (const char* format, ...);

const char plugin_name[] = "EPS PrEp plugin";
const char plugin_type[] = "prep/eps";
const uint32_t plugin_version = SLURM_VERSION_NUMBER;

//////////////////////////////
// EMA
//////////////////////////////

static char* ema_lib_path = "/tmp/EMA/lib/libEMA.so";
static void* ema_handle = NULL;

/* EMA function pointers */
static int (*ema_init_fn)(EMA_init_cb) = NULL;
static int (*ema_finalize_fn)() = NULL;
static int (*region_create_and_init_fn)(
    Region** region,
    const char* idf,
    Filter* filter,
    const char* file,
    unsigned int line,
    const char* func
) = NULL;
static int (*region_define_fn)(
    Region** region,
    const char* idf,
    Filter* filter,
    const char* file,
    unsigned int line,
    const char* func
) = NULL;
static int (*region_begin_fn)(Region* region) = NULL;
static int (*region_end_fn)(Region* region) = NULL;
static int (*region_finalize_fn)(Region* region) = NULL;
static int (*print_all_fn)(FILE* f) = NULL;

/* Plugin specific EMA functions */
int _EMA_init(const char* path)
{

    ema_handle = dlopen(path, RTLD_LAZY);

    if (!ema_handle) {
        slurm_error(dlerror());
        return 1;
    }

    ema_init_fn = dlsym(ema_handle, "EMA_init");
    char* error = dlerror();

    if (!ema_init_fn || error) {
        slurm_error(error);
        dlclose(ema_handle);
        return 1;
    }

    ema_finalize_fn = dlsym(ema_handle, "EMA_finalize");
    error = dlerror();

    if (!ema_finalize_fn || error) {
        slurm_error(error);
        dlclose(ema_handle);
        return 1;
    }

    region_create_and_init_fn = dlsym(ema_handle, "EMA_region_create_and_init");
    error = dlerror();

    if (!region_create_and_init_fn || error) {
        slurm_error(error);
        dlclose(ema_handle);
        return 1;
    }

    region_define_fn = dlsym(ema_handle, "EMA_region_define");
    error = dlerror();

    if (!region_define_fn || error) {
        slurm_error(error);
        dlclose(ema_handle);
        return 1;
    }

    region_begin_fn = dlsym(ema_handle, "EMA_region_begin");
    error = dlerror();

    if (!region_begin_fn || error) {
        slurm_error(error);
        dlclose(ema_handle);
        return 1;
    }

    region_end_fn = dlsym(ema_handle, "EMA_region_end");
    error = dlerror();

    if (!region_end_fn || error) {
        slurm_error(error);
        dlclose(ema_handle);
        return 1;
    }

    region_finalize_fn = dlsym(ema_handle, "EMA_region_finalize");
    error = dlerror();

    if (!region_finalize_fn || error) {
        slurm_error(error);
        dlclose(ema_handle);
        return 1;
    }

    print_all_fn = dlsym(ema_handle, "EMA_print_all");
    error = dlerror();

    if (!print_all_fn || error) {
        slurm_error(error);
        dlclose(ema_handle);
        return 1;
    }

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

/* EMA Region */
static Region* region = NULL;

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

        slurm_info("Defining EMA region...");
        int err = region_define_fn(
            &region,
            "slurm_job_region",
            NULL,
            __FILE__,
            207,
            __func__
        );
        if (err) {
            slurm_error("Failed to create and initialize EMA Region!");
            return SLURM_ERROR;
        }

        slurm_info("Starting EMA region...");
        err = region_begin_fn(region);
        if (err) {
            slurm_error("Failed to start EMA Region!");
            return SLURM_ERROR;
        }


	return SLURM_SUCCESS;
}

extern int prep_p_epilog(job_env_t* job_env, slurm_cred_t *cred)
{
        uint32_t jobid = get_jobid_from_env(job_env);

        slurm_info("Epilog: %s", plugin_name);
        slurm_info("\tJob ID: %d", jobid);

        slurm_info("Ending EMA region...");
        int err = region_end_fn(region);
        if (err) {
            slurm_error("Failed to end EMA Region!");
            return SLURM_ERROR;
        }

        slurm_info("Printing EMA measurements to stdout...");
        err = print_all_fn(stdout);
        if (err) {
            slurm_error("EMA print all function failed! ");
            return SLURM_ERROR;
        }
        
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
