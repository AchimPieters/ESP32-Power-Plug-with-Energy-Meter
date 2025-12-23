# ESP32 HomeKit Power Plug + Energy Meter (Lifecycle Manager)

This project turns an ESP32 into a HomeKit-enabled smart outlet with **energy monitoring** using the **Lifecycle Manager (LCM)** helpers. The firmware drives a relay for power control, keeps a single status LED in sync with device state, and reads a BL0937 energy metering IC to report live voltage/current/power metrics. LCM simplifies Wi‑Fi, OTA, and reset handling.

## Features

- **HomeKit outlet service** with on/off characteristic, OTA trigger, firmware revision, and accessory identification blink sequence.
- **Relay-driven plug control:** the blue LED mirrors the relay output so you always see the current state.
- **BL0937 energy metering:** voltage, current, power, power factor, frequency, and total consumption are published as HomeKit custom characteristics.
- **Button actions:**
  - Single press toggles the relay and notifies HomeKit clients.
  - Long press (10 seconds) performs a full Lifecycle Manager factory reset and reboots.
  - Double press is intentionally unused to avoid accidental triggers.
- **Lifecycle Manager integration:** NVS initialization, reset-state logging, Wi‑Fi startup, OTA request helper, and factory reset are all driven through `esp32-lcm` so only minimal glue code remains in `main.c`.

## Hardware connections

All GPIOs are configurable in `menuconfig` (StudioPieters menu). Defaults come from `main/Kconfig.projbuild`:

| Purpose | Kconfig option | Default GPIO |
|---------|----------------|--------------|
| Relay output | `CONFIG_ESP_RELAY_GPIO` | 3 |
| Blue LED (relay state) | `CONFIG_ESP_BLUE_LED_GPIO` | 4 |
| Button (active low by default) | `CONFIG_ESP_BUTTON_GPIO` | 7 |

The blue LED follows the relay output.

Energy metering pins are configured through the BL0937 component menuconfig entries:

- `CONFIG_ESP_BL0937_CF_GPIO`
- `CONFIG_ESP_BL0937_CF1_GPIO`
- `CONFIG_ESP_BL0937_SEL_GPIO`
- `CONFIG_ESP_BL0937_SEL_INVERTED`

Calibration and sampling are also configured there:

- `CONFIG_ESP_BL0937_VOLTAGE_CAL`
- `CONFIG_ESP_BL0937_CURRENT_CAL`
- `CONFIG_ESP_BL0937_POWER_CAL`
- `CONFIG_ESP_BL0937_POWER_MAX`
- `CONFIG_ESP_BL0937_FREQUENCY_HZ`
- `CONFIG_ESP_BL0937_SAMPLE_PERIOD_MS`

## Behavior overview

1. **Startup:**
   - LCM initializes NVS, logs the last reset reason, and binds the firmware revision plus OTA trigger to HomeKit.
   - GPIOs are reset; relay and blue LED start off.
2. **Wi‑Fi ready:** `wifi_start()` invokes `on_wifi_ready()` and starts the HomeKit server once.
3. **Button presses:**
   - Single press toggles the relay via `relay_set_state()` and sends a HomeKit notification.
   - Long press (10 seconds) runs `lifecycle_factory_reset_and_reboot()`.
4. **Accessory identify:** when triggered from the Home app, the blue LED blinks a short pattern before returning to the relay-indicated state.
5. **Energy updates:** BL0937 measurements are sampled and pushed into HomeKit custom characteristics via `custom_characteristics_update()`.

## Building and flashing

Prerequisites are declared in `main/idf_component.yml`:

- ESP-IDF `>= 5.0`
- `achimpieters/esp32-homekit >= 1.3.3`
- `achimpieters/esp32-button >= 1.2.3`
- `achimpieters/esp32-bl0937 >= 2.5.1`

Typical workflow:

```bash
idf.py set-target esp32          # or your specific target
idf.py menuconfig              # set GPIOs and HomeKit setup credentials
idf.py build
idf.py flash monitor
```

After provisioning Wi‑Fi, the outlet appears in HomeKit with the configured setup code (`CONFIG_ESP_SETUP_CODE`) and setup ID (`CONFIG_ESP_SETUP_ID`).

## Notes

- OTA can be triggered from the Home app via the Lifecycle Manager service or requested programmatically; a successful update will reboot automatically.
- Double-press events are logged but intentionally left without an action so they can be repurposed later without side effects.
