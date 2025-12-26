/**
   Copyright 2026 Achim Pieters | StudioPieters®

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NON INFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.

   for more information visit https://www.studiopieters.nl
 **/

#include "bl0937.h"
#include <sdkconfig.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "soc/soc_caps.h"

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "driver/gpio.h"

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 4, 0)
#error "bl0937 component requires ESP-IDF v5.4+"
#endif

#include "esp_rom_sys.h"
#ifndef BL0937_DELAY_US
#define BL0937_DELAY_US(us) esp_rom_delay_us((uint32_t)(us))
#endif

#if defined(SOC_PCNT_SUPPORTED) && SOC_PCNT_SUPPORTED
#define BL0937_HAS_PCNT 1
#include "driver/pulse_cnt.h"
#else
#define BL0937_HAS_PCNT 0
#endif

static const char *TAG = "bl0937";

// Default instance created by bl0937_start_default()
static bl0937_handle_t s_default_h = NULL;

#define BL0937_K_FCF 1721506.0f
#define BL0937_K_FCFU 15397.0f
#define BL0937_K_FCFI 94638.0f
#define BL0937_VREF_TYP 1.218f

#define BL0937_OC_HZ_MIN 6000.0f
#define BL0937_OC_HZ_MAX 7600.0f

typedef struct {
  float f_cf_hz;
  float f_cf1_hz;
  int sel_type;
} bl0937_raw_t;

struct bl0937 {
  bl0937_config_t cfg;

#if BL0937_HAS_PCNT
  pcnt_unit_handle_t unit_cf;
  pcnt_unit_handle_t unit_cf1;
  pcnt_channel_handle_t ch_cf;
  pcnt_channel_handle_t ch_cf1;
#else
  volatile uint32_t sw_cf;
  volatile uint32_t sw_cf1;
  portMUX_TYPE sw_mux;
  bool isr_service_installed_by_us;
#endif

  esp_timer_handle_t timer;
  SemaphoreHandle_t lock;

  bl0937_reading_t r;
  bl0937_raw_t raw;

  float last_f_v_hz;
  float last_f_i_hz;
};

bl0937_handle_t bl0937_default_handle(void) { return s_default_h; }
bool bl0937_default_is_ready(void) { return (s_default_h != NULL); }

esp_err_t bl0937_default_get_reading(bl0937_reading_t *out) {
  if (!s_default_h)
    return ESP_ERR_INVALID_STATE;
  return bl0937_get_reading(s_default_h, out);
}
esp_err_t bl0937_default_reset_energy(void) {
  if (!s_default_h)
    return ESP_ERR_INVALID_STATE;
  return bl0937_reset_energy(s_default_h);
}

static inline float hz_from_pulses(uint32_t pulses, uint32_t window_ms) {
  const float s = (float)window_ms / 1000.0f;
  return (s > 0.0f) ? ((float)pulses / s) : 0.0f;
}

static inline float divider_ratio(float r_top, float r_bot) {
  const float denom = r_top + r_bot;
  if (denom <= 0.0f)
    return 0.0f;
  return r_bot / denom;
}

static void compute_from_raw(bl0937_t *d, float f_cf_hz, float f_v_hz,
                             float f_i_hz, float *out_p_w, float *out_v_v,
                             float *out_i_a) {
  float p = 0, v = 0, i = 0;

  if (d->cfg.mode == BL0937_MODE_COEFF) {
    const bl0937_coeff_params_t *c = &d->cfg.cal.coeff;
    p = f_cf_hz * c->power_w_per_hz;
    v = f_v_hz * c->volt_v_per_hz;
    i = f_i_hz * c->curr_a_per_hz;
  } else {
    const bl0937_datasheet_params_t *ds = &d->cfg.cal.ds;

    const float vref = (ds->vref_v > 0.0f) ? ds->vref_v : BL0937_VREF_TYP;
    const float kdiv = divider_ratio(ds->r_div_top_ohm, ds->r_div_bottom_ohm);

    float vvp = (BL0937_K_FCFU > 0.0f) ? (f_v_hz * vref / BL0937_K_FCFU) : 0.0f;
    float vsh = (BL0937_K_FCFI > 0.0f) ? (f_i_hz * vref / BL0937_K_FCFI) : 0.0f;

    v = (kdiv > 0.0f) ? (vvp / kdiv) : 0.0f;
    i = (ds->r_shunt_ohm > 0.0f) ? (vsh / ds->r_shunt_ohm) : 0.0f;

    float p_w = 0.0f;
    if (ds->r_shunt_ohm > 0.0f && kdiv > 0.0f && BL0937_K_FCF > 0.0f) {
      const float vv_vi = f_cf_hz * (vref * vref) / BL0937_K_FCF;
      p_w = vv_vi / (kdiv * ds->r_shunt_ohm);
    }
    p = p_w;

    const float kp = (ds->k_power > 0.0f) ? ds->k_power : 1.0f;
    const float kv = (ds->k_voltage > 0.0f) ? ds->k_voltage : 1.0f;
    const float ki = (ds->k_current > 0.0f) ? ds->k_current : 1.0f;

    p *= kp;
    v *= kv;
    i *= ki;
  }

  *out_p_w = p;
  *out_v_v = v;
  *out_i_a = i;
}

#if BL0937_HAS_PCNT
static esp_err_t pcnt_make_counter(pcnt_unit_handle_t *out_unit,
                                   pcnt_channel_handle_t *out_ch,
                                   int gpio_pulse, uint32_t filter_us) {
  ESP_RETURN_ON_FALSE(out_unit && out_ch, ESP_ERR_INVALID_ARG, TAG, "null arg");
  *out_unit = NULL;
  *out_ch = NULL;

  // ESP-IDF v5.4+ PCNT bug/quirk: low_limit == 0 may be rejected as "invalid
  // range". Workaround: use -1 as low limit.
  // :contentReference[oaicite:1]{index=1}
  pcnt_unit_config_t unit_cfg = {
      .low_limit = -1,
      .high_limit = 32767,
      .flags.accum_count = 0,
  };
  ESP_RETURN_ON_ERROR(pcnt_new_unit(&unit_cfg, out_unit), TAG, "pcnt_new_unit");

  pcnt_chan_config_t chan_cfg = {
      .edge_gpio_num = gpio_pulse,
      .level_gpio_num = -1,
      .flags.io_loop_back = 0,
      .flags.invert_edge_input = 0,
      .flags.invert_level_input = 0,
      .flags.virt_edge_io_level = 0,
  };
  ESP_RETURN_ON_ERROR(pcnt_new_channel(*out_unit, &chan_cfg, out_ch), TAG,
                      "pcnt_new_channel");

  ESP_RETURN_ON_ERROR(
      pcnt_channel_set_edge_action(*out_ch, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                                   PCNT_CHANNEL_EDGE_ACTION_HOLD),
      TAG, "set_edge_action");

  if (filter_us > 0) {
    pcnt_glitch_filter_config_t flt = {.max_glitch_ns =
                                           (uint32_t)filter_us * 1000U};
    ESP_RETURN_ON_ERROR(pcnt_unit_set_glitch_filter(*out_unit, &flt), TAG,
                        "set_glitch_filter");
  }

  ESP_RETURN_ON_ERROR(pcnt_unit_enable(*out_unit), TAG, "unit_enable");
  ESP_RETURN_ON_ERROR(pcnt_unit_clear_count(*out_unit), TAG, "unit_clear");
  ESP_RETURN_ON_ERROR(pcnt_unit_start(*out_unit), TAG, "unit_start");

  return ESP_OK;
}
#else
static void IRAM_ATTR gpio_isr_cf(void *arg) {
  bl0937_t *d = (bl0937_t *)arg;
  portENTER_CRITICAL_ISR(&d->sw_mux);
  d->sw_cf++;
  portEXIT_CRITICAL_ISR(&d->sw_mux);
}
static void IRAM_ATTR gpio_isr_cf1(void *arg) {
  bl0937_t *d = (bl0937_t *)arg;
  portENTER_CRITICAL_ISR(&d->sw_mux);
  d->sw_cf1++;
  portEXIT_CRITICAL_ISR(&d->sw_mux);
}

static esp_err_t sw_counter_init(bl0937_t *d) {
  esp_err_t err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
  if (err == ESP_OK)
    d->isr_service_installed_by_us = true;
  else if (err == ESP_ERR_INVALID_STATE) {
    d->isr_service_installed_by_us = false;
    err = ESP_OK;
  }
  ESP_RETURN_ON_ERROR(err, TAG, "gpio_install_isr_service");

  ESP_RETURN_ON_ERROR(gpio_set_intr_type(d->cfg.gpio_cf, GPIO_INTR_POSEDGE),
                      TAG, "intr cf");
  ESP_RETURN_ON_ERROR(gpio_set_intr_type(d->cfg.gpio_cf1, GPIO_INTR_POSEDGE),
                      TAG, "intr cf1");

  ESP_RETURN_ON_ERROR(gpio_isr_handler_add(d->cfg.gpio_cf, gpio_isr_cf, d), TAG,
                      "isr add cf");
  ESP_RETURN_ON_ERROR(gpio_isr_handler_add(d->cfg.gpio_cf1, gpio_isr_cf1, d),
                      TAG, "isr add cf1");

  return ESP_OK;
}

static void sw_counter_deinit(bl0937_t *d) {
  (void)gpio_isr_handler_remove(d->cfg.gpio_cf);
  (void)gpio_isr_handler_remove(d->cfg.gpio_cf1);
  if (d->isr_service_installed_by_us)
    (void)gpio_uninstall_isr_service();
}
#endif

static void timer_cb(void *arg) {
  bl0937_t *d = (bl0937_t *)arg;

  uint32_t count_cf = 0;
  uint32_t count_cf1 = 0;

#if BL0937_HAS_PCNT
  int c0 = 0, c1 = 0;
  (void)pcnt_unit_get_count(d->unit_cf, &c0);
  (void)pcnt_unit_get_count(d->unit_cf1, &c1);
  (void)pcnt_unit_clear_count(d->unit_cf);
  (void)pcnt_unit_clear_count(d->unit_cf1);
  if (c0 > 0)
    count_cf = (uint32_t)c0;
  if (c1 > 0)
    count_cf1 = (uint32_t)c1;
#else
  portENTER_CRITICAL(&d->sw_mux);
  count_cf = d->sw_cf;
  count_cf1 = d->sw_cf1;
  d->sw_cf = 0;
  d->sw_cf1 = 0;
  portEXIT_CRITICAL(&d->sw_mux);
#endif

  const int64_t now_us = esp_timer_get_time();

  const float f_cf_hz = hz_from_pulses(count_cf, d->cfg.sample_ms);
  const float f_cf1_hz = hz_from_pulses(count_cf1, d->cfg.sample_ms);

  const bool oc = (f_cf_hz > BL0937_OC_HZ_MIN && f_cf_hz < BL0937_OC_HZ_MAX);

  const int cf1_type = d->r.sel_state;

  float f_v_hz = d->last_f_v_hz;
  float f_i_hz = d->last_f_i_hz;

  if (cf1_type == 0) {
    d->last_f_i_hz = f_cf1_hz;
    f_i_hz = f_cf1_hz;
  } else {
    d->last_f_v_hz = f_cf1_hz;
    f_v_hz = f_cf1_hz;
  }

  float p_w = 0, v_v = 0, i_a = 0;
  compute_from_raw(d, f_cf_hz, f_v_hz, f_i_hz, &p_w, &v_v, &i_a);

  if (d->lock && xSemaphoreTake(d->lock, pdMS_TO_TICKS(5)) == pdTRUE) {
    d->raw.f_cf_hz = f_cf_hz;
    d->raw.f_cf1_hz = f_cf1_hz;
    d->raw.sel_type = cf1_type;

    d->r.f_cf_hz = f_cf_hz;
    d->r.f_v_hz = d->last_f_v_hz;
    d->r.f_i_hz = d->last_f_i_hz;

    d->r.power_w = p_w;
    d->r.voltage_v = v_v;
    d->r.current_a = i_a;

    const float dt_s = (float)d->cfg.sample_ms / 1000.0f;
    d->r.energy_wh += (d->r.power_w * dt_s) / 3600.0f;

    d->r.overcurrent = oc;
    d->r.last_update_us = now_us;

    xSemaphoreGive(d->lock);
  }

  const int next = (d->r.sel_state == 0) ? 1 : 0;
  (void)gpio_set_level(d->cfg.gpio_sel, next);
  d->r.sel_state = next;

  if (d->cfg.sel_settle_us)
    BL0937_DELAY_US(d->cfg.sel_settle_us);
}

static void set_default_params(bl0937_config_t *cfg) {
  if (cfg->sample_ms == 0)
    cfg->sample_ms = 200;
  if (cfg->sel_settle_us == 0)
    cfg->sel_settle_us = 20;
  if (cfg->pcnt_filter_us == 0)
    cfg->pcnt_filter_us = 2;

  if (cfg->mode == BL0937_MODE_COEFF) {
    if (cfg->cal.coeff.power_w_per_hz <= 0.0f)
      cfg->cal.coeff.power_w_per_hz = 1.0f;
    if (cfg->cal.coeff.volt_v_per_hz <= 0.0f)
      cfg->cal.coeff.volt_v_per_hz = 1.0f;
    if (cfg->cal.coeff.curr_a_per_hz <= 0.0f)
      cfg->cal.coeff.curr_a_per_hz = 1.0f;
  } else {
    bl0937_datasheet_params_t *ds = &cfg->cal.ds;
    if (ds->vref_v <= 0.0f)
      ds->vref_v = BL0937_VREF_TYP;
    if (ds->k_power <= 0.0f)
      ds->k_power = 1.0f;
    if (ds->k_voltage <= 0.0f)
      ds->k_voltage = 1.0f;
    if (ds->k_current <= 0.0f)
      ds->k_current = 1.0f;
  }
}

esp_err_t bl0937_init(bl0937_handle_t *out, const bl0937_config_t *cfg_in) {
  esp_err_t ret = ESP_OK;
  ESP_RETURN_ON_FALSE(out && cfg_in, ESP_ERR_INVALID_ARG, TAG, "null arg");

  bl0937_t *d = (bl0937_t *)calloc(1, sizeof(*d));
  ESP_RETURN_ON_FALSE(d, ESP_ERR_NO_MEM, TAG, "no mem");

  d->cfg = *cfg_in;
  set_default_params(&d->cfg);

#if !BL0937_HAS_PCNT
  d->sw_mux = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
  d->sw_cf = 0;
  d->sw_cf1 = 0;
  d->isr_service_installed_by_us = false;
#endif

  d->lock = xSemaphoreCreateMutex();
  ESP_GOTO_ON_FALSE(d->lock, ESP_ERR_NO_MEM, fail, TAG, "mutex fail");

  gpio_config_t sel = {.pin_bit_mask = (1ULL << d->cfg.gpio_sel),
                       .mode = GPIO_MODE_OUTPUT,
                       .pull_up_en = GPIO_PULLUP_DISABLE,
                       .pull_down_en = GPIO_PULLDOWN_DISABLE,
                       .intr_type = GPIO_INTR_DISABLE};
  ESP_GOTO_ON_ERROR(gpio_config(&sel), fail, TAG, "gpio sel");

  gpio_config_t in = {.pin_bit_mask =
                          (1ULL << d->cfg.gpio_cf) | (1ULL << d->cfg.gpio_cf1),
                      .mode = GPIO_MODE_INPUT,
                      .pull_up_en = GPIO_PULLUP_DISABLE,
                      .pull_down_en = GPIO_PULLDOWN_DISABLE,
                      .intr_type = GPIO_INTR_DISABLE};
  ESP_GOTO_ON_ERROR(gpio_config(&in), fail, TAG, "gpio inputs");

  (void)gpio_set_level(d->cfg.gpio_sel, 0);
  d->r.sel_state = 0;

#if BL0937_HAS_PCNT
  ESP_GOTO_ON_ERROR(pcnt_make_counter(&d->unit_cf, &d->ch_cf, d->cfg.gpio_cf,
                                      d->cfg.pcnt_filter_us),
                    fail, TAG, "pcnt cf");
  ESP_GOTO_ON_ERROR(pcnt_make_counter(&d->unit_cf1, &d->ch_cf1, d->cfg.gpio_cf1,
                                      d->cfg.pcnt_filter_us),
                    fail, TAG, "pcnt cf1");
#else
  ESP_GOTO_ON_ERROR(sw_counter_init(d), fail, TAG, "sw_counter_init");
#endif

  esp_timer_create_args_t targs = {.callback = &timer_cb,
                                   .arg = d,
                                   .dispatch_method = ESP_TIMER_TASK,
                                   .name = "bl0937"};
  ESP_GOTO_ON_ERROR(esp_timer_create(&targs, &d->timer), fail, TAG,
                    "timer create");

  d->r.energy_wh = 0.0f;
  d->r.last_update_us = esp_timer_get_time();

  *out = d;
  return ESP_OK;

fail:
  bl0937_deinit(d);
  return ret;
}

void bl0937_start_default(void) {
  if (s_default_h)
    return;

  bl0937_config_t cfg = {
      .gpio_cf = CONFIG_ESP_BL0937_CF_GPIO,
      .gpio_cf1 = CONFIG_ESP_BL0937_CF1_GPIO,
      .gpio_sel = CONFIG_ESP_BL0937_SEL_GPIO,
      .sample_ms = CONFIG_ESP_BL0937_SAMPLE_MS,
      .sel_settle_us = CONFIG_ESP_BL0937_SEL_SETTLE_US,
      .pcnt_filter_us = CONFIG_ESP_BL0937_PCNT_FILTER_US,
      .mode = BL0937_MODE_COEFF,
  };

  cfg.cal.coeff.power_w_per_hz =
      (float)CONFIG_ESP_BL0937_CAL_POWER_MW_PER_HZ / 1000.0f;
  cfg.cal.coeff.volt_v_per_hz =
      (float)CONFIG_ESP_BL0937_CAL_VOLT_MV_PER_HZ / 1000.0f;
  cfg.cal.coeff.curr_a_per_hz =
      (float)CONFIG_ESP_BL0937_CAL_CURR_MA_PER_HZ / 1000.0f;

  esp_err_t err = bl0937_init(&s_default_h, &cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Default init failed: %s", esp_err_to_name(err));
    s_default_h = NULL;
    return;
  }

  err = bl0937_start(s_default_h);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Default start failed: %s", esp_err_to_name(err));
    bl0937_deinit(s_default_h);
    s_default_h = NULL;
    return;
  }

  ESP_LOGI(TAG, "Default started (CF=%d CF1=%d SEL=%d window=%ums)",
           cfg.gpio_cf, cfg.gpio_cf1, cfg.gpio_sel, (unsigned)cfg.sample_ms);
}

esp_err_t bl0937_start(bl0937_handle_t h) {
  ESP_RETURN_ON_FALSE(h, ESP_ERR_INVALID_ARG, TAG, "null handle");
  ESP_RETURN_ON_ERROR(
      esp_timer_start_periodic(h->timer, (uint64_t)h->cfg.sample_ms * 1000ULL),
      TAG, "timer start");
  return ESP_OK;
}

esp_err_t bl0937_stop(bl0937_handle_t h) {
  ESP_RETURN_ON_FALSE(h, ESP_ERR_INVALID_ARG, TAG, "null handle");
  if (h->timer)
    (void)esp_timer_stop(h->timer);
  return ESP_OK;
}

void bl0937_deinit(bl0937_handle_t h) {
  if (!h)
    return;

  (void)bl0937_stop(h);

  if (h->timer) {
    (void)esp_timer_delete(h->timer);
    h->timer = NULL;
  }

#if BL0937_HAS_PCNT
  if (h->unit_cf1) {
    (void)pcnt_unit_stop(h->unit_cf1);
    (void)pcnt_unit_disable(h->unit_cf1);
  }
  if (h->unit_cf) {
    (void)pcnt_unit_stop(h->unit_cf);
    (void)pcnt_unit_disable(h->unit_cf);
  }

  if (h->ch_cf1) {
    (void)pcnt_del_channel(h->ch_cf1);
    h->ch_cf1 = NULL;
  }
  if (h->ch_cf) {
    (void)pcnt_del_channel(h->ch_cf);
    h->ch_cf = NULL;
  }
  if (h->unit_cf1) {
    (void)pcnt_del_unit(h->unit_cf1);
    h->unit_cf1 = NULL;
  }
  if (h->unit_cf) {
    (void)pcnt_del_unit(h->unit_cf);
    h->unit_cf = NULL;
  }
#else
  sw_counter_deinit(h);
#endif

  if (h->lock) {
    vSemaphoreDelete(h->lock);
    h->lock = NULL;
  }
  free(h);
}

esp_err_t bl0937_set_mode(bl0937_handle_t h, bl0937_mode_t mode) {
  ESP_RETURN_ON_FALSE(h, ESP_ERR_INVALID_ARG, TAG, "null handle");
  xSemaphoreTake(h->lock, portMAX_DELAY);
  h->cfg.mode = mode;
  xSemaphoreGive(h->lock);
  return ESP_OK;
}

esp_err_t bl0937_set_coeff_params(bl0937_handle_t h,
                                  const bl0937_coeff_params_t *p) {
  ESP_RETURN_ON_FALSE(h && p, ESP_ERR_INVALID_ARG, TAG, "null arg");
  ESP_RETURN_ON_FALSE(p->power_w_per_hz > 0 && p->volt_v_per_hz > 0 &&
                          p->curr_a_per_hz > 0,
                      ESP_ERR_INVALID_ARG, TAG, "bad coeff");
  xSemaphoreTake(h->lock, portMAX_DELAY);
  h->cfg.cal.coeff = *p;
  xSemaphoreGive(h->lock);
  return ESP_OK;
}

esp_err_t bl0937_set_datasheet_params(bl0937_handle_t h,
                                      const bl0937_datasheet_params_t *p) {
  ESP_RETURN_ON_FALSE(h && p, ESP_ERR_INVALID_ARG, TAG, "null arg");
  ESP_RETURN_ON_FALSE(p->r_shunt_ohm > 0 && p->r_div_top_ohm >= 0 &&
                          p->r_div_bottom_ohm > 0,
                      ESP_ERR_INVALID_ARG, TAG, "bad ds params");

  xSemaphoreTake(h->lock, portMAX_DELAY);
  h->cfg.cal.ds = *p;
  if (h->cfg.cal.ds.vref_v <= 0.0f)
    h->cfg.cal.ds.vref_v = BL0937_VREF_TYP;
  if (h->cfg.cal.ds.k_power <= 0.0f)
    h->cfg.cal.ds.k_power = 1.0f;
  if (h->cfg.cal.ds.k_voltage <= 0.0f)
    h->cfg.cal.ds.k_voltage = 1.0f;
  if (h->cfg.cal.ds.k_current <= 0.0f)
    h->cfg.cal.ds.k_current = 1.0f;
  xSemaphoreGive(h->lock);
  return ESP_OK;
}

esp_err_t bl0937_get_reading(bl0937_handle_t h, bl0937_reading_t *out) {
  ESP_RETURN_ON_FALSE(h && out, ESP_ERR_INVALID_ARG, TAG, "null arg");
  xSemaphoreTake(h->lock, portMAX_DELAY);
  *out = h->r;
  xSemaphoreGive(h->lock);
  return ESP_OK;
}

esp_err_t bl0937_reset_energy(bl0937_handle_t h) {
  ESP_RETURN_ON_FALSE(h, ESP_ERR_INVALID_ARG, TAG, "null handle");
  xSemaphoreTake(h->lock, portMAX_DELAY);
  h->r.energy_wh = 0.0f;
  xSemaphoreGive(h->lock);
  return ESP_OK;
}

// --- calibration helpers unchanged (you can keep your existing ones) ---
static esp_err_t collect_avg_freqs(bl0937_handle_t h, uint32_t duration_ms,
                                   float *avg_cf, float *avg_v, float *avg_i) {
  ESP_RETURN_ON_FALSE(h, ESP_ERR_INVALID_ARG, TAG, "null handle");
  ESP_RETURN_ON_FALSE(duration_ms >= h->cfg.sample_ms * 2, ESP_ERR_INVALID_ARG,
                      TAG, "duration too short");

  const uint32_t step_ms = h->cfg.sample_ms;
  const int steps = (int)(duration_ms / step_ms);
  if (steps <= 0)
    return ESP_ERR_INVALID_ARG;

  float sum_cf = 0, sum_v = 0, sum_i = 0;
  int n = 0;

  vTaskDelay(pdMS_TO_TICKS(step_ms * 2));

  for (int k = 0; k < steps; k++) {
    bl0937_reading_t r;
    bl0937_get_reading(h, &r);
    sum_cf += r.f_cf_hz;
    sum_v += r.f_v_hz;
    sum_i += r.f_i_hz;
    n++;
    vTaskDelay(pdMS_TO_TICKS(step_ms));
  }

  *avg_cf = (n > 0) ? (sum_cf / n) : 0.0f;
  *avg_v = (n > 0) ? (sum_v / n) : 0.0f;
  *avg_i = (n > 0) ? (sum_i / n) : 0.0f;

  return ESP_OK;
}

esp_err_t bl0937_calibrate_known_load(bl0937_handle_t h,
                                      const bl0937_known_load_t *known,
                                      bl0937_cal_result_t *out_result,
                                      bool apply_now) {
  ESP_RETURN_ON_FALSE(h && known && out_result, ESP_ERR_INVALID_ARG, TAG,
                      "null arg");

  float avg_cf = 0, avg_v = 0, avg_i = 0;
  ESP_RETURN_ON_ERROR(
      collect_avg_freqs(h, known->duration_ms, &avg_cf, &avg_v, &avg_i), TAG,
      "collect_avg_freqs");

  memset(out_result, 0, sizeof(*out_result));
  out_result->mode = h->cfg.mode;
  out_result->avg_f_cf_hz = avg_cf;
  out_result->avg_f_v_hz = avg_v;
  out_result->avg_f_i_hz = avg_i;

  if (h->cfg.mode == BL0937_MODE_COEFF) {
    bl0937_coeff_params_t c = h->cfg.cal.coeff;

    if (known->has_power && avg_cf > 0.0f)
      c.power_w_per_hz = known->power_w / avg_cf;
    if (known->has_voltage && avg_v > 0.0f)
      c.volt_v_per_hz = known->voltage_v / avg_v;
    if (known->has_current && avg_i > 0.0f)
      c.curr_a_per_hz = known->current_a / avg_i;

    out_result->coeff = c;
    if (apply_now)
      return bl0937_set_coeff_params(h, &c);
    return ESP_OK;
  }

  bl0937_datasheet_params_t ds;
  xSemaphoreTake(h->lock, portMAX_DELAY);
  ds = h->cfg.cal.ds;
  xSemaphoreGive(h->lock);

  const float saved_kp = ds.k_power, saved_kv = ds.k_voltage,
              saved_ki = ds.k_current;
  ds.k_power = 1.0f;
  ds.k_voltage = 1.0f;
  ds.k_current = 1.0f;

  float p0 = 0, v0 = 0, i0 = 0;
  {
    bl0937_config_t tmp = h->cfg;
    tmp.cal.ds = ds;

    bl0937_t local = *h;
    local.cfg = tmp;
    compute_from_raw(&local, avg_cf, avg_v, avg_i, &p0, &v0, &i0);
  }

  float kp = (saved_kp > 0.0f) ? saved_kp : 1.0f;
  float kv = (saved_kv > 0.0f) ? saved_kv : 1.0f;
  float ki = (saved_ki > 0.0f) ? saved_ki : 1.0f;

  if (known->has_voltage && v0 > 0.0f)
    kv = known->voltage_v / v0;
  if (known->has_current && i0 > 0.0f)
    ki = known->current_a / i0;
  if (known->has_power && p0 > 0.0f)
    kp = known->power_w / p0;

  out_result->k_power = kp;
  out_result->k_voltage = kv;
  out_result->k_current = ki;

  if (apply_now) {
    ds.k_power = kp;
    ds.k_voltage = kv;
    ds.k_current = ki;
    return bl0937_set_datasheet_params(h, &ds);
  }

  return ESP_OK;
}
