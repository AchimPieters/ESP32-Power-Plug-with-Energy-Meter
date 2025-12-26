#include "bl0937.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_check.h>

#include "sdkconfig.h"

#include "custom_characteristics.h"

#ifndef BL0937_DEFAULT_SAMPLE_PERIOD_MS
#define BL0937_DEFAULT_SAMPLE_PERIOD_MS 1000
#endif

#ifndef BL0937_DEFAULT_POWER_SCALE
#define BL0937_DEFAULT_POWER_SCALE 1.0f
#endif

#ifndef BL0937_DEFAULT_VOLTAGE_SCALE
#define BL0937_DEFAULT_VOLTAGE_SCALE 1.0f
#endif

#ifndef BL0937_DEFAULT_CURRENT_SCALE
#define BL0937_DEFAULT_CURRENT_SCALE 0.001f
#endif

#ifndef BL0937_DEFAULT_MAINS_FREQUENCY
#define BL0937_DEFAULT_MAINS_FREQUENCY 50.0f
#endif

static const char *TAG = "BL0937";

static bl0937_config_t bl0937_cfg;
static TaskHandle_t bl0937_task_handle;

static portMUX_TYPE bl0937_spinlock = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t bl0937_cf_pulses;
static volatile uint32_t bl0937_cf1_pulses;

static float bl0937_voltage;
static float bl0937_current;
static float bl0937_power;
static float bl0937_energy_kwh;

static void IRAM_ATTR bl0937_cf_isr_handler(void *arg) {
        (void)arg;
        bl0937_cf_pulses++;
}

static void IRAM_ATTR bl0937_cf1_isr_handler(void *arg) {
        (void)arg;
        bl0937_cf1_pulses++;
}

static void bl0937_reset_counters(uint32_t *cf_count, uint32_t *cf1_count) {
        taskENTER_CRITICAL(&bl0937_spinlock);
        *cf_count = bl0937_cf_pulses;
        *cf1_count = bl0937_cf1_pulses;
        bl0937_cf_pulses = 0;
        bl0937_cf1_pulses = 0;
        taskEXIT_CRITICAL(&bl0937_spinlock);
}

static float bl0937_calculate_power_factor(void) {
        if (bl0937_voltage <= 0.0f || bl0937_current <= 0.0f) {
                return 0.0f;
        }

        float pf = bl0937_power / (bl0937_voltage * bl0937_current);
        if (pf < 0.0f) {
                pf = 0.0f;
        } else if (pf > 1.0f) {
                pf = 1.0f;
        }
        return pf;
}

static void bl0937_sample_task(void *arg) {
        (void)arg;
        bool measure_current = true;

        gpio_set_level(bl0937_cfg.sel_gpio, measure_current ? 0 : 1);

        while (1) {
                vTaskDelay(pdMS_TO_TICKS(bl0937_cfg.sample_period_ms));

                uint32_t cf_count = 0;
                uint32_t cf1_count = 0;
                bl0937_reset_counters(&cf_count, &cf1_count);

                bl0937_power = cf_count * bl0937_cfg.power_scale;
                bl0937_energy_kwh +=
                        (bl0937_power * (bl0937_cfg.sample_period_ms / 1000.0f)) / 3600000.0f;

                if (measure_current) {
                        bl0937_current = cf1_count * bl0937_cfg.current_scale;
                } else {
                        bl0937_voltage = cf1_count * bl0937_cfg.voltage_scale;
                }

                float power_factor = bl0937_calculate_power_factor();

                custom_characteristics_update(bl0937_voltage,
                                              bl0937_current,
                                              bl0937_power,
                                              power_factor,
                                              bl0937_cfg.nominal_frequency,
                                              bl0937_energy_kwh);

                measure_current = !measure_current;
                gpio_set_level(bl0937_cfg.sel_gpio, measure_current ? 0 : 1);
        }
}

static esp_err_t bl0937_configure_gpio(gpio_num_t gpio, gpio_mode_t mode) {
        gpio_config_t cfg = {
                .intr_type = GPIO_INTR_DISABLE,
                .mode = mode,
                .pin_bit_mask = 1ULL << gpio,
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .pull_up_en = (mode == GPIO_MODE_INPUT) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        };
        return gpio_config(&cfg);
}

static esp_err_t bl0937_attach_isr(gpio_num_t gpio, gpio_isr_t handler) {
        ESP_RETURN_ON_ERROR(gpio_set_intr_type(gpio, GPIO_INTR_POSEDGE), TAG, "set intr type");
        return gpio_isr_handler_add(gpio, handler, NULL);
}

esp_err_t bl0937_init(const bl0937_config_t *config) {
        ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "config is null");
        ESP_RETURN_ON_FALSE(config->cf_gpio >= 0 && config->cf1_gpio >= 0 && config->sel_gpio >= 0,
                            ESP_ERR_INVALID_ARG, TAG, "invalid GPIO selection");
        ESP_RETURN_ON_FALSE(GPIO_IS_VALID_GPIO(config->cf_gpio) &&
                            GPIO_IS_VALID_GPIO(config->cf1_gpio) &&
                            GPIO_IS_VALID_OUTPUT_GPIO(config->sel_gpio),
                            ESP_ERR_INVALID_ARG, TAG, "GPIO selection not supported on target %s", CONFIG_IDF_TARGET);
        ESP_RETURN_ON_FALSE(bl0937_task_handle == NULL, ESP_ERR_INVALID_STATE, TAG,
                            "driver already initialised");

        bl0937_cfg = *config;
        if (bl0937_cfg.sample_period_ms == 0) {
                bl0937_cfg.sample_period_ms = BL0937_DEFAULT_SAMPLE_PERIOD_MS;
        }

        ESP_RETURN_ON_ERROR(bl0937_configure_gpio(bl0937_cfg.cf_gpio, GPIO_MODE_INPUT), TAG, "CF gpio");
        ESP_RETURN_ON_ERROR(bl0937_configure_gpio(bl0937_cfg.cf1_gpio, GPIO_MODE_INPUT), TAG, "CF1 gpio");
        ESP_RETURN_ON_ERROR(bl0937_configure_gpio(bl0937_cfg.sel_gpio, GPIO_MODE_OUTPUT), TAG, "SEL gpio");

        esp_err_t isr_err = gpio_install_isr_service(0);
        if (isr_err != ESP_OK && isr_err != ESP_ERR_INVALID_STATE) {
                ESP_RETURN_ON_ERROR(isr_err, TAG, "install isr service");
        }
        ESP_RETURN_ON_ERROR(bl0937_attach_isr(bl0937_cfg.cf_gpio, bl0937_cf_isr_handler), TAG, "CF isr");
        ESP_RETURN_ON_ERROR(bl0937_attach_isr(bl0937_cfg.cf1_gpio, bl0937_cf1_isr_handler), TAG, "CF1 isr");

        if (bl0937_task_handle == NULL) {
                BaseType_t created = xTaskCreate(bl0937_sample_task,
                                                 "bl0937-sample",
                                                 4096,
                                                 NULL,
                                                 tskIDLE_PRIORITY + 1,
                                                 &bl0937_task_handle);
                ESP_RETURN_ON_FALSE(created == pdPASS, ESP_FAIL, TAG, "failed to start task");
        }

        ESP_LOGI(TAG, "BL0937 monitoring started: CF=%d CF1=%d SEL=%d", bl0937_cfg.cf_gpio,
                 bl0937_cfg.cf1_gpio, bl0937_cfg.sel_gpio);
        return ESP_OK;
}

esp_err_t bl0937_start_default(void) {
        bl0937_config_t cfg = {
                .cf_gpio = CONFIG_ESP_BL0937_CF_GPIO,
                .cf1_gpio = CONFIG_ESP_BL0937_CF1_GPIO,
                .sel_gpio = CONFIG_ESP_BL0937_SEL_GPIO,
                .power_scale = BL0937_DEFAULT_POWER_SCALE,
                .voltage_scale = BL0937_DEFAULT_VOLTAGE_SCALE,
                .current_scale = BL0937_DEFAULT_CURRENT_SCALE,
                .nominal_frequency = BL0937_DEFAULT_MAINS_FREQUENCY,
                .sample_period_ms = BL0937_DEFAULT_SAMPLE_PERIOD_MS,
        };

        return bl0937_init(&cfg);
}

