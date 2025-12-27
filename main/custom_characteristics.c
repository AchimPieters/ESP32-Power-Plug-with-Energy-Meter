#include "custom_characteristics.h"

void custom_characteristics_init(void) {
        custom_voltage = HOMEKIT_CHARACTERISTIC_(CUSTOM_VOLTAGE, 0);
        custom_current = HOMEKIT_CHARACTERISTIC_(CUSTOM_CURRENT, 0);
        custom_power = HOMEKIT_CHARACTERISTIC_(CUSTOM_POWER, 0);
        custom_total_consumption = HOMEKIT_CHARACTERISTIC_(CUSTOM_TOTAL_CONSUMPTION, 0);
        custom_power_factor = HOMEKIT_CHARACTERISTIC_(CUSTOM_POWER_FACTOR, 0);
}
