<<<<<<< HEAD
// Quản lý trạng thái ứng dụng
let state = {
    activeTab: sessionStorage.getItem('activeTab') || 'home',
    devices: [],
    nodes: [],
    acState: { power: false, temp: 24, mode: 1 },
    fanState: { power: false, speed: 3, swing: false },
    currentWizard: null,
    selectedDevice: null
};

// Khởi tạo
window.onload = () => {
    switchTab(state.activeTab);
    fetchDevices();
    fetchWeather();
    setInterval(updateStates, 5000); // Polling trạng thái mỗi 5s
    setInterval(fetchWeather, 60000); // Cập nhật thời tiết mỗi phút
};

function refreshIcons() { lucide.createIcons(); }

// --- Chuyển Tab ---
function switchTab(tabId) {
    document.querySelectorAll('.tab-view').forEach(v => v.classList.remove('active'));
    document.getElementById(`view-${tabId}`).classList.add('active');

    document.querySelectorAll('.nav-item').forEach(i => i.classList.remove('active'));
    document.getElementById(`nav-${tabId}`).classList.add('active');

    state.activeTab = tabId;
    sessionStorage.setItem('activeTab', tabId);

    if (tabId === 'setup') fetchDevices();
    if (tabId === 'settings') fetchNodes();
    refreshIcons();
}

// --- Quản lý thiết bị ---
async function fetchDevices() {
    try {
        const res = await fetch('/api/brand/list');
        if (res.ok) {
            state.devices = await res.json();
            renderHome();
            renderSetup();
        }
    } catch (e) { console.error("Fetch devices failed"); }
}

function renderHome() {
    const container = document.getElementById('home-controls');
    if (!container) return;
    container.innerHTML = '';

    if (state.devices.length === 0) {
        container.innerHTML = `
            <div class="card bento-full" onclick="switchTab('setup')" style="padding:40px; text-align:center;">
                <p style="color:var(--text-secondary)">Chưa có thiết bị. Nhấn để thêm.</p>
            </div>`;
        return;
    }

    state.devices.forEach(dev => {
        const card = document.createElement('div');
        card.className = dev.type === 'AC' ? 'card bento-full' : 'card';
        const icon = dev.type === 'AC' ? 'snowflake' : 
                     dev.type === 'FAN' ? 'wind' : 
                     dev.type === 'TV' ? 'tv' : (dev.icon || 'layers');
        
        card.onclick = () => showControl(dev);
        
        let statusText = 'Sẵn sàng';
        if (dev.type === 'AC' || dev.type === 'FAN') {
            statusText = 'Đang tải...';
        }
        
        card.innerHTML = `
            <div style="display:flex; justify-content:space-between; width: 100%; align-items: center;">
                <div>
                    <div class="card-icon"><i data-lucide="${icon}"></i></div>
                    <div class="card-label">${dev.name}</div>
                    <div class="card-status" id="status-${dev.name}">${statusText}</div>
                </div>
                ${(dev.type === 'AC' || dev.type === 'FAN') ? `<input type="checkbox" class="switch" id="sw-${dev.name}" onclick="event.stopPropagation(); togglePower('${dev.name}', '${dev.type}')">` : ''}
            </div>
        `;
        container.appendChild(card);
    });
    refreshIcons();
    updateStates();
}

function renderSetup() {
    const container = document.getElementById('device-list-container');
    if (!container) return;
    container.innerHTML = '';

    state.devices.forEach(dev => {
        const item = document.createElement('div');
        item.className = 'device-item';
        const icon = dev.type === 'AC' ? 'snowflake' : 
                     dev.type === 'FAN' ? 'wind' : 
                     dev.type === 'TV' ? 'tv' : (dev.icon || 'layers');
        
        item.onclick = () => manageDevice(dev);
        
        item.innerHTML = `
            <div style="background:var(--bg-tertiary); padding:12px; border-radius:12px">
                <i data-lucide="${icon}"></i>
            </div>
            <div class="device-info">
                <div class="device-name">${dev.name}</div>
                <div class="device-type">${dev.type} • Tùy chỉnh</div>
            </div>
            <i data-lucide="chevron-right" style="color:var(--text-secondary)"></i>
        `;
        container.appendChild(item);
    });
    refreshIcons();
}

function manageDevice(dev) {
    state.selectedDevice = dev;
    document.getElementById('manage-title').innerText = dev.name;
    document.getElementById('manage-info').innerText = `Loại: ${dev.type} | Protocol: NexusIR v2`;
    document.getElementById('modal-manage').classList.add('active');
}

async function renameDevicePrompt() {
    const newName = prompt("Nhập tên mới cho thiết bị:", state.selectedDevice.name);
    if (!newName || newName === state.selectedDevice.name) return;
    
    try {
        const res = await fetch(`/api/brand/rename?old=${encodeURIComponent(state.selectedDevice.name)}&new=${encodeURIComponent(newName)}`, { method: 'POST' });
        if (res.ok) {
            closeModal();
            fetchDevices();
        }
    } catch (e) { alert("Lỗi khi đổi tên!"); }
}

async function deleteDeviceConfirm() {
    if (!confirm(`Bạn có chắc muốn xóa thiết bị "${state.selectedDevice.name}"? Dữ liệu hồng ngoại đã học sẽ bị mất.`)) return;
    
    try {
        const res = await fetch(`/api/brand/delete?name=${encodeURIComponent(state.selectedDevice.name)}`, { method: 'POST' });
        if (res.ok) {
            alert("Đã xóa. Vui lòng khởi động lại Hub để cập nhật Apple Home / RainMaker.");
            closeModal();
            fetchDevices();
        }
    } catch (e) { alert("Lỗi khi xóa!"); }
}

// --- Điều khiển ---
async function updateStates() {
    try {
        const resAC = await fetch('/api/ac/state');
        if (resAC.ok) {
            state.acState = await resAC.json();
            const ac = state.devices.find(d => d.type === 'AC');
            if (ac) {
                const statusEl = document.getElementById(`status-${ac.name}`);
                if (statusEl) statusEl.innerText = state.acState.power ? `Bật - ${state.acState.temp}°C` : 'Tắt';
                const swEl = document.getElementById(`sw-${ac.name}`);
                if (swEl) swEl.checked = state.acState.power;
            }
        }
        
        const resFan = await fetch('/api/fan/state');
        if (resFan.ok) {
            state.fanState = await resFan.json();
            const fan = state.devices.find(d => d.type === 'FAN');
            if (fan) {
                const statusEl = document.getElementById(`status-${fan.name}`);
                if (statusEl) statusEl.innerText = state.fanState.power ? `Bật - Tốc độ ${state.fanState.speed}` : 'Tắt';
                const swEl = document.getElementById(`sw-${fan.name}`);
                if (swEl) swEl.checked = state.fanState.power;
            }
        }
    } catch (e) {}
}

async function togglePower(name, type) {
    if (type === 'AC') {
        state.acState.power = !state.acState.power;
        await sendACUpdate();
    } else if (type === 'FAN') {
        state.fanState.power = !state.fanState.power;
        await sendFanUpdate();
    }
}

function showControl(dev) {
    state.currentControlDevice = dev; // Store reference to current device
    if (dev.type === 'AC') {
        document.getElementById('ac-ctrl-title').innerText = dev.name;
        document.getElementById('ac-power').checked = state.acState.power;
        document.getElementById('ac-temp-val').innerText = state.acState.temp;
        updateACModeUI();
        document.getElementById('modal-ac-control').classList.add('active');
    } else if (dev.type === 'FAN') {
        document.getElementById('fan-ctrl-title').innerText = dev.name;
        document.getElementById('fan-power').checked = state.fanState.power;
        document.getElementById('fan-swing').checked = state.fanState.swing;
        updateFanSpeedUI();
        document.getElementById('modal-fan-control').classList.add('active');
    } else if (dev.type === 'TV') {
        document.getElementById('tv-ctrl-title').innerText = dev.name;
        document.getElementById('modal-tv-control').classList.add('active');
    } else if (dev.type === 'CUSTOM') {
        document.getElementById('custom-ctrl-title').innerText = dev.name;
        document.getElementById('modal-custom-control').classList.add('active');
        loadCustomKeys(dev.name);
    }
}

async function sendTVCommand(suffix) {
    if (!state.currentControlDevice) return;
    const devName = state.currentControlDevice.name;
    const safeDevName = devName.replace(/[^a-zA-Z0-9]/g, '').toUpperCase().substring(0, 15);
    const keyName = `T_${safeDevName}_${suffix}`;
    await fetch(`/api/send?key=${encodeURIComponent(keyName)}`, { method: 'POST' });
}

// Custom Device Logic
async function loadCustomKeys(devName) {
    const container = document.getElementById('custom-keys-container');
    container.innerHTML = '<p style="text-align:center; grid-column: span 2;">Đang tải...</p>';
    
    try {
        const res = await fetch('/api/ir/list');
        if (res.ok) {
            const allKeys = await res.json();
            const safeDevName = devName.replace(/[^a-zA-Z0-9]/g, '').toUpperCase().substring(0, 15);
            const prefix = `C_${safeDevName}_`;
            
            const keys = allKeys.filter(k => k.startsWith(prefix));
            
            if (keys.length === 0) {
                container.innerHTML = '<p style="text-align:center; grid-column: span 2; color: var(--text-secondary);">Chưa có nút nào. Bấm dấu + để thêm.</p>';
            } else {
                container.innerHTML = '';
                keys.forEach(k => {
                    const suffix = k.substring(prefix.length);
                    const btn = document.createElement('button');
                    btn.className = 'card';
                    btn.style.padding = '15px';
                    btn.style.textAlign = 'center';
                    btn.style.border = 'none';
                    btn.style.cursor = 'pointer';
                    btn.style.display = 'block';
                    btn.innerText = suffix;
                    
                    // Touch events for long press
                    btn.onpointerdown = () => handleCustomKeyPointerDown(suffix);
                    btn.onpointerup = () => handleCustomKeyPointerUp(suffix);
                    btn.onpointercancel = () => { if(holdTimer) { clearTimeout(holdTimer); holdTimer = null; } };
                    
                    container.appendChild(btn);
                });
            }
        }
    } catch (e) {
        container.innerHTML = '<p style="text-align:center; grid-column: span 2; color: var(--danger);">Lỗi tải nút</p>';
    }
}

let holdTimer = null;
function handleCustomKeyPointerDown(suffix) {
    holdTimer = setTimeout(() => {
        holdTimer = null;
        promptCustomKeyEditDelete(suffix);
    }, 600);
}

function handleCustomKeyPointerUp(suffix) {
    if (holdTimer) {
        clearTimeout(holdTimer);
        holdTimer = null;
        sendCustomCommand(suffix);
    }
}

async function sendCustomCommand(suffix) {
    if (!state.currentControlDevice) return;
    const devName = state.currentControlDevice.name;
    const safeDevName = devName.replace(/[^a-zA-Z0-9]/g, '').toUpperCase().substring(0, 15);
    const keyName = `C_${safeDevName}_${suffix}`;
    
    // Add brief animation
    if (navigator.vibrate) navigator.vibrate(50);
    
    await fetch(`/api/send?key=${encodeURIComponent(keyName)}`, { method: 'POST' });
}

function openAddCustomKeyPrompt() {
    const btnName = prompt('Nhập tên nút mới (VD: Bật đèn, Nút 1):');
    if (!btnName) return;
    
    // Clean name for suffix
    const suffix = btnName.replace(/[^a-zA-Z0-9]/g, '').toUpperCase().substring(0, 8);
    if (!suffix) return alert('Tên nút không hợp lệ!');
    
    // Override wizard logic
    WIZARD_KEYS['CUSTOM'] = [{ label: btnName, keySuffix: suffix, instruct: `Bấm nút ${btnName} trên remote gốc` }];
    
    // Close control modal temporarily and open wizard
    document.getElementById('modal-custom-control').classList.remove('active');
    
    state.currentWizard = 'CUSTOM';
    document.getElementById('dev-name').value = state.currentControlDevice.name;
    currentKeyIndex = 0;
    
    document.getElementById('wiz-type').innerText = 'Thiết bị khác';
    document.getElementById('wizard-step-1').style.display = 'none';
    document.getElementById('wizard-step-2').style.display = 'block';
    
    updateKeyUI();
    document.getElementById('modal-wizard').classList.add('active');
    
    startLearningAPI();
}

function startWizard(type) {
    closeModal();
    state.currentWizard = type;
    document.getElementById('wiz-type').innerText = type === 'AC' ? 'Điều hòa' : (type === 'FAN' ? 'Quạt' : (type === 'TV' ? 'Tivi' : 'Thiết bị khác'));
    
    const iconPicker = document.getElementById('custom-icon-picker');
    if (iconPicker) {
        iconPicker.style.display = type === 'CUSTOM' ? 'block' : 'none';
        if (type === 'CUSTOM') state.selectedCustomIcon = 'layers'; // Reset default
        document.querySelectorAll('.icon-select').forEach(b => {
            b.style.border = b.dataset.icon === 'layers' ? '2px solid var(--accent)' : 'none';
            b.style.opacity = b.dataset.icon === 'layers' ? '1' : '0.5';
        });
    }
    
    document.getElementById('wizard-step-1').style.display = 'block';
    document.getElementById('wizard-step-2').style.display = 'none';
    document.getElementById('modal-wizard').classList.add('active');
}

function selectIcon(iconName) {
    state.selectedCustomIcon = iconName;
    document.querySelectorAll('.icon-select').forEach(btn => {
        if (btn.dataset.icon === iconName) {
            btn.style.border = '2px solid var(--accent)';
            btn.style.opacity = '1';
        } else {
            btn.style.border = 'none';
            btn.style.opacity = '0.5';
        }
    });
}

function promptCustomKeyEditDelete(suffix) {
    if (!state.currentControlDevice) return;
    if (navigator.vibrate) navigator.vibrate(100);
    
    const devName = state.currentControlDevice.name;
    const safeDevName = devName.replace(/[^a-zA-Z0-9]/g, '').toUpperCase().substring(0, 15);
    const oldKey = `C_${safeDevName}_${suffix}`;
    
    const action = confirm(`Bạn muốn làm gì với nút "${suffix}"?\n\n[OK] để Xóa nút này\n[Cancel] để Đổi tên nút`);
    if (action) {
        // Delete
        fetch(`/api/delete?key=${encodeURIComponent(oldKey)}`, { method: 'POST' }).then(() => {
            loadCustomKeys(devName);
        });
    } else {
        // Rename
        const newSuffixRaw = prompt('Nhập tên nút mới:', suffix);
        if (newSuffixRaw && newSuffixRaw !== suffix) {
            const newSuffix = newSuffixRaw.replace(/[^a-zA-Z0-9]/g, '').toUpperCase().substring(0, 8);
            if (newSuffix) {
                const newKey = `C_${safeDevName}_${newSuffix}`;
                fetch(`/api/rename?old=${encodeURIComponent(oldKey)}&new=${encodeURIComponent(newKey)}`, { method: 'POST' }).then(() => {
                    loadCustomKeys(devName);
                });
            }
        }
    }
}

async function controlAC() {
    state.acState.power = document.getElementById('ac-power').checked;
    await sendACUpdate();
}

async function changeTemp(delta) {
    let newTemp = state.acState.temp + delta;
    if (newTemp < 16) newTemp = 16;
    if (newTemp > 30) newTemp = 30;
    state.acState.temp = newTemp;
    document.getElementById('ac-temp-val').innerText = newTemp;
    await sendACUpdate();
}

async function setACMode(mode) {
    state.acState.mode = mode;
    updateACModeUI();
    await sendACUpdate();
}

function updateACModeUI() {
    document.querySelectorAll('.mode-btn').forEach(b => b.style.background = 'var(--bg-secondary)');
    const activeMode = document.getElementById(`ac-mode-${state.acState.mode}`);
    if (activeMode) activeMode.style.background = 'var(--bg-tertiary)';
}

async function sendACUpdate() {
    await fetch('/api/ac/control', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(state.acState)
    });
    updateStates();
}

async function controlFan() {
    state.fanState.power = document.getElementById('fan-power').checked;
    state.fanState.swing = document.getElementById('fan-swing').checked;
    await sendFanUpdate();
}

async function setFanSpeed(speed) {
    state.fanState.speed = speed;
    updateFanSpeedUI();
    await sendFanUpdate();
}

function updateFanSpeedUI() {
    document.querySelectorAll('.speed-btn').forEach(b => b.classList.remove('btn-primary'));
    const activeSpeed = document.getElementById(`fan-speed-${state.fanState.speed}`);
    if (activeSpeed) activeSpeed.classList.add('btn-primary');
}

async function sendFanUpdate() {
    await fetch('/api/fan/control', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(state.fanState)
    });
    updateStates();
}

// --- Quản lý Node & Hệ thống ---
=======
// NexusIR Modern Web UI Implementation
// Mobile-First Logic

// --- Constants & State ---
const DOM = {
    tabs: document.querySelectorAll('.tab-view'),
    navTriggers: document.querySelectorAll('.nav-trigger'),
    debugHidden: document.querySelectorAll('.debug-hidden'),
    acToggle: document.getElementById('ac-toggle'),
    tempDisplay: document.getElementById('target-temp'),
    modeIcon: document.getElementById('ac-mode-icon'),
    modeText: document.getElementById('ac-mode-text'),
    logTerminal: document.getElementById('log-terminal'),
    // New Elements
    acBrandSelect: document.getElementById('ac-brand-select'),
    fanBrandSelect: document.getElementById('fan-brand-select'),
    acCustomBrandsGroup: document.getElementById('ac-custom-brands-group'),
    fanCustomBrandsGroup: document.getElementById('fan-custom-brands-group'),
    btnDelAcBrand: document.getElementById('btn-del-ac-brand'),
    btnDelFanBrand: document.getElementById('btn-del-fan-brand'),
    learnStatus: document.getElementById('learn-status'),
    learnBtn: document.getElementById('btn-learn-toggle'),
    // Fan Elements
    fanToggle: document.getElementById('fan-toggle'),
    fanStatus: document.getElementById('fan-status-text'),
    fanSwing: document.getElementById('fan-swing'),
    // Node Elements
    nodeList: document.getElementById('node-list'),
};

let state = {
    isDebug: false,
    ac: { power: false, temp: 24, mode: 0, fan: 1, brand: 0 },
    debugClickCount: 0,
    debugTimer: null,
    isLearning: false,
    learnInterval: null,
    learnTarget: 'ac', // 'ac' or 'fan'
    customBrands: [],
    fan: { power: false, speed: 1, swing: false, brand: 0 },
    nodes: []
};

const MODES = [
    { id: 0, name: 'Auto', icon: '🤖' },
    { id: 1, name: 'Cool', icon: '❄️' },
    { id: 2, name: 'Heat', icon: '☀️' },
    { id: 3, name: 'Fan', icon: '💨' },
    { id: 4, name: 'Dry', icon: '💧' }
];

// --- Initialization ---
window.onload = () => {
    if (localStorage.getItem('nexus_debug') === 'true') enableDebug(true);
    else enableDebug(false);

    fetchCustomBrands().then(() => {
        fetchACState(); // Fetch state after brands loaded to set select correctly
    });

    setInterval(() => {
        if (state.isDebug && !document.getElementById('view-logs').classList.contains('hidden')) {
            fetchLogs();
        }
        if (!document.getElementById('view-home').classList.contains('hidden')) {
            // Light polling for state sync
        }
    }, 2000);

    fetchFanState();
    fetchNodes();
};

// --- Navigation Logic ---
function switchTab(targetId) {
    DOM.tabs.forEach(el => el.classList.add('hidden'));
    const targetView = document.getElementById(`view-${targetId}`);
    if (targetView) targetView.classList.remove('hidden');
    DOM.navTriggers.forEach(el => {
        if (el.dataset.target === targetId) el.classList.add('active');
        else el.classList.remove('active');
    });
}

// --- Debug Mode Logic ---
function triggerDebug() {
    state.debugClickCount++;
    if (state.debugTimer) clearTimeout(state.debugTimer);
    if (state.debugClickCount >= 5) {
        const newState = !state.isDebug;
        enableDebug(newState);
        alert(newState ? "Engineer Mode Enabled" : "Consumer Mode Enabled");
        state.debugClickCount = 0;
    } else {
        state.debugTimer = setTimeout(() => state.debugClickCount = 0, 500);
    }
}

function enableDebug(enabled) {
    state.isDebug = enabled;
    localStorage.setItem('nexus_debug', enabled);
    DOM.debugHidden.forEach(el => {
        if (enabled) el.classList.remove('hidden');
        else el.classList.add('hidden');
    });
    if (!enabled) switchTab('home');
}

// --- AC Control Logic ---
async function fetchACState() {
    try {
        const res = await fetch('/api/ac/full_state'); // Use full_state
        if (res.ok) {
            const data = await res.json();
            // Data format: { brand: int, is_custom: bool, custom_brand_name: str, state: {...} }

            // Map to local state structure
            state.ac = data.state;

            if (data.is_custom && data.custom_brand_name) {
                state.ac.brand = 'custom_' + data.custom_brand_name;
            } else {
                state.ac.brand = data.brand;
            }

            renderAC();
        }
    } catch (e) {
        console.log("Simulating offline state");
        // fallback
    }
}

function renderAC() {
    DOM.acToggle.checked = state.ac.power;
    DOM.tempDisplay.innerText = state.ac.temp;
    const mode = MODES.find(m => m.id === state.ac.mode) || MODES[0];
    DOM.modeIcon.innerText = mode.icon;
    DOM.modeText.innerText = mode.name;
    document.getElementById('ac-status-text').innerText = state.ac.power ? "Running" : "Off";
    document.getElementById('fan-slider').value = state.ac.fan;

    // Set Brand Selector
    if (DOM.acBrandSelect) {
        DOM.acBrandSelect.value = state.ac.brand;
        checkBrandType('ac', state.ac.brand);
    }
}

function toggleAC() {
    state.ac.power = !state.ac.power;
    renderAC();
    sendACCommand();
}

function adjustTemp(delta) {
    state.ac.temp += delta;
    if (state.ac.temp < 16) state.ac.temp = 16;
    if (state.ac.temp > 30) state.ac.temp = 30;
    DOM.tempDisplay.innerText = state.ac.temp;
    sendACCommand();
}

function cycleMode() {
    state.ac.mode = (state.ac.mode + 1) % MODES.length;
    renderAC();
    sendACCommand();
}

function setFan(val) {
    state.ac.fan = parseInt(val);
    sendACCommand();
}

function changeACBrand(val) {
    state.ac.brand = val;
    checkBrandType('ac', val);
    sendACCommand();
}

function changeFanBrand(val) {
    state.fan.brand = val;
    checkBrandType('fan', val);
    sendFanCommand();
}

function checkBrandType(target, val) {
    // Show Delete button only for custom brands
    const isCustom = (typeof val === 'string' && val.startsWith('custom_'));
    if (target === 'ac') {
        if (DOM.btnDelAcBrand) DOM.btnDelAcBrand.classList.toggle('hidden', !isCustom);
    } else {
        if (DOM.btnDelFanBrand) DOM.btnDelFanBrand.classList.toggle('hidden', !isCustom);
    }
}

async function sendACCommand() {
    let brandToSend = state.ac.brand;
    if (typeof brandToSend === 'string' && brandToSend.startsWith('custom_')) {
        brandToSend = brandToSend.replace('custom_', '');
    } else {
        brandToSend = parseInt(brandToSend);
    }

    const payload = { ...state.ac, brand: brandToSend };

    try {
        await fetch('/api/ac/control', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });
    } catch (e) { console.error("API Error"); }
}

// --- Brand Management ---
async function fetchCustomBrands() {
    if (!DOM.acCustomBrandsGroup || !DOM.fanCustomBrandsGroup) return;
    try {
        const res = await fetch('/api/brand/list');
        const brands = await res.json();
        state.customBrands = brands;

        const populateOptions = (group) => {
            group.innerHTML = '';
            if (brands && brands.length > 0) {
                brands.forEach(brand => {
                    const opt = document.createElement('option');
                    opt.value = 'custom_' + brand;
                    opt.textContent = decodeURIComponent(brand);
                    group.appendChild(opt);
                });
                group.parentElement.disabled = false;
            }
        };

        populateOptions(DOM.acCustomBrandsGroup);
        populateOptions(DOM.fanCustomBrandsGroup);
    } catch (e) { console.error('Error fetching brands'); }
}

async function addBrand() {
    const name = prompt("Enter new Custom Brand name:");
    if (!name) return;
    if (state.customBrands.includes(name)) { alert("Brand already exists"); return; }

    try {
        const res = await fetch(`/api/brand/add?name=${encodeURIComponent(name)}`, { method: 'POST' });
        if (res.ok) {
            await fetchCustomBrands();
            // Automatically select the new brand based on learn target
            const target = state.learnTarget;
            const select = target === 'ac' ? DOM.acBrandSelect : DOM.fanBrandSelect;
            if (select) {
                select.value = 'custom_' + name;
                if (target === 'ac') changeACBrand('custom_' + name);
                else changeFanBrand('custom_' + name);
            }
        } else {
            alert("Failed to add brand");
        }
    } catch (e) { alert("Error adding brand"); }
}

async function deleteBrand(target) {
    const select = target === 'ac' ? DOM.acBrandSelect : DOM.fanBrandSelect;
    const val = select ? select.value : "";
    if (!val.startsWith('custom_')) return;

    const name = val.replace('custom_', '');
    if (!confirm(`Delete custom brand "${name}"? This cannot be undone.`)) return;

    try {
        const res = await fetch(`/api/brand/delete?name=${encodeURIComponent(name)}`, { method: 'POST' });
        if (res.ok) {
            await fetchCustomBrands();
            if (target === 'ac') {
                DOM.acBrandSelect.value = "0";
                changeACBrand("0");
            } else {
                DOM.fanBrandSelect.value = "0";
                changeFanBrand("0");
            }
        } else {
            alert("Failed to delete brand");
        }
    } catch (e) { alert("Error deleting brand"); }
}

// --- Fan Control Logic ---
async function fetchFanState() {
    try {
        const res = await fetch('/api/fan/state');
        if (res.ok) {
            state.fan = await res.json();
            renderFan();
        }
    } catch (e) { console.error("Fan fetch error"); }
}

function renderFan() {
    if (DOM.fanToggle) DOM.fanToggle.checked = state.fan.power;
    if (DOM.fanStatus) DOM.fanStatus.innerText = state.fan.power ? "Running" : "Off";
    if (DOM.fanSwing) DOM.fanSwing.checked = state.fan.swing;
    
    // Update speed buttons
    document.querySelectorAll('.fan-speed-btn').forEach(btn => {
        btn.classList.remove('btn-primary');
        btn.classList.add('btn-secondary');
    });
    const activeBtn = document.getElementById(`fan-speed-${state.fan.speed}`);
    if (activeBtn) {
        activeBtn.classList.remove('btn-secondary');
        activeBtn.classList.add('btn-primary');
    }

    if (DOM.fanBrandSelect) {
        if (state.fan.brand && typeof state.fan.brand === 'string' && state.fan.brand.startsWith('custom_')) {
            DOM.fanBrandSelect.value = state.fan.brand;
        } else {
            DOM.fanBrandSelect.value = state.fan.brand || "0";
        }
        checkBrandType('fan', DOM.fanBrandSelect.value);
    }
}

function toggleFan() {
    state.fan.power = DOM.fanToggle.checked;
    renderFan();
    sendFanCommand();
}

function setFanSpeed(speed) {
    state.fan.speed = speed;
    state.fan.power = true; // Auto turn on when speed set
    renderFan();
    sendFanCommand();
}

function toggleFanSwing() {
    state.fan.swing = DOM.fanSwing.checked;
    sendFanCommand();
}

async function sendFanCommand() {
    let brandToSend = state.fan.brand;
    if (typeof brandToSend === 'string' && brandToSend.startsWith('custom_')) {
        brandToSend = brandToSend.replace('custom_', '');
    } else {
        brandToSend = parseInt(brandToSend || 0);
    }

    const payload = { ...state.fan, brand: brandToSend };

    try {
        await fetch('/api/fan/control', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });
    } catch (e) { console.error("Fan API Error"); }
}

// --- Node Management ---
>>>>>>> 23262fa7d5edab1511d7550405a5120c98d1e31d
async function fetchNodes() {
    try {
        const res = await fetch('/api/nodes');
        if (res.ok) {
            state.nodes = await res.json();
            renderNodes();
        }
<<<<<<< HEAD
    } catch (e) { renderNodes(); }
}

function renderNodes() {
    const list = document.getElementById('node-list');
    if (!list) return;
    list.innerHTML = '';
    state.nodes.forEach(mac => {
        const item = document.createElement('div');
        item.className = 'list-item';
        item.innerHTML = `<span>${mac}</span><button class="btn-danger" style="background:none; border:none" onclick="removeNode('${mac}')"><i data-lucide="trash-2" size="16"></i></button>`;
        list.appendChild(item);
    });
    refreshIcons();
}

async function addNodePrompt() {
    const mac = prompt("Nhập địa chỉ MAC của Slave Node (XX:XX:XX:XX:XX:XX):");
    if (!mac) return;
    if (!/^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$/.test(mac)) {
        alert("Địa chỉ MAC không hợp lệ!");
        return;
    }
    try {
        await fetch('/api/nodes', {
=======
    } catch (e) { console.error("Nodes fetch error"); }
}

function renderNodes() {
    if (!DOM.nodeList) return;
    DOM.nodeList.innerHTML = '';
    if (state.nodes.length === 0) {
        DOM.nodeList.innerHTML = '<div class="list-item"><span class="list-label" style="color:var(--text-secondary); font-size:14px;">No nodes added</span></div>';
        return;
    }
    state.nodes.forEach(mac => {
        const item = document.createElement('div');
        item.className = 'list-item';
        item.innerHTML = `
            <span class="list-label">${mac}</span>
            <button class="btn-secondary" onclick="removeNode('${mac}')" style="padding:4px 10px; font-size:12px; border-radius:8px; color:var(--danger); border:none;">Remove</button>
        `;
        DOM.nodeList.appendChild(item);
    });
}

async function addNodePrompt() {
    const mac = prompt("Enter Slave Node MAC (XX:XX:XX:XX:XX:XX):");
    if (!mac) return;
    if (!/^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$/.test(mac)) {
        alert("Invalid MAC format");
        return;
    }
    try {
        const res = await fetch('/api/nodes', {
>>>>>>> 23262fa7d5edab1511d7550405a5120c98d1e31d
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ mac: mac.toLowerCase() })
        });
<<<<<<< HEAD
        fetchNodes();
    } catch (e) { fetchNodes(); }
}

async function removeNode(mac) {
    if (!confirm(`Xóa node ${mac}?`)) return;
    try {
        await fetch('/api/nodes', {
=======
        if (res.ok) fetchNodes();
        else alert("Failed to add node");
    } catch (e) { alert("Error adding node"); }
}

async function removeNode(mac) {
    if (!confirm(`Remove node ${mac}?`)) return;
    try {
        const res = await fetch('/api/nodes', {
>>>>>>> 23262fa7d5edab1511d7550405a5120c98d1e31d
            method: 'DELETE',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ mac: mac })
        });
<<<<<<< HEAD
        fetchNodes();
    } catch (e) { fetchNodes(); }
}

async function restartHub() {
    if (confirm("Bạn có chắc muốn khởi động lại Hub?")) {
        await fetch('/api/system/restart', { method: 'POST' });
        alert("Hub đang khởi động lại...");
        location.reload();
    }
}

async function fetchWeather() {
    try {
        const res = await fetch('/api/weather');
        if (res.ok) {
            const data = await res.json();
            document.getElementById('weather-text').innerText = `${data.temp.toFixed(1)}°C • ${data.condition}`;
        }
    } catch (e) { document.getElementById('weather-text').innerText = "Sunny - 25°C"; }
}

// --- Wizard ---
function openAddModal() { document.getElementById('modal-add').classList.add('active'); }
function closeModal() { document.querySelectorAll('.modal-overlay').forEach(m => m.classList.remove('active')); }

const WIZARD_KEYS = {
    'AC': [{ label: "Tắt Nguồn", keySuffix: "OFF", instruct: "Bấm nút TẮT nguồn trên remote" }],
    'TV': [
        { label: "Nguồn", keySuffix: "PWR", instruct: "Bấm nút Nguồn" },
        { label: "Âm lượng +", keySuffix: "VOL_UP", instruct: "Bấm nút Tăng âm lượng" },
        { label: "Âm lượng -", keySuffix: "VOL_DOWN", instruct: "Bấm nút Giảm âm lượng" },
        { label: "Kênh +", keySuffix: "CH_UP", instruct: "Bấm nút Kênh tiếp theo" },
        { label: "Kênh -", keySuffix: "CH_DOWN", instruct: "Bấm nút Kênh lùi lại" },
        { label: "Tắt tiếng", keySuffix: "MUTE", instruct: "Bấm nút Mute (Tắt tiếng)" }
    ],
    'FAN': [
        { label: "Nguồn", keySuffix: "PWR", instruct: "Bấm nút Nguồn" },
        { label: "Tốc độ 1", keySuffix: "S1", instruct: "Bấm nút Tốc độ chậm (1)" },
        { label: "Tốc độ 2", keySuffix: "S2", instruct: "Bấm nút Tốc độ trung bình (2)" },
        { label: "Tốc độ 3", keySuffix: "S3", instruct: "Bấm nút Tốc độ nhanh (3)" },
        { label: "Đảo gió", keySuffix: "SWG", instruct: "Bấm nút Tuốc năng / Đảo gió" }
    ],
    'CUSTOM': [] // CUSTOM is now dynamic, but for wizard we just create the device. Wait, wizard needs at least 1 step to create the device!
};

// Add a default button for custom to initialize it
WIZARD_KEYS['CUSTOM'] = [{ label: "Nút 1", keySuffix: "BTN1", instruct: "Bấm phím chức năng bất kỳ để hoàn thành thêm thiết bị" }];

for(let t = 16; t <= 30; t++) {
    WIZARD_KEYS['AC'].push({ 
        label: `Làm mát ${t}°C`, 
        instruct: t === 16 ? "Chỉnh remote về COOL, 16°C. Sau đó hướng vào Hub và bấm nút Gửi." : `Bấm nút Tăng nhiệt độ (+) lên ${t}°C`,
        isMatrix: true
    });
}

let currentKeyIndex = 0;
let learningPollInterval;



function proceedToLearn() {
    if (!document.getElementById('dev-name').value) { alert("Nhập tên thiết bị!"); return; }
    document.getElementById('wizard-step-1').style.display = 'none';
    document.getElementById('wizard-step-2').style.display = 'block';
    updateKeyUI();
    startLearningAPI();
}

function updateKeyUI() {
    const currentObj = WIZARD_KEYS[state.currentWizard][currentKeyIndex];
    document.getElementById('current-key').innerText = currentObj.label;
    document.getElementById('key-instruction').innerText = currentObj.instruct;
    
    if(currentObj.isMatrix) {
        document.getElementById('ac-matrix-ui').style.display = 'block';
        document.getElementById('btn-skip').style.display = 'none';
        let matrixSteps = WIZARD_KEYS[state.currentWizard].filter(k => k.isMatrix).length;
        let currentMatrixIndex = WIZARD_KEYS[state.currentWizard].slice(0, currentKeyIndex + 1).filter(k => k.isMatrix).length;
        document.getElementById('learning-progress').style.width = `${(currentMatrixIndex / matrixSteps) * 100}%`;
        document.getElementById('matrix-progress-text').innerText = `${currentMatrixIndex}/${matrixSteps}`;
    } else {
        document.getElementById('ac-matrix-ui').style.display = 'none';
        document.getElementById('btn-skip').style.display = 'inline-flex';
    }
}

async function startLearningAPI() {
    try {
        await fetch('/api/learn/start', { method: 'POST' });
        if (learningPollInterval) clearInterval(learningPollInterval);
        learningPollInterval = setInterval(async () => {
            const res = await fetch('/api/learn/status');
            const data = await res.json();
            if (!data.learning && data.captured > 0) {
                clearInterval(learningPollInterval);
                onKeyCaptured();
            }
        }, 1000);
    } catch (e) {
        console.warn("Learning API failed");
    }
}

async function onKeyCaptured() {
    const devName = document.getElementById('dev-name').value;
    try {
        if (state.currentWizard === 'AC') {
            await fetch(`/api/matrix/save?dev_id=${encodeURIComponent(devName)}&index=${currentKeyIndex}`, { method: 'POST' });
        } else {
            const currentObj = WIZARD_KEYS[state.currentWizard][currentKeyIndex];
            const suffix = currentObj.keySuffix || currentObj.label;
            const safeDevName = devName.replace(/[^a-zA-Z0-9]/g, '').toUpperCase().substring(0, 15);
            const keyName = `${state.currentWizard.charAt(0)}_${safeDevName}_${suffix}`;
            await fetch(`/api/save?key=${encodeURIComponent(keyName)}`, { method: 'POST' });
        }
    } catch (e) {}

    currentKeyIndex++;
    if (currentKeyIndex < WIZARD_KEYS[state.currentWizard].length) {
        updateKeyUI();
        startLearningAPI();
    } else {
        let apiUrl = `/api/brand/add?name=${encodeURIComponent(devName)}&type=${state.currentWizard}`;
        if (state.currentWizard === 'CUSTOM' && state.selectedCustomIcon) {
            apiUrl += `&icon=${state.selectedCustomIcon}`;
        }
        await fetch(apiUrl, { method: 'POST' });
        alert("Thành công! Bạn cần 'Khởi động lại Hub' (trong tab Cài đặt) để đồng bộ thiết bị mới sang Apple Home / RainMaker.");
        stopWizard();
        fetchDevices();
    }
}

function skipKey() {
    currentKeyIndex++;
    if (currentKeyIndex < WIZARD_KEYS[state.currentWizard].length) {
        updateKeyUI();
        startLearningAPI();
    } else {
        stopWizard();
    }
}

function stopWizard() { 
    state.currentWizard = null; 
    closeModal(); 
    fetch('/api/learn/stop', { method: 'POST' }).catch(() => {});
}
=======
        if (res.ok) fetchNodes();
    } catch (e) { alert("Error removing node"); }
}

// --- Learning Logic ---
function setLearnTarget(target) {
    state.learnTarget = target;
    document.getElementById('btn-learn-ac').classList.toggle('active', target === 'ac');
    document.getElementById('btn-learn-fan').classList.toggle('active', target === 'fan');
    
    // Update cmd suffix choices based on target
    const select = document.getElementById('learn-cmd-suffix');
    select.innerHTML = '';
    if (target === 'ac') {
        select.innerHTML = `
            <option value="ON">Power On</option>
            <option value="OFF">Power Off</option>
            <option value="MOD">Mode</option>
            <option value="FAN">Fan Speed</option>
            <optgroup label="Temperature">
                ${[...Array(14).keys()].map(i => `<option value="T${17+i}">${17+i}°C</option>`).join('')}
            </optgroup>
        `;
    } else {
        select.innerHTML = `
            <option value="ON">Power On</option>
            <option value="OFF">Power Off</option>
            <option value="PWR">Power Toggle</option>
            <option value="S1">Speed 1 (Low)</option>
            <option value="S2">Speed 2 (Med)</option>
            <option value="S3">Speed 3 (High)</option>
            <option value="SUP">Speed Cycle/Up</option>
            <option value="SWG">Swing</option>
            <option value="TMR">Timer</option>
        `;
    }
}

function toggleLearning() {
    if (state.isLearning) stopLearning();
    else startLearning();
}

async function startLearning() {
    try {
        await fetch('/api/learn/start', { method: 'POST' });
        state.isLearning = true;
        DOM.learnStatus.innerText = "Listening...";
        DOM.learnStatus.style.color = "var(--success)";
        DOM.learnBtn.innerText = "Stop";
        DOM.learnBtn.classList.add('btn-primary');

        if (state.learnInterval) clearInterval(state.learnInterval);
        state.learnInterval = setInterval(checkLearnStatus, 1000);
    } catch (e) { alert("Failed to start learning"); }
}

async function stopLearning() {
    try { await fetch('/api/learn/stop', { method: 'POST' }); } catch (e) { }
    state.isLearning = false;
    DOM.learnStatus.innerText = "Ready";
    DOM.learnStatus.style.color = "var(--text-secondary)";
    DOM.learnBtn.innerText = "Start";
    DOM.learnBtn.classList.remove('btn-primary');
    if (state.learnInterval) clearInterval(state.learnInterval);
}

async function checkLearnStatus() {
    try {
        const res = await fetch('/api/learn/status');
        const data = await res.json();
        if (data.captured > 0) {
            DOM.learnStatus.innerText = `Captured (${data.captured})`;
            // learnControls removal



            if (!data.learning) {
                state.isLearning = false;
                clearInterval(state.learnInterval);
                DOM.learnBtn.innerText = "Start";
                DOM.learnBtn.classList.remove('btn-primary');

                // Auto-Save Logic
                const target = state.learnTarget;
                const select = target === 'ac' ? DOM.acBrandSelect : DOM.fanBrandSelect;
                const currentBrand = select ? select.value : "";
                
                if (!currentBrand.startsWith('custom_')) {
                    alert(`Learned commands can only be saved to 'Custom Brands'.\n\nPlease select a Custom Brand in the ${target.toUpperCase()} section, or click '+ New' to create one.`);
                    return;
                }

                const brandName = currentBrand.replace('custom_', '');
                
                // NVS Key limit is 15 characters!
                // Compact Naming: Prefix(2) + Brand(8) + Suffix(4) = 14 total.
                const shortBrand = brandName.replace(/[^a-zA-Z0-9]/g, '').substring(0, 8);
                const cmdSuffix = document.getElementById('learn-cmd-suffix').value;
                const keyName = state.learnTarget === 'ac' ? `A_${shortBrand}_${cmdSuffix}` : `F_${shortBrand}_${cmdSuffix}`;

                DOM.learnStatus.innerText = "Saving...";

                try {
                    const saveRes = await fetch('/api/save?key=' + encodeURIComponent(keyName), { method: 'POST' });
                    if (saveRes.ok) {
                        // Success Feedback
                        DOM.learnStatus.innerText = `Saved: ${cmdSuffix}`;
                        DOM.learnStatus.style.color = "var(--success)";

                        // Optional: Auto-increment temp or just let user pick next
                        setTimeout(() => {
                            DOM.learnStatus.innerText = "Ready";
                            DOM.learnStatus.style.color = "var(--text-secondary)";
                        }, 2000);
                    } else {
                        DOM.learnStatus.innerText = "Save Failed";
                        DOM.learnStatus.style.color = "var(--danger)";
                    }
                } catch (e) {
                    DOM.learnStatus.innerText = "Error";
                }
            }
        }
    } catch (e) { }
}
// Removed manual saveLearnedKey function as it is auto-handled

// --- Logs & Helpers ---
async function fetchLogs() {
    try {
        const res = await fetch('/api/system/logs');
        DOM.logTerminal.innerHTML = (await res.text()).replace(/\n/g, '<br>');
        DOM.logTerminal.scrollTop = DOM.logTerminal.scrollHeight;
    } catch (e) { }
}

function openLedModal() {
    if (confirm("Open LED Control?")) alert("LED Control placeholder");
}

function promptWifi() { alert("Use serial provisioning"); }
>>>>>>> 23262fa7d5edab1511d7550405a5120c98d1e31d
