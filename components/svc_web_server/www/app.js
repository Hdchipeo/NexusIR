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
async function fetchNodes() {
    try {
        const res = await fetch('/api/nodes');
        if (res.ok) {
            state.nodes = await res.json();
            renderNodes();
        }
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
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ mac: mac.toLowerCase() })
        });
        fetchNodes();
    } catch (e) { fetchNodes(); }
}

async function removeNode(mac) {
    if (!confirm(`Xóa node ${mac}?`)) return;
    try {
        await fetch('/api/nodes', {
            method: 'DELETE',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ mac: mac })
        });
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
