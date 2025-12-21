#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
        float voltage;
        float current;
        float power;
        float energy_kwh;
} bl0937_measurement_t;

typedef struct {
        // Pins
        gpio_num_t gpio_cf;
        gpio_num_t gpio_cf1;
        gpio_num_t gpio_sel;
        bool sel_inverted;

        // Calibration factors:
        // physical_value = hz * factor
        float watts_per_hz;
        float volts_per_hz;
        float amps_per_hz;

        // Timing
        uint32_t sample_ms;

        // Internal state
        float last_voltage;
        float last_current;
        float last_power;
        float energy_kwh;

        // Smoothing (0..1)
        float ema_alpha;

        // CF1 SEL state: true => measuring voltage, false => current
        bool measuring_voltage;

        // Pulse counters
        volatile uint32_t cf_pulses;
        volatile uint32_t cf1_pulses;

        // Running flag
        bool running;

        // esp_timer handle (opaque)
        void *timer;
} bl0937_t;

esp_err_t bl0937_init(bl0937_t *dev,
                      gpio_num_t cf,
                      gpio_num_t cf1,
                      gpio_num_t sel,
                      bool sel_inverted);

esp_err_t bl0937_start(bl0937_t *dev, uint32_t sample_ms);

void bl0937_stop(bl0937_t *dev);

void bl0937_set_calibration(bl0937_t *dev, float watts_per_hz, float volts_per_hz, float amps_per_hz);

void bl0937_set_smoothing(bl0937_t *dev, float alpha);

esp_err_t bl0937_get_latest(bl0937_t *dev, bl0937_measurement_t *out);

void bl0937_reset_energy(bl0937_t *dev);

#ifdef __cplusplus
}
#endif
