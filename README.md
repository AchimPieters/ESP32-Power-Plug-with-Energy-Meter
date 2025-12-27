# ESP32 HomeKit Power Plug (Lifecycle Manager)

This project turns an ESP32 into a HomeKit-enabled smart outlet using the **Lifecycle Manager (LCM)** helpers. The firmware drives a relay for power control, keeps two status LEDs in sync with device state, and relies on LCM to simplify Wi‑Fi, OTA, and reset handling.

## Features

- **HomeKit outlet service** with on/off characteristic, OTA trigger, firmware revision, and accessory identification blink sequence.
- **Relay-driven plug control:** the blue LED mirrors the relay output so you always see the current state.
- **Connection feedback:** the red LED is on while the device is provisioning or offline, and it turns off once Wi‑Fi is ready.
- **Button actions:**
  - Single press toggles the relay and notifies HomeKit clients.
  - Long press (10 seconds) performs a full Lifecycle Manager factory reset and reboots.
  - Double press is intentionally unused to avoid accidental triggers.
- **Lifecycle Manager integration:** NVS initialization, reset-state logging, Wi‑Fi startup, OTA request helper, and factory reset are all driven through `esp32-lcm` so only minimal glue code remains in `main.c`.

## Hardware connections

All GPIOs are configurable in `menuconfig` (StudioPieters menu). Defaults come from `main/Kconfig.projbuild` and are tailored per ESP32 family to stay within each package's pinout:

| Target | Relay (`CONFIG_ESP_RELAY_GPIO`) | Blue LED (`CONFIG_ESP_BLUE_LED_GPIO`) | Red LED (`CONFIG_ESP_RED_LED_GPIO`) | Button (`CONFIG_ESP_BUTTON_GPIO`) | BL0937 CF (`CONFIG_ESP_BL0937_CF_GPIO`) | BL0937 CF1 (`CONFIG_ESP_BL0937_CF1_GPIO`) | BL0937 SEL (`CONFIG_ESP_BL0937_SEL_GPIO`) |
|--------|---------------------------------|---------------------------------------|-------------------------------------|-----------------------------------|-------------------------------------------|---------------------------------------------|---------------------------------------------|
| ESP32 | 16 | 2 | 19 | 23 | 34 | 35 | 25 |
| ESP32-S2 | 18 | 17 | 16 | 15 | 6 | 7 | 18 |
| ESP32-S3 | 18 | 17 | 16 | 15 | 6 | 7 | 18 |
| ESP32-C2 | 4 | 3 | 2 | 1 | 8 | 9 | 10 |
| ESP32-C3 | 6 | 20 | 3 | 7 | 4 | 5 | 21 |
| ESP32-C5 | 6 | 7 | 3 | 5 | 8 | 9 | 18 |
| ESP32-C6 | 6 | 7 | 3 | 5 | 8 | 9 | 18 |

The blue LED follows the relay output. The red LED stays on until Wi‑Fi is connected; it also lights up when Wi‑Fi fails to start or provisioning is required.

## Behavior overview

1. **Startup:**
   - LCM initializes NVS, logs the last reset reason, and binds the firmware revision plus OTA trigger to HomeKit.
   - GPIOs are reset; relay and blue LED start off, red LED turns on while Wi‑Fi comes up.
2. **Wi‑Fi ready:** `wifi_start()` invokes `on_wifi_ready()`, clears the red LED, and starts the HomeKit server once.
3. **Button presses:**
   - Single press toggles the relay via `relay_set_state()` and sends a HomeKit notification.
   - Long press (10 seconds) runs `lifecycle_factory_reset_and_reboot()`.
4. **Accessory identify:** when triggered from the Home app, the blue LED blinks a short pattern before returning to the relay-indicated state.

## Building and flashing

Prerequisites are declared in `main/idf_component.yml`:

- ESP-IDF `>= 5.0`
- `achimpieters/esp32-homekit >= 1.3.3`
- `achimpieters/esp32-button >= 1.2.3`

Typical workflow:

```bash
idf.py set-target esp32c3        # or your specific target
idf.py menuconfig              # set GPIOs and HomeKit setup credentials
idf.py build
idf.py flash monitor
```

### Target compatibility

- The firmware is **Wi‑Fi only**. Builds are blocked at compile time when targeting non-Wi‑Fi MCUs (for example ESP32-H2 or ESP32-P4).
- GPIO defaults are provided for ESP32, ESP32-S2/S3, ESP32-C2/C3/C5/C6 families; adjust them in `menuconfig` if your board wiring differs.

After provisioning Wi‑Fi, the outlet appears in HomeKit with the configured setup code (`CONFIG_ESP_SETUP_CODE`) and setup ID (`CONFIG_ESP_SETUP_ID`).

## Notes

- OTA can be triggered from the Home app via the Lifecycle Manager service or requested programmatically; a successful update will reboot automatically.
- Double-press events are logged but intentionally left without an action so they can be repurposed later without side effects.
