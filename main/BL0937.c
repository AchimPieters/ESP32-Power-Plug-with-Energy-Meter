#include "BL0937.h"

#include <string.h>
#include <math.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

static const char *TAG = "BL0937";

static portMUX_TYPE s_bl0937_mux = portMUX_INITIALIZER_UNLOCKED;

static inline int sel_level(const bl0937_t *dev, bool measure_voltage) {
        // BL0937: SEL=1 -> CF1 voltage RMS freq, SEL=0 -> CF1 current RMS freq
        int level = measure_voltage ? 1 : 0;
        if (dev->sel_inverted) {
                level = !level;
        }
        return level;
}

static void IRAM_ATTR cf_isr(void *arg) {
        bl0937_t *dev = (bl0937_t *)arg;
        dev->cf_pulses++;
}

static void IRAM_ATTR cf1_isr(void *arg) {
        bl0937_t *dev = (bl0937_t *)arg;
        dev->cf1_pulses++;
}

static void bl0937_timer_cb(void *arg) {
        bl0937_t *dev = (bl0937_t *)arg;

        uint32_t cf_cnt, cf1_cnt;
        portENTER_CRITICAL(&s_bl0937_mux);
        cf_cnt = dev->cf_pulses;
        cf1_cnt = dev->cf1_pulses;
        dev->cf_pulses = 0;
        dev->cf1_pulses = 0;
        portEXIT_CRITICAL(&s_bl0937_mux);

        const float dt_s = (float)dev->sample_ms / 1000.0f;
        if (dt_s <= 0.0f) {
                return;
        }

        const float f_cf_hz = (float)cf_cnt / dt_s;
        const float f_cf1_hz = (float)cf1_cnt / dt_s;

        float power_w = f_cf_hz * dev->watts_per_hz;

        float voltage_v = dev->last_voltage;
        float current_a = dev->last_current;

        if (dev->measuring_voltage) {
                voltage_v = f_cf1_hz * dev->volts_per_hz;
        } else {
                current_a = f_cf1_hz * dev->amps_per_hz;
        }

        if (!isfinite(power_w) || power_w < 0) {
                power_w = 0;
        }
        if (!isfinite(voltage_v) || voltage_v < 0) {
                voltage_v = 0;
        }
        if (!isfinite(current_a) || current_a < 0) {
                current_a = 0;
        }

        float a = dev->ema_alpha;
        if (a < 0.0f) {
                a = 0.0f;
        } else if (a > 1.0f) {
                a = 1.0f;
        }

        if (a == 1.0f) {
                dev->last_power = power_w;
                dev->last_voltage = voltage_v;
                dev->last_current = current_a;
        } else if (a > 0.0f) {
                dev->last_power = dev->last_power + a * (power_w - dev->last_power);
                dev->last_voltage = dev->last_voltage + a * (voltage_v - dev->last_voltage);
                dev->last_current = dev->last_current + a * (current_a - dev->last_current);
        } else {
                // a==0: nothing changes
        }

        dev->energy_kwh += (dev->last_power * dt_s) / 3600000.0f;

        dev->measuring_voltage = !dev->measuring_voltage;
        gpio_set_level(dev->gpio_sel, sel_level(dev, dev->measuring_voltage));
}

esp_err_t bl0937_init(bl0937_t *dev,
                      gpio_num_t cf,
                      gpio_num_t cf1,
                      gpio_num_t sel,
                      bool sel_inverted) {
        if (!dev) {
                return ESP_ERR_INVALID_ARG;
        }

        memset(dev, 0, sizeof(*dev));

        dev->gpio_cf = cf;
        dev->gpio_cf1 = cf1;
        dev->gpio_sel = sel;
        dev->sel_inverted = sel_inverted;

        dev->watts_per_hz = 1.0f;
        dev->volts_per_hz = 1.0f;
        dev->amps_per_hz = 1.0f;

        dev->ema_alpha = 0.35f;
        dev->measuring_voltage = true;

        gpio_config_t in_cfg = {
                .pin_bit_mask = (1ULL << dev->gpio_cf) | (1ULL << dev->gpio_cf1),
                .mode = GPIO_MODE_INPUT,
                .pull_up_en = GPIO_PULLUP_DISABLE,
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .intr_type = GPIO_INTR_POSEDGE,
        };
        ESP_RETURN_ON_ERROR(gpio_config(&in_cfg), TAG, "gpio_config input failed");

        gpio_config_t sel_cfg = {
                .pin_bit_mask = (1ULL << dev->gpio_sel),
                .mode = GPIO_MODE_OUTPUT,
                .pull_up_en = GPIO_PULLUP_DISABLE,
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_RETURN_ON_ERROR(gpio_config(&sel_cfg), TAG, "gpio_config sel failed");

        gpio_set_level(dev->gpio_sel, sel_level(dev, dev->measuring_voltage));

        // ISR service might already be installed by other code; treat INVALID_STATE as OK
        esp_err_t isr_err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
        if (isr_err != ESP_OK && isr_err != ESP_ERR_INVALID_STATE) {
                ESP_LOGE(TAG, "install isr service failed: %s", esp_err_to_name(isr_err));
                return isr_err;
        }

        ESP_RETURN_ON_ERROR(gpio_isr_handler_add(dev->gpio_cf, cf_isr, dev), TAG, "add cf isr failed");
        ESP_RETURN_ON_ERROR(gpio_isr_handler_add(dev->gpio_cf1, cf1_isr, dev), TAG, "add cf1 isr failed");

        return ESP_OK;
}

esp_err_t bl0937_start(bl0937_t *dev, uint32_t sample_ms) {
        if (!dev) {
                return ESP_ERR_INVALID_ARG;
        }
        if (dev->running) {
                return ESP_OK;
        }
        if (sample_ms < 250) {
                return ESP_ERR_INVALID_ARG;
        }

        dev->sample_ms = sample_ms;

        const esp_timer_create_args_t targs = {
                .callback = &bl0937_timer_cb,
                .arg = dev,
                .dispatch_method = ESP_TIMER_TASK,
                .name = "bl0937",
                .skip_unhandled_events = true,
        };

        esp_timer_handle_t t = NULL;
        ESP_RETURN_ON_ERROR(esp_timer_create(&targs, &t), TAG, "timer create failed");
        dev->timer = (void *)t;

        ESP_RETURN_ON_ERROR(esp_timer_start_periodic(t, (uint64_t)sample_ms * 1000ULL), TAG, "timer start failed");
        dev->running = true;

        return ESP_OK;
}

void bl0937_stop(bl0937_t *dev) {
        if (!dev || !dev->running) {
                return;
        }

        esp_timer_handle_t t = (esp_timer_handle_t)dev->timer;
        if (t) {
                esp_timer_stop(t);
                esp_timer_delete(t);
        }
        dev->timer = NULL;
        dev->running = false;

        gpio_isr_handler_remove(dev->gpio_cf);
        gpio_isr_handler_remove(dev->gpio_cf1);
}

void bl0937_set_calibration(bl0937_t *dev, float watts_per_hz, float volts_per_hz, float amps_per_hz) {
        if (!dev) {
                return;
        }
        dev->watts_per_hz = watts_per_hz;
        dev->volts_per_hz = volts_per_hz;
        dev->amps_per_hz = amps_per_hz;
}

void bl0937_set_smoothing(bl0937_t *dev, float alpha) {
        if (!dev) {
                return;
        }
        dev->ema_alpha = alpha;
}

esp_err_t bl0937_get_latest(bl0937_t *dev, bl0937_measurement_t *out) {
        if (!dev || !out) {
                return ESP_ERR_INVALID_ARG;
        }

        out->voltage = dev->last_voltage;
        out->current = dev->last_current;
        out->power = dev->last_power;
        out->energy_kwh = dev->energy_kwh;

        return ESP_OK;
}

void bl0937_reset_energy(bl0937_t *dev) {
        if (!dev) {
                return;
        }
        dev->energy_kwh = 0.0f;
}
