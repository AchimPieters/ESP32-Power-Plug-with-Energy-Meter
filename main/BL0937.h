#pragma once

#include <stdbool.h>

#include <driver/uart.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uart_port_t uart_num;
    int tx_gpio;
    int rx_gpio;
    int baud_rate;
    uint32_t voltage_multiplier_uv;
    uint32_t current_multiplier_ua;
    uint32_t power_multiplier_uw;
    uint32_t energy_multiplier_uwh;
    float overcurrent_limit_amps;
} bl0937_config_t;

typedef struct {
    float voltage;
    float current;
    float power;
    float total_kwh;
    bool overcurrent;
} bl0937_reading_t;

typedef void (*bl0937_overcurrent_cb_t)(float current_amps, void *context);

esp_err_t bl0937_init(const bl0937_config_t *config);
esp_err_t bl0937_init_default(void);
esp_err_t bl0937_get_reading(bl0937_reading_t *out_reading);
void bl0937_set_overcurrent_limit(float amps);
void bl0937_set_overcurrent_enabled(bool enabled);
bool bl0937_get_overcurrent_enabled(void);
void bl0937_register_overcurrent_callback(bl0937_overcurrent_cb_t callback, void *context);

#ifdef __cplusplus
}
#endif
