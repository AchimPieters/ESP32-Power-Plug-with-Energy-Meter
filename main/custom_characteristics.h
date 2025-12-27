#pragma once

#include <homekit/characteristics.h>
#include <homekit/homekit.h>

#ifndef HOMEKIT_CUSTOM_UUID
#define HOMEKIT_CUSTOM_UUID(value) (value "-0e36-4a42-ad11-745a73b84f2b")
#endif

#define HOMEKIT_CHARACTERISTIC_CUSTOM_VOLTAGE "E863F10A-079E-48FF-8F27-9C2605A29F52"
#define HOMEKIT_CHARACTERISTIC_CUSTOM_CURRENT "E863F126-079E-48FF-8F27-9C2605A29F52"
#define HOMEKIT_CHARACTERISTIC_CUSTOM_POWER "E863F10D-079E-48FF-8F27-9C2605A29F52"
#define HOMEKIT_CHARACTERISTIC_CUSTOM_TOTAL_CONSUMPTION "E863F10C-079E-48FF-8F27-9C2605A29F52"
#define HOMEKIT_CHARACTERISTIC_CUSTOM_POWER_FACTOR "E863F110-079E-48FF-8F27-9C2605A29F52"

#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_VOLTAGE(_value, ...) \
        .type = HOMEKIT_CHARACTERISTIC_CUSTOM_VOLTAGE, \
        .description = "Voltage", \
        .format = homekit_format_float, \
        .permissions = homekit_permissions_paired_read \
                       | homekit_permissions_notify, \
        .min_value = (float[]){0}, \
        .max_value = (float[]){400}, \
        .min_step = (float[]){0.1f}, \
        .value = HOMEKIT_FLOAT_(_value), \
        ## __VA_ARGS__

#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_CURRENT(_value, ...) \
        .type = HOMEKIT_CHARACTERISTIC_CUSTOM_CURRENT, \
        .description = "Current", \
        .format = homekit_format_float, \
        .permissions = homekit_permissions_paired_read \
                       | homekit_permissions_notify, \
        .min_value = (float[]){0}, \
        .max_value = (float[]){32}, \
        .min_step = (float[]){0.01f}, \
        .value = HOMEKIT_FLOAT_(_value), \
        ## __VA_ARGS__

#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_POWER(_value, ...) \
        .type = HOMEKIT_CHARACTERISTIC_CUSTOM_POWER, \
        .description = "Power", \
        .format = homekit_format_float, \
        .permissions = homekit_permissions_paired_read \
                       | homekit_permissions_notify, \
        .min_value = (float[]){0}, \
        .max_value = (float[]){4000}, \
        .min_step = (float[]){0.1f}, \
        .value = HOMEKIT_FLOAT_(_value), \
        ## __VA_ARGS__

#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_TOTAL_CONSUMPTION(_value, ...) \
        .type = HOMEKIT_CHARACTERISTIC_CUSTOM_TOTAL_CONSUMPTION, \
        .description = "Total Consumption", \
        .format = homekit_format_float, \
        .permissions = homekit_permissions_paired_read \
                       | homekit_permissions_notify, \
        .min_value = (float[]){0}, \
        .min_step = (float[]){0.01f}, \
        .value = HOMEKIT_FLOAT_(_value), \
        ## __VA_ARGS__

#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_POWER_FACTOR(_value, ...) \
        .type = HOMEKIT_CHARACTERISTIC_CUSTOM_POWER_FACTOR, \
        .description = "Power Factor", \
        .format = homekit_format_float, \
        .unit = homekit_unit_percentage, \
        .permissions = homekit_permissions_paired_read \
                       | homekit_permissions_notify, \
        .min_value = (float[]){0}, \
        .max_value = (float[]){100}, \
        .min_step = (float[]){1}, \
        .value = HOMEKIT_FLOAT_(_value), \
        ## __VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_VOLTAGE(...) HOMEKIT_CHARACTERISTIC_(CUSTOM_VOLTAGE, __VA_ARGS__)
#define HOMEKIT_CHARACTERISTIC_CUSTOM_CURRENT(...) HOMEKIT_CHARACTERISTIC_(CUSTOM_CURRENT, __VA_ARGS__)
#define HOMEKIT_CHARACTERISTIC_CUSTOM_POWER(...) HOMEKIT_CHARACTERISTIC_(CUSTOM_POWER, __VA_ARGS__)
#define HOMEKIT_CHARACTERISTIC_CUSTOM_TOTAL_CONSUMPTION(...) HOMEKIT_CHARACTERISTIC_(CUSTOM_TOTAL_CONSUMPTION, __VA_ARGS__)
#define HOMEKIT_CHARACTERISTIC_CUSTOM_POWER_FACTOR(...) HOMEKIT_CHARACTERISTIC_(CUSTOM_POWER_FACTOR, __VA_ARGS__)

extern homekit_characteristic_t custom_voltage;
extern homekit_characteristic_t custom_current;
extern homekit_characteristic_t custom_power;
extern homekit_characteristic_t custom_total_consumption;
extern homekit_characteristic_t custom_power_factor;

void custom_characteristics_init(void);
