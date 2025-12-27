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

#include <esp_log.h>
#include <esp_err.h>
#include <nvs.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <stddef.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>

#include "bl0937.h"
#include "esp32-lcm.h"

#include "custom_characteristics.h"
#include <button.h>

// -------- GPIO configuration (set these in sdkconfig) --------
#define BUTTON_GPIO      CONFIG_ESP_BUTTON_GPIO
#define RELAY_GPIO       CONFIG_ESP_RELAY_GPIO
#define BLUE_LED_GPIO    CONFIG_ESP_BLUE_LED_GPIO
#define RED_LED_GPIO     CONFIG_ESP_RED_LED_GPIO   // Rode LED: WiFi/lifecycle-indicator

#define BL0937_CF_POWER_SCALE_W_PER_HZ       1125.0
#define BL0937_CF_ENERGY_SCALE_WH_PER_PULSE  0.3125
#define BL0937_CF1_VOLTAGE_SCALE_V_PER_HZ    1.0
#define BL0937_CF1_CURRENT_SCALE_A_PER_HZ    1.0

static const char *RELAY_TAG   = "RELAY";
static const char *BUTTON_TAG  = "BUTTON";
static const char *IDENT_TAG   = "IDENT";
static const char *BL0937_TAG  = "BL0937";

// Relay / plug state (enige bron van waarheid)
static bool relay_on = false;
static bool homekit_ready = false;

// ---------- Low-level GPIO helpers ----------

static inline void relay_write(bool on) {
        gpio_set_level(RELAY_GPIO, on ? 1 : 0);
}

static inline void blue_led_write(bool on) {
        // Blauwe LED: uitsluitend als aan/uit-indicator voor de relay (active low/high afhankelijk van hardware)
        gpio_set_level(BLUE_LED_GPIO, on ? 1 : 0);
}

static inline void red_led_write(bool on) {
        // Rode LED is active high: 1 = AAN, 0 = UIT
        gpio_set_level(RED_LED_GPIO, on ? 1 : 0);
}

// Forward declaration van de characteristic zodat we hem in functies kunnen gebruiken
extern homekit_characteristic_t relay_on_characteristic;
extern homekit_characteristic_t outlet_in_use_characteristic;

// Centrale functie: zet state, stuurt hardware aan en (optioneel) HomeKit-notify
static void relay_set_state(bool on, bool notify_homekit) {
        if (relay_on == on) {
                // Geen verandering, niets te doen
                return;
        }

        relay_on = on;

        // Hardware aansturen
        relay_write(relay_on);
        blue_led_write(relay_on); // Blauwe LED volgt altijd de relay-status

        ESP_LOGI(RELAY_TAG, "Relay state -> %s", relay_on ? "ON" : "OFF");

        // HomeKit characteristic-snapshot updaten
        relay_on_characteristic.value = HOMEKIT_BOOL(relay_on);
        outlet_in_use_characteristic.value = HOMEKIT_BOOL(relay_on);

        // Eventueel HomeKit-clients informeren
        if (notify_homekit) {
                homekit_characteristic_notify(&relay_on_characteristic,
                                              relay_on_characteristic.value);
                homekit_characteristic_notify(&outlet_in_use_characteristic,
                                              outlet_in_use_characteristic.value);
        }
}

// All GPIO Settings
void gpio_init(void) {
        // Relay
        gpio_reset_pin(RELAY_GPIO);
        gpio_set_direction(RELAY_GPIO, GPIO_MODE_OUTPUT);

        // Blauwe LED (aan/uit)
        gpio_reset_pin(BLUE_LED_GPIO);
        gpio_set_direction(BLUE_LED_GPIO, GPIO_MODE_OUTPUT);

        // Rode LED: WiFi-status-indicator
        gpio_reset_pin(RED_LED_GPIO);
        gpio_set_direction(RED_LED_GPIO, GPIO_MODE_OUTPUT);

        // Initial state: alles uit, in sync brengen
        relay_on = false;
        relay_on_characteristic.value = HOMEKIT_BOOL(false);
        outlet_in_use_characteristic.value = HOMEKIT_BOOL(false);
        relay_write(false);
        blue_led_write(false);

        // Bij start is er nog geen WiFi -> rode LED AAN
        red_led_write(true);
}

static void bl0937_notify_reading(const bl0937_reading_t *reading)
{
        custom_voltage.value = HOMEKIT_FLOAT(reading->voltage_v);
        custom_current.value = HOMEKIT_FLOAT(reading->current_a);
        custom_power.value = HOMEKIT_FLOAT(reading->active_power_w);
        custom_total_consumption.value = HOMEKIT_FLOAT(reading->energy_wh / 1000.0); // kWh

        const double apparent_power = reading->voltage_v * reading->current_a;
        double power_factor = 0.0;
        if (apparent_power > 0.01) {
                power_factor = reading->active_power_w / apparent_power;
                power_factor = fmin(1.0, fmax(0.0, power_factor));
        }
        custom_power_factor.value = HOMEKIT_FLOAT(power_factor * 100.0); // percentage

        if (homekit_ready) {
                homekit_characteristic_notify(&custom_voltage, custom_voltage.value);
                homekit_characteristic_notify(&custom_current, custom_current.value);
                homekit_characteristic_notify(&custom_power, custom_power.value);
                homekit_characteristic_notify(&custom_total_consumption,
                                              custom_total_consumption.value);
                homekit_characteristic_notify(&custom_power_factor,
                                              custom_power_factor.value);
        }
}

static void bl0937_task(void *args)
{
        const bl0937_config_t cfg = {
                .cf_pin = CONFIG_ESP_BL0937_CF_GPIO,
                .cf1_pin = CONFIG_ESP_BL0937_CF1_GPIO,
                .sel_pin = CONFIG_ESP_BL0937_SEL_GPIO,
                .sample_period_ms = CONFIG_ESP_BL0937_SAMPLE_PERIOD_MS,
                .pcnt_glitch_ns = CONFIG_ESP_BL0937_PCNT_GLITCH_NS,
                .cf_power_scale = BL0937_CF_POWER_SCALE_W_PER_HZ,
                .cf_energy_scale = BL0937_CF_ENERGY_SCALE_WH_PER_PULSE,
                .cf1_voltage_scale = BL0937_CF1_VOLTAGE_SCALE_V_PER_HZ,
                .cf1_current_scale = BL0937_CF1_CURRENT_SCALE_A_PER_HZ,
        };

        esp_err_t err = bl0937_init(&cfg);
        if (err != ESP_OK) {
                ESP_LOGE(BL0937_TAG, "BL0937 init failed: %s", esp_err_to_name(err));
                vTaskDelete(NULL);
                return;
        }

        while (true) {
                bl0937_reading_t reading = {0};
                err = bl0937_sample(&reading);
                if (err == ESP_OK) {
                        bl0937_notify_reading(&reading);
                } else {
                        ESP_LOGW(BL0937_TAG, "Sample failed: %s", esp_err_to_name(err));
                }

                vTaskDelay(pdMS_TO_TICKS(100));
        }
}

// ---------- Accessory identification (Blue LED) ----------

void accessory_identify_task(void *args) {
        // Blink BLUE LED to identify, then restore previous state
        bool previous_led_state = relay_on; // LED volgt normaal relay_on

        for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 2; j++) {
                        blue_led_write(true);
                        vTaskDelay(pdMS_TO_TICKS(100));
                        blue_led_write(false);
                        vTaskDelay(pdMS_TO_TICKS(100));
                }
                vTaskDelay(pdMS_TO_TICKS(250));
        }

        // Zet LED terug naar de normale toestand (afhankelijk van relay_on)
        blue_led_write(previous_led_state);

        vTaskDelete(NULL);
}

void accessory_identify(homekit_value_t _value) {
        ESP_LOGI(IDENT_TAG, "Accessory identify");
        xTaskCreate(accessory_identify_task, "Accessory identify", configMINIMAL_STACK_SIZE,
                    NULL, 2, NULL);
}

// ---------- HomeKit characteristics ----------

#define DEVICE_NAME          "HomeKit Plug"
#define DEVICE_MANUFACTURER  "StudioPieters®"
#define DEVICE_SERIAL        "NLCC7DFD193A"
#define DEVICE_MODEL         "LS066NL/A"
#define FW_VERSION           "0.0.1"

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, DEVICE_NAME);
homekit_characteristic_t manufacturer = HOMEKIT_CHARACTERISTIC_(MANUFACTURER, DEVICE_MANUFACTURER);
homekit_characteristic_t serial = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, DEVICE_SERIAL);
homekit_characteristic_t model = HOMEKIT_CHARACTERISTIC_(MODEL, DEVICE_MODEL);
homekit_characteristic_t revision = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION, LIFECYCLE_DEFAULT_FW_VERSION);
homekit_characteristic_t ota_trigger = API_OTA_TRIGGER;

// Getter: HomeKit vraagt huidige toestand op
homekit_value_t relay_on_get() {
        return HOMEKIT_BOOL(relay_on);
}

// Setter: aangeroepen door HomeKit (Home-app / Siri / automations)
void relay_on_set(homekit_value_t value) {
        if (value.format != homekit_format_bool) {
                ESP_LOGE(RELAY_TAG, "Invalid value format: %d", value.format);
                return;
        }

        bool new_state = value.bool_value;

        // Via centrale functie, maar ZONDER notify (originator is HomeKit zelf)
        relay_set_state(new_state, false);
}

// We keep a handle to ON characteristic so we can notify on button presses
homekit_characteristic_t relay_on_characteristic =
        HOMEKIT_CHARACTERISTIC_(ON, false, .getter = relay_on_get, .setter = relay_on_set);
homekit_characteristic_t outlet_in_use_characteristic =
        HOMEKIT_CHARACTERISTIC_(OUTLET_IN_USE, false);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverride-init"
homekit_accessory_t *accessories[] = {
        HOMEKIT_ACCESSORY(
                .id = 1,
                .category = homekit_accessory_category_outlets, // Smart plug / outlet
                .services = (homekit_service_t *[]) {
                HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t *[]) {
                        &name,
                        &manufacturer,
                        &serial,
                        &model,
                        &revision,
                        HOMEKIT_CHARACTERISTIC(IDENTIFY, accessory_identify),
                        NULL
                }),
                HOMEKIT_SERVICE(OUTLET, .primary = true, .characteristics = (homekit_characteristic_t *[]) {
                        HOMEKIT_CHARACTERISTIC(NAME, "HomeKit Plug"),
                        &relay_on_characteristic,
                        &outlet_in_use_characteristic,
                        &custom_voltage,
                        &custom_current,
                        &custom_power,
                        &custom_total_consumption,
                        &custom_power_factor,
                        &ota_trigger,
                        NULL
                }),
                NULL
        }),
        NULL
};
#pragma GCC diagnostic pop

homekit_server_config_t config = {
        .accessories = accessories,
        .password = CONFIG_ESP_SETUP_CODE,
        .setupId = CONFIG_ESP_SETUP_ID,
};

// ---------- Button handling ----------

void button_callback(button_event_t event, void *context) {
        switch (event) {
        case button_event_single_press: {
                ESP_LOGI(BUTTON_TAG, "Single press -> toggle relay");

                bool new_state = !relay_on;

                // 1) Zelfde logica als HomeKit, maar nu MET notify
                relay_set_state(new_state, true);

                break;
        }
        case button_event_double_press:
                // Do nothing, by design
                ESP_LOGI(BUTTON_TAG, "Double press -> no action");
                break;
        case button_event_long_press:
                ESP_LOGI(BUTTON_TAG, "Long press (10s) -> factory reset + reboot");
                lifecycle_factory_reset_and_reboot();
                break;
        default:
                ESP_LOGI(BUTTON_TAG, "Unknown button event: %d", event);
                break;
        }
}

// ---------- Wi-Fi / HomeKit startup ----------

void on_wifi_ready() {
        static bool homekit_started = false;

        // WiFi is nu up -> rode LED uit
        red_led_write(false);

        if (homekit_started) {
                ESP_LOGI("INFORMATION", "HomeKit server already running; skipping re-initialization");
                return;
        }

        ESP_LOGI("INFORMATION", "Starting HomeKit server...");
        homekit_server_init(&config);
        homekit_ready = true;
        homekit_started = true;
}

// ---------- app_main ----------

void app_main(void) {
        ESP_ERROR_CHECK(lifecycle_nvs_init());
        lifecycle_log_post_reset_state("INFORMATION");
        ESP_ERROR_CHECK(lifecycle_configure_homekit(&revision, &ota_trigger, "INFORMATION"));

        custom_characteristics_init();
        gpio_init();

        if (xTaskCreate(bl0937_task, "bl0937", 4096, NULL, 5, NULL) != pdPASS) {
                ESP_LOGE(BL0937_TAG, "Failed to start BL0937 task");
        }

        button_config_t btn_cfg = button_config_default(button_active_low);
        btn_cfg.max_repeat_presses = 3;
        btn_cfg.long_press_time = 10000; // 10 seconds for lifecycle_factory_reset_and_reboot

        if (button_create(BUTTON_GPIO, btn_cfg, button_callback, NULL)) {
                ESP_LOGE(BUTTON_TAG, "Failed to initialize button");
        }

        esp_err_t wifi_err = wifi_start(on_wifi_ready);
        if (wifi_err == ESP_ERR_NVS_NOT_FOUND) {
                ESP_LOGW("WIFI", "WiFi configuration not found; provisioning required");
                // Geen geldige WiFi-config -> rode LED AAN
                red_led_write(true);
        } else if (wifi_err != ESP_OK) {
                ESP_LOGE("WIFI", "Failed to start WiFi: %s", esp_err_to_name(wifi_err));
                // Fout bij starten WiFi -> rode LED AAN
                red_led_write(true);
        }
}
