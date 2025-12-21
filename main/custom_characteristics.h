#pragma once

#include <stdbool.h>

#include <homekit/characteristics.h>

#ifndef HOMEKIT_CUSTOM_UUID
#define HOMEKIT_CUSTOM_UUID(value) (value "-0e36-4a42-ad11-745a73b84f2b")
#endif

#define HOMEKIT_SERVICE_CUSTOM_ENERGY_METER HOMEKIT_CUSTOM_UUID("00000010")

#define HOMEKIT_CHARACTERISTIC_CUSTOM_VOLTAGE HOMEKIT_CUSTOM_UUID("00000011")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_VOLTAGE(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_VOLTAGE, \
    .description = "Voltage (V)", \
    .format = homekit_format_float, \
    .unit = homekit_unit_none, \
    .permissions = homekit_permissions_paired_read \
        | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {300}, \
    .min_step = (float[]) {0.1}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_CURRENT HOMEKIT_CUSTOM_UUID("00000012")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_CURRENT(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_CURRENT, \
    .description = "Current (A)", \
    .format = homekit_format_float, \
    .unit = homekit_unit_none, \
    .permissions = homekit_permissions_paired_read \
        | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {100}, \
    .min_step = (float[]) {0.001}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_POWER HOMEKIT_CUSTOM_UUID("00000013")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_POWER(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_POWER, \
    .description = "Active Power (W)", \
    .format = homekit_format_float, \
    .unit = homekit_unit_none, \
    .permissions = homekit_permissions_paired_read \
        | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {10000}, \
    .min_step = (float[]) {0.1}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_ENERGY HOMEKIT_CUSTOM_UUID("00000014")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_ENERGY(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_ENERGY, \
    .description = "Energy (Wh)", \
    .format = homekit_format_float, \
    .unit = homekit_unit_none, \
    .permissions = homekit_permissions_paired_read \
        | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {1000000}, \
    .min_step = (float[]) {0.1}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_POWER_FACTOR HOMEKIT_CUSTOM_UUID("00000015")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_POWER_FACTOR(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_POWER_FACTOR, \
    .description = "Power Factor", \
    .format = homekit_format_float, \
    .unit = homekit_unit_none, \
    .permissions = homekit_permissions_paired_read \
        | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {1}, \
    .min_step = (float[]) {0.01}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_(id, _value, ...) \
    homekit_characteristic_t id##_characteristic = (homekit_characteristic_t)HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_##id(_value, ##__VA_ARGS__)

#define HOMEKIT_CHARACTERISTIC_CUSTOM_VOLTAGE(value, ...) HOMEKIT_CHARACTERISTIC_CUSTOM_(VOLTAGE, value, ##__VA_ARGS__)
#define HOMEKIT_CHARACTERISTIC_CUSTOM_CURRENT(value, ...) HOMEKIT_CHARACTERISTIC_CUSTOM_(CURRENT, value, ##__VA_ARGS__)
#define HOMEKIT_CHARACTERISTIC_CUSTOM_POWER(value, ...) HOMEKIT_CHARACTERISTIC_CUSTOM_(POWER, value, ##__VA_ARGS__)
#define HOMEKIT_CHARACTERISTIC_CUSTOM_ENERGY(value, ...) HOMEKIT_CHARACTERISTIC_CUSTOM_(ENERGY, value, ##__VA_ARGS__)
#define HOMEKIT_CHARACTERISTIC_CUSTOM_POWER_FACTOR(value, ...) HOMEKIT_CHARACTERISTIC_CUSTOM_(POWER_FACTOR, value, ##__VA_ARGS__)

typedef struct {
    homekit_characteristic_t *voltage;
    homekit_characteristic_t *current;
    homekit_characteristic_t *power;
    homekit_characteristic_t *energy;
    homekit_characteristic_t *power_factor;
} bl0937_characteristics_t;

void bl0937_characteristics_update(const bl0937_characteristics_t *characteristics,
                                   float voltage,
                                   float current,
                                   float power,
                                   float energy_wh,
                                   float power_factor,
                                   bool notify);
