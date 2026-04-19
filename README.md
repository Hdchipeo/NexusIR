# Lamp IR Device v1.5.0

**Lamp IR Device** is an Advanced Agentic IoT controller powered by ESP32-C3/S3. It features a fully modular **ESP-NOW Master-Slave Architecture** and acts as an **Apple HomeKit Bridge**, unifying control of Air Conditioners, Fans, Temperature Sensors, and up to 5 independent LED Lamps.

## Documentation

For comprehensive technical documentation, including architecture, hardware design, and API details, please refer to [Project Documentation](docs/PROJECT_DOCS.md) and [Workflow](WORKFLOW.md).

## 🚀 What's New in v1.5.x

*   **ESP-NOW Modular Architecture**: Configure any device (AC, Fan, Temp, LEDs) as Master, Slave, Standalone, or Disabled.
*   **Multi-Lamp Support**: Natively control up to 5 independent LED lamps (WS2812B or PWM) via HomeKit and ESP-NOW.
*   **Per-Device MAC Addressing**: Assign specific ESP-NOW Slave MAC addresses for targeted control of each device.
*   **HomeKit Bridge**: Automatically instantiates Bridged Accessories for enabled devices, displaying custom names defined in `menuconfig`.
*   **Dynamic Hardware Allocation**: Disabled devices bypass hardware initialization, saving RAM and GPIO pins.
*   **Fan IR Protocol Support**: Added robust learning and transmission for custom Fan brands.

---

### 🎮 Smart HomeKit Bridge
*   **Unified Control**: Bridges local ESP-NOW Slaves into a single HomeKit interface.
*   **Universal AC & Fan Engine**: Built-in support for major brands + Custom Learning via RMT.
*   **Local Web Interface**: Access advanced configurations, view IR Dashboards, and manage Wi-Fi via `http://lampac.local`.

### ⚡ ESP-NOW Master/Slave
*   **Scalable**: Master node relays state commands; Slaves translate state to IR pulses.
*   **Modular Config**: Mix and match Masters and Slaves for AC, Fan, and LEDs natively in `menuconfig`.
*   **Targeted Routing**: Custom MAC address definitions per device.

### 🌈 5-Lamp LED Control
*   **Multi-Instance**: Control up to 5 distinct RGB/Single-color LED lamps.
*   **Dynamic Effects**: Rainbow, Breathing, Fire, Sparkle, and more per lamp.

### 🛡 Reliability
*   **Robust OTA**: Dual-partition update system with auto-rollback.
*   **State Persistence**: AC, Fan, and LED settings saved to NVS, instantly restored on reboot.

---

## 🛠 Hardware Configuration

| Component | Default GPIO | Notes |
| :--- | :--- | :--- |
| **IR Transmitter** | Configure in menuconfig | Used by Slaves for AC/Fan control |
| **IR Receiver** | Configure in menuconfig | Used for Learning Mode |
| **RGB LED** | Configure in menuconfig | Supports up to 5 individual pins |
| **AHT20 SDA/SCL** | Configure in menuconfig | I2C Temperature Sensor |

*GPIOs are configurable via `idf.py menuconfig` -> Hardware Configuration.*

---

## 📲 Provisioning Guide

### iOS (HomeKit Bridge + Captive Portal)
1.  Power on device (LED breathes **Cyan**).
2.  Connect to `Lamp-Setup-XXXX` WiFi network.
3.  Captive portal auto-opens for WiFi configuration.
4.  After WiFi connection, add device in Apple Home app using the pairing code `111-22-333` (Setup ID: `LP4C`).
5.  All enabled devices (AC, Fan, Temp, LEDs 1-5) will automatically appear as bridged accessories.

---

## 🌐 Web Interface

The Web UI is **DISABLED by default** to save resources.

**To Enable (Android):**
1.  Open ESP RainMaker App.
2.  Toggle **"WebUI Config Mode"** to ON.
3.  Visit `http://lampac.local` in browser.

**To Enable (iOS):**
*   Web UI is available at `http://lampac.local` when connected to the same network.

---

## 📂 Project Components

| Component | Description |
| :--- | :--- |
| **`components/svc_*`** | Core Services (WiFi, Web, Storage, OTA, Log) |
| **`components/drv_*`** | Hardware Drivers (IR, LED, Button, AHT20) |
| **`components/mgr_*`** | Business Logic (AC, Fan, IR Protocols) |
| **`components/int_*`** | Integrations (HomeKit, RainMaker) |
| **`main`** | Application Entry & Configuration |

---

## 📦 Build & Flash

```bash
# Set target
idf.py set-target esp32c3  # or esp32s3

# Configure (optional - select platform, sensors, GPIOs)
idf.py menuconfig

# Build
idf.py build

# Flash & Monitor
idf.py -p PORT flash monitor
```

**Platform Selection** (in menuconfig):
- `Mobile Platform` -> Android (RainMaker) or iOS (HomeKit)
- `Sensors` -> Enable AHT20 if connected

---

## 📄 License
MIT License.
