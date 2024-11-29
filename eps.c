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
int slurm_spank_local_user_init(spank_t sp, int ac, char **av) {
    slurm_info("Init: " PLUGIN_NAME);

    if (spank_context() == S_CTX_LOCAL) {
        time_t now;
        uint32_t jid, nnodes;
        uint32_t uid = getuid();

        time(&now);

        spank_err_t err = spank_get_item(sp, S_JOB_ID, &jid);
        if (err) {
            slurm_info("jobid: error: %s", spank_strerror(err));
            return 1;
        }

        err = spank_get_item(sp, S_JOB_NNODES, &nnodes);
        if (err) {
            slurm_info("nnodes: error: %s", spank_strerror(err));
            return 1;
        }

        eps_meta_data_t data;

        data.jobid = (int)jid;
        data.userid = (int)uid;
        data.nnodes = (int)nnodes;
        data.tstart = now;
        data.resources = NULL;

        PGconn* db_connection = connect_db();

        int connection_is_not_ok = check_connection(db_connection);

        if (connection_is_not_ok) {
            slurm_error(
                "problems with db connection: %s",
                PQerrorMessage(db_connection)
            );
            PQfinish(db_connection);
            return 1;
        }

        
        int insert_err = insert_meta_data(db_connection, &data);
        if (insert_err) {
            slurm_error("failed to write data to db");
        }

        slurm_info("Closing DB connection...");
        PQfinish(db_connection);
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
