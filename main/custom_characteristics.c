#include "custom_characteristics.h"

#include <math.h>
#include <stdbool.h>

#include "esp_timer.h"

static uint64_t s_last_notify_us = 0;

static inline bool changed_enough(float a, float b, float eps) {
        if (!isfinite(a) || !isfinite(b)) {
                return true;
        }
        return fabsf(a - b) >= eps;
}

homekit_characteristic_t eve_voltage = {
        .type = EVE_UUID_VOLTAGE,
        .description = "Voltage",
        .format = homekit_format_float,
        .permissions = homekit_permissions_paired_read | homekit_permissions_notify,
        .value = HOMEKIT_FLOAT(0.0f),
};

homekit_characteristic_t eve_current = {
        .type = EVE_UUID_AMPERE,
        .description = "Current",
        .format = homekit_format_float,
        .permissions = homekit_permissions_paired_read | homekit_permissions_notify,
        .value = HOMEKIT_FLOAT(0.0f),
};

homekit_characteristic_t eve_power = {
        .type = EVE_UUID_WATT,
        .description = "Power",
        .format = homekit_format_float,
        .permissions = homekit_permissions_paired_read | homekit_permissions_notify,
        .value = HOMEKIT_FLOAT(0.0f),
};

homekit_characteristic_t eve_total_kwh = {
        .type = EVE_UUID_TOTAL_CONSUMPTION,
        .description = "Total Consumption",
        .format = homekit_format_float,
        .permissions = homekit_permissions_paired_read | homekit_permissions_notify,
        .value = HOMEKIT_FLOAT(0.0f),
};

void eve_energy_update(float voltage, float current, float power, float total_kwh) {
        const uint64_t now = esp_timer_get_time();
        const bool time_ok = (now - s_last_notify_us) >= 2000000ULL; // max 1x per 2s notify

        bool any = false;

        if (changed_enough(eve_voltage.value.float_value, voltage, 0.5f)) {
                eve_voltage.value = HOMEKIT_FLOAT(voltage);
                any = true;
        }
        if (changed_enough(eve_current.value.float_value, current, 0.02f)) {
                eve_current.value = HOMEKIT_FLOAT(current);
                any = true;
        }
        if (changed_enough(eve_power.value.float_value, power, 0.5f)) {
                eve_power.value = HOMEKIT_FLOAT(power);
                any = true;
        }
        if (changed_enough(eve_total_kwh.value.float_value, total_kwh, 0.001f)) {
                eve_total_kwh.value = HOMEKIT_FLOAT(total_kwh);
                any = true;
        }

        if (any && time_ok) {
                homekit_characteristic_notify(&eve_voltage, eve_voltage.value);
                homekit_characteristic_notify(&eve_current, eve_current.value);
                homekit_characteristic_notify(&eve_power, eve_power.value);
                homekit_characteristic_notify(&eve_total_kwh, eve_total_kwh.value);
                s_last_notify_us = now;
        }
}
