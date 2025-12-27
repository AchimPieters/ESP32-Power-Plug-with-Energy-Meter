#pragma once

#include "driver/gpio.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#define BL0937_CF_POWER_SCALE_W_PER_HZ       1125.0
#define BL0937_CF_ENERGY_SCALE_WH_PER_PULSE  0.3125
#define BL0937_CF1_VOLTAGE_SCALE_V_PER_HZ    1.0
#define BL0937_CF1_CURRENT_SCALE_A_PER_HZ    1.0

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuration parameters for the BL0937 energy metering IC.
 *
 * The BL0937 outputs frequency-proportional pulses on the CF and CF1 pins.
 * The conversion factors below translate measured pulse frequencies into
 * user-facing units. They can be calibrated per-device by comparing to a
 * known-good energy meter.
 *
 * Compatibility: the driver uses the ESP-IDF PCNT driver available on ESP32,
 * ESP32-S2/S3, ESP32-C2/C3/C5/C6, ESP32-H2, ESP32-P4 and other targets that
 * declare PCNT support in @c soc_caps.h.
 */
typedef struct {
    gpio_num_t cf_pin;            ///< CF pin connected to a GPIO capable of PCNT input
    gpio_num_t cf1_pin;           ///< CF1 pin connected to a GPIO capable of PCNT input
    gpio_num_t sel_pin;           ///< SEL pin used to switch CF1 between voltage/current
    uint32_t sample_period_ms;    ///< Duration for each measurement window (defaults to 1000ms when zero)
    uint32_t pcnt_glitch_ns;      ///< Glitch filter in nanoseconds (0 to disable)
    double cf_power_scale;        ///< Watts per Hz measured on the CF pin
    double cf_energy_scale;       ///< Watt-hours per pulse measured on the CF pin
    double cf1_voltage_scale;     ///< Volts per Hz on CF1 when SEL=0
    double cf1_current_scale;     ///< Amps per Hz on CF1 when SEL=1
} bl0937_config_t;

/**
 * @brief Latest measurement results from the BL0937.
 */
typedef struct {
    double voltage_v;
    double current_a;
    double active_power_w;
    double energy_wh;
} bl0937_reading_t;

typedef void (*bl0937_reading_cb_t)(const bl0937_reading_t *reading, void *user_data);

/**
 * @brief Initialise the BL0937 driver.
 *
 * @param config Pointer to configuration structure.
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_STATE if already initialised,
 *         or ESP_ERR_NOT_SUPPORTED when requesting unsupported PCNT features
 *         (such as glitch filtering on some targets).
 */
esp_err_t bl0937_init(const bl0937_config_t *config);

/**
 * @brief Perform a blocking measurement over the configured sampling window.
 *
 * This function measures the pulse frequencies on CF and CF1 and converts them
 * to voltage, current, power and accumulated energy values using the provided
 * calibration factors.
 *
 * @param out Pointer to structure to receive the measurements.
 * @return esp_err_t ESP_OK on success, error code otherwise.
 */
esp_err_t bl0937_sample(bl0937_reading_t *out);

/**
 * @brief Convenience helper returning a config seeded from sdkconfig defaults and
 *        standard BL0937 scale factors.
 */
bl0937_config_t bl0937_default_config(void);

/**
 * @brief Start a background sampling task that periodically invokes the provided
 *        callback with fresh readings.
 */
esp_err_t bl0937_start_task(const bl0937_config_t *config, bl0937_reading_cb_t callback, void *user_data);

/**
 * @brief Stop the background sampling task created via @ref bl0937_start_task.
 */
esp_err_t bl0937_stop_task(void);

#ifdef __cplusplus
}
#endif

