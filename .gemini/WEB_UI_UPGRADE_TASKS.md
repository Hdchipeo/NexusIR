# 🚀 WEB UI UPGRADE IMPLEMENTATION TASKS

> **Project:** Lamp-ir-device  
> **Component:** svc_web_server  
> **Timeline:** 8 weeks (4 phases)  
> **Status:** Planning Complete ✅

---

## ✅ PHASE 1: Security & Stability (Weeks 1-2)

### Task 1.1: Authentication System
**Priority:** CRITICAL 🔴  
**Estimated Effort:** 8 hours  

**Backend Changes:**
```c
// File: components/svc_web_server/src/svc_web_auth.c (NEW)
typedef struct {
  char username[32];
  char password_hash[65];  // SHA-256
  uint32_t session_token;
  int64_t expires_at;
} web_session_t;

esp_err_t web_auth_check_credentials(const char *user, const char *pass);
web_session_t* web_auth_create_session(const char *user);
bool web_auth_verify_session(httpd_req_t *req);
void web_auth_middleware(httpd_req_t *req);
```

**Frontend Changes:**
```html
<!-- File: www/login.html (NEW) -->
<form id="login-form">
  <input type="text" id="username" placeholder="Username">
  <input type="password" id="password" placeholder="Password">
  <button type="submit">Login</button>
</form>
```

**Checklist:**
- [ ] Create `svc_web_auth.c` and `svc_web_auth.h`
- [ ] Implement SHA-256 password hashing
- [ ] Add session token generation (crypto random)
- [ ] Create login page UI
- [ ] Add middleware to all protected endpoints
- [ ] Store session in NVS (survive reboot)
- [ ] Add logout functionality
- [ ] Test: Login with correct credentials
- [ ] Test: Reject invalid credentials
- [ ] Test: Session expiry after 1 hour

---

### Task 1.2: Input Validation
**Priority:** HIGH 🟠  
**Estimated Effort:** 6 hours  

**Implementation:**
```c
// File: components/svc_web_server/src/svc_web_validation.c (NEW)
typedef struct {
  const char *field;
  json_type_t type;
  int min_value;
  int max_value;
  size_t max_length;
} validation_rule_t;

esp_err_t validate_json_object(cJSON *obj, validation_rule_t *rules, int rule_count);
esp_err_t validate_string_param(const char *str, size_t max_len);
esp_err_t validate_numeric_range(int value, int min, int max);
```

**Validation Rules:**
```c
// AC Control
validation_rule_t ac_rules[] = {
  {"power", JSON_BOOL, 0, 1, 0},
  {"temp", JSON_NUMBER, 16, 30, 0},
  {"mode", JSON_NUMBER, 0, 4, 0},
  {"fan", JSON_NUMBER, 0, 3, 0}
};

// Brand Name
{"name", JSON_STRING, 0, 0, 31}
```

**Checklist:**
- [ ] Create validation framework
- [ ] Add rules for all API endpoints
- [ ] Return 400 with detailed error message
- [ ] Test: Send invalid JSON
- [ ] Test: Send out-of-range values
- [ ] Test: Send oversized strings
- [ ] Test: Send null/missing fields

---

### Task 1.3: Error Response Standardization
**Priority:** MEDIUM 🟡  
**Estimated Effort:** 4 hours  

**Standard Error Format:**
```json
{
  "error": {
    "code": 400,
    "message": "Invalid temperature value",
    "field": "temp",
    "detail": "Temperature must be between 16 and 30"
  }
}
```

**Implementation:**
```c
typedef enum {
  API_ERR_INVALID_JSON = 1000,
  API_ERR_INVALID_PARAM = 1001,
  API_ERR_OUT_OF_RANGE = 1002,
  API_ERR_NOT_FOUND = 1003,
  API_ERR_ALREADY_EXISTS = 1004,
  API_ERR_MAX_LIMIT = 1005
} api_error_code_t;

esp_err_t send_api_error(httpd_req_t *req, int http_code, api_error_code_t err_code, const char *msg);
```

**Checklist:**
- [ ] Define error code enum
- [ ] Create `send_api_error()` helper
- [ ] Replace all `httpd_resp_send_500()` calls
- [ ] Replace all `httpd_resp_send_404()` calls
- [ ] Update frontend to parse error format
- [ ] Test: Frontend displays error message

---

### Task 1.4: LED Modal Implementation
**Priority:** CRITICAL 🔴  
**Estimated Effort:** 10 hours  

**UI Structure:**
```html
<!-- File: www/index.html - Add modal -->
<div id="led-modal" class="modal hidden">
  <div class="modal-content">
    <h2>LED Ring Control</h2>
    
    <!-- Effect Selector -->
    <select id="led-effect"></select>
    
    <!-- Color Palette (8 slots) -->
    <div class="color-palette">
      <input type="color" class="color-slot" data-index="0">
      <!-- ... 7 more ... -->
    </div>
    
    <!-- Brightness Slider -->
    <input type="range" id="led-brightness" min="0" max="100">
    
    <!-- Speed Slider (for effects) -->
    <input type="range" id="led-speed" min="0" max="100">
    
    <!-- Save Button -->
    <button onclick="saveLedConfig()">Save</button>
  </div>
</div>
```

**JavaScript Logic:**
```javascript
async function openLedModal() {
  // Fetch current config
  const res = await fetch('/api/led/config');
  const config = await res.json();
  
  // Populate UI
  DOM.ledEffect.value = config.effect;
  DOM.ledBrightness.value = config.brightness;
  config.colors.forEach((rgb, idx) => {
    setColorSlot(idx, rgbToHex(rgb));
  });
  
  // Show modal
  DOM.ledModal.classList.remove('hidden');
}

async function saveLedConfig() {
  const payload = {
    effect: DOM.ledEffect.value,
    brightness: parseInt(DOM.ledBrightness.value),
    speed: parseInt(DOM.ledSpeed.value),
    colors: getColorPalette()  // Read all 8 slots
  };
  
  await fetch('/api/led/config', {
    method: 'POST',
    body: JSON.stringify(payload)
  });
  
  closeLedModal();
}
```

**Checklist:**
- [ ] Create modal HTML structure
- [ ] Add modal CSS (overlay, animation)
- [ ] Implement `openLedModal()` with API fetch
- [ ] Add color picker for 8 slots
- [ ] Implement effect dropdown (from backend)
- [ ] Add brightness/speed sliders with % display
- [ ] Implement `saveLedConfig()` POST handler
- [ ] Add "State Colors" configuration tab
- [ ] Test: Open modal, change color, save
- [ ] Test: Change effect, verify speed/color relevance
- [ ] Test: Close modal without saving

---

### Task 1.5: Request Logging
**Priority:** LOW 🟢  
**Estimated Effort:** 2 hours  

**Implementation:**
```c
static void log_request(httpd_req_t *req) {
  char client_ip[16];
  httpd_req_get_client_ip(req, client_ip, sizeof(client_ip));
  
  ESP_LOGI(TAG, "[%s] %s %s", client_ip, 
           http_method_str(req->method), req->uri);
}

// In each handler:
esp_err_t api_ac_control_handler(httpd_req_t *req) {
  log_request(req);
  // ...
}
```

**Checklist:**
- [ ] Create logging helper
- [ ] Add to all API handlers
- [ ] Include timestamp, IP, method, URI
- [ ] Test: View logs via `/api/system/logs`

---

## ✅ PHASE 2: UX Improvements (Weeks 3-4)

### Task 2.1: Loading States & Spinners
**Priority:** MEDIUM 🟡  
**Estimated Effort:** 4 hours  

**CSS Components:**
```css
.loader {
  border: 4px solid var(--bg-tertiary);
  border-top: 4px solid var(--accent);
  border-radius: 50%;
  width: 24px;
  height: 24px;
  animation: spin 1s linear infinite;
}

@keyframes spin {
  0% { transform: rotate(0deg); }
  100% { transform: rotate(360deg); }
}
```

**JavaScript Helpers:**
```javascript
function showLoader(elementId) {
  const el = document.getElementById(elementId);
  el.innerHTML = '<div class="loader"></div>';
  el.disabled = true;
}

function hideLoader(elementId, originalText) {
  const el = document.getElementById(elementId);
  el.innerHTML = originalText;
  el.disabled = false;
}
```

**Checklist:**
- [ ] Create loader CSS
- [ ] Add loading state to AC toggle button
- [ ] Add loading state to temperature buttons
- [ ] Add loading state to learning button
- [ ] Add loading state to brand add/delete
- [ ] Add global page loader for initial load
- [ ] Test: All buttons show spinner during API call

---

### Task 2.2: Toast Notification System
**Priority:** MEDIUM 🟡  
**Estimated Effort:** 5 hours  

**HTML Structure:**
```html
<div id="toast-container"></div>
```

**CSS:**
```css
.toast {
  position: fixed;
  bottom: 100px;
  left: 50%;
  transform: translateX(-50%);
  background: var(--bg-secondary);
  padding: 16px 24px;
  border-radius: 12px;
  box-shadow: 0 10px 30px rgba(0,0,0,0.2);
  animation: slideUp 0.3s ease-out;
}

.toast-success { border-left: 4px solid var(--success); }
.toast-error { border-left: 4px solid var(--danger); }
.toast-info { border-left: 4px solid var(--accent); }

@keyframes slideUp {
  from { opacity: 0; transform: translate(-50%, 20px); }
  to { opacity: 1; transform: translate(-50%, 0); }
}
```

**JavaScript Implementation:**
```javascript
function showToast(message, type = 'info', duration = 3000) {
  const toast = document.createElement('div');
  toast.className = `toast toast-${type}`;
  toast.textContent = message;
  
  const container = document.getElementById('toast-container');
  container.appendChild(toast);
  
  setTimeout(() => {
    toast.style.animation = 'slideDown 0.3s ease-out';
    setTimeout(() => toast.remove(), 300);
  }, duration);
}

// Usage:
showToast('AC turned on', 'success');
showToast('Failed to connect', 'error');
```

**Checklist:**
- [ ] Create toast container
- [ ] Implement toast CSS
- [ ] Create `showToast()` function
- [ ] Add to all API success/error cases
- [ ] Test: Success toast on AC toggle
- [ ] Test: Error toast on network failure
- [ ] Test: Multiple toasts stack correctly

---

### Task 2.3: Form Validation (Frontend)
**Priority:** MEDIUM 🟡  
**Estimated Effort:** 3 hours  

**Validation Functions:**
```javascript
const validators = {
  brandName: (name) => {
    if (!name || name.trim() === '') throw 'Brand name cannot be empty';
    if (name.length > 31) throw 'Brand name too long (max 31 chars)';
    if (!/^[a-zA-Z0-9_-]+$/.test(name)) throw 'Only letters, numbers, - and _ allowed';
  },
  
  temperature: (temp) => {
    if (temp < 16 || temp > 30) throw 'Temperature must be 16-30°C';
  }
};

// Usage:
async function addBrand() {
  const name = prompt('Enter brand name:');
  try {
    validators.brandName(name);
    // Proceed with API call
  } catch (err) {
    showToast(err, 'error');
  }
}
```

**Checklist:**
- [ ] Create validation utilities
- [ ] Add to brand add/rename
- [ ] Add visual feedback (red border on invalid)
- [ ] Test: Empty brand name rejected
- [ ] Test: Special characters rejected
- [ ] Test: Long names truncated

---

### Task 2.4: WebSocket Real-Time Updates
**Priority:** HIGH 🟠  
**Estimated Effort:** 8 hours  

**Backend (ESP-IDF):**
```c
// File: components/svc_web_server/src/svc_web_ws.c (NEW)
static httpd_handle_t ws_server = NULL;
static int ws_client_fds[MAX_WS_CLIENTS] = {0};

static esp_err_t ws_handler(httpd_req_t *req) {
  if (req->method == HTTP_GET) {
    // Upgrade connection
    return ESP_OK;
  }
  
  // Handle incoming WS frame
  httpd_ws_frame_t ws_pkt;
  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;
  
  esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
  // Process message
  
  return ret;
}

void ws_broadcast_state_change(const char *json) {
  for (int i = 0; i < MAX_WS_CLIENTS; i++) {
    if (ws_client_fds[i] > 0) {
      httpd_ws_send_frame_async(ws_server, ws_client_fds[i], json);
    }
  }
}
```

**Frontend:**
```javascript
let ws = null;

function connectWebSocket() {
  ws = new WebSocket('ws://' + window.location.hostname + ':8080/ws');
  
  ws.onopen = () => {
    console.log('WebSocket connected');
    document.getElementById('connection-status').style.background = 'var(--success)';
  };
  
  ws.onmessage = (event) => {
    const data = JSON.parse(event.data);
    
    if (data.type === 'ac_state') {
      state.ac = data.state;
      renderAC();
    } else if (data.type === 'learn_status') {
      updateLearnStatus(data.status);
    }
  };
  
  ws.onclose = () => {
    document.getElementById('connection-status').style.background = 'var(--danger)';
    setTimeout(connectWebSocket, 5000);  // Reconnect
  };
}

// Initialize on load
window.onload = () => {
  connectWebSocket();
  // ...
};
```

**Checklist:**
- [ ] Implement WS handler in backend
- [ ] Register WS endpoint (`/ws`)
- [ ] Create broadcast function
- [ ] Trigger broadcast on AC state change
- [ ] Implement frontend WS client
- [ ] Update UI on WS message received
- [ ] Add reconnection logic
- [ ] Remove polling intervals
- [ ] Test: Change AC via RainMaker, Web UI updates
- [ ] Test: Multiple clients receive updates

---

### Task 2.5: Real Dashboard Telemetry
**Priority:** LOW 🟢  
**Estimated Effort:** 6 hours  

**Chart.js Integration:**
```html
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
```

**JavaScript Implementation:**
```javascript
let memChart = null;
const chartData = {
  labels: [],
  datasets: [{
    label: 'RAM Usage (%)',
    data: [],
    borderColor: 'rgb(75, 192, 192)',
    tension: 0.1
  }, {
    label: 'PSRAM Usage (%)',
    data: [],
    borderColor: 'rgb(255, 99, 132)',
    tension: 0.1
  }]
};

function initChart() {
  const ctx = document.getElementById('memChart').getContext('2d');
  memChart = new Chart(ctx, {
    type: 'line',
    data: chartData,
    options: {
      responsive: true,
      scales: {
        y: { beginAtZero: true, max: 100 }
      }
    }
  });
}

async function updateDashboard() {
  const res = await fetch('/api/system/stats');
  const data = await res.json();
  
  // Update text values
  document.querySelector('.cpu-value').textContent = data.cpu + '%';
  document.querySelector('.heap-value').textContent = (data.free_heap / 1024).toFixed(1) + ' KB';
  document.querySelector('.temp-value').textContent = data.temp.toFixed(1) + '°C';
  
  // Update chart
  const now = new Date().toLocaleTimeString();
  chartData.labels.push(now);
  chartData.datasets[0].data.push(data.ram);
  chartData.datasets[1].data.push(data.psram);
  
  if (chartData.labels.length > 20) {
    chartData.labels.shift();
    chartData.datasets[0].data.shift();
    chartData.datasets[1].data.shift();
  }
  
  memChart.update();
}

// Update every 5 seconds when dashboard visible
setInterval(() => {
  if (!document.getElementById('view-dashboard').classList.contains('hidden')) {
    updateDashboard();
  }
}, 5000);
```

**Checklist:**
- [ ] Add Chart.js library (CDN or local)
- [ ] Create chart initialization
- [ ] Fetch real stats from `/api/system/stats`
- [ ] Update chart with rolling window (last 20 points)
- [ ] Add CPU, RAM, PSRAM, Temp gauges
- [ ] Test: Dashboard shows real-time data
- [ ] Test: Chart updates every 5s

---

## ✅ PHASE 3: Advanced Features (Weeks 5-6)

### Task 3.1: Scene Management System
**Priority:** MEDIUM 🟡  
**Estimated Effort:** 10 hours  

**Data Structure:**
```c
typedef enum {
  SCENE_ACTION_AC,
  SCENE_ACTION_LED,
  SCENE_ACTION_DELAY
} scene_action_type_t;

typedef struct {
  scene_action_type_t type;
  union {
    ir_ac_state_t ac_state;
    led_config_t led_config;
    uint32_t delay_ms;
  };
} scene_action_t;

typedef struct {
  char name[32];
  scene_action_t actions[MAX_SCENE_ACTIONS];
  int action_count;
} scene_t;

// NVS storage
esp_err_t scene_save(const char *name, scene_t *scene);
esp_err_t scene_load(const char *name, scene_t *scene);
esp_err_t scene_list(char **names, int *count);
esp_err_t scene_run(const char *name);
```

**API Endpoints:**
```c
POST /api/scene/create
{
  "name": "Sleep Mode",
  "actions": [
    {"type": "ac", "power": true, "temp": 26, "mode": 1},
    {"type": "led", "effect": "breathing", "brightness": 10}
  ]
}

GET /api/scene/list
["Sleep Mode", "Movie Mode", "Party Mode"]

POST /api/scene/run?name=Sleep%20Mode
```

**UI:**
```html
<!-- New Tab: Scenes -->
<section id="view-scenes" class="tab-view hidden">
  <h1>Scenes</h1>
  <div class="scene-list" id="scene-list"></div>
  <button onclick="createScene()">+ Create Scene</button>
</section>
```

**Checklist:**
- [ ] Define scene data structures
- [ ] Implement scene storage in NVS
- [ ] Create API handlers (create, list, run, delete)
- [ ] Build scene creation UI
- [ ] Add scene execution logic
- [ ] Test: Create "Sleep Mode" scene
- [ ] Test: Run scene, verify AC + LED change
- [ ] Test: List saved scenes
- [ ] Test: Delete scene

---

### Task 3.2: Multi-Language Support (i18n)
**Priority:** LOW 🟢  
**Estimated Effort:** 8 hours  

**Translation Files:**
```javascript
// File: www/lang/en.js
const translations_en = {
  'app.title': 'Goku Smart Home',
  'ac.title': 'Air Conditioner',
  'ac.mode.cool': 'Cool',
  'ac.mode.heat': 'Heat',
  'ac.mode.fan': 'Fan',
  'ac.mode.dry': 'Dry',
  'settings.wifi': 'Wi-Fi Network',
  'settings.firmware': 'Firmware Update',
  // ...
};

// File: www/lang/vi.js
const translations_vi = {
  'app.title': 'Nhà Thông Minh Goku',
  'ac.title': 'Máy Lạnh',
  'ac.mode.cool': 'Làm Lạnh',
  'ac.mode.heat': 'Sưởi Ấm',
  // ...
};
```

**i18n Implementation:**
```javascript
let currentLang = localStorage.getItem('lang') || 'en';
let translations = translations_en;

function setLanguage(lang) {
  currentLang = lang;
  localStorage.setItem('lang', lang);
  translations = window['translations_' + lang];
  updateUI();
}

function t(key) {
  return translations[key] || key;
}

function updateUI() {
  // Update static text
  document.querySelectorAll('[data-i18n]').forEach(el => {
    el.textContent = t(el.dataset.i18n);
  });
  
  // Update placeholders
  document.querySelectorAll('[data-i18n-placeholder]').forEach(el => {
    el.placeholder = t(el.dataset.i18nPlaceholder);
  });
}
```

**HTML Usage:**
```html
<h1 data-i18n="ac.title">Air Conditioner</h1>
<button data-i18n="settings.save">Save</button>
```

**Checklist:**
- [ ] Create English translation file
- [ ] Create Vietnamese translation file
- [ ] Implement i18n framework
- [ ] Add language selector in Settings
- [ ] Mark all UI strings with `data-i18n`
- [ ] Test: Switch language, verify UI updates
- [ ] Test: Language persists on reload

---

### Task 3.3: OTA Progress UI
**Priority:** MEDIUM 🟡  
**Estimated Effort:** 6 hours  

**Backend Enhancement:**
```c
// Add progress tracking
static int ota_progress_percent = 0;

static esp_err_t api_ota_progress_handler(httpd_req_t *req) {
  char resp[64];
  snprintf(resp, sizeof(resp), "{\"progress\":%d, \"status\":\"%s\"}", 
           ota_progress_percent, ota_status_str);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

// In OTA callback (from svc_ota)
void ota_progress_callback(int percent) {
  ota_progress_percent = percent;
  // Broadcast via WebSocket
  ws_broadcast_ota_progress(percent);
}
```

**Frontend:**
```html
<!-- OTA Modal -->
<div id="ota-modal" class="modal hidden">
  <div class="modal-content">
    <h2>Firmware Update</h2>
    <p>Current: <span id="ota-current-ver"></span></p>
    <p>Latest: <span id="ota-latest-ver"></span></p>
    <div class="progress-bar">
      <div class="progress-fill" id="ota-progress"></div>
    </div>
    <p id="ota-status">Ready to update</p>
    <button id="ota-start-btn" onclick="startOTA()">Start Update</button>
  </div>
</div>
```

```javascript
async function startOTA() {
  const btn = document.getElementById('ota-start-btn');
  btn.disabled = true;
  
  await fetch('/api/ota/start', { method: 'POST' });
  
  // Poll progress
  const interval = setInterval(async () => {
    const res = await fetch('/api/ota/progress');
    const data = await res.json();
    
    document.getElementById('ota-progress').style.width = data.progress + '%';
    document.getElementById('ota-status').textContent = data.status;
    
    if (data.progress === 100) {
      clearInterval(interval);
      showToast('Update complete! Rebooting...', 'success');
    }
  }, 1000);
}
```

**Checklist:**
- [ ] Add OTA progress tracking in backend
- [ ] Create progress endpoint
- [ ] Build OTA modal UI
- [ ] Add progress bar animation
- [ ] Implement progress polling
- [ ] Show status messages ("Downloading", "Installing", "Verifying")
- [ ] Test: Start OTA, see progress bar update
- [ ] Test: Handle OTA failure gracefully

---

### Task 3.4: WiFi Manager UI
**Priority:** MEDIUM 🟡  
**Estimated Effort:** 6 hours  

**Enhanced WiFi Scan:**
```javascript
async function scanWiFi() {
  showLoader('wifi-scan-btn');
  const res = await fetch('/api/wifi/scan');
  const networks = await res.json();
  
  const list = document.getElementById('wifi-list');
  list.innerHTML = '';
  
  networks.forEach(net => {
    const item = document.createElement('div');
    item.className = 'list-item';
    item.innerHTML = `
      <span>${net.ssid}</span>
      <div>
        <span class="signal-strength">${getSignalBars(net.rssi)}</span>
        <span class="list-arrow">›</span>
      </div>
    `;
    item.onclick = () => connectToNetwork(net.ssid);
    list.appendChild(item);
  });
  
  hideLoader('wifi-scan-btn');
}

function getSignalBars(rssi) {
  if (rssi > -50) return '▂▃▅▆▇';
  if (rssi > -60) return '▂▃▅▆';
  if (rssi > -70) return '▂▃▅';
  return '▂▃';
}

async function connectToNetwork(ssid) {
  const password = prompt(`Password for ${ssid}:`);
  if (!password) return;
  
  await fetch(`/api/wifi/config?ssid=${encodeURIComponent(ssid)}&password=${encodeURIComponent(password)}`, {
    method: 'POST'
  });
  
  showToast('Connecting to ' + ssid, 'info');
  // Wait for connection status update via WebSocket
}
```

**Checklist:**
- [ ] Add WiFi scan button in Settings
- [ ] Display scanned networks with RSSI bars
- [ ] Add "Connect" flow with password input
- [ ] Show connection status (connecting/connected/failed)
- [ ] Add "Forget Network" option
- [ ] Test: Scan networks, see results
- [ ] Test: Connect to network, verify success
- [ ] Test: Handle incorrect password

---

## ✅ PHASE 4: Optimization (Weeks 7-8)

### Task 4.1: GZIP Compression
**Priority:** MEDIUM 🟡  
**Estimated Effort:** 4 hours  

**Build Script:**
```bash
#!/bin/bash
# File: tools/compress_www.sh

cd components/svc_web_server/www

for file in *.html *.css *.js; do
  if [ -f "$file" ]; then
    gzip -9 -c "$file" > "$file.gz"
    echo "Compressed: $file -> $file.gz"
  fi
done
```

**Backend Support:**
```c
static esp_err_t static_file_handler(httpd_req_t *req) {
  char filepath[280];
  char filepath_gz[284];
  
  // Build paths
  snprintf(filepath, sizeof(filepath), "%s%s", MOUNT_POINT, req->uri);
  snprintf(filepath_gz, sizeof(filepath_gz), "%s.gz", filepath);
  
  FILE *f = fopen(filepath_gz, "r");
  if (f) {
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
  } else {
    f = fopen(filepath, "r");
  }
  
  // ... rest of handler
}
```

**Checklist:**
- [ ] Create compression script
- [ ] Integrate into CMake build
- [ ] Update file handler to prefer .gz
- [ ] Add Content-Encoding header
- [ ] Test: Verify browser receives gzipped files
- [ ] Measure: File size reduction (~70% expected)

---

### Task 4.2: Caching Strategy
**Priority:** LOW 🟢  
**Estimated Effort:** 3 hours  

**Implementation:**
```c
static esp_err_t static_file_handler(httpd_req_t *req) {
  // Check If-None-Match header
  char etag_hdr[32];
  if (httpd_req_get_hdr_value_str(req, "If-None-Match", etag_hdr, sizeof(etag_hdr)) == ESP_OK) {
    if (strcmp(etag_hdr, PROJECT_VERSION) == 0) {
      httpd_resp_set_status(req, "304 Not Modified");
      httpd_resp_send(req, NULL, 0);
      return ESP_OK;
    }
  }
  
  // Set caching headers
  httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=86400");
  httpd_resp_set_hdr(req, "ETag", PROJECT_VERSION);
  
  // ... serve file
}
```

**Checklist:**
- [ ] Add ETag header (version-based)
- [ ] Add Cache-Control header (24h)
- [ ] Implement 304 Not Modified response
- [ ] Test: Second page load uses cache
- [ ] Test: Version change invalidates cache

---

### Task 4.3: Bundle Size Optimization
**Priority:** LOW 🟢  
**Estimated Effort:** 4 hours  

**Minification:**
```bash
# Install terser (JS minifier)
npm install -g terser

# Minify app.js
terser www/app.js -c -m -o www/app.min.js

# Minify CSS (csso)
npm install -g csso-cli
csso www/style.css -o www/style.min.css
```

**HTML Minification:**
```bash
npm install -g html-minifier
html-minifier --collapse-whitespace --remove-comments \
  www/index.html -o www/index.min.html
```

**Update References:**
```html
<link rel="stylesheet" href="style.min.css">
<script src="app.min.js"></script>
```

**Checklist:**
- [ ] Set up minification pipeline
- [ ] Minify JS, CSS, HTML
- [ ] Update file references
- [ ] Test: Verify functionality unchanged
- [ ] Measure: Bundle size reduction target < 25KB

---

### Task 4.4: Lazy Loading
**Priority:** LOW 🟢  
**Estimated Effort:** 3 hours  

**Implementation:**
```javascript
const lazyModules = {
  dashboard: {
    loaded: false,
    load: async () => {
      if (!lazyModules.dashboard.loaded) {
        await loadScript('/chart.min.js');
        initChart();
        lazyModules.dashboard.loaded = true;
      }
    }
  },
  scenes: {
    loaded: false,
    load: async () => {
      await loadScript('/scene-manager.js');
      lazyModules.scenes.loaded = true;
    }
  }
};

async function switchTab(targetId) {
  // ... show tab ...
  
  // Lazy load module
  if (lazyModules[targetId]) {
    await lazyModules[targetId].load();
  }
}

function loadScript(src) {
  return new Promise((resolve, reject) => {
    const script = document.createElement('script');
    script.src = src;
    script.onload = resolve;
    script.onerror = reject;
    document.head.appendChild(script);
  });
}
```

**Checklist:**
- [ ] Split Chart.js to separate file
- [ ] Split scene manager to separate file
- [ ] Implement lazy loader
- [ ] Test: Initial load faster
- [ ] Test: Dashboard loads chart on first open

---

### Task 4.5: Performance Testing
**Priority:** MEDIUM 🟡  
**Estimated Effort:** 4 hours  

**Test Suite:**
```bash
#!/bin/bash
# File: tools/perf_test.sh

# Load test
ab -n 1000 -c 10 http://lamp-ir.local:8080/api/system/stats

# Expected:
# - > 100 req/s
# - 0 failed requests
# - Average response < 30ms

# Page load test
curl -w "@curl-format.txt" -o /dev/null -s http://lamp-ir.local:8080/

# Memory test
curl http://lamp-ir.local:8080/api/system/stats | jq '.free_heap'
# Run multiple times, verify no leak
```

**Metrics to Track:**
| Metric | Target | Current | Status |
|--------|--------|---------|--------|
| Page Load Time | < 300ms | TBD | ⏳ |
| API Response Time | < 30ms | TBD | ⏳ |
| Memory Peak | < 200KB | TBD | ⏳ |
| Bundle Size | < 25KB | ~35KB | ⏳ |

**Checklist:**
- [ ] Set up Apache Bench
- [ ] Run load tests (1000 req)
- [ ] Measure page load time
- [ ] Check for memory leaks
- [ ] Verify no heap fragmentation
- [ ] Document results

---

## 📊 PROGRESS TRACKING

### Overall Status
- [ ] Phase 1: Security & Stability (0/5 tasks)
- [ ] Phase 2: UX Improvements (0/5 tasks)
- [ ] Phase 3: Advanced Features (0/4 tasks)
- [ ] Phase 4: Optimization (0/5 tasks)

**Total: 0/19 tasks complete**

---

## 🎯 DEFINITION OF DONE

Each task is considered complete when:
- [x] Code implemented and tested locally
- [x] Unit tests written (where applicable)
- [x] Integration test passed
- [x] Code reviewed
- [x] Documentation updated
- [x] Committed to feature branch
- [x] Merged to main after testing

---

## 📝 NOTES

### Dependencies
- ESP-IDF v5.x
- Chart.js (for dashboard)
- No other external libraries

### Risks
- WebSocket implementation may use significant RAM
- SPIFFS partition size may need increase for compressed assets
- Authentication may break existing API clients (e.g., mobile app)

### Mitigation
- Monitor heap usage during WS testing
- Increase SPIFFS partition from current size if needed
- Add authentication toggle in Kconfig for backward compatibility

---

**Last Updated:** 2026-01-15  
**Next Review:** After Phase 1 completion
