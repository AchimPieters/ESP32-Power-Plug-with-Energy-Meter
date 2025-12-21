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

All GPIOs are configurable in `menuconfig` (StudioPieters menu). Defaults come from `main/Kconfig.projbuild`:

| Purpose | Kconfig option | Default GPIO |
|---------|----------------|--------------|
| Relay output | `CONFIG_ESP_RELAY_GPIO` | 3 |
| Blue LED (relay state) | `CONFIG_ESP_BLUE_LED_GPIO` | 4 |
| Red LED (Wi‑Fi/Lifecycle indicator) | `CONFIG_ESP_RED_LED_GPIO` | 6 |
| Button (active low by default) | `CONFIG_ESP_BUTTON_GPIO` | 7 |

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

After provisioning Wi‑Fi, the outlet appears in HomeKit with the configured setup code (`CONFIG_ESP_SETUP_CODE`) and setup ID (`CONFIG_ESP_SETUP_ID`).

## Notes

- OTA can be triggered from the Home app via the Lifecycle Manager service or requested programmatically; a successful update will reboot automatically.
- Double-press events are logged but intentionally left without an action so they can be repurposed later without side effects.
