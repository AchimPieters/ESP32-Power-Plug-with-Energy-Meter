#pragma once

#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

// Start the BL0937 metering task with default configuration.
void bl0937_start_default(void);

// Update HomeKit characteristics with the latest BL0937 readings.
void bl0937_update_measurements(float voltage, float current, float power,
                                float total_consumption);

#ifdef __cplusplus
}
#endif

