#pragma once

#include <homekit/characteristics.h>
#include <homekit/homekit.h>

#ifndef __HOMEKIT_CUSTOM_ENERGY_CHARACTERISTICS__
#define __HOMEKIT_CUSTOM_ENERGY_CHARACTERISTICS__

#ifndef HOMEKIT_CUSTOM_UUID
#define HOMEKIT_CUSTOM_UUID(value) (value "-0e36-4a42-ad11-745a73b84f2b")
#endif

#define HOMEKIT_SERVICE_CUSTOM_ENERGY_METER HOMEKIT_CUSTOM_UUID("000000FA")

#define HOMEKIT_CHARACTERISTIC_CUSTOM_VOLTAGE HOMEKIT_CUSTOM_UUID("F0000002")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_VOLTAGE(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_VOLTAGE, \
    .description = "Voltage", \
    .format = homekit_format_float, \
    .permissions = homekit_permissions_paired_read | homekit_permissions_notify, \
    .unit = homekit_unit_none, \
    .value = HOMEKIT_FLOAT_(_value), \
    .min_value = (float[]) { 0.0f }, \
    .max_value = (float[]) { 300.0f }, \
    .min_step = (float[]) { 0.1f }, \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_CURRENT HOMEKIT_CUSTOM_UUID("F0000003")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_CURRENT(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_CURRENT, \
    .description = "Current", \
    .format = homekit_format_float, \
    .permissions = homekit_permissions_paired_read | homekit_permissions_notify, \
    .unit = homekit_unit_none, \
    .value = HOMEKIT_FLOAT_(_value), \
    .min_value = (float[]) { 0.0f }, \
    .max_value = (float[]) { 16.0f }, \
    .min_step = (float[]) { 0.001f }, \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_POWER HOMEKIT_CUSTOM_UUID("F0000004")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_POWER(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_POWER, \
    .description = "Power", \
    .format = homekit_format_float, \
    .permissions = homekit_permissions_paired_read | homekit_permissions_notify, \
    .unit = homekit_unit_none, \
    .value = HOMEKIT_FLOAT_(_value), \
    .min_value = (float[]) { 0.0f }, \
    .max_value = (float[]) { 4000.0f }, \
    .min_step = (float[]) { 0.1f }, \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_ENERGY HOMEKIT_CUSTOM_UUID("F0000005")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_ENERGY(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_ENERGY, \
    .description = "Energy", \
    .format = homekit_format_float, \
    .permissions = homekit_permissions_paired_read | homekit_permissions_notify, \
    .unit = homekit_unit_none, \
    .value = HOMEKIT_FLOAT_(_value), \
    .min_value = (float[]) { 0.0f }, \
    .max_value = (float[]) { 100000.0f }, \
    .min_step = (float[]) { 0.001f }, \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_POWER_FACTOR HOMEKIT_CUSTOM_UUID("F0000006")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_POWER_FACTOR(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_POWER_FACTOR, \
    .description = "PowerFactor", \
    .format = homekit_format_float, \
    .permissions = homekit_permissions_paired_read | homekit_permissions_notify, \
    .unit = homekit_unit_none, \
    .value = HOMEKIT_FLOAT_(_value), \
    .min_value = (float[]) { 0.0f }, \
    .max_value = (float[]) { 1.0f }, \
    .min_step = (float[]) { 0.001f }, \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_FREQUENCY HOMEKIT_CUSTOM_UUID("F0000007")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_FREQUENCY(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_FREQUENCY, \
    .description = "Frequency", \
    .format = homekit_format_float, \
    .permissions = homekit_permissions_paired_read | homekit_permissions_notify, \
    .unit = homekit_unit_none, \
    .value = HOMEKIT_FLOAT_(_value), \
    .min_value = (float[]) { 0.0f }, \
    .max_value = (float[]) { 100.0f }, \
    .min_step = (float[]) { 0.01f }, \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_TOTAL_CONSUMPTION HOMEKIT_CUSTOM_UUID("F0000008")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_TOTAL_CONSUMPTION(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_TOTAL_CONSUMPTION, \
    .description = "TotalConsumption", \
    .format = homekit_format_float, \
    .permissions = homekit_permissions_paired_read | homekit_permissions_notify, \
    .unit = homekit_unit_none, \
    .value = HOMEKIT_FLOAT_(_value), \
    .min_value = (float[]) { 0.0f }, \
    .max_value = (float[]) { 100000.0f }, \
    .min_step = (float[]) { 0.001f }, \
    ##__VA_ARGS__

#define API_VOLTAGE HOMEKIT_CHARACTERISTIC_(CUSTOM_VOLTAGE, 0)
#define API_CURRENT HOMEKIT_CHARACTERISTIC_(CUSTOM_CURRENT, 0)
#define API_POWER HOMEKIT_CHARACTERISTIC_(CUSTOM_POWER, 0)
#define API_ENERGY HOMEKIT_CHARACTERISTIC_(CUSTOM_ENERGY, 0)
#define API_POWER_FACTOR HOMEKIT_CHARACTERISTIC_(CUSTOM_POWER_FACTOR, 0)
#define API_FREQUENCY HOMEKIT_CHARACTERISTIC_(CUSTOM_FREQUENCY, 0)
#define API_TOTAL_CONSUMPTION HOMEKIT_CHARACTERISTIC_(CUSTOM_TOTAL_CONSUMPTION, 0)

#endif /* __HOMEKIT_CUSTOM_ENERGY_CHARACTERISTICS__ */

#ifdef __cplusplus
extern "C" {
#endif

homekit_service_t *custom_characteristics_service(void);

void custom_characteristics_update(float voltage,
                                   float current,
                                   float power,
                                   float energy,
                                   float power_factor,
                                   float frequency,
                                   float total_consumption);

#ifdef __cplusplus
}
#endif
