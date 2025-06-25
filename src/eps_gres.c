#include <eps_gres.h>
#include <eps_nvml.h>
#include <eps_utils.h>

#include <src/interfaces/prep.h>

static int _process_gres_count(
  node_info_t node_rec,
  char* idx,
  unsigned int* gres_idxs,
  size_t* gres_count
)
{
    int ret = parse_gres(node_rec.gres_used, &idx);
    if (ret < 0)
    {
        slurm_info("Failed to parse gres_used!");
        return 1;
    }
    if (ret > 0)
    {
        gres_idxs = parse_index_range(idx, gres_count);
        free(idx);
        if (gres_count && !gres_idxs)
        {
            slurm_info("Failed to parse gres indexes substring!");
            return 1;
        }
    }
    return 0;
}

static int _process_gres(
    char** gres_uuid_list,
    int* gres_uuid_count,
    node_info_msg_t* node_info_list
)
{
    if (node_info_list->record_count > 0)
    {
        node_info_t node_rec = node_info_list->node_array[0];

        size_t gres_count = 0;
        unsigned int* gres_idxs = NULL;
        char* idx = NULL;

        int err = _process_gres_count(
            node_rec,
            idx,
            gres_idxs,
            &gres_count
        );
        if (err)
        {
            slurm_info("error: process_gres_count");
            return 1;
        }

        if (gres_count > 0)
        {
            #ifdef HAS_NVML
            gres_uuid_list = (char **)malloc(gres_count * sizeof(char*));
            err = nvml_process_gres(
                gres_idxs,
                gres_uuid_list,
                gres_uuid_count,
                gres_count
            );
            if (err) slurm_info("error: process_gres");
            #endif
        }
        free(gres_idxs);
    }
    return 0;
}

int init_gres(char** gres_uuid_list, int* gres_uuid_count)
{
    char hostname[HOST_NAME_MAX];
    hostname[0] = '\0';

    int err = gethostname(hostname, HOST_NAME_MAX);
    if (err)
    {
        perror("gethostname: ");
        slurm_info("error: gethostname");
        return 1;
    }

    uint16_t show_flags = 0;
    show_flags |= SHOW_ALL;
    show_flags |= SHOW_DETAIL;

    node_info_msg_t* node_info_list = NULL;
    err = slurm_load_node_single(&node_info_list, hostname, show_flags);
    if (err != SLURM_SUCCESS)
    {
        slurm_info("error: slurm_load_node_single: %d", err);
        return 1;
    }

    err = _process_gres(gres_uuid_list, gres_uuid_count, node_info_list);
    slurm_free_node_info_msg(node_info_list);
    if (err)
    {
        slurm_info("error: process_gres: %d", err);
        return err;
    }
    return 0;
}
