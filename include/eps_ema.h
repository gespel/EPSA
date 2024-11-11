#ifndef _EPS_EMA_H
#define _EPS_EMA_H

#include <EMA.h>

typedef unsigned long long* Measurements;

void log_ema_devices();

void measure_energy(Measurements m);
void log_measurements(Measurements m);

#endif
