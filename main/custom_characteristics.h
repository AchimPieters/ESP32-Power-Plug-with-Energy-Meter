#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <homekit/homekit.h>
#include <homekit/characteristics.h>

// Eve Energy UUIDs (Voltage / Current / Power / Total Consumption)
#define EVE_UUID_VOLTAGE          "E863F10A-079E-48FF-8F27-9C2605A29F52"
#define EVE_UUID_AMPERE           "E863F126-079E-48FF-8F27-9C2605A29F52"
#define EVE_UUID_WATT             "E863F10D-079E-48FF-8F27-9C2605A29F52"
#define EVE_UUID_TOTAL_CONSUMPTION "E863F10C-079E-48FF-8F27-9C2605A29F52"

extern homekit_characteristic_t eve_voltage;
extern homekit_characteristic_t eve_current;
extern homekit_characteristic_t eve_power;
extern homekit_characteristic_t eve_total_kwh;

void eve_energy_update(float voltage, float current, float power, float total_kwh);

#ifdef __cplusplus
}
#endif
