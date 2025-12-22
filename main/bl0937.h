#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <driver/gpio.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>

typedef enum {
    BL0937_CF1_MODE_CURRENT = 0,
    BL0937_CF1_MODE_VOLTAGE = 1,
} bl0937_cf1_mode_t;

typedef struct {
    gpio_num_t cf_pin;
    gpio_num_t cf1_pin;
    gpio_num_t sel_pin;
    float power_coeff;
    float current_coeff;
    float voltage_coeff;
} bl0937_config_t;

typedef struct {
    bl0937_config_t config;
    bl0937_cf1_mode_t cf1_mode;
    uint32_t cf_pulse_count;
    uint32_t cf1_pulse_count;
    uint32_t cf_period_us;
    uint32_t cf1_period_us;
    int64_t last_cf_us;
    int64_t last_cf1_us;
    uint32_t last_energy_pulse_count;
    float power_w;
    float current_a;
    float voltage_v;
    float energy_wh;
    portMUX_TYPE lock;
} bl0937_t;

esp_err_t bl0937_init(bl0937_t *dev, const bl0937_config_t *config);
void bl0937_set_cf1_mode(bl0937_t *dev, bl0937_cf1_mode_t mode);
void bl0937_update(bl0937_t *dev);
float bl0937_get_power_w(const bl0937_t *dev);
float bl0937_get_current_a(const bl0937_t *dev);
float bl0937_get_voltage_v(const bl0937_t *dev);
float bl0937_get_energy_wh(const bl0937_t *dev);
