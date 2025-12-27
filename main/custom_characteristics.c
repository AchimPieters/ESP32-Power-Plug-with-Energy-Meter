#include "custom_characteristics.h"

homekit_characteristic_t custom_voltage;
homekit_characteristic_t custom_current;
homekit_characteristic_t custom_power;
homekit_characteristic_t custom_total_consumption;
homekit_characteristic_t custom_power_factor;

void custom_characteristics_init(void) {
        custom_voltage = HOMEKIT_CHARACTERISTIC_(CUSTOM_VOLTAGE, 0);
        custom_current = HOMEKIT_CHARACTERISTIC_(CUSTOM_CURRENT, 0);
        custom_power = HOMEKIT_CHARACTERISTIC_(CUSTOM_POWER, 0);
        custom_total_consumption = HOMEKIT_CHARACTERISTIC_(CUSTOM_TOTAL_CONSUMPTION, 0);
        custom_power_factor = HOMEKIT_CHARACTERISTIC_(CUSTOM_POWER_FACTOR, 0);
}
