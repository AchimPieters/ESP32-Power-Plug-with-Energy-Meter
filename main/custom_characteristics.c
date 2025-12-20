/**
   Copyright 2026 Achim Pieters | StudioPieters®

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NON INFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   for more information visit https://www.studiopieters.nl
 **/

#include "custom_characteristics.h"
#include <math.h>

// -----------------------------------------------------------------------------
// Custom UUIDs (keep these stable once shipped)
// -----------------------------------------------------------------------------
#define UUID_VOLTAGE "E863F10A-079E-48FF-8F27-9C2605A29F52"
#define UUID_CURRENT "E863F126-079E-48FF-8F27-9C2605A29F52"
#define UUID_POWER   "E863F10D-079E-48FF-8F27-9C2605A29F52"
#define UUID_ENERGY  "E863F10C-079E-48FF-8F27-9C2605A29F52"

// -----------------------------------------------------------------------------
// Characteristics (manual init — no macros, no override-init warnings)
// -----------------------------------------------------------------------------
homekit_characteristic_t ch_voltage = {
    .type = UUID_VOLTAGE,
    .description = "Voltage (V)",
    .format = homekit_format_float,
    .permissions = homekit_permissions_paired_read | homekit_permissions_notify,
    .value = HOMEKIT_FLOAT(0.0f),
};

homekit_characteristic_t ch_current = {
    .type = UUID_CURRENT,
    .description = "Current (A)",
    .format = homekit_format_float,
    .permissions = homekit_permissions_paired_read | homekit_permissions_notify,
    .value = HOMEKIT_FLOAT(0.0f),
};

homekit_characteristic_t ch_power = {
    .type = UUID_POWER,
    .description = "Power (W)",
    .format = homekit_format_float,
    .permissions = homekit_permissions_paired_read | homekit_permissions_notify,
    .value = HOMEKIT_FLOAT(0.0f),
};

homekit_characteristic_t ch_energy = {
    .type = UUID_ENERGY,
    .description = "Energy (Wh)",
    .format = homekit_format_float,
    .permissions = homekit_permissions_paired_read | homekit_permissions_notify,
    .value = HOMEKIT_FLOAT(0.0f),
};

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------
static inline float sane_float(float x) {
    if (isnan(x) || isinf(x)) return 0.0f;
    return x;
}

static bool notifications_ready = false;

void custom_characteristics_set_notify_ready(bool ready) {
    notifications_ready = ready;
}

// -----------------------------------------------------------------------------
// Init
// -----------------------------------------------------------------------------
void custom_characteristics_init(void) {
    ch_voltage.value = HOMEKIT_FLOAT(0.0f);
    ch_current.value = HOMEKIT_FLOAT(0.0f);
    ch_power.value   = HOMEKIT_FLOAT(0.0f);
    ch_energy.value  = HOMEKIT_FLOAT(0.0f);
}

// -----------------------------------------------------------------------------
// Update + notify helpers
// -----------------------------------------------------------------------------
void hk_update_voltage(float v) {
    v = sane_float(v);
    ch_voltage.value = HOMEKIT_FLOAT(v);
    if (notifications_ready) {
        homekit_characteristic_notify(&ch_voltage, ch_voltage.value);
    }
}

void hk_update_current(float a) {
    a = sane_float(a);
    ch_current.value = HOMEKIT_FLOAT(a);
    if (notifications_ready) {
        homekit_characteristic_notify(&ch_current, ch_current.value);
    }
}

void hk_update_power(float w) {
    w = sane_float(w);
    ch_power.value = HOMEKIT_FLOAT(w);
    if (notifications_ready) {
        homekit_characteristic_notify(&ch_power, ch_power.value);
    }
}

void hk_update_energy(float wh) {
    wh = sane_float(wh);
    if (wh < 0.0f) wh = 0.0f;
    ch_energy.value = HOMEKIT_FLOAT(wh);
    if (notifications_ready) {
        homekit_characteristic_notify(&ch_energy, ch_energy.value);
    }
}
