You are a senior IoT System Architect, Embedded Lead Engineer, and Technical Writer.

Your task is to generate a COMPLETE, PROFESSIONAL PROJECT DOCUMENTATION
for an ESP32-based IoT product.

The documentation must be clear, structured, scalable, and suitable for:
- Embedded developers
- Mobile developers (Android / iOS)
- Web developers
- QA / Tester
- Future maintainers

Write the document in MARKDOWN (.md) format.

--------------------------------------------------
PROJECT CONTEXT
--------------------------------------------------
Project type: ESP32 IoT Device  
Framework: ESP-IDF
Programming language: C/C++  
Connectivity: WiFi  
Target platforms:
- Embedded firmware (ESP32)
- WebUI (served by ESP32)
- Android App
- iOS App

Main features:
- WiFi provisioning (SoftAP + Captive Portal)
- Web Portal for WiFi configuration
- WebUI for device control
- WS2812 LED control
- Device state management
- Communication protocol between device ↔ apps
- OTA update support (if relevant)

--------------------------------------------------
DOCUMENT STRUCTURE REQUIREMENTS
--------------------------------------------------

Generate the documentation with the following sections:

1. PROJECT OVERVIEW
   - Project goal
   - Key features
   - Target users
   - Use cases
   - System constraints (hardware, memory, power, network)

2. SYSTEM ARCHITECTURE
   - High-level architecture overview
   - Block diagram description (textual)
   - Relationship between:
     - ESP32 firmware
     - WebUI
     - Android app
     - iOS app
     - Network (LAN / Internet)
   - Data flow explanation

3. HARDWARE DESIGN
   - MCU: ESP32 (specify variant if needed)
   - Power supply
   - WS2812 LED wiring
   - Buttons / sensors (if any)
   - GPIO usage table
   - Notes about hardware limitations

4. FIRMWARE ARCHITECTURE (ESP-IDF)
   - Folder structure
   - Main components/modules:
     - WiFi Manager
     - Provisioning Service
     - LED Controller
     - Web Server
     - Protocol Handler
     - Storage (NVS)
     - OTA Manager
   - Task design (FreeRTOS tasks)
   - Event handling model
   - State machine overview

5. WIFI PROVISIONING FLOW
   - Boot flow (factory reset vs normal boot)
   - SoftAP mode
   - Captive Portal behavior
   - Web portal flow
   - Credentials storage (NVS)
   - Reconnection logic
   - Error handling

6. WEB PORTAL CONFIG WIFI
   - Captive portal purpose
   - Web pages structure
   - API endpoints
   - User flow
   - Security considerations

7. WEBUI (DEVICE CONTROL)
   - Features
   - UI states
   - APIs used
   - Real-time update strategy
   - Example API request/response (JSON)

8. LED WS2812 CONTROL
   - LED architecture
   - Supported effects
   - Control methods
   - Timing & performance considerations
   - Example LED control flow

9. COMMUNICATION PROTOCOL
   - Protocol type (HTTP / WebSocket / MQTT / BLE – choose & justify)
   - Message format (JSON schema)
   - Command list
   - Device state synchronization
   - Error codes

10. ANDROID PLATFORM
    - Architecture overview
    - Communication with device
    - Pairing / discovery
    - UI flow
    - State management

11. IOS PLATFORM
    - Architecture overview
    - Communication with device
    - Pairing / discovery
    - UI flow
    - State management

12. SECURITY CONSIDERATIONS
    - WiFi security
    - API authentication
    - Local network risks
    - Firmware protection

13. OTA UPDATE STRATEGY
    - OTA flow
    - Server requirements
    - Rollback strategy
    - Failure handling

14. TESTING STRATEGY
    - Unit testing
    - Integration testing
    - Manual testing
    - Edge cases

15. FUTURE EXPANSION
    - Scalability ideas
    - Cloud integration
    - Multi-device support
    - AI / automation ideas

16. GLOSSARY & KEY TERMS

--------------------------------------------------
STYLE REQUIREMENTS
--------------------------------------------------
- Write clearly and professionally
- Use concise explanations
- Prefer bullet points & tables where appropriate
- Think like a real-world commercial IoT product
- Make assumptions explicit
- Be future-proof and scalable

--------------------------------------------------
OUTPUT
--------------------------------------------------
- Output ONLY the markdown document
- Do NOT explain what you are doing
- Do NOT include meta comments