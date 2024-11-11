#include <eps_ema.h>
#include <eps_utils.h>

extern DevicePtrArray devices;

void log_ema_devices()
{
    if (!devices.size) {
        slurm_warn("No EMA devices!");
        return;
    };

    slurm_info("Num EMA devices: %ld", devices.size);
    slurm_info("EMA devices:");
    for (size_t i = 0; i < devices.size; i++)
        slurm_info("\t%s", EMA_get_device_name(devices.array[i]));
}
