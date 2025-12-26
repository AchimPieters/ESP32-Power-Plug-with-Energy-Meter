#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <homekit/characteristics.h>
#include <homekit/homekit.h>

#include "bl0937.h"
#include "custom_characteristics.h"
#include "esp32-lcm.h"
#include <button.h>

#define BUTTON_GPIO CONFIG_ESP_BUTTON_GPIO
#define RELAY_GPIO CONFIG_ESP_RELAY_GPIO
#define BLUE_LED_GPIO CONFIG_ESP_BLUE_LED_GPIO
#define RED_LED_GPIO CONFIG_ESP_RED_LED_GPIO

static const char *RELAY_TAG = "RELAY";
static const char *BUTTON_TAG = "BUTTON";
static const char *IDENT_TAG = "IDENT";
static const char *CFG_TAG = "gpio_cfg";
static const char *ENERGY_TAG = "energy";
static const char *INFO_TAG = "INFORMATION";

static bool relay_on = false;
static bool homekit_started = false;

extern homekit_characteristic_t relay_on_characteristic;
extern homekit_characteristic_t outlet_in_use_characteristic;

static bool is_flash_pin_esp32(int gpio_num) {
  return (gpio_num >= 6 && gpio_num <= 11);
}
static bool is_output_capable_esp32(int gpio_num) {
  return (gpio_num >= 0 && gpio_num <= 33);
}

static void fatal_gpio_config(const char *why) {
  ESP_LOGE(CFG_TAG, "FATAL GPIO CONFIG: %s", why);
  ESP_LOGE(CFG_TAG, "Fix in menuconfig and reflash. Halting (no reboot loop).");
  while (true)
    vTaskDelay(pdMS_TO_TICKS(1000));
}

static void validate_gpio_config(void) {
#ifdef CONFIG_IDF_TARGET_ESP32
  const int gpios[] = {RELAY_GPIO,
                       BLUE_LED_GPIO,
                       RED_LED_GPIO,
                       BUTTON_GPIO,
                       CONFIG_ESP_BL0937_CF_GPIO,
                       CONFIG_ESP_BL0937_CF1_GPIO,
                       CONFIG_ESP_BL0937_SEL_GPIO};
  for (size_t i = 0; i < sizeof(gpios) / sizeof(gpios[0]); i++) {
    if (is_flash_pin_esp32(gpios[i])) {
      fatal_gpio_config(
          "One or more GPIOs are 6..11 (SPI flash pins) on ESP32-WROOM.");
    }
  }
  if (!is_output_capable_esp32(RELAY_GPIO))
    fatal_gpio_config("Relay GPIO must be 0..33 on ESP32.");
  if (!is_output_capable_esp32(BLUE_LED_GPIO))
    fatal_gpio_config("Blue LED GPIO must be 0..33 on ESP32.");
  if (!is_output_capable_esp32(RED_LED_GPIO))
    fatal_gpio_config("Red LED GPIO must be 0..33 on ESP32.");
  if (!is_output_capable_esp32(CONFIG_ESP_BL0937_SEL_GPIO))
    fatal_gpio_config("BL0937 SEL GPIO must be 0..33 on ESP32.");

  if (BUTTON_GPIO >= 34 && BUTTON_GPIO <= 39) {
    ESP_LOGW(CFG_TAG,
             "Button GPIO %d has NO internal pullups (34..39). Use external "
             "pull-up.",
             BUTTON_GPIO);
  }
#endif
}

static inline void relay_write(bool on) {
  gpio_set_level(RELAY_GPIO, on ? 1 : 0);
}
static inline void blue_led_write(bool on) {
  gpio_set_level(BLUE_LED_GPIO, on ? 1 : 0);
}
static inline void red_led_write(bool on) {
  gpio_set_level(RED_LED_GPIO, on ? 1 : 0);
}

static inline void update_characteristics(bool on) {
  relay_on_characteristic.value = HOMEKIT_BOOL(on);
  outlet_in_use_characteristic.value = HOMEKIT_BOOL(on);
}

/**
 * IMPORTANT (bulletproof rule):
 * Do NOT call homekit_characteristic_notify() from here (button task / wifi
 * task / etc). This library can crash during /accessories if notified
 * concurrently.
 */
static void relay_set_state(bool on) {
  if (relay_on == on)
    return;
  relay_on = on;

  relay_write(relay_on);
  blue_led_write(relay_on);

  ESP_LOGI(RELAY_TAG, "Relay state -> %s", relay_on ? "ON" : "OFF");

  // Keep a local snapshot value (HomeKit reads via getter too).
  update_characteristics(relay_on);
}

void gpio_init(void) {
  gpio_reset_pin(RELAY_GPIO);
  gpio_set_direction(RELAY_GPIO, GPIO_MODE_OUTPUT);

  gpio_reset_pin(BLUE_LED_GPIO);
  gpio_set_direction(BLUE_LED_GPIO, GPIO_MODE_OUTPUT);

  gpio_reset_pin(RED_LED_GPIO);
  gpio_set_direction(RED_LED_GPIO, GPIO_MODE_OUTPUT);

  relay_on = false;
  update_characteristics(false);

  relay_write(false);
  blue_led_write(false);

  // No WiFi yet
  red_led_write(true);
}

// ---------- Identify ----------

static void accessory_identify_task(void *args) {
  bool previous_led_state = relay_on;

  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 2; j++) {
      blue_led_write(true);
      vTaskDelay(pdMS_TO_TICKS(100));
      blue_led_write(false);
      vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }

  blue_led_write(previous_led_state);
  vTaskDelete(NULL);
}

void accessory_identify(homekit_value_t _value) {
  ESP_LOGI(IDENT_TAG, "Accessory identify");
  xTaskCreate(accessory_identify_task, "Accessory identify",
              configMINIMAL_STACK_SIZE, NULL, 2, NULL);
}

// ---------- HomeKit characteristics ----------

#define DEVICE_NAME "HomeKit Plug"
#define DEVICE_MANUFACTURER "StudioPieters®"
#define DEVICE_SERIAL "NLCC7DFD193A"
#define DEVICE_MODEL "LS066NL/A"

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, DEVICE_NAME);
homekit_characteristic_t manufacturer =
    HOMEKIT_CHARACTERISTIC_(MANUFACTURER, DEVICE_MANUFACTURER);
homekit_characteristic_t serial =
    HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, DEVICE_SERIAL);
homekit_characteristic_t model = HOMEKIT_CHARACTERISTIC_(MODEL, DEVICE_MODEL);
homekit_characteristic_t revision =
    HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION, LIFECYCLE_DEFAULT_FW_VERSION);
homekit_characteristic_t ota_trigger = API_OTA_TRIGGER;

// Relay getter/setter
static homekit_value_t relay_on_get(void) { return HOMEKIT_BOOL(relay_on); }
static void relay_on_set(homekit_value_t value) {
  if (value.format != homekit_format_bool) {
    ESP_LOGE(RELAY_TAG, "Invalid value format: %d", value.format);
    return;
  }
  // Called by HomeKit thread -> safe
  relay_set_state(value.bool_value);
}

// OUTLET_IN_USE: we report "in use" when relay is on
static homekit_value_t outlet_in_use_get(void) {
  return HOMEKIT_BOOL(relay_on);
}

// Energy getters: HomeKit will call these from its own thread while building
// /accessories or on reads. No background notify needed -> no concurrency
// crash.
static bool bl0937_try_read(bl0937_reading_t *out) {
  esp_err_t err = bl0937_default_get_reading(out);
  if (err == ESP_OK)
    return true;
  if (err != ESP_ERR_INVALID_STATE) {
    ESP_LOGW(ENERGY_TAG, "BL0937 read failed: %s", esp_err_to_name(err));
  }
  return false;
}

static float read_voltage(const bl0937_reading_t *r) { return r->voltage_v; }
static float read_current(const bl0937_reading_t *r) { return r->current_a; }
static float read_power(const bl0937_reading_t *r) { return r->power_w; }
static float read_total_consumption(const bl0937_reading_t *r) {
  return r->energy_wh / 1000.0f;
}

static homekit_value_t
read_energy_value(float (*extractor)(const bl0937_reading_t *)) {
  bl0937_reading_t r;
  if (!bl0937_try_read(&r))
    return HOMEKIT_FLOAT(0.0f);
  return HOMEKIT_FLOAT(extractor(&r));
}

static homekit_value_t voltage_get(void) {
  return read_energy_value(read_voltage);
}
static homekit_value_t current_get(void) {
  return read_energy_value(read_current);
}
static homekit_value_t power_get(void) { return read_energy_value(read_power); }
static homekit_value_t total_consumption_get(void) {
  return read_energy_value(read_total_consumption);
}

// We keep a handle to ON characteristic
homekit_characteristic_t relay_on_characteristic = HOMEKIT_CHARACTERISTIC_(
    ON, false, .getter = relay_on_get, .setter = relay_on_set);

homekit_characteristic_t outlet_in_use_characteristic =
    HOMEKIT_CHARACTERISTIC_(OUTLET_IN_USE, false, .getter = outlet_in_use_get);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverride-init"
homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(
            .id = 1, .category = homekit_accessory_category_outlets,
            .services =
                (homekit_service_t *[]){
                    HOMEKIT_SERVICE(ACCESSORY_INFORMATION,
                                    .characteristics =
                                        (homekit_characteristic_t *[]){
                                            &name, &manufacturer, &serial,
                                            &model, &revision,
                                            HOMEKIT_CHARACTERISTIC(
                                                IDENTIFY, accessory_identify),
                                            NULL}),
                    HOMEKIT_SERVICE(
                        OUTLET, .primary = true,
                        .characteristics =
                            (homekit_characteristic_t *[]){
                                HOMEKIT_CHARACTERISTIC(NAME, "HomeKit Plug"),
                                &relay_on_characteristic,
                                &outlet_in_use_characteristic,
                                &voltage_characteristic,
                                &current_characteristic, &power_characteristic,
                                &total_consumption_characteristic, &ota_trigger,
                                NULL}),
                    NULL}),
    NULL};
#pragma GCC diagnostic pop

homekit_server_config_t config = {
    .accessories = accessories,
    .password = CONFIG_ESP_SETUP_CODE,
    .setupId = CONFIG_ESP_SETUP_ID,
};

// ---------- Button handling ----------

void button_callback(button_event_t event, void *context) {
  switch (event) {
  case button_event_single_press:
    ESP_LOGI(BUTTON_TAG, "Single press -> toggle relay");
    relay_set_state(!relay_on);
    break;
  case button_event_double_press:
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
  // WiFi up
  red_led_write(false);

  if (homekit_started) {
    ESP_LOGI(INFO_TAG, "HomeKit server already running; ignoring duplicate "
                       "Wi-Fi ready callback");
    return;
  }

  ESP_LOGI(INFO_TAG, "Starting HomeKit server...");
  homekit_server_init(&config);
  homekit_started = true;
}

// ---------- app_main ----------

void app_main(void) {
  validate_gpio_config();

  ESP_ERROR_CHECK(lifecycle_nvs_init());
  lifecycle_log_post_reset_state("INFORMATION");
  ESP_ERROR_CHECK(
      lifecycle_configure_homekit(&revision, &ota_trigger, "INFORMATION"));

  gpio_init();

  // Attach energy getters BEFORE starting HomeKit server
  voltage_characteristic.getter = voltage_get;
  current_characteristic.getter = current_get;
  power_characteristic.getter = power_get;
  total_consumption_characteristic.getter = total_consumption_get;

  // Start BL0937 (non-fatal if it fails; see your patched
  // bl0937_start_default())
  bl0937_start_default();

  button_config_t btn_cfg = button_config_default(button_active_low);
  btn_cfg.max_repeat_presses = 3;
  btn_cfg.long_press_time = 10000;

  if (button_create(BUTTON_GPIO, btn_cfg, button_callback, NULL) != 0) {
    ESP_LOGE(BUTTON_TAG, "Failed to initialize button");
  }

  esp_err_t wifi_err = wifi_start(on_wifi_ready);
  if (wifi_err != ESP_OK) {
    ESP_LOGE("WIFI", "wifi_start failed: %s", esp_err_to_name(wifi_err));
    red_led_write(true);
  }
}
