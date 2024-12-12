#include <slurm/slurm.h>
#include <slurm/slurm_errno.h>

#include <eps_data.h>
#include <eps_db.h>

#include <src/interfaces/prep.h>


const char plugin_name[] = "EPS PrEp plugin";
const char plugin_type[] = "prep/eps";
const uint32_t plugin_version = SLURM_VERSION_NUMBER;

unsigned long long tstart, tend;
time_t timestamp;

/********************************
 *
 * Slurm Plugin API hooks
 *
 ********************************/

extern int init(void)
{
        slurm_info("Init: %s", plugin_name);
	return SLURM_SUCCESS;
}

extern void fini(void)
{
        slurm_info("Fini: %s", plugin_name);
}

extern void prep_p_register_callbacks(prep_callbacks_t* callbacks) {}

extern int prep_p_prolog(job_env_t* job_env, slurm_cred_t *cred)
{
        slurm_info("Prolog: %s", plugin_name);
	return SLURM_SUCCESS;
}

extern int prep_p_epilog(job_env_t* job_env, slurm_cred_t *cred)
{
        slurm_info("Epilog: %s", plugin_name);
	return SLURM_SUCCESS;
}

extern int prep_p_prolog_slurmctld(job_record_t* job_ptr, bool* async)
{
        slurm_info("Ctld_prolog: %s", plugin_name);

        slurm_info("Collecting job allocation data...");
        eps_allocation_data_t* data = get_allocation_data(job_ptr);

        PGconn* db_connection = connect_db();

        int connection_is_not_ok = check_connection(db_connection);

        if (connection_is_not_ok) {
            slurm_info(
                "error: problems with db connection: %s",
                PQerrorMessage(db_connection)
            );
            PQfinish(db_connection);
            return SLURM_ERROR;
        }
        int err = insert_allocation_data(db_connection, data);
        free_allocation_data(data);

        if (err) {
            slurm_info("error: failed to write data to db");
            return SLURM_ERROR;
        }

        slurm_info("Closing DB connection...");
        PQfinish(db_connection);

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
