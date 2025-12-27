#include "bl0937.h"

#include <limits.h>
#include <math.h>
#include <string.h>

#include "driver/pulse_cnt.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"

#if !SOC_PCNT_SUPPORTED
#error "BL0937 driver requires pulse counter (PCNT) peripheral support on this target"
#endif

#define BL0937_DEFAULT_SAMPLE_PERIOD_MS 1000

static const char *TAG = "bl0937";

typedef struct {
    bl0937_config_t cfg;
    pcnt_unit_handle_t cf_unit;
    pcnt_unit_handle_t cf1_unit;
    SemaphoreHandle_t lock;
    double energy_accum_wh;
    bool initialised;
} bl0937_context_t;

static bl0937_context_t s_ctx = {0};

static void bl0937_cleanup(void)
{
    if (s_ctx.cf_unit) {
        pcnt_unit_stop(s_ctx.cf_unit);
        pcnt_unit_disable(s_ctx.cf_unit);
        pcnt_del_unit(s_ctx.cf_unit);
        s_ctx.cf_unit = NULL;
    }

    if (s_ctx.cf1_unit) {
        pcnt_unit_stop(s_ctx.cf1_unit);
        pcnt_unit_disable(s_ctx.cf1_unit);
        pcnt_del_unit(s_ctx.cf1_unit);
        s_ctx.cf1_unit = NULL;
    }

    if (s_ctx.lock) {
        vSemaphoreDelete(s_ctx.lock);
        s_ctx.lock = NULL;
    }

    s_ctx.energy_accum_wh = 0;
    s_ctx.initialised = false;
    memset(&s_ctx.cfg, 0, sizeof(s_ctx.cfg));
}

static esp_err_t bl0937_init_channel(pcnt_unit_handle_t unit, gpio_num_t pin)
{
    pcnt_chan_config_t chan_cfg = {
        .edge_gpio_num = pin,
        .level_gpio_num = -1,
        .pos_mode = PCNT_CHANNEL_EDGE_ACTION_INCREASE,
        .neg_mode = PCNT_CHANNEL_EDGE_ACTION_INCREASE,
        .hctrl_mode = PCNT_CHANNEL_LEVEL_ACTION_KEEP,
        .lctrl_mode = PCNT_CHANNEL_LEVEL_ACTION_KEEP,
    };

    pcnt_channel_handle_t channel = NULL;
    ESP_RETURN_ON_ERROR(pcnt_new_channel(unit, &chan_cfg, &channel), TAG, "new channel");

    return ESP_OK;
}

static esp_err_t bl0937_init_unit(pcnt_unit_handle_t *unit)
{
    pcnt_unit_config_t unit_cfg = {
        .high_limit = INT16_MAX,
        .low_limit = 0,
    };

    ESP_RETURN_ON_ERROR(pcnt_new_unit(&unit_cfg, unit), TAG, "unit create");

    if (s_ctx.cfg.pcnt_glitch_ns > 0) {
#if defined(SOC_PCNT_SUPPORT_GLITCH_FILTER) && SOC_PCNT_SUPPORT_GLITCH_FILTER
        pcnt_glitch_filter_config_t filter_cfg = {
            .max_glitch_ns = s_ctx.cfg.pcnt_glitch_ns,
        };
        ESP_RETURN_ON_ERROR(pcnt_unit_set_glitch_filter(*unit, &filter_cfg), TAG, "set glitch");
#else
        ESP_RETURN_ON_ERROR(ESP_ERR_NOT_SUPPORTED, TAG, "glitch filter unsupported");
#endif
    }

    return ESP_OK;
}

static esp_err_t bl0937_measure_count(pcnt_unit_handle_t unit, TickType_t window_ticks, int *count)
{
    ESP_RETURN_ON_ERROR(pcnt_unit_disable(unit), TAG, "disable before clear");
    ESP_RETURN_ON_ERROR(pcnt_unit_clear_count(unit), TAG, "clear");
    ESP_RETURN_ON_ERROR(pcnt_unit_enable(unit), TAG, "enable");
    ESP_RETURN_ON_ERROR(pcnt_unit_start(unit), TAG, "start");

    vTaskDelay(window_ticks);

    ESP_RETURN_ON_ERROR(pcnt_unit_get_count(unit, count), TAG, "get count");
    ESP_RETURN_ON_ERROR(pcnt_unit_stop(unit), TAG, "stop");

    return ESP_OK;
}

static double bl0937_frequency_from_counts(int counts, uint32_t window_ms)
{
    if (window_ms == 0) {
        return 0.0;
    }

    const double period_seconds = (double)window_ms / 1000.0;
    return counts / period_seconds;
}

esp_err_t bl0937_init(const bl0937_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_ctx.initialised) {
        return ESP_ERR_INVALID_STATE;
    }

    bl0937_cleanup();
    s_ctx.cfg = *config;
    if (s_ctx.cfg.sample_period_ms == 0) {
        s_ctx.cfg.sample_period_ms = BL0937_DEFAULT_SAMPLE_PERIOD_MS;
    }

    if (s_ctx.cfg.cf_pin == GPIO_NUM_NC || s_ctx.cfg.cf1_pin == GPIO_NUM_NC || s_ctx.cfg.sel_pin == GPIO_NUM_NC) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!GPIO_IS_VALID_GPIO(s_ctx.cfg.cf_pin) || !GPIO_IS_VALID_GPIO(s_ctx.cfg.cf1_pin) ||
        !GPIO_IS_VALID_OUTPUT_GPIO(s_ctx.cfg.sel_pin)) {
        return ESP_ERR_INVALID_ARG;
    }

#if !(defined(SOC_PCNT_SUPPORT_GLITCH_FILTER) && SOC_PCNT_SUPPORT_GLITCH_FILTER)
    if (s_ctx.cfg.pcnt_glitch_ns > 0) {
        return ESP_ERR_NOT_SUPPORTED;
    }
#endif

    s_ctx.lock = xSemaphoreCreateMutex();
    if (!s_ctx.lock) {
        bl0937_cleanup();
        return ESP_ERR_NO_MEM;
    }

    gpio_config_t io_cfg = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << s_ctx.cfg.sel_pin,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&io_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SEL config failed: %s", esp_err_to_name(err));
        bl0937_cleanup();
        return err;
    }
    gpio_set_level(s_ctx.cfg.sel_pin, 0);

    err = bl0937_init_unit(&s_ctx.cf_unit);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "CF unit init failed: %s", esp_err_to_name(err));
        bl0937_cleanup();
        return err;
    }
    err = bl0937_init_channel(s_ctx.cf_unit, s_ctx.cfg.cf_pin);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "CF channel init failed: %s", esp_err_to_name(err));
        bl0937_cleanup();
        return err;
    }

    err = bl0937_init_unit(&s_ctx.cf1_unit);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "CF1 unit init failed: %s", esp_err_to_name(err));
        bl0937_cleanup();
        return err;
    }
    err = bl0937_init_channel(s_ctx.cf1_unit, s_ctx.cfg.cf1_pin);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "CF1 channel init failed: %s", esp_err_to_name(err));
        bl0937_cleanup();
        return err;
    }

    s_ctx.energy_accum_wh = 0;
    s_ctx.initialised = true;

    ESP_LOGI(TAG, "BL0937 initialised: CF=%d CF1=%d SEL=%d", s_ctx.cfg.cf_pin, s_ctx.cfg.cf1_pin, s_ctx.cfg.sel_pin);
    return ESP_OK;
}

esp_err_t bl0937_sample(bl0937_reading_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_ctx.initialised) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_ctx.lock, pdMS_TO_TICKS(2000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    TickType_t window_ticks = pdMS_TO_TICKS(s_ctx.cfg.sample_period_ms);
    if (window_ticks == 0) {
        window_ticks = 1; // ensure we wait at least one tick on very small sampling windows
    }

    int cf_counts = 0;
    int cf1_counts_voltage = 0;
    int cf1_counts_current = 0;

    // Measure active power via CF
    esp_err_t err = bl0937_measure_count(s_ctx.cf_unit, window_ticks, &cf_counts);
    if (err != ESP_OK) {
        goto exit;
    }

    // Measure voltage on CF1 (SEL = 0)
    gpio_set_level(s_ctx.cfg.sel_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    err = bl0937_measure_count(s_ctx.cf1_unit, window_ticks, &cf1_counts_voltage);
    if (err != ESP_OK) {
        goto exit;
    }

    // Measure current on CF1 (SEL = 1)
    gpio_set_level(s_ctx.cfg.sel_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    err = bl0937_measure_count(s_ctx.cf1_unit, window_ticks, &cf1_counts_current);
    if (err != ESP_OK) {
        goto exit;
    }

    const double cf_freq = bl0937_frequency_from_counts(cf_counts, s_ctx.cfg.sample_period_ms);
    const double cf1_voltage_freq = bl0937_frequency_from_counts(cf1_counts_voltage, s_ctx.cfg.sample_period_ms);
    const double cf1_current_freq = bl0937_frequency_from_counts(cf1_counts_current, s_ctx.cfg.sample_period_ms);

    out->active_power_w = cf_freq * s_ctx.cfg.cf_power_scale;
    const double energy_delta = cf_counts * s_ctx.cfg.cf_energy_scale;
    s_ctx.energy_accum_wh += energy_delta;
    out->energy_wh = s_ctx.energy_accum_wh;
    out->voltage_v = cf1_voltage_freq * s_ctx.cfg.cf1_voltage_scale;
    out->current_a = cf1_current_freq * s_ctx.cfg.cf1_current_scale;

exit:
    xSemaphoreGive(s_ctx.lock);
    return err;
}

