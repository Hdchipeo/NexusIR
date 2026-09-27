# NexusIR ESP-NOW Device Management Protocol

This document describes the architecture and logic for the device management system (Discovery, Add, Remove) based on the **Master-Pull** mechanism for the **NexusIR** ecosystem.

## 1. Architectural Overview
Instead of manual MAC address configuration, the system utilizes a **Master-Initiated Discovery (Pull)** mechanism:
* **Master (Hub)**: Sends a Broadcast packet to search for nearby nodes.
* **Slave (End-device)**: Listens and responds only when a request from the Master is received.
* **Management**: All Peer information (MAC, Name) is persistently stored in the Master's **NVS** (Non-Volatile Storage).

---

## 2. Packet Structure (Data Structure)

To ensure consistency and filter out interference, all ESP-NOW packets must adhere to the following structure:

```c
// Message type definitions
typedef enum {
    NEXUS_SCAN_REQ    = 0x01,  // Master: "Is anyone there?"
    NEXUS_SCAN_RESP   = 0x02,  // Slave: "I'm here + [Information]"
    NEXUS_CONTROL     = 0x03,  // Master: Send control command (IR/LED)
    NEXUS_KEEPALIVE   = 0x04   // Slave: Periodic status report
} nexus_msg_type_t;

// NexusIR Packet Structure
typedef struct {
    uint8_t magic_byte;        // Project Identifier (Default: 0x4E - 'N')
    uint8_t msg_type;          // Message type (nexus_msg_type_t)
    char device_name[16];      // Device name (e.g., "Nexus-Fan-01")
    uint8_t device_type;       // Device type (1: Fan, 2: AC, 3: Light)
    uint8_t payload[200];      // Custom data (IR commands, status, etc.)
} nexus_packet_t;
```

---

## 3. Operational Workflows

### 3.1. Discovery Flow
This process allows the Master to find Slave devices within range without prior knowledge of their MAC addresses.

1.  **Trigger**: User clicks **"Scan"** on the Web UI.
2.  **Broadcast**: Master sends a `NEXUS_SCAN_REQ` packet to the broadcast address `FF:FF:FF:FF:FF:FF`.
3.  **Slave Processing**:
    * Receives the packet and verifies `magic_byte == 0x4E`.
    * Waits for a **Random Delay (0-500ms)** to prevent packet collisions.
    * Sends a `NEXUS_SCAN_RESP` packet (containing MAC and Name) directly to the Master (Unicast).
4.  **Collection**: Master opens a data reception window for 5 seconds, collecting all responses into a temporary array.
5.  **UI Display**: Returns a JSON list containing: MAC, Name, and RSSI to the Web UI.

### 3.2. Add Peer Flow
The process of registering a Slave into the official management list.

1.  **Selection**: User selects a device from the discovered list and clicks **"Add"**.
2.  **API Call**: Web UI sends a `POST /api/espnow/add` request with the MAC Address.
3.  **Registration**: 
    * Master calls `esp_now_add_peer()` to register the protocol.
    * Saves the `{MAC, Name, Type}` information into a table within the **NVS**.
4.  **Confirmation**: Master sends a vibration or LED flash command to the Slave to confirm successful connection.

### 3.3. Disconnect/Unpair Flow

1.  **Action**: User clicks **"Delete"** next to a device in the "My Devices" list.
2.  **Logic**: 
    * Master removes the MAC from the ESP-NOW Peer list using `esp_now_del_peer()`.
    * Deletes the corresponding record from the NVS.
3.  **Update**: Web UI refreshes to reflect the current device list.

---

## 4. State Management and Connectivity (Keep-alive)

Although ESP-NOW is "Connectionless," the following is used to display Online/Offline status on the Web UI:
* **Heartbeat**: Added Slaves will send a `NEXUS_KEEPALIVE` packet every 60 seconds.
* **Timeout**: If the Master does not receive a message from a specific MAC for 3 minutes, the device is marked as **Offline**.
* **RSSI**: Signal bars are updated based on the signal strength of the most recent Heartbeat packet.

---

## 5. Web UI & Backend Integration

### Required API Endpoints
| Method | Endpoint | Description |
| :--- | :--- | :--- |
| `GET` | `/api/espnow/scan` | Triggers broadcast and returns the list of discovered devices. |
| `POST` | `/api/espnow/add` | Adds the MAC to the NVS and Peer List. |
| `GET` | `/api/espnow/list` | Retrieves the list of already paired devices. |
| `DELETE` | `/api/espnow/remove` | Removes a device from the management system. |

### NVS Data Handling
* Data is stored as a **Blob** or using a structured `peer_X` key format (where X is an index) to optimize read/write operations without exhausting NVS memory.

---

> **Technical Note**: In environments with multiple Wi-Fi networks, ensure the Master and all Slaves are operating on the same **Wi-Fi Channel** so that ESP-NOW can discover each other.

Does this English version match the professional tone you were looking for for your project?