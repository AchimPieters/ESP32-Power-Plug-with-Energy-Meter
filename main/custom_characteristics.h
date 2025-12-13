#pragma once

#include <homekit/homekit.h>
#include <homekit/characteristics.h>

// Exposed characteristics (add these to your service list)
extern homekit_characteristic_t ch_voltage;  // V
extern homekit_characteristic_t ch_current;  // A
extern homekit_characteristic_t ch_power;    // W
extern homekit_characteristic_t ch_energy;   // Wh

// Init + update helpers
void custom_characteristics_init(void);

void hk_update_voltage(float v);
void hk_update_current(float a);
void hk_update_power(float w);
void hk_update_energy(float wh);
