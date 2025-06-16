#include <eps_gres.h>
#include <eps_utils.h>

#include <src/interfaces/prep.h>


int process_gres_count(
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
