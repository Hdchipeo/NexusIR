# NexusIR - Project Documentation

## 1. PROJECT OVERVIEW

### Project Goal
To develop a robust, ESP32-based IoT device capable of controlling Infrared (IR) appliances (like ACs, TVs) and driving WS2812B LED strips, integrated with modern smart home ecosystems (Apple HomeKit and ESP RainMaker).

### Key Features
- **ESP-NOW Modular Architecture**:
  - Distributes logic across Master and Slave nodes.
  - Custom MAC routing per device.
- **Apple HomeKit Bridge**:
  - Single ESP32 acts as a Bridge for AC, Fan, Temp Sensor, and up to 5 LED Lamps.
  - Dynamically hides or shows devices based on `menuconfig` selection.
- **Universal IR Control**:
<<<<<<< HEAD
  - Receiver for learning commands (Custom Brands for Fan, AC, Tivi).
  - Matrix Learning system for rapid command indexing and storage.
  - Transmitter for controlling ACs (Samsung, Daikin, etc.), Fans, and Tivis.
- **Dynamic Device Synchronization**:
  - Automatically bridges learned IR Tivi and Custom buttons into HomeKit/RainMaker.
  - Reboot-to-Sync architecture ensures high stability for bridged accessories.
=======
  - Receiver for learning commands (Custom Brands for Fan and AC).
  - Transmitter for controlling ACs (Samsung, Daikin, etc.) and Fans.
>>>>>>> 23262fa7d5edab1511d7550405a5120c98d1e31d
- **Smart LED Control**:
  - Control up to 5 independent WS2812B RGB or PWM LED strips.
  - Adjustable usage (brightness, color, effects) per lamp.
- **Environmental Sensing**:
  - AHT20 Temperature & Humidity sensor integration via I2C.
- **Local Web Interface & Provisioning**:
  - Local WebUI for configuration and IR learning.
  - iOS Captive Portal for Wi-Fi provisioning.
- **OTA Updates**:
  - Over-the-Air firmware updates for easy maintenance.

### Target Users
- DIY Smart Home enthusiasts.
- Users needing to retro-fit legacy IR appliances into smart ecosystems.

### System Constraints
- **Hardware**: ESP32-C3 / ESP32-S3 (RISC-V/Xtensa).
- **Power**: Requires stable 5V USB power (max current depends on LED count).
- **Network**: 2.4GHz WiFi (802.11 b/g/n).

---

## 2. SYSTEM ARCHITECTURE

### High-Level Architecture
The system utilizes the ESP32 as a Master controller/Bridge or a hardware Slave node via **ESP-NOW**. The Master bridges physical peripherals (Slaves managing IR, LEDs, Sensors) into Apple HomeKit.

### Block Diagram Description
- **Master Node**: ESP32 SoC (WiFi/HAP).
  - Runs Apple HomeKit server.
  - Exposes Bridged Accessories.
  - Sends commands to Slaves via ESP-NOW.
- **Slave Nodes**: ESP32 SoC (ESP-NOW only).
  - Receive commands from Master.
  - Control hardware directly (IR, LEDs, AHT20).
- **Peripherals (on Slaves or Standalone)**:
  - **IR TX/RX**: Connected via GPIOs for signal processing.
  - **LED Lamps (x5)**: Up to 5 independent LED strips.
  - **AHT20**: I2C bus (SDA/SCL).

### Data Flow
1. **User Input** (HomeKit App/Web) -> **Master Node (Protocol Handler)**.
2. **Master** updates state locally and sends payload via **ESP-NOW** to Slave's specific MAC.
3. **Slave Node** receives payload.
4. **Manager Layer** (AC/Fan Logic / LED Mgr) on the Slave maps state to hardware commands.
5. **Driver Layer** executes physical actions (IR send, LED PWM).
6. **Feedback** is sent back to Master (e.g., Temp Sensor data), which updates HomeKit.

---

## 3. HARDWARE DESIGN

### MCU
- **Module**: ESP32-C3-Mini or ESP32-S3-WROOM.
- **Architecture**: RISC-V (C3) or Xtensa (S3).

### Wiring & GPIO Table

| Peripheral | GPIO (ESP32-C3) | Connection | Notes |
| :--- | :--- | :--- | :--- |
| **WS2812B LED** | GPIO 2 | Data In | 5V Power required |
| **User Button** | GPIO 3 | Active Low/High | Configurable |
| **I2C SCL** | GPIO 5 | AHT20 SCL | Optional |
| **I2C SDA** | GPIO 6 | AHT20 SDA | Optional |
| **IR Transmitter** | GPIO 8 | Anode/Transistor | Hardware driver required |
| **IR Receiver** | GPIO 9 | Signal Out | 38kHz Demodulator |

### Hardware Limitations
- **Power**: Ensuring sufficient current for WS2812B is critical. ESP32 GPIOs output 3.3V logic; level shifter (3.3V -> 5V) recommended for LED data line if stability issues occur.
- **Memory**: OTA requires partition table optimization (factory/ota_0/ota_1).

---

## 4. FIRMWARE ARCHITECTURE (ESP-IDF)

### Folder Structure
- `main/`: Application entry point (`app_main.c`), Kconfig (`Kconfig.projbuild`), CMakeLists.
- `components/`: Modularized logic.
  - `drv_*`: Hardware Drivers (AHT20, Button, IR, LED).
  - `svc_*`: System Services (WiFi, Web, OTA, Storage, Logging, ESP-NOW).
<<<<<<< HEAD
  - `mgr_*`: Business Logic (AC/Fan Protocols, IR Managers, Matrix Engine).
  - `int_*`: Integrations (HomeKit, RainMaker).
=======
  - `mgr_*`: Business Logic (AC/Fan Protocols, IR Managers).
  - `int_*`: Integrations (HomeKit).
>>>>>>> 23262fa7d5edab1511d7550405a5120c98d1e31d

### Main Components
- **ESP-NOW Manager** (`svc_espnow`): Routes P2P commands between Master and Slave MAC addresses.
- **HomeKit Bridge** (`int_homekit`): Dynamically populates Thermostats, Fans, and Lightbulbs.
- **WiFi Manager** (`svc_wifi`): Handles station mode, reconnection, and SoftAP.
- **Provisioning** (`svc_wifi_prov`): SoftAP captive portal for credentials.
- **IR Protocols** (`mgr_ir_protocols`): Encoding/Decoding AC and Fan signals using custom brands.

### Event Handling
- Uses `esp_event` loop (default loop).
- HomeKit `write_cb` directly triggers `svc_espnow_bridge_*_send` to relay state.
- Sensor updates broadcasted via ESP-NOW trigger HomeKit characteristic updates on the Master.

---

## 5. WIFI PROVISIONING FLOW

### Boot Flow
1. **Init NVS**.
2. Check for saved **WiFi Credentials**.
3. **If Found**: Connect to WiFi -> Start Application.
4. **If Not Found**: Start **Provisioning Mode** (SoftAP).

### SoftAP Mode
- **SSID**: `NexusIR-Setup-XXXX` (Last 2 bytes of MAC).
- **IP**: `192.168.4.1`.
- **DNS**: Captive Portal redirects all queries to the Web Portal.

### Web Portal Flow
1. User connects to SoftAP.
2. Captive portal opens `index.html`.
3. User scans for networks.
4. User enters Credentials.
5. ESP32 saves to NVS and reboots to connect.

---

## 6. WEB PORTAL (WiFi Config)

### API Endpoints
- `GET /api/wifi/scan`: Returns JSON list of APs.
- `POST /api/wifi/save`: Receives SSID/Password.

### User Flow
1. Connect to `NexusIR-Setup`.
2. Browser popup appears.
3. Select Home WiFi.
4. Enter Password.
5. Click "Connect".

---

## 7. WEBUI (Device Control)

### Features
- Dashboard with current status (AC, LED, Sensor).
- Interactive IR Remote UI.
- LED Color Picker & Effect Selector.
- OTA Firmware Update page.

### Real-Time Updates
- Polls `/api/status` for device state.
- (Future) WebSocket for push events.

---

## 8. LED CONTROL

### LED Architecture
- Capable of managing up to **5 distinct LED Lamps**.
- Controlled via `drv_led` using an array structure (`lamp_id` 0 to 4).
- Supports both WS2812B (RMT Driver) and single-color LEDs (LEDC PWM Driver).

### Supported Features
- Static Color & Brightness mapping from HomeKit.
- Effects: Static, Rainbow, Breathing, Fire.
- State persists in NVS and syncs across ESP-NOW Master and Slaves.

---

## 9. COMMUNICATION PROTOCOL

### Overview
- **HAP (HomeKit Accessory Protocol)**: TCP/IP for Master <-> iOS interaction.
- **ESP-NOW**: P2P Custom payload for Master <-> Slave interaction.

### ESP-NOW Format
**Payload Structure (`espnow_packet_t`)**:
- `type`: Enum (0: AC, 1: LED, 2: FAN, 3: TEMP).
- `union`: Contains specific struct data (e.g., `lamp_id`, `power`, `effect`, `temperature`).

---

## 10. APPLE HOMEKIT PLATFORM

### Overview
- Native **Apple Home** integration.
- Device acts as a **HAP Bridge**.
- **Dynamic Accessories Exposed** (based on `menuconfig`):
  - Thermostat (AC).
  - Fan v2 (Fan).
  - Lightbulb (LED Lamps 1-5).
  - Humidity/Temp Sensor (AHT20).
<<<<<<< HEAD
  - **Tivi Power Switches**: Dynamically created for each learned Tivi.
  - **Custom Buttons**: Dynamically created for each learned custom IR key.
=======
>>>>>>> 23262fa7d5edab1511d7550405a5120c98d1e31d

### Pairing
- Setup Code: `111-22-333`.
- Setup ID: `LP4C`.
- Scan QR code generated in terminal logs.

---

## 12. SECURITY CONSIDERATIONS

- **WiFi**: WPA2/WPA3 support.
- **Provisioning**: PoP (Proof of Possession) recommended for production.
- **OTA**: HTTPS required (`server_cert.pem` embedding recommended).
- **HomeKit**: End-to-end encryption (ChaCha20-Poly1305).

---

## 13. OTA UPDATE STRATEGY

### Flow
1. Admin uploads `nexus-ir.bin` to secure server.
2. User clicks "Update" on WebUI or RainMaker.
3. ESP32 downloads -> Verifies App Image -> Writes to Next OTA Slot.
4. Set Boot Partition -> Reboot.
5. Rollback to previous slot if boot fails.

---

## 14. TESTING STRATEGY

- **Unit Testing**: Test generic logic (IR encoders, JSON parsers).
- **Manual Testing**:
  - **WiFi**: Provisioning, Reconnection, Range.
  - **IR**: Verify against real appliances.
  - **Stability**: Long-run test (24h+) for memory leaks.

---

## 15. FUTURE EXPANSION

- **Matter Support**: Upgrade to Matter for cross-platform local control.
- **AI Integration**: AI-based AC scheduling based on AHT20 readings.
- **Voice Control**: Alexa/Google Home via RainMaker or Matter.

---

## 16. GLOSSARY

- **HAP**: HomeKit Accessory Protocol.
- **NVS**: Non-Volatile Storage.
- **OTA**: Over-The-Air update.
- **SoftAP**: Software Access Point.
- **PoP**: Proof of Possession (Password).
