#pragma once

#include <homekit/characteristics.h>

#ifndef HOMEKIT_CUSTOM_UUID
#define HOMEKIT_CUSTOM_UUID(value) (value "-0e36-4a42-ad11-745a73b84f2b")
#endif

#define HOMEKIT_SERVICE_CUSTOM_ENERGY_METER HOMEKIT_CUSTOM_UUID("000000A0")

#define HOMEKIT_CHARACTERISTIC_CUSTOM_VOLTAGE HOMEKIT_CUSTOM_UUID("000000A1")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_VOLTAGE(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_VOLTAGE, \
    .description = "Voltage", \
    .format = homekit_format_float, \
    .permissions = homekit_permissions_paired_read \
        | homekit_permissions_notify, \
    .unit = homekit_unit_volt, \
    .min_value = HOMEKIT_FLOAT_(0), \
    .max_value = HOMEKIT_FLOAT_(260), \
    .step_value = HOMEKIT_FLOAT_(0.1f), \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_CURRENT HOMEKIT_CUSTOM_UUID("000000A2")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_CURRENT(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_CURRENT, \
    .description = "Current", \
    .format = homekit_format_float, \
    .permissions = homekit_permissions_paired_read \
        | homekit_permissions_notify, \
    .unit = homekit_unit_ampere, \
    .min_value = HOMEKIT_FLOAT_(0), \
    .max_value = HOMEKIT_FLOAT_(16), \
    .step_value = HOMEKIT_FLOAT_(0.01f), \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_POWER HOMEKIT_CUSTOM_UUID("000000A3")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_POWER(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_POWER, \
    .description = "Power", \
    .format = homekit_format_float, \
    .permissions = homekit_permissions_paired_read \
        | homekit_permissions_notify, \
    .unit = homekit_unit_watt, \
    .min_value = HOMEKIT_FLOAT_(0), \
    .max_value = HOMEKIT_FLOAT_(3680), \
    .step_value = HOMEKIT_FLOAT_(0.1f), \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_TOTAL_KWH HOMEKIT_CUSTOM_UUID("000000A4")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_TOTAL_KWH(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_TOTAL_KWH, \
    .description = "Total kWh", \
    .format = homekit_format_float, \
    .permissions = homekit_permissions_paired_read \
        | homekit_permissions_notify, \
    .unit = homekit_unit_kwh, \
    .min_value = HOMEKIT_FLOAT_(0), \
    .max_value = HOMEKIT_FLOAT_(100000), \
    .step_value = HOMEKIT_FLOAT_(0.001f), \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_OVERCURRENT_PROTECTION HOMEKIT_CUSTOM_UUID("000000A5")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_OVERCURRENT_PROTECTION(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_OVERCURRENT_PROTECTION, \
    .description = "Overcurrent Protection", \
    .format = homekit_format_bool, \
    .permissions = homekit_permissions_paired_read \
        | homekit_permissions_paired_write \
        | homekit_permissions_notify, \
    .value = HOMEKIT_BOOL_(_value), \
    ##__VA_ARGS__

#ifdef __cplusplus
extern "C" {
#endif

void custom_characteristics_set_defaults(homekit_characteristic_t *voltage,
                                         homekit_characteristic_t *current,
                                         homekit_characteristic_t *power,
                                         homekit_characteristic_t *total_kwh);

#ifdef __cplusplus
}
#endif
