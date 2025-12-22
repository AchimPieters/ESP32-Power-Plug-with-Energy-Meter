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

#include "bl0937.h"

#include <inttypes.h>

#include <esp_log.h>
#include <esp_timer.h>

static const char *BL0937_TAG = "BL0937";

static void bl0937_set_sel_gpio(const bl0937_t *dev, bl0937_cf1_mode_t mode) {
    if (dev == NULL) {
        return;
    }

    gpio_set_level(dev->config.sel_pin, mode == BL0937_CF1_MODE_VOLTAGE ? 1 : 0);
}

static void IRAM_ATTR bl0937_cf_isr(void *arg) {
    bl0937_t *dev = (bl0937_t *)arg;
    int64_t now = esp_timer_get_time();

    portENTER_CRITICAL_ISR(&dev->lock);
    if (dev->last_cf_us > 0) {
        dev->cf_period_us = (uint32_t)(now - dev->last_cf_us);
    }
    dev->last_cf_us = now;
    dev->cf_pulse_count++;
    portEXIT_CRITICAL_ISR(&dev->lock);
}

static void IRAM_ATTR bl0937_cf1_isr(void *arg) {
    bl0937_t *dev = (bl0937_t *)arg;
    int64_t now = esp_timer_get_time();

    portENTER_CRITICAL_ISR(&dev->lock);
    if (dev->last_cf1_us > 0) {
        dev->cf1_period_us = (uint32_t)(now - dev->last_cf1_us);
    }
    dev->last_cf1_us = now;
    dev->cf1_pulse_count++;
    portEXIT_CRITICAL_ISR(&dev->lock);
}

esp_err_t bl0937_init(bl0937_t *dev, const bl0937_config_t *config) {
    if (dev == NULL || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *dev = (bl0937_t) {
        .config = *config,
        .cf1_mode = BL0937_CF1_MODE_VOLTAGE,
        .lock = portMUX_INITIALIZER_UNLOCKED,
    };

    gpio_config_t input_cfg = {
        .pin_bit_mask = (1ULL << config->cf_pin) | (1ULL << config->cf1_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    gpio_config(&input_cfg);

    gpio_config_t sel_cfg = {
        .pin_bit_mask = (1ULL << config->sel_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&sel_cfg);

    bl0937_set_sel_gpio(dev, dev->cf1_mode);

    esp_err_t err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(BL0937_TAG, "Failed to install GPIO ISR service: %s", esp_err_to_name(err));
        return err;
    }

    err = gpio_isr_handler_add(config->cf_pin, bl0937_cf_isr, dev);
    if (err != ESP_OK) {
        ESP_LOGE(BL0937_TAG, "Failed to add CF ISR handler: %s", esp_err_to_name(err));
        return err;
    }

    err = gpio_isr_handler_add(config->cf1_pin, bl0937_cf1_isr, dev);
    if (err != ESP_OK) {
        ESP_LOGE(BL0937_TAG, "Failed to add CF1 ISR handler: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(BL0937_TAG,
             "BL0937 initialized (CF=%d, CF1=%d, SEL=%d)",
             config->cf_pin, config->cf1_pin, config->sel_pin);
    return ESP_OK;
}

void bl0937_set_cf1_mode(bl0937_t *dev, bl0937_cf1_mode_t mode) {
    if (dev == NULL) {
        return;
    }

    portENTER_CRITICAL(&dev->lock);
    dev->cf1_mode = mode;
    portEXIT_CRITICAL(&dev->lock);

    bl0937_set_sel_gpio(dev, mode);
}

static uint32_t bl0937_delta_pulses(uint32_t current, uint32_t last) {
    if (current >= last) {
        return current - last;
    }
    return (UINT32_MAX - last) + current + 1;
}

void bl0937_update(bl0937_t *dev) {
    if (dev == NULL) {
        return;
    }

    uint32_t cf_count;
    uint32_t cf_period;
    uint32_t cf1_period;
    bl0937_cf1_mode_t cf1_mode;

    portENTER_CRITICAL(&dev->lock);
    cf_count = dev->cf_pulse_count;
    cf_period = dev->cf_period_us;
    cf1_period = dev->cf1_period_us;
    cf1_mode = dev->cf1_mode;
    portEXIT_CRITICAL(&dev->lock);

    float cf_freq = cf_period > 0 ? (1000000.0f / (float)cf_period) : 0.0f;
    float cf1_freq = cf1_period > 0 ? (1000000.0f / (float)cf1_period) : 0.0f;

    dev->power_w = cf_freq * dev->config.power_coeff;

    if (cf1_mode == BL0937_CF1_MODE_VOLTAGE) {
        dev->voltage_v = cf1_freq * dev->config.voltage_coeff;
    } else {
        dev->current_a = cf1_freq * dev->config.current_coeff;
    }

    uint32_t delta_pulses = bl0937_delta_pulses(cf_count, dev->last_energy_pulse_count);
    dev->last_energy_pulse_count = cf_count;
    dev->energy_wh += (delta_pulses * dev->config.power_coeff) / 3600.0f;
}

float bl0937_get_power_w(const bl0937_t *dev) {
    return dev ? dev->power_w : 0.0f;
}

float bl0937_get_current_a(const bl0937_t *dev) {
    return dev ? dev->current_a : 0.0f;
}

float bl0937_get_voltage_v(const bl0937_t *dev) {
    return dev ? dev->voltage_v : 0.0f;
}

float bl0937_get_energy_wh(const bl0937_t *dev) {
    return dev ? dev->energy_wh : 0.0f;
}
