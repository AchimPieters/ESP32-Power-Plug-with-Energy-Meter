#include <stddef.h>

#include <esp_log.h>
#include <homekit/characteristics.h>
#include <homekit/homekit.h>

#include "bl0937.h"
#include "custom_characteristics.h"

static const char *BL0937_TAG = "BL0937";

static void set_float_and_notify(homekit_characteristic_t *characteristic, float value) {
        if (characteristic == NULL) {
                return;
        }

        characteristic->value = HOMEKIT_FLOAT(value);
        homekit_characteristic_notify(characteristic, characteristic->value);
}

void bl0937_update_measurements(float voltage, float current, float power,
                                float total_consumption) {
        set_float_and_notify(&voltage_characteristic, voltage);
        set_float_and_notify(&current_characteristic, current);
        set_float_and_notify(&power_characteristic, power);
        set_float_and_notify(&total_consumption_characteristic, total_consumption);
}

void bl0937_start_default(void) {
        ESP_LOGI(BL0937_TAG, "Starting BL0937 with default configuration");

        // Initialize HomeKit-facing characteristics with zeroed measurements.
        bl0937_update_measurements(0.0f, 0.0f, 0.0f, 0.0f);
}
