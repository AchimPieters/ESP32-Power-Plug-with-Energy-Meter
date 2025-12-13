#include "custom_characteristics.h"
#include <math.h>

// NOTE:
// We use custom UUIDs so we don't depend on library-specific “known” characteristic IDs.
// Home apps that don't recognize them will still show them in some 3rd-party apps.
// If you prefer official HAP types and your library supports them, we can switch later.

// StudioPieters custom UUID namespace (example UUIDs; keep stable once shipped!)
#define UUID_VOLTAGE "E863F10A-079E-48FF-8F27-9C2605A29F52"
#define UUID_CURRENT "E863F126-079E-48FF-8F27-9C2605A29F52"
#define UUID_POWER   "E863F10D-079E-48FF-8F27-9C2605A29F52"
#define UUID_ENERGY  "E863F10C-079E-48FF-8F27-9C2605A29F52"

// Characteristics
homekit_characteristic_t ch_voltage = HOMEKIT_CHARACTERISTIC_(
    CUSTOM, 0, .type = UUID_VOLTAGE, .description = "Voltage (V)"
);

homekit_characteristic_t ch_current = HOMEKIT_CHARACTERISTIC_(
    CUSTOM, 0, .type = UUID_CURRENT, .description = "Current (A)"
);

homekit_characteristic_t ch_power = HOMEKIT_CHARACTERISTIC_(
    CUSTOM, 0, .type = UUID_POWER, .description = "Power (W)"
);

homekit_characteristic_t ch_energy = HOMEKIT_CHARACTERISTIC_(
    CUSTOM, 0, .type = UUID_ENERGY, .description = "Energy (Wh)"
);

static inline float sane_float(float x) {
    if (isnan(x) || isinf(x)) return 0.0f;
    return x;
}

void custom_characteristics_init(void) {
    ch_voltage.value = HOMEKIT_FLOAT(0.0f);
    ch_current.value = HOMEKIT_FLOAT(0.0f);
    ch_power.value   = HOMEKIT_FLOAT(0.0f);
    ch_energy.value  = HOMEKIT_FLOAT(0.0f);
}

void hk_update_voltage(float v) {
    v = sane_float(v);
    ch_voltage.value = HOMEKIT_FLOAT(v);
    homekit_characteristic_notify(&ch_voltage, ch_voltage.value);
}

void hk_update_current(float a) {
    a = sane_float(a);
    ch_current.value = HOMEKIT_FLOAT(a);
    homekit_characteristic_notify(&ch_current, ch_current.value);
}

void hk_update_power(float w) {
    w = sane_float(w);
    ch_power.value = HOMEKIT_FLOAT(w);
    homekit_characteristic_notify(&ch_power, ch_power.value);
}

void hk_update_energy(float wh) {
    wh = sane_float(wh);
    if (wh < 0) wh = 0;
    ch_energy.value = HOMEKIT_FLOAT(wh);
    homekit_characteristic_notify(&ch_energy, ch_energy.value);
}
