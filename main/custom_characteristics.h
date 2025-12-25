#pragma once

#include <homekit/characteristics.h>
#include <homekit/homekit.h>

#define HOMEKIT_CHARACTERISTIC_CUSTOM_VOLTAGE "E863F10A-079E-48FF-8F27-9C2605A29F52"
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_VOLTAGE(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_VOLTAGE, \
    .description = "Voltage", \
    .format = homekit_format_float, \
    .permissions = homekit_permissions_paired_read \
        | homekit_permissions_notify, \
    .min_value = (float[]){0}, \
    .max_value = (float[]){400}, \
    .min_step = (float[]){0.1}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_VOLTAGE_(...) (homekit_characteristic_t){{HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_VOLTAGE(__VA_ARGS__)}}

#define HOMEKIT_CHARACTERISTIC_CUSTOM_CURRENT "E863F126-079E-48FF-8F27-9C2605A29F52"
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_CURRENT(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_CURRENT, \
    .description = "Current", \
    .format = homekit_format_float, \
    .permissions = homekit_permissions_paired_read \
        | homekit_permissions_notify, \
    .min_value = (float[]){0}, \
    .max_value = (float[]){63}, \
    .min_step = (float[]){0.01}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_CURRENT_(...) (homekit_characteristic_t){{HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_CURRENT(__VA_ARGS__)}}

#define HOMEKIT_CHARACTERISTIC_CUSTOM_POWER "E863F10D-079E-48FF-8F27-9C2605A29F52"
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_POWER(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_POWER, \
    .description = "Power", \
    .format = homekit_format_float, \
    .permissions = homekit_permissions_paired_read \
        | homekit_permissions_notify, \
    .min_value = (float[]){0}, \
    .max_value = (float[]){3600}, \
    .min_step = (float[]){0.1}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_POWER_(...) (homekit_characteristic_t){{HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_POWER(__VA_ARGS__)}}

#define HOMEKIT_CHARACTERISTIC_CUSTOM_TOTAL_CONSUMPTION "E863F10C-079E-48FF-8F27-9C2605A29F52"
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_TOTAL_CONSUMPTION(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_TOTAL_CONSUMPTION, \
    .description = "Total Consumption", \
    .format = homekit_format_float, \
    .permissions = homekit_permissions_paired_read \
        | homekit_permissions_notify, \
    .min_value = (float[]){0}, \
    .max_value = (float[]){4294967295.0f}, \
    .min_step = (float[]){0.001}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_TOTAL_CONSUMPTION_(...) (homekit_characteristic_t){{HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_TOTAL_CONSUMPTION(__VA_ARGS__)}}

extern homekit_characteristic_t voltage_characteristic;
extern homekit_characteristic_t current_characteristic;
extern homekit_characteristic_t power_characteristic;
extern homekit_characteristic_t total_consumption_characteristic;

