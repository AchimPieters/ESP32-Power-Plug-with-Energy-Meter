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

#include <stdio.h>
#include <math.h>

#include <esp_log.h>
#include <esp_err.h>
#include <nvs.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>

#include "esp32-lcm.h"
#include <button.h>

#include "custom_characteristics.h"

// BL0937
#include "bl0937.h"
#include "bl0937_nvs.h"
#include "bl0937_nvs_keys.h"


// -------- GPIO configuration (set these in sdkconfig) --------
#define BUTTON_GPIO      CONFIG_ESP_BUTTON_GPIO
#define RELAY_GPIO       CONFIG_ESP_RELAY_GPIO
#define BLUE_LED_GPIO    CONFIG_ESP_BLUE_LED_GPIO
#define RED_LED_GPIO     CONFIG_ESP_RED_LED_GPIO   // Rode LED: WiFi/lifecycle-indicator

#define CF_GPIO          CONFIG_ESP_CF_PIN
#define CF1_GPIO         CONFIG_ESP_CF1_PIN
#define SEL_GPIO         CONFIG_ESP_SEL_PIN

// -------- Overcurrent via menuconfig (Kconfig) --------
// Verwacht dat je deze opties hebt toegevoegd onder menu "StudioPieters":
//   CONFIG_ESP_OVERCURRENT_ENABLE (bool)
//   CONFIG_ESP_OVERCURRENT_A_X1000 (int, mA)
//   CONFIG_ESP_OVERCURRENT_DEBOUNCE_SAMPLES (int)
//   CONFIG_ESP_OVERCURRENT_COOLDOWN_MS (int)
#ifdef CONFIG_ESP_OVERCURRENT_ENABLE
  #define OC_ENABLED              1
  #define OC_TRIP_CURRENT_A       ((float)CONFIG_ESP_OVERCURRENT_A_X1000 / 1000.0f)
  #define OC_DEBOUNCE_SAMPLES     (CONFIG_ESP_OVERCURRENT_DEBOUNCE_SAMPLES)
  #define OC_COOLDOWN_MS          (CONFIG_ESP_OVERCURRENT_COOLDOWN_MS)
#else
  #define OC_ENABLED              0
#endif

static const char *RELAY_TAG   = "RELAY";
static const char *BUTTON_TAG  = "BUTTON";
static const char *IDENT_TAG   = "IDENT";
static const char *METER_TAG   = "BL0937";
static const char *OC_TAG      = "OVERCURRENT";

// Relay / plug state (enige bron van waarheid)
static bool relay_on = false;

// WiFi status (voor rode LED restore na OC flash)
static bool wifi_ready = false;

// Overcurrent state
static bool oc_latched = false;
static int oc_over_count = 0;
static TickType_t oc_release_tick = 0;

// BL0937 handle + task handle
static bl0937_handle_t *meter = NULL;
static TaskHandle_t meter_task_handle = NULL;

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

// Centrale functie: zet state, stuurt hardware aan en (optioneel) HomeKit-notify
static void relay_set_state(bool on, bool notify_homekit) {

#if OC_ENABLED
        // Blokkeer ON tijdens cooldown
        if (on && oc_latched) {
                TickType_t now = xTaskGetTickCount();
                if (OC_COOLDOWN_MS > 0 && now < oc_release_tick) {
                        ESP_LOGW(OC_TAG, "Relay ON blocked (cooldown active)");
                        return;
                }
                // Cooldown voorbij -> vrijgeven
                oc_latched = false;
                oc_over_count = 0;
        }
#endif

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

        // Eventueel HomeKit-clients informeren
        if (notify_homekit) {
                homekit_characteristic_notify(&relay_on_characteristic,
                                              relay_on_characteristic.value);
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
        relay_write(false);
        blue_led_write(false);

        // Bij start is er nog geen WiFi -> rode LED AAN
        wifi_ready = false;
        red_led_write(true);
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
#define DEVICE_NAME          "HomeKit Plug with Energy Meter"
#define DEVICE_MANUFACTURER  "StudioPieters®"
#define DEVICE_SERIAL        "NLCC7DFD193A"
#define DEVICE_MODEL         "LS087NL/A"
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

                        // OTA
                        &ota_trigger,

                        // Custom energy characteristics
                        &ch_voltage,
                        &ch_current,
                        &ch_power,
                        &ch_energy,

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

// ---------- BL0937 metering + Overcurrent policy ----------

static esp_err_t meter_init(void) {
        bl0937_config_t cfg = {
                .gpio_cf = CF_GPIO,
                .gpio_cf1 = CF1_GPIO,
                .gpio_sel = SEL_GPIO,

                // Als je board externe pull-ups heeft kun je dit op false zetten
                .cf_pull_up = true,
                .cf1_pull_up = true,

                // SEL=0 => IRMS, SEL=1 => VRMS (veelgebruikte wiring)
                .sel0_is_irms = true,

                // Cal factors (defaults; kunnen overschreven worden door NVS calib blob)
                .cal_vrms  = 1.0f,
                .cal_irms  = 1.0f,
                .cal_power = 1.0f,

#ifdef CONFIG_BL0937_DEFAULT_EMA_ALPHA_V
                .ema_alpha_v = CONFIG_BL0937_DEFAULT_EMA_ALPHA_V,
                .ema_alpha_i = CONFIG_BL0937_DEFAULT_EMA_ALPHA_I,
                .ema_alpha_p = CONFIG_BL0937_DEFAULT_EMA_ALPHA_P,
#endif
        };

        // Optioneel: calib uit NVS laden (namespace/key model is library-dependent).
        // Als er geen calib is, blijven defaults actief.
        // (Dit is veilig: geen error = gewoon verder.)
        bl0937_calib_blob_t cal;
        char key[32] = {0};
        bl0937_make_cal_key_from_mac(key, sizeof(key));
        if (bl0937_nvs_load("bl0937", key, &cal) == ESP_OK) {
                bl0937_apply_calib(&cfg, &cal);
                ESP_LOGI(METER_TAG, "Calibration loaded (ns=bl0937 key=%s)", key);
        } else {
                ESP_LOGW(METER_TAG, "No calibration found (ns=bl0937 key=%s) -> defaults", key);
        }

        ESP_ERROR_CHECK(bl0937_create(&cfg, &meter));
        ESP_LOGI(METER_TAG, "BL0937 ready (CF=%d CF1=%d SEL=%d)", CF_GPIO, CF1_GPIO, SEL_GPIO);
        return ESP_OK;
}

static void meter_task(void *args) {
        const int sample_ms = 500;
        const int period_ms = 1000;

        while (1) {
                if (meter) {
                        bl0937_reading_t r;
                        esp_err_t err = bl0937_sample_va_w(meter, sample_ms, &r);
                        if (err == ESP_OK) {

#if OC_ENABLED
                                // Overcurrent policy op gemeten stroom (robust).
                                if (relay_on && !oc_latched) {
                                        if (r.current_a > OC_TRIP_CURRENT_A) {
                                                oc_over_count++;
                                                if (oc_over_count >= OC_DEBOUNCE_SAMPLES) {
                                                        ESP_LOGE(OC_TAG,
                                                                 "TRIP! I=%.3fA > %.3fA -> relay OFF (cooldown %dms)",
                                                                 r.current_a, OC_TRIP_CURRENT_A, OC_COOLDOWN_MS);

                                                        oc_latched = true;
                                                        oc_over_count = 0;

                                                        if (OC_COOLDOWN_MS > 0) {
                                                                oc_release_tick =
                                                                        xTaskGetTickCount() + pdMS_TO_TICKS(OC_COOLDOWN_MS);
                                                        } else {
                                                                oc_release_tick = 0;
                                                        }

                                                        // Relay uit + HomeKit notify
                                                        relay_set_state(false, true);

                                                        // korte “trip flash” op rood, daarna terug naar WiFi-status
                                                        red_led_write(true);
                                                        vTaskDelay(pdMS_TO_TICKS(200));
                                                        red_led_write(!wifi_ready);
                                                }
                                        } else {
                                                oc_over_count = 0;
                                        }
                                } else if (oc_latched) {
                                        // tijdens latch geen debounce opbouwen
                                        oc_over_count = 0;
                                }
#endif

                                // Push metingen naar HomeKit
                                hk_update_voltage(r.voltage_v);
                                hk_update_current(r.current_a);
                                hk_update_power(r.power_w);
                                hk_update_energy(r.energy_wh);

                        } else {
                                ESP_LOGW(METER_TAG, "Sample failed: %s", esp_err_to_name(err));
                        }
                }

                vTaskDelay(pdMS_TO_TICKS(period_ms));
        }
}

// ---------- Wi-Fi / HomeKit startup ----------
void on_wifi_ready() {
        static bool homekit_started = false;

        wifi_ready = true;

        // WiFi is nu up -> rode LED uit
        red_led_write(false);

        if (!homekit_started) {
                ESP_LOGI("INFORMATION", "Starting HomeKit server...");
                homekit_server_init(&config);
                homekit_started = true;
        } else {
                ESP_LOGI("INFORMATION", "HomeKit server already running; skipping re-initialization");
        }

        // Start metering task (1x)
        if (meter_task_handle == NULL) {
                xTaskCreate(meter_task, "bl0937_meter", 4096, NULL, 2, &meter_task_handle);
        }
}

// ---------- app_main ----------
void app_main(void) {
        ESP_ERROR_CHECK(lifecycle_nvs_init());
        lifecycle_log_post_reset_state("INFORMATION");
        ESP_ERROR_CHECK(lifecycle_configure_homekit(&revision, &ota_trigger, "INFORMATION"));

        // Init custom HK characteristics (startwaarden)
        custom_characteristics_init();

        gpio_init();

        // BL0937 init
        ESP_ERROR_CHECK(meter_init());

        button_config_t btn_cfg = button_config_default(button_active_low);
        btn_cfg.max_repeat_presses = 3;
        btn_cfg.long_press_time = 10000; // 10 seconds for lifecycle_factory_reset_and_reboot

        int button_create_result = button_create(BUTTON_GPIO, btn_cfg, button_callback, NULL);
        if (button_create_result != 0) {
                ESP_LOGE(BUTTON_TAG, "Failed to initialize button (err=%d)", button_create_result);
        } else {
                ESP_LOGI(BUTTON_TAG,
                         "Button configured: gpio=%d, active_level=%s, long_press=%dms, max_repeats=%d",
                         BUTTON_GPIO,
                         btn_cfg.active_level == button_active_low ? "low" : "high",
                         btn_cfg.long_press_time,
                         btn_cfg.max_repeat_presses);
        }

        esp_err_t wifi_err = wifi_start(on_wifi_ready);
        if (wifi_err == ESP_ERR_NVS_NOT_FOUND) {
                ESP_LOGW("WIFI", "WiFi configuration not found; provisioning required");
                wifi_ready = false;
                // Geen geldige WiFi-config -> rode LED AAN
                red_led_write(true);
        } else if (wifi_err != ESP_OK) {
                ESP_LOGE("WIFI", "Failed to start WiFi: %s", esp_err_to_name(wifi_err));
                wifi_ready = false;
                // Fout bij starten WiFi -> rode LED AAN
                red_led_write(true);
        }
}
