/**
   Copyright 2026 Achim Pieters | StudioPieters®

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NON INFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   for more information visit https://www.studiopieters.nl
 **/

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <driver/gpio.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gpio_num_t cf_gpio;
    gpio_num_t cf1_gpio;
    gpio_num_t sel_gpio;
    bool sel_inverted;
    uint32_t power_ratio;
    uint32_t voltage_ratio;
    uint32_t current_ratio;
    uint32_t power_calibration;
    uint32_t voltage_calibration;
    uint32_t current_calibration;
    uint32_t pulses_per_kwh;
    uint32_t update_interval_ms;
} bl0937_config_t;

typedef struct {
    float voltage;
    float current;
    float power;
    float energy;
} bl0937_reading_t;

typedef void (*bl0937_update_cb_t)(const bl0937_reading_t *reading, void *context);

esp_err_t bl0937_init(const bl0937_config_t *config);

esp_err_t bl0937_start(bl0937_update_cb_t callback, void *context);

void bl0937_stop(void);

bool bl0937_get_last_reading(bl0937_reading_t *out_reading);

#ifdef __cplusplus
}
#endif
