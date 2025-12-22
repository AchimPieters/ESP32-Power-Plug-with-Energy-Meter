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

#include "BL0937.h"

#include <math.h>

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define BL0937_POWER_PROBE_TIME_US (10LL * 1000000)

static const char *BL0937_TAG = "BL0937";

typedef struct {
    bl0937_config_t config;
    bl0937_update_cb_t callback;
    void *callback_context;
    TaskHandle_t task_handle;
    portMUX_TYPE mux;
    volatile uint32_t cf_pulse_length;
    volatile uint64_t cf_pulse_last_time;
    volatile uint32_t cf_summed_pulse_length;
    volatile uint32_t cf_pulse_counter;
    volatile uint32_t cf1_pulse_length;
    volatile uint64_t cf1_pulse_last_time;
    volatile uint32_t cf1_summed_pulse_length;
    volatile uint32_t cf1_pulse_counter;
    volatile uint32_t energy_pulse_counter;
    volatile bool load_off;
    uint32_t cf_power_pulse_length;
    uint32_t cf1_voltage_pulse_length;
    uint32_t cf1_current_pulse_length;
    volatile uint32_t cf1_timer;
    bool select_voltage;
    bl0937_reading_t last_reading;
} bl0937_state_t;

static bl0937_state_t bl0937_state = {
    .mux = portMUX_INITIALIZER_UNLOCKED,
};

static void IRAM_ATTR bl0937_cf_isr(void *arg) {
    (void)arg;
    uint64_t now = (uint64_t)esp_timer_get_time();

    portENTER_CRITICAL_ISR(&bl0937_state.mux);
    if (bl0937_state.load_off) {
        bl0937_state.cf_pulse_last_time = now;
        bl0937_state.load_off = false;
    } else {
        bl0937_state.cf_pulse_length = now - bl0937_state.cf_pulse_last_time;
        bl0937_state.cf_pulse_last_time = now;
        bl0937_state.cf_summed_pulse_length += bl0937_state.cf_pulse_length;
        bl0937_state.cf_pulse_counter++;
        bl0937_state.energy_pulse_counter++;
    }
    portEXIT_CRITICAL_ISR(&bl0937_state.mux);
}

static void IRAM_ATTR bl0937_cf1_isr(void *arg) {
    (void)arg;
    uint64_t now = (uint64_t)esp_timer_get_time();

    portENTER_CRITICAL_ISR(&bl0937_state.mux);
    bl0937_state.cf1_pulse_length = now - bl0937_state.cf1_pulse_last_time;
    bl0937_state.cf1_pulse_last_time = now;
    if ((bl0937_state.cf1_timer > 1) && (bl0937_state.cf1_timer < 8)) {
        bl0937_state.cf1_summed_pulse_length += bl0937_state.cf1_pulse_length;
        bl0937_state.cf1_pulse_counter++;
    }
    portEXIT_CRITICAL_ISR(&bl0937_state.mux);
}

static void bl0937_set_sel_output(bool select_voltage) {
    uint32_t level = select_voltage ? 0 : 1;
    if (bl0937_state.config.sel_inverted) {
        level = level ? 0 : 1;
    }
    gpio_set_level(bl0937_state.config.sel_gpio, (int)level);
}

static float bl0937_calculate_power(uint32_t pulse_length) {
    if (pulse_length == 0) {
        return 0.0f;
    }

    float ratio = (float)bl0937_state.config.power_ratio;
    float calibration = (float)bl0937_state.config.power_calibration;
    float value = (ratio * calibration) / (float)pulse_length;

    return value / 10.0f;
}

static float bl0937_calculate_voltage(uint32_t pulse_length) {
    if (pulse_length == 0) {
        return 0.0f;
    }

    float ratio = (float)bl0937_state.config.voltage_ratio;
    float calibration = (float)bl0937_state.config.voltage_calibration;
    float value = (ratio * calibration) / (float)pulse_length;

    return value / 10.0f;
}

static float bl0937_calculate_current(uint32_t pulse_length) {
    if (pulse_length == 0) {
        return 0.0f;
    }

    float ratio = (float)bl0937_state.config.current_ratio;
    float calibration = (float)bl0937_state.config.current_calibration;
    float value = (ratio * calibration) / (float)pulse_length;

    return value / 1000.0f;
}

static void bl0937_task(void *arg) {
    (void)arg;
    uint32_t update_interval_ms = bl0937_state.config.update_interval_ms;
    if (update_interval_ms == 0) {
        ESP_LOGW(BL0937_TAG, "Update interval was 0 ms; defaulting to 1000 ms");
        update_interval_ms = 1000;
    }
    const TickType_t delay_ticks = pdMS_TO_TICKS(update_interval_ms);
    uint32_t sample_counter = 0;
    uint32_t cycle_divider = 1000 / update_interval_ms;

    if (cycle_divider < 2) {
        ESP_LOGW(BL0937_TAG,
                 "Update interval %u ms yields short CF1 cycles; using divider 2 for stability",
                 (unsigned)update_interval_ms);
        cycle_divider = 2;
    }

    bl0937_state.select_voltage = true;
    bl0937_set_sel_output(true);

    while (true) {
        vTaskDelay(delay_ticks);

        uint64_t cf_pulse_last_time;
        uint32_t cf_summed_pulse_length;
        uint32_t cf_pulse_counter;
        uint32_t cf1_summed_pulse_length;
        uint32_t cf1_pulse_counter;
        uint32_t energy_pulse_counter;

        portENTER_CRITICAL(&bl0937_state.mux);
        cf_pulse_last_time = bl0937_state.cf_pulse_last_time;
        cf_summed_pulse_length = bl0937_state.cf_summed_pulse_length;
        cf_pulse_counter = bl0937_state.cf_pulse_counter;
        cf1_summed_pulse_length = bl0937_state.cf1_summed_pulse_length;
        cf1_pulse_counter = bl0937_state.cf1_pulse_counter;
        energy_pulse_counter = bl0937_state.energy_pulse_counter;
        bl0937_state.cf_summed_pulse_length = 0;
        bl0937_state.cf_pulse_counter = 0;
        bl0937_state.cf1_summed_pulse_length = 0;
        bl0937_state.cf1_pulse_counter = 0;
        bl0937_state.energy_pulse_counter = 0;
        portEXIT_CRITICAL(&bl0937_state.mux);

        uint64_t now = (uint64_t)esp_timer_get_time();
        if (now - cf_pulse_last_time > BL0937_POWER_PROBE_TIME_US) {
            portENTER_CRITICAL(&bl0937_state.mux);
            if (now - bl0937_state.cf_pulse_last_time > BL0937_POWER_PROBE_TIME_US) {
                bl0937_state.cf_pulse_length = 0;
                bl0937_state.load_off = true;
            }
            portEXIT_CRITICAL(&bl0937_state.mux);
        }

        if (cf_pulse_counter && !bl0937_state.load_off) {
            bl0937_state.cf_power_pulse_length = cf_summed_pulse_length / cf_pulse_counter;
        } else if (bl0937_state.load_off) {
            bl0937_state.cf_power_pulse_length = 0;
        }

        if (cf1_pulse_counter) {
            uint32_t cf1_pulse_length = cf1_summed_pulse_length / cf1_pulse_counter;
            if (bl0937_state.select_voltage) {
                bl0937_state.cf1_voltage_pulse_length = cf1_pulse_length;
            } else {
                bl0937_state.cf1_current_pulse_length = cf1_pulse_length;
            }
        }

        float power = bl0937_calculate_power(bl0937_state.cf_power_pulse_length);
        float voltage = bl0937_calculate_voltage(bl0937_state.cf1_voltage_pulse_length);
        float current = bl0937_calculate_current(bl0937_state.cf1_current_pulse_length);

        if (bl0937_state.load_off) {
            power = 0.0f;
            current = 0.0f;
        }

        if (energy_pulse_counter && bl0937_state.config.pulses_per_kwh > 0) {
            bl0937_state.last_reading.energy +=
                    (float)energy_pulse_counter / (float)bl0937_state.config.pulses_per_kwh;
        }

        bl0937_state.last_reading.power = power;
        bl0937_state.last_reading.voltage = voltage;
        bl0937_state.last_reading.current = current;

        if (bl0937_state.callback) {
            bl0937_state.callback(&bl0937_state.last_reading, bl0937_state.callback_context);
        }

        sample_counter++;
        bl0937_state.cf1_timer++;

        if (sample_counter >= cycle_divider) {
            sample_counter = 0;
            bl0937_state.cf1_timer = 0;
            bl0937_state.select_voltage = !bl0937_state.select_voltage;
            bl0937_set_sel_output(bl0937_state.select_voltage);
        }
    }
}

esp_err_t bl0937_init(const bl0937_config_t *config) {
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    bl0937_state.config = *config;
    bl0937_state.callback = NULL;
    bl0937_state.callback_context = NULL;
    bl0937_state.cf_pulse_length = 0;
    bl0937_state.cf_pulse_last_time = (uint64_t)esp_timer_get_time();
    bl0937_state.cf_summed_pulse_length = 0;
    bl0937_state.cf_pulse_counter = 0;
    bl0937_state.cf1_pulse_length = 0;
    bl0937_state.cf1_pulse_last_time = bl0937_state.cf_pulse_last_time;
    bl0937_state.cf1_summed_pulse_length = 0;
    bl0937_state.cf1_pulse_counter = 0;
    bl0937_state.energy_pulse_counter = 0;
    bl0937_state.load_off = true;
    bl0937_state.cf_power_pulse_length = 0;
    bl0937_state.cf1_voltage_pulse_length = 0;
    bl0937_state.cf1_current_pulse_length = 0;
    bl0937_state.cf1_timer = 0;
    bl0937_state.select_voltage = true;
    bl0937_state.last_reading = (bl0937_reading_t){0};

    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    gpio_config_t cf_config = {
        .pin_bit_mask = 1ULL << config->cf_gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&cf_config));

    gpio_config_t cf1_config = {
        .pin_bit_mask = 1ULL << config->cf1_gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&cf1_config));

    gpio_config_t sel_config = {
        .pin_bit_mask = 1ULL << config->sel_gpio,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&sel_config));

    ESP_ERROR_CHECK(gpio_isr_handler_add(config->cf_gpio, bl0937_cf_isr, NULL));
    ESP_ERROR_CHECK(gpio_isr_handler_add(config->cf1_gpio, bl0937_cf1_isr, NULL));

    ESP_LOGI(BL0937_TAG, "BL0937 initialized (CF=%d CF1=%d SEL=%d)",
             config->cf_gpio, config->cf1_gpio, config->sel_gpio);

    return ESP_OK;
}

esp_err_t bl0937_start(bl0937_update_cb_t callback, void *context) {
    if (bl0937_state.task_handle != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    bl0937_state.callback = callback;
    bl0937_state.callback_context = context;

    BaseType_t created = xTaskCreate(bl0937_task, "bl0937_task", 4096, NULL, 5,
                                     &bl0937_state.task_handle);
    if (created != pdPASS) {
        bl0937_state.task_handle = NULL;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

void bl0937_stop(void) {
    if (bl0937_state.task_handle) {
        TaskHandle_t handle = bl0937_state.task_handle;
        bl0937_state.task_handle = NULL;
        vTaskDelete(handle);
    }
}

bool bl0937_get_last_reading(bl0937_reading_t *out_reading) {
    if (out_reading == NULL) {
        return false;
    }

    *out_reading = bl0937_state.last_reading;
    return true;
}
