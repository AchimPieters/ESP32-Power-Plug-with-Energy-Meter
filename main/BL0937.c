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
#include <string.h>

#include <sdkconfig.h>

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#define BL0937_FRAME_LENGTH 24
#define BL0937_HEADER_BYTE_0 0x55
#define BL0937_HEADER_BYTE_1 0x5A
#define BL0937_UART_BUFFER_SIZE 128

static const char *BL0937_TAG = "BL0937";

typedef struct {
    bl0937_config_t config;
    bl0937_reading_t reading;
    bool overcurrent_enabled;
    float overcurrent_limit_amps;
    uint64_t last_update_us;
    uint64_t last_overcurrent_trip_us;
    SemaphoreHandle_t mutex;
    bl0937_overcurrent_cb_t overcurrent_callback;
    void *overcurrent_context;
} bl0937_state_t;

static bl0937_state_t s_state;

static uint32_t bl0937_parse_u24_le(const uint8_t *data) {
    return ((uint32_t)data[0]) | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16);
}

static bool bl0937_validate_frame(const uint8_t *frame) {
    if (frame[0] != BL0937_HEADER_BYTE_0 || frame[1] != BL0937_HEADER_BYTE_1) {
        return false;
    }

    uint8_t checksum = 0;
    for (int i = 0; i < BL0937_FRAME_LENGTH - 1; i++) {
        checksum += frame[i];
    }

    return checksum == frame[BL0937_FRAME_LENGTH - 1];
}

static esp_err_t bl0937_read_frame(uint8_t *frame, TickType_t timeout_ticks) {
    uint8_t byte = 0;
    while (uart_read_bytes(s_state.config.uart_num, &byte, 1, timeout_ticks) == 1) {
        if (byte != BL0937_HEADER_BYTE_0) {
            continue;
        }

        uint8_t next = 0;
        if (uart_read_bytes(s_state.config.uart_num, &next, 1, timeout_ticks) != 1) {
            return ESP_ERR_TIMEOUT;
        }

        if (next != BL0937_HEADER_BYTE_1) {
            continue;
        }

        frame[0] = BL0937_HEADER_BYTE_0;
        frame[1] = BL0937_HEADER_BYTE_1;

        int remaining = BL0937_FRAME_LENGTH - 2;
        int read = uart_read_bytes(s_state.config.uart_num, frame + 2, remaining, timeout_ticks);
        if (read != remaining) {
            return ESP_ERR_TIMEOUT;
        }

        return ESP_OK;
    }

    return ESP_ERR_TIMEOUT;
}

static void bl0937_update_reading_from_frame(const uint8_t *frame) {
    uint32_t raw_voltage = bl0937_parse_u24_le(&frame[2]);
    uint32_t raw_current = bl0937_parse_u24_le(&frame[5]);
    uint32_t raw_power = bl0937_parse_u24_le(&frame[8]);
    uint32_t raw_energy = bl0937_parse_u24_le(&frame[11]);

    float voltage = (raw_voltage * (s_state.config.voltage_multiplier_uv / 1000000.0f));
    float current = (raw_current * (s_state.config.current_multiplier_ua / 1000000.0f));
    float power = (raw_power * (s_state.config.power_multiplier_uw / 1000000.0f));

    float total_kwh = s_state.reading.total_kwh;
    uint64_t now_us = esp_timer_get_time();
    if (s_state.config.energy_multiplier_uwh > 0) {
        float energy_wh = raw_energy * (s_state.config.energy_multiplier_uwh / 1000000.0f);
        total_kwh = energy_wh / 1000.0f;
    } else if (s_state.last_update_us != 0) {
        float delta_hours = (now_us - s_state.last_update_us) / 3600000000.0f;
        total_kwh += (power * delta_hours) / 1000.0f;
    }

    s_state.reading.voltage = voltage;
    s_state.reading.current = current;
    s_state.reading.power = power;
    s_state.reading.total_kwh = total_kwh;

    s_state.last_update_us = now_us;

    bool overcurrent = s_state.overcurrent_enabled &&
        s_state.overcurrent_limit_amps > 0.0f &&
        current >= s_state.overcurrent_limit_amps;

    if (overcurrent && !s_state.reading.overcurrent && s_state.overcurrent_callback) {
        if (now_us - s_state.last_overcurrent_trip_us >= 30000000ULL) {
            s_state.last_overcurrent_trip_us = now_us;
            s_state.overcurrent_callback(current, s_state.overcurrent_context);
        }
    }

    s_state.reading.overcurrent = overcurrent;
}

static void bl0937_task(void *args) {
    uint8_t frame[BL0937_FRAME_LENGTH] = {0};

    while (true) {
        if (bl0937_read_frame(frame, pdMS_TO_TICKS(500)) == ESP_OK) {
            if (!bl0937_validate_frame(frame)) {
                ESP_LOGW(BL0937_TAG, "Invalid frame checksum");
                continue;
            }

            if (xSemaphoreTake(s_state.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                bl0937_update_reading_from_frame(frame);
                xSemaphoreGive(s_state.mutex);
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
}

esp_err_t bl0937_init(const bl0937_config_t *config) {
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(&s_state, 0, sizeof(s_state));
    s_state.config = *config;

    if (s_state.config.voltage_multiplier_uv == 0) {
        s_state.config.voltage_multiplier_uv = 1;
    }
    if (s_state.config.current_multiplier_ua == 0) {
        s_state.config.current_multiplier_ua = 1;
    }
    if (s_state.config.power_multiplier_uw == 0) {
        s_state.config.power_multiplier_uw = 1;
    }

    s_state.overcurrent_enabled = true;
    s_state.overcurrent_limit_amps = config->overcurrent_limit_amps;
    s_state.last_overcurrent_trip_us = 0;

    s_state.mutex = xSemaphoreCreateMutex();
    if (s_state.mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    uart_config_t uart_config = {
        .baud_rate = config->baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_EVEN,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_param_config(config->uart_num, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(config->uart_num, config->tx_gpio, config->rx_gpio,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(config->uart_num, BL0937_UART_BUFFER_SIZE,
                                        BL0937_UART_BUFFER_SIZE, 0, NULL, 0));

    ESP_LOGI(BL0937_TAG, "BL0937 init complete (UART %d, baud %d)",
             config->uart_num, config->baud_rate);

    xTaskCreate(bl0937_task, "bl0937_task", 4096, NULL, 5, NULL);
    return ESP_OK;
}

esp_err_t bl0937_init_default(void) {
    bl0937_config_t config = {
        .uart_num = CONFIG_BL0937_UART_NUM,
        .tx_gpio = CONFIG_BL0937_UART_TX_GPIO,
        .rx_gpio = CONFIG_BL0937_UART_RX_GPIO,
        .baud_rate = CONFIG_BL0937_BAUDRATE,
        .voltage_multiplier_uv = CONFIG_BL0937_VOLTAGE_MULTIPLIER_UV,
        .current_multiplier_ua = CONFIG_BL0937_CURRENT_MULTIPLIER_UA,
        .power_multiplier_uw = CONFIG_BL0937_POWER_MULTIPLIER_UW,
        .energy_multiplier_uwh = CONFIG_BL0937_ENERGY_MULTIPLIER_UWH,
        .overcurrent_limit_amps = CONFIG_BL0937_OVERCURRENT_LIMIT_MA / 1000.0f,
    };

    return bl0937_init(&config);
}

esp_err_t bl0937_get_reading(bl0937_reading_t *out_reading) {
    if (out_reading == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_state.mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_state.mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    *out_reading = s_state.reading;
    xSemaphoreGive(s_state.mutex);
    return ESP_OK;
}

void bl0937_set_overcurrent_limit(float amps) {
    if (amps < 0.0f) {
        amps = 0.0f;
    }
    s_state.overcurrent_limit_amps = amps;
}

void bl0937_set_overcurrent_enabled(bool enabled) {
    s_state.overcurrent_enabled = enabled;
}

bool bl0937_get_overcurrent_enabled(void) {
    return s_state.overcurrent_enabled;
}

void bl0937_register_overcurrent_callback(bl0937_overcurrent_cb_t callback, void *context) {
    s_state.overcurrent_callback = callback;
    s_state.overcurrent_context = context;
}
