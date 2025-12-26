#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bl0937 bl0937_t;
typedef bl0937_t *bl0937_handle_t;

typedef enum {
  BL0937_MODE_COEFF = 0,     // W/Hz, V/Hz, A/Hz (kalibratie-coeffs)
  BL0937_MODE_DATASHEET = 1, // Pure datasheet formules + shunt/divider + Vref
} bl0937_mode_t;

typedef struct {
  float vref_v;           // default 1.218
  float r_shunt_ohm;      // shunt in ohm (bv 0.001)
  float r_div_top_ohm;    // som van weerstanden van L naar VP
  float r_div_bottom_ohm; // som van weerstanden van VP naar N

  float k_power;
  float k_voltage;
  float k_current;
} bl0937_datasheet_params_t;

typedef struct {
  float power_w_per_hz;
  float volt_v_per_hz;
  float curr_a_per_hz;
} bl0937_coeff_params_t;

typedef struct {
  int gpio_cf;  // CF: active power pulses
  int gpio_cf1; // CF1: voltage/current pulses (via SEL)
  int gpio_sel; // SEL: 0=current, 1=voltage

  uint32_t sample_ms;
  uint32_t sel_settle_us;
  uint32_t pcnt_filter_us;

  bl0937_mode_t mode;

  union {
    bl0937_coeff_params_t coeff;
    bl0937_datasheet_params_t ds;
  } cal;
} bl0937_config_t;

typedef struct {
  float power_w;
  float voltage_v;
  float current_a;

  float f_cf_hz;
  float f_v_hz;
  float f_i_hz;

  float energy_wh;
  bool overcurrent;
  int sel_state;
  int64_t last_update_us;
} bl0937_reading_t;

typedef struct {
  bool has_voltage;
  bool has_current;
  bool has_power;
  float voltage_v;
  float current_a;
  float power_w;
  uint32_t duration_ms;
} bl0937_known_load_t;

typedef struct {
  bl0937_mode_t mode;

  bl0937_coeff_params_t coeff;

  float k_power;
  float k_voltage;
  float k_current;

  float avg_f_cf_hz;
  float avg_f_v_hz;
  float avg_f_i_hz;
} bl0937_cal_result_t;

esp_err_t bl0937_init(bl0937_handle_t *out, const bl0937_config_t *cfg);
esp_err_t bl0937_start(bl0937_handle_t h);
esp_err_t bl0937_stop(bl0937_handle_t h);
void bl0937_deinit(bl0937_handle_t h);

esp_err_t bl0937_set_mode(bl0937_handle_t h, bl0937_mode_t mode);
esp_err_t bl0937_set_coeff_params(bl0937_handle_t h,
                                  const bl0937_coeff_params_t *p);
esp_err_t bl0937_set_datasheet_params(bl0937_handle_t h,
                                      const bl0937_datasheet_params_t *p);

esp_err_t bl0937_get_reading(bl0937_handle_t h, bl0937_reading_t *out);
esp_err_t bl0937_reset_energy(bl0937_handle_t h);

esp_err_t bl0937_calibrate_known_load(bl0937_handle_t h,
                                      const bl0937_known_load_t *known,
                                      bl0937_cal_result_t *out_result,
                                      bool apply_now);

/**
 * @brief Initialize + start BL0937 using Kconfig defaults.
 *
 * Non-fatal: logs errors and returns without crashing/reboot-looping.
 */
void bl0937_start_default(void);

// Default-instance helpers (for apps using bl0937_start_default()).
bl0937_handle_t bl0937_default_handle(void);
esp_err_t bl0937_default_get_reading(bl0937_reading_t *out);
esp_err_t bl0937_default_reset_energy(void);
bool bl0937_default_is_ready(void);

#ifdef __cplusplus
}
#endif
