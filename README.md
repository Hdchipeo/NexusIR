# NexusIR — Smart IoT Hub for ESP32

[🇻🇳 Tiếng Việt](README_VI.md)

**NexusIR** is a modular IoT smart hub built on **ESP-IDF** for ESP32 / ESP32-C3 / ESP32-S3. It bridges IR appliances, RGB LED strips, relays, and sensors into **Apple HomeKit** or **ESP RainMaker** — controllable via Siri, Alexa, Google Assistant, or a local Web UI.

<p align="center">
  <img src="hardware/3d_print/white.png" width="280" alt="NexusIR Device">
  <img src="hardware/3d_print/Case.png" width="280" alt="Exploded View">
  <img src="hardware/3d_print/sanpham2.png" width="280" alt="Internal PCB">
</p>

---

## Key Features

| Category | Details |
|----------|---------|
| **Platforms** | Apple HomeKit (iOS) · ESP RainMaker (Android/Alexa/Google) |
| **IR Control** | Air Conditioner + Fan — multi-brand, IR learning via Web UI |
| **LED Lighting** | Up to 5× WS2812B RGB strips — color, brightness, effects |
| **Relay** | 2× relay outputs with touch button inputs |
| **Sensors** | AHT20 temperature & humidity |
| **Networking** | ESP-NOW Master/Slave mesh · Wi-Fi · mDNS · OTA updates |
| **Web UI** | Embedded dashboard — gzip-compressed, served from SPIFFS |

---

## Architecture

Each peripheral can be configured as **Master**, **Slave**, **Standalone**, or **Disabled** via `idf.py menuconfig`:

```
┌─────────────────────────────────────────────────────────┐
│  iOS Home App / RainMaker App / Web UI                  │
└────────────────────────┬────────────────────────────────┘
                         │ Wi-Fi / HAP
               ┌─────────▼──────────┐
               │   Master Node      │
               │  HomeKit + Sync    │
               │  Engine + Web UI   │
               └──┬──────────────┬──┘
        ESP-NOW   │              │   ESP-NOW
           ┌──────▼────┐   ┌────▼───────┐
           │ Slave: AC  │   │ Slave: LED │
           │ IR TX/RX   │   │ WS2812B ×5 │
           └────────────┘   └────────────┘
```

- **Master** — Exposes accessories to HomeKit/RainMaker, forwards commands via ESP-NOW.
- **Slave** — Receives ESP-NOW commands, drives local hardware (IR, LED, Relay).
- **Standalone** — All-in-one on a single chip, no ESP-NOW.
- **Disabled** — Feature excluded from build entirely.

---

## Hardware — Default GPIO Map

> All pins configurable in `menuconfig`. Defaults shown for ESP32-C3.

| Peripheral | GPIO | Type |
|------------|------|------|
| IR Transmitter | 4 | RMT Output |
| IR Receiver | 7 | RMT Input |
| LED Lamp 1–5 | 2, 8, 9, 10, 18 | RMT / PWM |
| I2C (AHT20) | SDA=6, SCL=5 | Open Drain |
| Relay 1 / 2 | 12, 13 | Output |
| Touch Button 1 / 2 | 14, 15 | Input |
| System Button | 3 | Input |

---

## Quick Start

### 1. Build & Flash

```bash
# Source ESP-IDF environment
. ~/esp/esp-idf/export.sh

# Set target chip
idf.py set-target esp32c3    # or esp32, esp32s3

# Configure features
idf.py menuconfig

# Build and flash
idf.py build
idf.py -p /dev/tty.usbserial-XXXX flash monitor
```

### 2. Wi-Fi Provisioning

| Platform | Method |
|----------|--------|
| **iOS** | Connect to `NexusIR-Setup-XXXX` hotspot → captive portal auto-opens → select Wi-Fi → enter password |
| **Android** | ESP RainMaker app → Add Device → scan QR or select BLE/SoftAP → PoP: `12345678` |

### 3. Pair to Smart Home

| Platform | Steps |
|----------|-------|
| **iOS HomeKit** | Home App → "+" → Add Accessory → More Options → NexusIR Bridge → Setup Code: `111-22-333` |
| **RainMaker** | Devices appear automatically after provisioning. Link to Alexa/Google via RainMaker skill. |

---

## Menuconfig Overview

Run `idf.py menuconfig` to access:

| Menu | Key Options |
|------|-------------|
| **Device Configuration** | AC, Fan, Temp Sensor, LED 1–5, Relay — set mode (Master/Slave/Standalone/Disabled), GPIOs, names |
| **ESP-NOW Configuration** | Device name, sync peer MACs, LED color order & max brightness |
| **Mobile Platform** | iOS (HomeKit) or Android (RainMaker) |
| **WiFi Configuration** | Provisioning method (SoftAP/BLE), fallback AP, mDNS hostname |
| **OTA Configuration** | Enable/disable auto-update, server URL, check interval |
| **Weather Configuration** | Enable/disable Open-Meteo weather service |

---

## Pre-Built Firmware

Flash without building — binaries in the [`firmware/`](firmware/) directory:

```
firmware/
├── esp32/
│   ├── android/    # RainMaker build
│   └── ios/        # HomeKit build
└── esp32c3/
    ├── android/
    └── ios/
```

**Flash command (ESP32-C3, iOS example):**

```bash
python -m esptool --chip esp32c3 -b 460800 write_flash \
  0x0     firmware/esp32c3/ios/bootloader.bin \
  0x8000  firmware/esp32c3/ios/partition-table.bin \
  0x15000 firmware/esp32c3/ios/ota_data_initial.bin \
  0x20000 firmware/esp32c3/ios/nexus-ir.bin \
  0x3e0000 firmware/esp32c3/ios/storage.bin
```

> Replace `ios` with `android` for RainMaker builds.

---

## Web UI

Embedded local dashboard for IR learning, LED effects, and device configuration.

- **iOS**: Auto-discovered via mDNS → `http://nexusir-xxxx.local`
- **Android**: Enable "WebUI Config Mode" toggle in RainMaker app first.

**Recompile web assets:**

```bash
./components/svc_web_server/compress_web.sh
idf.py build
```

---

## 3D Enclosure

Dome-shaped case with 360° IR transparency. Print files (`.3mf`) in [`hardware/3d_print/`](hardware/3d_print/).

| Black | White | Orange | Pink |
|:-----:|:-----:|:------:|:----:|
| ![Black](hardware/3d_print/Black.png) | ![White](hardware/3d_print/white.png) | ![Orange](hardware/3d_print/Orange.png) | ![Pink](hardware/3d_print/Pink.png) |

---

## License

MIT License — see [LICENSE](LICENSE) for details.
