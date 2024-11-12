#include <eps_ema.h>
#include <eps_utils.h>
#include <stdio.h>

extern DevicePtrArray devices;

void log_ema_devices()
{
    if (!devices.size) {
        WARN("No EMA devices!");
        return;
    };

    INFO("Num EMA devices: %ld", devices.size);
    INFO("EMA devices:");
    for (size_t i = 0; i < devices.size; i++)
        printf("\t%s", EMA_get_device_name(devices.array[i]));
}

void measure_energy(Measurements m)
{
    if (!devices.size) {
        WARN("No devices to measure energy!");
        return;
    };

    INFO("Measuring energy values...");
    for (size_t i = 0; i < devices.size; i++)
    {
        m[i] = EMA_get_energy_uj(devices.array[i]);
    }
}

void print_measurements(Measurements m)
{
    for (size_t i = 0; i < devices.size; i++)
            printf("%s: %lld", EMA_get_device_name(
                devices.array[i]), m[i]
            );
}
