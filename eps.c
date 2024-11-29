#include <eps_db.h>
#include <slurm/spank.h>

#define PLUGIN_NAME "Spank/Eps"

SPANK_PLUGIN(eps, 1)

/********************************
 *
 * Spank functions
 *
 ********************************/
int slurm_spank_init(spank_t sp, int ac, char **av) {
    slurm_info("Init: " PLUGIN_NAME);

    if (spank_context() == S_CTX_LOCAL) {
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
