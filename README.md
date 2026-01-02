# ESP32 Power Plug with Energy Meter  
**BL0937 • HomeKit (HAP) • ESP-IDF 5.4+**

This repository contains firmware for a **smart mains power plug** based on **ESP32** and **ESP-IDF**, featuring **real‑time energy monitoring** via the **BL0937** metering IC and **Apple HomeKit (HAP)** integration.

The project is intended for **advanced users** with experience in embedded systems and mains‑powered hardware.

---

## ⚠️ HIGH VOLTAGE WARNING

⚡ **DANGER: MAINS ELECTRICITY CAN KILL**

This project involves a device that connects directly to **230V AC power**.

- Always unplug before opening  
- Never work on the device while live  
- Use insulated tools  
- Avoid touching exposed conductors  
- Proceed only if you understand electrical risks  

The author accepts **no liability** for damage, injury, or accidents.

If you are unsure — **consult a licensed electrician**.

---

## Features

- **Apple HomeKit Outlet**
  - On / Off switching
  - Energy monitoring characteristics
- **Energy measurements**
  - Voltage (V)
  - Current (A)
  - Power (W)
  - Accumulated energy (kWh)
- **BL0937 energy metering IC**
  - Stable fixed‑interval sampling (default: 1s)
  - Glitch filtering & SEL settle timing
  - Software calibration multipliers
- **Status LEDs**
  - Blue / Red indicators
- **Hardware button**
  - Configurable via `menuconfig`
- **ESP Component Manager**
  - Automatic dependency management
- **ESP-IDF 5.4+ compatible**

---

## Supported Hardware

- **ESP32-WROOM-32D**
- **ESP8685-WROOM-03**

> Other ESP32 variants may work but are not officially tested.

---

## Project Structure

```
.
├── main/
│   ├── app_main.c
│   ├── bl0937.c / bl0937.h
│   ├── homekit.c
│   ├── gpio.c
│   ├── idf_component.yml
│   └── ...
├── components/
├── CMakeLists.txt
├── sdkconfig.defaults
└── README.md
```

---

## Build Environment

### Requirements

- ESP-IDF **v5.4 or newer**
- Python 3.9+
- ESP-IDF Component Manager (included with ESP-IDF)

### Setup

```bash
git clone https://github.com/StudioPieters/ESP32-Power-Plug-with-Energy-Meter.git
cd ESP32-Power-Plug-with-Energy-Meter
idf.py set-target esp32
idf.py menuconfig
idf.py build
idf.py flash monitor
```

---

## Configuration (`menuconfig`)

Important configuration options:

- **GPIO assignments**
  - Relay control
  - BL0937 CF / CF1 / SEL pins
  - Status LEDs
  - Button input
- **BL0937 calibration**
  - Voltage multiplier
  - Current multiplier
  - Power multiplier
- **HomeKit settings**
  - Device name
  - Pairing configuration
- **LED & button behavior**

---

## BL0937 Calibration Notes

Calibration is **mandatory** for accurate measurements.

Typical steps:

1. Connect a known resistive load
2. Measure real voltage/current with a trusted meter
3. Adjust calibration multipliers in `menuconfig`
4. Verify readings over time

---

## Safety Notes

This project is designed for **mains-powered hardware (230V / 110V)**.

- Use proper isolation, fusing, and PCB creepage/clearance
- Use certified power supplies and relays
- Prefer testing with:
  - Isolation transformer
  - Variac
  - Protected bench setup

You are **fully responsible** for your hardware implementation.

---

## Disclaimer

This project is provided for **educational and experimental purposes** only.  
Use at your own risk.

---

## License

```c
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

   For more information visit https://www.studiopieters.nl
 **/
```
