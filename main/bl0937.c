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
#include "custom_characteristics.h"
#include "esp32-lcm.h"

#include <inttypes.h>
#include <limits.h>

#include <driver/gpio.h>
#include <driver/pulse_cnt.h>
#include <esp_err.h>
#include <esp_idf_version.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <nvs.h>
#include <sdkconfig.h>

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 4, 0)
#error "ESP-IDF 5.4+ is required for the pulse_cnt driver API used by BL0937"
#endif

#define BL0937_SAMPLE_INTERVAL_MS 1000
#define ENERGY_NVS_NAMESPACE "energy"
#define ENERGY_NVS_KEY_WH "energy_wh"
#define ENERGY_NVS_SAVE_INTERVAL_US (60LL * 1000LL * 1000LL)
// Calibration values are provided via Kconfig (see ESP_BL0937_* options).

static const char *BL_TAG = "BL0937";

static esp_timer_handle_t sample_timer;
static bool sel_voltage = false;
static float last_voltage = 0.0f;
static float last_current = 0.0f;
static uint64_t energy_wh = 0;
static float energy_wh_fraction = 0.0f;
static int64_t last_energy_save_us = 0;

static bl0937_overcurrent_cb_t overcurrent_cb;
static void *overcurrent_ctx;

#if CONFIG_ESP_OVERCURRENT_ENABLE
static int overcurrent_hits = 0;
static int64_t overcurrent_block_until_us = 0;
#endif
static bool sampling_running = false;

static pcnt_unit_handle_t pcnt_unit_cf;
static pcnt_unit_handle_t pcnt_unit_cf1;
static pcnt_channel_handle_t pcnt_channel_cf;
static pcnt_channel_handle_t pcnt_channel_cf1;

static inline float hz_per_unit_w(void) {
    return CONFIG_ESP_BL0937_CF_HZ_PER_W_X1000 / 1000.0f;
}

static inline float hz_per_unit_v(void) {
    return CONFIG_ESP_BL0937_CF1_HZ_PER_V_X1000 / 1000.0f;
}

static inline float hz_per_unit_a(void) {
    return CONFIG_ESP_BL0937_CF1_HZ_PER_A_X1000 / 1000.0f;
}

static float energy_total_wh(void) {
    return (float)energy_wh + energy_wh_fraction;
}

static void energy_load_from_nvs(void) {
    esp_err_t init_err = lifecycle_nvs_init();
    if (init_err != ESP_OK) {
        ESP_LOGW(BL_TAG, "NVS init failed for energy restore: %s",
                 esp_err_to_name(init_err));
        return;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(ENERGY_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        uint64_t stored_wh = 0;
        err = nvs_get_u64(handle, ENERGY_NVS_KEY_WH, &stored_wh);
        nvs_close(handle);
        if (err == ESP_OK) {
            energy_wh = stored_wh;
            energy_wh_fraction = 0.0f;
            hk_update_energy(energy_total_wh());
            ESP_LOGI(BL_TAG, "Restored energy counter: %" PRIu64 " Wh",
                     energy_wh);
        } else if (err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(BL_TAG, "Failed to read energy counter: %s",
                     esp_err_to_name(err));
        }
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(BL_TAG, "Failed to open energy NVS namespace: %s",
                 esp_err_to_name(err));
    }
}

static void energy_save_to_nvs(bool force) {
    const int64_t now_us = esp_timer_get_time();
    if (!force && last_energy_save_us != 0 &&
        now_us - last_energy_save_us < ENERGY_NVS_SAVE_INTERVAL_US) {
        return;
    }

    esp_err_t init_err = lifecycle_nvs_init();
    if (init_err != ESP_OK) {
        ESP_LOGW(BL_TAG, "NVS init failed for energy save: %s",
                 esp_err_to_name(init_err));
        return;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(ENERGY_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(BL_TAG, "Failed to open energy NVS namespace: %s",
                 esp_err_to_name(err));
        return;
    }

    err = nvs_set_u64(handle, ENERGY_NVS_KEY_WH, energy_wh);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGW(BL_TAG, "Failed to persist energy counter: %s",
                 esp_err_to_name(err));
        return;
    }

    last_energy_save_us = now_us;
}

static void pcnt_setup(pcnt_unit_handle_t *unit,
                       pcnt_channel_handle_t *channel,
                       gpio_num_t gpio) {
    pcnt_unit_config_t unit_cfg = {
        .low_limit = INT16_MIN,
        .high_limit = INT16_MAX,
    };
    pcnt_chan_config_t chan_cfg = {
        .edge_gpio_num = gpio,
        .level_gpio_num = -1,
    };

    ESP_ERROR_CHECK(pcnt_new_unit(&unit_cfg, unit));
    ESP_ERROR_CHECK(pcnt_new_channel(*unit, &chan_cfg, channel));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(
        *channel, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(
        *channel, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP));
    ESP_ERROR_CHECK(pcnt_unit_enable(*unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(*unit));
    ESP_ERROR_CHECK(pcnt_unit_start(*unit));
}

static float freq_to_value(float freq_hz, float hz_per_unit) {
    if (hz_per_unit <= 0.0f) {
        return 0.0f;
    }

    return freq_hz / hz_per_unit;
}

static void handle_overcurrent(float current_a) {
#if CONFIG_ESP_OVERCURRENT_ENABLE
    const float current_ma = current_a * 1000.0f;
    const int64_t now_us = esp_timer_get_time();

    if (now_us < overcurrent_block_until_us) {
        return;
    }

    if (current_ma >= CONFIG_ESP_OVERCURRENT_A_X1000) {
        overcurrent_hits++;
        if (overcurrent_hits >= CONFIG_ESP_OVERCURRENT_DEBOUNCE_SAMPLES) {
            overcurrent_hits = 0;
            if (CONFIG_ESP_OVERCURRENT_COOLDOWN_MS > 0) {
                overcurrent_block_until_us = now_us +
                    ((int64_t)CONFIG_ESP_OVERCURRENT_COOLDOWN_MS * 1000LL);
            }

            if (overcurrent_cb) {
                overcurrent_cb(overcurrent_ctx, current_a);
            }
        }
    } else {
        overcurrent_hits = 0;
    }
#else
    (void)current_a;
#endif
}

static void sample_timer_cb(void *arg) {
    (void)arg;

    int cf_count = 0;
    int cf1_count = 0;
    const float interval_s = BL0937_SAMPLE_INTERVAL_MS / 1000.0f;

    esp_err_t cf_err = pcnt_unit_get_count(pcnt_unit_cf, &cf_count);
    esp_err_t cf1_err = pcnt_unit_get_count(pcnt_unit_cf1, &cf1_count);
    if (cf_err != ESP_OK || cf1_err != ESP_OK) {
        ESP_LOGW(BL_TAG,
                 "Skipping sample due to pcnt read error (CF=%s CF1=%s)",
                 esp_err_to_name(cf_err),
                 esp_err_to_name(cf1_err));
        return;
    }

    if (cf_count < 0) {
        cf_count = 0;
    }
    if (cf1_count < 0) {
        cf1_count = 0;
    }

    pcnt_unit_clear_count(pcnt_unit_cf);
    pcnt_unit_clear_count(pcnt_unit_cf1);

    const float cf_freq = cf_count / interval_s;
    const float cf1_freq = cf1_count / interval_s;

    const float power_w = freq_to_value(cf_freq, hz_per_unit_w());

    if (sel_voltage) {
        last_voltage = freq_to_value(cf1_freq, hz_per_unit_v());
        hk_update_voltage(last_voltage);
    } else {
        last_current = freq_to_value(cf1_freq, hz_per_unit_a());
        hk_update_current(last_current);
        handle_overcurrent(last_current);
    }

    hk_update_power(power_w);

    const float delta_wh = (power_w * interval_s) / 3600.0f;
    if (delta_wh > 0.0f) {
        energy_wh_fraction += delta_wh;
        if (energy_wh_fraction >= 1.0f) {
            const uint64_t whole_wh = (uint64_t)energy_wh_fraction;
            energy_wh += whole_wh;
            energy_wh_fraction -= (float)whole_wh;
        }
    }
    hk_update_energy(energy_total_wh());
    energy_save_to_nvs(false);

#if CONFIG_ESP_BL0937_CALIBRATION_LOG
    ESP_LOGI(BL_TAG,
             "Calibration sample: CF=%.2fHz CF1=%.2fHz SEL=%s",
             cf_freq,
             cf1_freq,
             sel_voltage ? "V" : "A");
#endif

    sel_voltage = !sel_voltage;
    gpio_set_level(CONFIG_ESP_SEL_PIN, sel_voltage ? 1 : 0);
}

void bl0937_set_overcurrent_callback(bl0937_overcurrent_cb_t cb, void *ctx) {
    overcurrent_cb = cb;
    overcurrent_ctx = ctx;
}

void bl0937_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << CONFIG_ESP_CF_PIN) | (1ULL << CONFIG_ESP_CF1_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&io_conf));

    gpio_config_t sel_conf = {
        .pin_bit_mask = (1ULL << CONFIG_ESP_SEL_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&sel_conf));
    sel_voltage = false;
    gpio_set_level(CONFIG_ESP_SEL_PIN, 0);

    pcnt_setup(&pcnt_unit_cf, &pcnt_channel_cf, CONFIG_ESP_CF_PIN);
    pcnt_setup(&pcnt_unit_cf1, &pcnt_channel_cf1, CONFIG_ESP_CF1_PIN);

    esp_timer_create_args_t timer_args = {
        .callback = &sample_timer_cb,
        .name = "bl0937_sample",
    };

    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &sample_timer));

    ESP_LOGI(BL_TAG, "BL0937 initialized on CF=%d CF1=%d SEL=%d",
             CONFIG_ESP_CF_PIN, CONFIG_ESP_CF1_PIN, CONFIG_ESP_SEL_PIN);
    ESP_LOGI(BL_TAG, "Calibration Hz/unit: W=%.3f V=%.3f A=%.3f",
             hz_per_unit_w(), hz_per_unit_v(), hz_per_unit_a());

    energy_load_from_nvs();
}

void bl0937_start(void) {
    if (!sample_timer) {
        ESP_LOGW(BL_TAG, "BL0937 not initialized");
        return;
    }

    if (sampling_running) {
        ESP_LOGW(BL_TAG, "BL0937 sampling already running");
        return;
    }

    ESP_ERROR_CHECK(esp_timer_start_periodic(sample_timer,
                                             BL0937_SAMPLE_INTERVAL_MS * 1000));
    sampling_running = true;
}

void bl0937_stop(void) {
    if (!sample_timer) {
        return;
    }

    esp_timer_stop(sample_timer);
    sampling_running = false;
    energy_save_to_nvs(true);
}
