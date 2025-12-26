#include "custom_characteristics.h"

homekit_characteristic_t voltage_characteristic = API_VOLTAGE;
homekit_characteristic_t current_characteristic = API_CURRENT;
homekit_characteristic_t power_characteristic = API_POWER;
homekit_characteristic_t power_factor_characteristic = API_POWER_FACTOR;
homekit_characteristic_t frequency_characteristic = API_FREQUENCY;
homekit_characteristic_t total_consumption_characteristic = API_TOTAL_CONSUMPTION;

static void custom_characteristic_set_float(homekit_characteristic_t *characteristic,
                                            float value) {
        if (characteristic->value.format != homekit_format_float) {
                characteristic->value = HOMEKIT_FLOAT(value);
                homekit_characteristic_notify(characteristic, characteristic->value);
                return;
        }

        if (characteristic->value.float_value == value) {
                return;
        }

        characteristic->value.float_value = value;
        homekit_characteristic_notify(characteristic, characteristic->value);
}

void custom_characteristics_update(float voltage,
                                   float current,
                                   float power,
                                   float power_factor,
                                   float frequency,
                                   float total_consumption) {
        custom_characteristic_set_float(&voltage_characteristic, voltage);
        custom_characteristic_set_float(&current_characteristic, current);
        custom_characteristic_set_float(&power_characteristic, power);
        custom_characteristic_set_float(&power_factor_characteristic, power_factor);
        custom_characteristic_set_float(&frequency_characteristic, frequency);
        custom_characteristic_set_float(&total_consumption_characteristic,
                                        total_consumption);
}
