#include <stdlib.h>
#include <string.h>

#include <nvml.h>
#include <src/interfaces/prep.h>

#include <eps_nvml.h>

#define NVML_HANDLE_RET(RET, FN) do { \
    if (RET != NVML_SUCCESS) \
    { \
        slurm_info("error: " FN ": %s", nvmlErrorString(RET)); \
        free(gres_uuid_list); \
        free(gres_idxs); \
        return 1; \
    } \
} while(0)


int nvml_process_gres(
    unsigned int* gres_idxs,
    char** gres_uuid_list,
    int* gres_uuid_count,
    size_t gres_count
)
{
    nvmlReturn_t ret = nvmlInitWithFlags(NVML_INIT_FLAG_NO_GPUS);
    NVML_HANDLE_RET(ret, "nvmlInitWithFlags");

    unsigned int device_count;
    ret = nvmlDeviceGetCount_v2(&device_count);
    NVML_HANDLE_RET(ret, "nvmlDeviceGetCount_v2");

    for (unsigned int i = 0; i < device_count; i++)
    {
        nvmlDevice_t handle;
        ret = nvmlDeviceGetHandleByIndex_v2(i, &handle);
        NVML_HANDLE_RET(ret, "nvmlDeviceGetHandleByIndex_v2");

        unsigned int minor;
        ret = nvmlDeviceGetMinorNumber(handle, &minor);
        NVML_HANDLE_RET(ret, "nvmlDeviceGetMinorNumber");

        int match = 0;
        for (int i = 0; i < gres_count; i++)
        {
            unsigned int idx = gres_idxs[i];
            if (minor == idx) match = 1;
        }

        if (!match) continue;

        char uuid[NVML_DEVICE_UUID_BUFFER_SIZE];
        ret = nvmlDeviceGetUUID(
            handle,
            uuid,
            NVML_DEVICE_UUID_BUFFER_SIZE
        );
        NVML_HANDLE_RET(ret, "nvmlDeviceGetUUID");

        gres_uuid_list[*gres_uuid_count] = strdup(uuid);
        *gres_uuid_count = *gres_uuid_count + 1;
    }

    ret = nvmlShutdown();
    if (ret != NVML_SUCCESS)
    {
        slurm_info("error: nvmlShutdown: %s", nvmlErrorString(ret));
        return 1;
    }
    return 0;
}
