#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <esp_err.h>
#include <driver/gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
        gpio_num_t cf_gpio;
        gpio_num_t cf1_gpio;
        gpio_num_t sel_gpio;
        float power_scale;
        float voltage_scale;
        float current_scale;
        float nominal_frequency;
        uint32_t sample_period_ms;
} bl0937_config_t;

/**
 * @brief Initialise the BL0937 measurement task with the provided configuration.
 */
esp_err_t bl0937_init(const bl0937_config_t *config);

/**
 * @brief Convenience helper that uses sdkconfig defaults for GPIO mapping.
 */
esp_err_t bl0937_start_default(void);

#ifdef __cplusplus
}
#endif

