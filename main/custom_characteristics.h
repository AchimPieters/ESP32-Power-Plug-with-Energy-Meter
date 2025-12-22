#pragma once

#include <homekit/homekit.h>
#include <homekit/characteristics.h>

#ifndef HOMEKIT_CUSTOM_UUID
#define HOMEKIT_CUSTOM_UUID(value) (value "-0e36-4a42-ad11-745a73b84f2b")
#endif

#define HOMEKIT_SERVICE_CUSTOM_ENERGY_METER HOMEKIT_CUSTOM_UUID("00000010")

#define HOMEKIT_CHARACTERISTIC_CUSTOM_POWER HOMEKIT_CUSTOM_UUID("00000011")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_POWER(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_POWER, \
    .description = "Power", \
    .format = homekit_format_float, \
    .permissions = homekit_permissions_paired_read | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {3680}, \
    .min_step = (float[]) {0.1f}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_VOLTAGE HOMEKIT_CUSTOM_UUID("00000012")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_VOLTAGE(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_VOLTAGE, \
    .description = "Voltage", \
    .format = homekit_format_float, \
    .permissions = homekit_permissions_paired_read | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {260}, \
    .min_step = (float[]) {0.1f}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_CURRENT HOMEKIT_CUSTOM_UUID("00000013")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_CURRENT(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_CURRENT, \
    .description = "Current", \
    .format = homekit_format_float, \
    .permissions = homekit_permissions_paired_read | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {16}, \
    .min_step = (float[]) {0.01f}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_TOTAL_CONSUMPTION HOMEKIT_CUSTOM_UUID("00000014")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_TOTAL_CONSUMPTION(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_TOTAL_CONSUMPTION, \
    .description = "TotalConsumption", \
    .format = homekit_format_float, \
    .permissions = homekit_permissions_paired_read | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {100000}, \
    .min_step = (float[]) {0.01f}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

void custom_characteristics_update_float(homekit_characteristic_t *characteristic,
                                         float value,
                                         float epsilon);
