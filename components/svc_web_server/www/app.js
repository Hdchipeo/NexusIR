// Goku Modern Web UI Implementation
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
    if (localStorage.getItem('goku_debug') === 'true') enableDebug(true);
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
    localStorage.setItem('goku_debug', enabled);
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
async function fetchNodes() {
    try {
        const res = await fetch('/api/nodes');
        if (res.ok) {
            state.nodes = await res.json();
            renderNodes();
        }
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
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ mac: mac.toLowerCase() })
        });
        if (res.ok) fetchNodes();
        else alert("Failed to add node");
    } catch (e) { alert("Error adding node"); }
}

async function removeNode(mac) {
    if (!confirm(`Remove node ${mac}?`)) return;
    try {
        const res = await fetch('/api/nodes', {
            method: 'DELETE',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ mac: mac })
        });
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
