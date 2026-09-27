# 📚 WEB SERVER API REFERENCE

> **Quick reference guide cho lập trình viên**  
> **Component:** svc_web_server  
> **Endpoint Base:** `http://lamp-ir.local:8080`

---

## 🔌 API ENDPOINTS

### 🌡️ AC Control APIs

#### POST /api/ac/control
Điều khiển máy lạnh

**Request Body:**
```json
{
  "power": 1,           // 0 = off, 1 = on
  "mode": 1,            // 0=Auto, 1=Cool, 2=Heat, 3=Fan, 4=Dry
  "temp": 24,           // 16-30
  "fan": 2,             // 0=Auto, 1=Low, 2=Med, 3=High
  "brand": "Samsung"    // String = custom brand, Number = preset
}
```

**Response:** `200 OK`

---

#### GET /api/ac/state
Lấy trạng thái AC cơ bản

**Response:**
```json
{
  "power": true,
  "mode": 1,
  "temp": 24,
  "fan": 2,
  "brand": 1
}
```

---

#### GET /api/ac/full_state
Lấy trạng thái AC đầy đủ (bao gồm custom brand info)

**Response:**
```json
{
  "brand": 1,
  "is_custom": true,
  "custom_brand_name": "Samsung",
  "state": {
    "power": true,
    "mode": 1,
    "temp": 24,
    "fan": 2,
    "swing_v": 0,
    "swing_h": 0
  }
}
```

---

### 📡 IR Learning APIs

#### POST /api/learn/start
Bắt đầu học tín hiệu IR

**Response:** `200 "Started"`

---

#### POST /api/learn/stop
Dừng học tín hiệu IR

**Response:** `200 "Stopped"`

---

#### GET /api/learn/status
Kiểm tra trạng thái học tín hiệu

**Response:**
```json
{
  "learning": true,
  "captured": 1234
}
```

---

#### POST /api/save?key={key}
Lưu tín hiệu đã học

**Parameters:**
- `key`: Tên tín hiệu (e.g., `Samsung_temp_24`)

**Response:** `200 "Saved"`

---

### 📤 IR Transmission APIs

#### POST /api/send?key={key}
Gửi tín hiệu IR đã lưu

**Parameters:**
- `key`: Tên tín hiệu

**Response:** `200 "Sent"`

---

#### GET /api/ir/list
Lấy danh sách tín hiệu IR đã lưu

**Response:**
```json
["Samsung_on", "Samsung_off", "Samsung_temp_24", ...]
```

---

#### POST /api/ir/delete?key={key}
Xóa tín hiệu IR

**Parameters:**
- `key`: Tên tín hiệu

**Response:** `200 "Deleted"`

---

#### POST /api/ir/rename?old={old}&new={new}
Đổi tên tín hiệu IR

**Parameters:**
- `old`: Tên cũ
- `new`: Tên mới

**Response:** `200 "Renamed"`

---

### 🏷️ Brand Management APIs

#### GET /api/brand/list
Lấy danh sách custom brands

**Response:**
```json
["Samsung", "LG Custom", "My AC"]
```

---

#### POST /api/brand/add?name={name}
Thêm custom brand

**Parameters:**
- `name`: Tên brand (max 31 chars, alphanumeric + - _)

**Response:** `200 "OK"`

**Errors:**
- `400` - Brand already exists
- `500` - Max brands reached (20)

---

#### POST /api/brand/rename?old={old}&new={new}
Đổi tên custom brand

**Parameters:**
- `old`: Tên cũ
- `new`: Tên mới

**Response:** `200 "OK"`

**Errors:**
- `404` - Brand not found

---

#### POST /api/brand/delete?name={name}
Xóa custom brand

**Parameters:**
- `name`: Tên brand

**Response:** `200 "OK"`

**Errors:**
- `404` - Brand not found

---

### 💡 LED Control APIs

#### GET /api/led/config
Lấy cấu hình LED hiện tại

**Optional Query:**
- `effect`: Lấy config của hiệu ứng cụ thể

**Response:**
```json
{
  "speed": 50,
  "brightness": 80,
  "effect": "rainbow",
  "colors": [
    [255, 0, 0],
    [0, 255, 0],
    [0, 0, 255],
    ...
  ]
}
```

---

#### POST /api/led/config
Cấu hình LED

**Query Parameters:**
- `effect`: `static|rainbow|running|breathing|blink|knight_rider|loading|color_wipe|theater_chase|fire|sparkle|random|auto_cycle`
- `brightness`: 0-100
- `speed`: 0-100
- `index`: Color slot index (0-7, or 255 for all)
- `r`, `g`, `b`: RGB values (0-255)

**Example:**
```
POST /api/led/config?effect=rainbow&brightness=80&speed=50
POST /api/led/config?effect=static&index=0&r=255&g=0&b=0
```

**Response:** `200 "Saved"`

---

#### POST /api/led/save_preset
Lưu cấu hình LED hiện tại vào NVS

**Response:** `200 "Saved"`

---

#### GET /api/led/state-config
Lấy màu sắc cho các trạng thái LED

**Response:**
```json
[
  {"id": 0, "name": "WiFi Prov", "r": 0, "g": 0, "b": 255},
  {"id": 1, "name": "WiFi Conn", "r": 0, "g": 255, "b": 0},
  {"id": 2, "name": "OTA", "r": 255, "g": 165, "b": 0},
  ...
]
```

---

#### POST /api/led/state-config
Cấu hình màu sắc trạng thái LED

**Query Parameters:**
- `id`: State ID (0-6)
- `r`, `g`, `b`: RGB values

**Example:**
```
POST /api/led/state-config?id=0&r=255&g=0&b=0
```

**Response:** `200 "Saved"`

---

### 🔄 OTA APIs

#### GET /api/ota/check
Kiểm tra firmware update

**Response:**
```json
{
  "current": "1.0.0",
  "latest": "1.1.0",
  "available": true,
  "time_synced": true
}
```

---

#### POST /api/ota/start
Bắt đầu OTA update

**Response:** `200 "Started"`

---

### 📶 WiFi APIs

#### GET /api/wifi/scan
Quét mạng WiFi

**Response:**
```json
[
  {"ssid": "MyNetwork", "rssi": -45},
  {"ssid": "NeighborWiFi", "rssi": -72},
  ...
]
```

---

#### POST /api/wifi/config?ssid={ssid}&password={password}
Cấu hình WiFi credentials

**Parameters:**
- `ssid`: Tên mạng (max 32 chars)
- `password`: Mật khẩu (max 64 chars)

**Response:** `200 "Saved"`

---

### 🖥️ System APIs

#### GET /api/system/stats
Lấy thống kê hệ thống

**Response:**
```json
{
  "led_enabled": true,
  "uptime": 3600,
  "free_heap": 142336,
  "min_free_heap": 120000,
  "ram": 35,
  "psram_free": 1900000,
  "psram_total": 2000000,
  "psram": 5,
  "cpu": 0,
  "temp": 45.6,
  "rssi": -54,
  "ssid": "MyNetwork",
  "version": "1.0.0"
}
```

---

#### GET /api/system/logs
Lấy system logs

**Response:** Plain text logs

---

#### POST /api/system/logs/clear
Xóa system logs

**Response:** `200 "Cleared"`

---

## 🛠️ CODE EXAMPLES

### JavaScript (Frontend)

#### Send AC Command
```javascript
async function setACTemp(temp) {
  const payload = {
    power: 1,
    mode: 1,  // Cool
    temp: temp,
    fan: 2,   // Medium
  };
  
  try {
    const res = await fetch('/api/ac/control', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    });
    
    if (res.ok) {
      console.log('AC updated');
    }
  } catch (err) {
    console.error('Failed:', err);
  }
}
```

---

#### Learning Flow
```javascript
// Start learning
async function startLearning() {
  await fetch('/api/learn/start', { method: 'POST' });
  
  // Poll status
  const interval = setInterval(async () => {
    const res = await fetch('/api/learn/status');
    const data = await res.json();
    
    if (data.captured > 0) {
      console.log('Captured:', data.captured);
      
      if (!data.learning) {
        clearInterval(interval);
        // Auto-save
        await fetch('/api/save?key=MyCommand', { method: 'POST' });
      }
    }
  }, 1000);
}
```

---

#### Set LED Color
```javascript
async function setLedColor(r, g, b) {
  await fetch(`/api/led/config?effect=static&index=255&r=${r}&g=${g}&b=${b}`, {
    method: 'POST'
  });
}

// Example: Set red
setLedColor(255, 0, 0);
```

---

### C (Backend)

#### Create New API Handler
```c
static esp_err_t api_my_handler(httpd_req_t *req) {
  // 1. Parse request (if needed)
  char buf[128];
  int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
  if (ret <= 0) {
    return ESP_FAIL;
  }
  buf[ret] = 0;
  
  // 2. Process (call manager/driver)
  // my_logic_function(buf);
  
  // 3. Return response
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "status", "ok");
  
  char *json_str = cJSON_PrintUnformatted(root);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
  
  cJSON_Delete(root);
  free(json_str);
  return ESP_OK;
}

// Register in svc_web_start()
static const httpd_uri_t my_endpoint = {
  .uri = "/api/my/endpoint",
  .method = HTTP_GET,
  .handler = api_my_handler
};
REG_URI(&my_endpoint);
```

---

#### Broadcast State Change (WebSocket)
```c
// When AC state changes
void on_ac_state_changed(ir_ac_state_t *state) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "type", "ac_state");
  
  cJSON *state_obj = cJSON_CreateObject();
  cJSON_AddBoolToObject(state_obj, "power", state->power);
  cJSON_AddNumberToObject(state_obj, "temp", state->temp);
  // ... add other fields
  
  cJSON_AddItemToObject(root, "state", state_obj);
  
  char *json_str = cJSON_PrintUnformatted(root);
  ws_broadcast(json_str);  // Send to all connected clients
  
  free(json_str);
  cJSON_Delete(root);
}
```

---

## 🔒 SECURITY NOTES

### Current Status (v1.0)
- ⚠️ **No authentication** - Anyone on network can access
- ⚠️ **No HTTPS** - Traffic unencrypted
- ⚠️ **No rate limiting** - Vulnerable to DoS

### Planned (v2.0)
- ✅ Basic Auth or session-based login
- ✅ Input validation on all endpoints
- ✅ Rate limiting (max 60 req/min per IP)

### Best Practices (for now)
1. Only use on trusted networks (home WiFi)
2. Don't expose port 8080 to internet
3. Use strong WiFi password
4. Keep firmware updated

---

## ⚡ PERFORMANCE TIPS

### Frontend
```javascript
// ❌ Bad: Polling every 100ms
setInterval(fetchState, 100);

// ✅ Good: WebSocket + smart polling
ws.onmessage = (e) => updateUI(e.data);
```

### Backend
```c
// ❌ Bad: Dynamic allocation in hot path
char *buf = malloc(1024);

// ✅ Good: Stack allocation
char buf[1024];
```

### API Usage
```javascript
// ❌ Bad: Individual requests
await fetch('/api/ac/state');
await fetch('/api/led/config');
await fetch('/api/system/stats');

// ✅ Good: Use full_state endpoint
await fetch('/api/ac/full_state');  // Returns everything
```

---

## 🐛 DEBUGGING

### Enable Debug Logs
```c
// In sdkconfig
CONFIG_LOG_DEFAULT_LEVEL_DEBUG=y

// In code
ESP_LOGD(TAG, "Variable: %d", value);
```

### Check HTTP Server Status
```bash
# Monitor logs
idf.py monitor

# Look for:
# [svc_web_server] Starting HTTP Server...
# [svc_web_server] Registered /api/ac/control
```

### Test API from Command Line
```bash
# GET request
curl http://lamp-ir.local:8080/api/system/stats | jq

# POST request
curl -X POST http://lamp-ir.local:8080/api/ac/control \
  -H "Content-Type: application/json" \
  -d '{"power":1, "temp":24}'
```

---

## 📖 ERROR CODES

| HTTP Code | Meaning | Common Cause |
|-----------|---------|--------------|
| 200 | OK | Success |
| 400 | Bad Request | Invalid JSON, missing params |
| 404 | Not Found | Invalid endpoint or key not found |
| 500 | Internal Error | Server crash, NVS error |

### Example Error Response (Future)
```json
{
  "error": {
    "code": 1002,
    "message": "Temperature out of range",
    "field": "temp",
    "detail": "Must be 16-30"
  }
}
```

---

## 🔗 RELATED COMPONENTS

- **mgr_ac_logic** - AC state management
- **mgr_ir_protocols** - IR encoding/decoding
- **drv_led_strip** - LED control
- **svc_nvs** - Persistent storage
- **svc_wifi** - WiFi management
- **svc_ota** - OTA updates

---

## 📝 CHANGELOG

### v1.0.0 (Current)
- ✅ AC control API
- ✅ IR learning API
- ✅ Custom brand management
- ✅ LED control API (conditional)
- ✅ System stats API
- ✅ OTA check/start

### v2.0.0 (Planned)
- 🔜 Authentication
- 🔜 WebSocket support
- 🔜 Scene management
- 🔜 Multi-language
- 🔜 GZIP compression

---

**Last Updated:** 2026-01-15  
**Maintainer:** Senior Embedded System Architect  
**Support:** Check logs via `/api/system/logs`
