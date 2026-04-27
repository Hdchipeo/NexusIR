# Project Workflows

This document explains the internal workflows and logic for the core components of the NexusIR Device.

## 1. Infrared (IR) Workflow

### Learning Mode
1.  **Trigger**: User clicks "Start Learning" on Web UI or holds the physical button (Long Press).
2.  **State Change**: LED changes to **Yellow/Blink**.
3.  **RX Task**: The RMT Receiver channel is enabled and waits for signals.
4.  **Capture**: When an IR signal is detected, the RMT interrupt captures the pulse train into a buffer.
5.  **Save**: User calls "Save" via Web UI, storing it under a custom brand (e.g. `Fan1`).
    *   The captured symbols are serialized and written to **NVS**.
6.  **Idle**: System returns to Idle state.

### Sending Mode (ESP-NOW Master-Slave)
1.  **Trigger**: HomeKit App command (e.g., Turn AC/Fan ON) triggers the Master node.
2.  **Broadcast**: Master node sends the state (power, mode, temp/speed) via ESP-NOW to the specific Slave MAC address configured in `menuconfig`.
3.  **Slave Translation**:
    *   The Slave node receives the ESP-NOW packet.
    *   It looks up the configured `brand` in its local NVS.
    *   If found, the pulse data (symbols) is loaded into RAM.
4.  **Transmission**:
    *   RMT Transmitter channel on the Slave sends the carrier-modulated signal (38kHz) to the IR LED.

---

## 2. Wi-Fi Configuration Workflow

### Provisioning (First Time Setup)
1.  **Boot**: Device checks if Wi-Fi credentials exist in NVS.
2.  **Missing Credentials**:
    *   Device starts **SoftAP Mode** with SSID `NexusIR-Setup-XXXX`.
    *   LED indicates **Provisioning Mode** (Blue Running).
    *   User connects to SoftAP or uses RainMaker App to scan QR code.
3.  **Handshake**:
    *   App sends Wi-Fi SSID/Password via Protocomm (HTTP/BLE).
    *   Device verifies Proof of Possession (PoP).
4.  **Connect**:
    *   Device saves credentials to NVS.
    *   Device reboots or switches to **Station Mode** to connect to the router.

### Connection & Fallback
1.  **Station Mode**: Device attempts to connect to configured Wi-Fi.
    *   **Success**: LED changes to **Green/Static** (or User Color). Connects to RainMaker MQTT.
    *   **Failure**: If connection fails repeatedly (timeout):
        *   Device enables **Fallback AP** (`NexusIR-Recovery`).
        *   LED indicates **Warning/Provisioning**.
        *   Web UI is accessible via the AP to allow re-configuration.

---

## 3. Over-The-Air (OTA) Updates

### Automatic Check (Background)
1.  **Timer**: A background task runs periodically (default: 1 hour).
2.  **Check Version**:
    *   Device fetches `version.txt` from `CONFIG_OTA_SERVER_URL` (e.g., Surge.sh).
    *   Compares remote version with `PROJECT_VERSION`.
3.  **Update**:
    *   If remote > local: Downloads `.bin` firmware.
    *   Writes to the Passive Partition.
    *   Sets Passive as Active and Reboots.
4.  **Rollback**: If new firmware crashes on boot, ESP32 automatically rolls back to the previous partition.

### Manual Trigger (Web UI)
1.  **User Action**: User clicks "Check Update" -> "Update Now" on Web UI.
2.  **API Call**: Front-end checks version API.
3.  **Execution**: Triggers the same internal download/flash logic as the automatic process.

---

## 4. Apple HomeKit Workflow (Bridge Mode)

### Initialization
1.  **HAP Init**: Application initializes the HomeKit Accessory Protocol (HAP) over Wi-Fi.
2.  **Bridge Creation**: A single HomeKit Bridge accessory is created.
3.  **Dynamic Accessories**: Based on `menuconfig`, bridged accessories are instantiated:
    *   **Thermostat**: For AC control.
    *   **Fan v2**: For Fan control.
    *   **Lightbulbs (1-5)**: For each enabled LED Lamp.
    *   **Temperature Sensor**: For AHT20 sensor data.
4.  **State Restoration**: Reads the last known state from NVS and populates HomeKit characteristics *before* starting the HAP server.

### Cloud to Device (Control)
1.  **User**: Toggles "Power On" in Apple Home App.
2.  **HAP Server**: Triggers the `write_cb` (e.g. `led_write`, `fan_write`, `ac_write`).
3.  **Device Logic**:
    *   Code updates the state locally.
    *   If Master, it bridges the command via ESP-NOW to the specific Slave's MAC address.
    *   If Standalone, it handles the hardware directly (e.g. PWM LED or IR send).
    *   Saves new state to NVS.

### Device to Cloud (State Update)
1.  **Event**: ESP-NOW Slave sends sensor data, or Web UI changes a state locally.
2.  **Report**: Device calls `hap_char_update_val()` to notify Apple HomeKit of the change.
3.  **App**: UI immediately reflects the new state.

---

## 5. Web UI Workflow

### Architecture
*   **Server**: ESP32 runs a lightweight HTTP Server (`esp_http_server`).
*   **Front-end**: Single Page Application (embedded HTML/CSS/JS string in `app_web.c`).

### Interaction
1.  **Access**: User visits `http://nexusir-xxxx.local`.
2.  **Load**: Browser downloads the HTML/JS.
3.  **JS Init**: JavaScript `fetch()` calls `/api/ir/list` to get saved keys.
4.  **Commands**:
    *   **Save Key**: `POST /api/save?key=my_key` -> Device enters Learn Mode -> Saves Result.
    *   **Send Key**: `POST /api/send?key=my_key` -> Device transmits IR.
    *   **Wifi Config**: `POST /api/wifi/config` -> Updates NVS -> Restart.
