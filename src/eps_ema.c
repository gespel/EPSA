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

void measure_energy(Measurements m)
{
    if (!devices.size) {
        slurm_warn("No devices to measure energy!");
        return;
    };

    slurm_info("Measuring energy values...");
    for (size_t i = 0; i < devices.size; i++)
    {
        m[i] = EMA_get_energy_uj(devices.array[i]);
    }
}

void log_measurements(Measurements m)
{
    for (size_t i = 0; i < devices.size; i++)
            slurm_info("%s: %lld", EMA_get_device_name(
                devices.array[i]), m[i]
            );
}
