#include "custom_characteristics.h"

#include <stddef.h>

static homekit_characteristic_t voltage_characteristic = API_VOLTAGE;
static homekit_characteristic_t current_characteristic = API_CURRENT;
static homekit_characteristic_t power_characteristic = API_POWER;
static homekit_characteristic_t energy_characteristic = API_ENERGY;
static homekit_characteristic_t power_factor_characteristic = API_POWER_FACTOR;
static homekit_characteristic_t frequency_characteristic = API_FREQUENCY;
static homekit_characteristic_t total_consumption_characteristic = API_TOTAL_CONSUMPTION;
static homekit_characteristic_t energy_service_name =
        HOMEKIT_CHARACTERISTIC_(NAME, "Energy Meter");

homekit_service_t custom_characteristics_service = HOMEKIT_SERVICE_(CUSTOM_ENERGY_METER,
        .characteristics = (homekit_characteristic_t *[]) {
                &energy_service_name,
                &voltage_characteristic,
                &current_characteristic,
                &power_characteristic,
                &energy_characteristic,
                &power_factor_characteristic,
                &frequency_characteristic,
                &total_consumption_characteristic,
                NULL
        });

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
                                   float energy,
                                   float power_factor,
                                   float frequency,
                                   float total_consumption) {
        custom_characteristic_set_float(&voltage_characteristic, voltage);
        custom_characteristic_set_float(&current_characteristic, current);
        custom_characteristic_set_float(&power_characteristic, power);
        custom_characteristic_set_float(&energy_characteristic, energy);
        custom_characteristic_set_float(&power_factor_characteristic, power_factor);
        custom_characteristic_set_float(&frequency_characteristic, frequency);
        custom_characteristic_set_float(&total_consumption_characteristic,
                                        total_consumption);
}
