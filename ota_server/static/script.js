// Tab Switching
document.querySelectorAll('.nav-item').forEach(item => {
    item.addEventListener('click', (e) => {
        e.preventDefault();
        document.querySelectorAll('.nav-item').forEach(n => n.classList.remove('active'));
        document.querySelectorAll('.tab-content').forEach(t => t.classList.remove('active'));
        
        item.classList.add('active');
        const tab = item.getAttribute('data-tab');
        document.getElementById(`${tab}-section`).classList.add('active');
        document.getElementById('page-title').innerText = item.innerText;
        
        loadTabData(tab);
    });
});

async function loadTabData(tab) {
    if (tab === 'dashboard') loadStats();
    if (tab === 'versions') loadVersions();
    if (tab === 'devices') loadDevices();
}

// Global modal helpers
function showModal(id) {
    document.getElementById(id).style.display = 'block';
}

function closeModal(id) {
    document.getElementById(id).style.display = 'none';
}

// Format size helper
function formatBytes(bytes, decimals = 2) {
    if (bytes === 0) return '0 Bytes';
    const k = 1024;
    const dm = decimals < 0 ? 0 : decimals;
    const sizes = ['Bytes', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];
}

// Stats & Insight Chart & Recent Activity
async function loadStats() {
    const res = await fetch('/api/dashboard/stats');
    if (!res.ok) return;
    const data = await res.json();
    
    // Core counter stats
    document.getElementById('stat-devices').innerText = data.total_devices || 0;
    document.getElementById('stat-online').innerText = data.online_devices || 0;
    document.getElementById('stat-version').innerText = data.active_version || '--';
    document.getElementById('stat-total-versions').innerText = data.total_versions || 0;

    // Draw Donut Chart
    const total = data.total_devices || 0;
    const online = data.online_devices || 0;
    const offline = total - online;
    const onlinePct = total > 0 ? Math.round((online / total) * 100) : 0;
    
    const pctElement = document.getElementById('donut-online-pct');
    if (pctElement) pctElement.innerText = `${onlinePct}%`;
    
    const ringCircumference = 345;
    const onlineSegment = document.getElementById('donut-segment-online');
    const offlineSegment = document.getElementById('donut-segment-offline');
    
    if (onlineSegment && offlineSegment) {
        if (total === 0) {
            onlineSegment.style.strokeDasharray = `0 ${ringCircumference}`;
            offlineSegment.style.strokeDasharray = `0 ${ringCircumference}`;
        } else {
            const onlineRatio = online / total;
            const offlineRatio = offline / total;
            
            const onlineLength = ringCircumference * onlineRatio;
            const offlineLength = ringCircumference * offlineRatio;
            
            onlineSegment.style.strokeDasharray = `${onlineLength} ${ringCircumference}`;
            onlineSegment.style.strokeDashoffset = '0';
            
            offlineSegment.style.strokeDasharray = `${offlineLength} ${ringCircumference}`;
            offlineSegment.style.strokeDashoffset = -onlineLength;
        }
    }

    // Version distribution rendering
    const chartContainer = document.getElementById('version-chart');
    if (chartContainer) {
        if (data.version_distribution && data.version_distribution.length > 0) {
            const maxCount = Math.max(...data.version_distribution.map(d => d.count), 1);
            chartContainer.innerHTML = data.version_distribution.map(dist => {
                const pct = (dist.count / maxCount) * 100;
                return `
                    <div class="chart-bar-group">
                        <div class="chart-bar-label">${dist.version}</div>
                        <div class="chart-bar-track">
                            <div class="chart-bar-fill" style="width: ${pct}%">${dist.count}</div>
                        </div>
                    </div>
                `;
            }).join('');
        } else {
            chartContainer.innerHTML = '<div class="empty-state"><p>No version distribution data available.</p></div>';
        }
    }

    // OTA Update Tasks Checklist rendering
    const tasksContainer = document.getElementById('ota-tasks-list');
    if (tasksContainer) {
        try {
            const devRes = await fetch('/api/devices');
            if (devRes.ok) {
                const devices = await devRes.json();
                if (devices.length > 0) {
                    tasksContainer.innerHTML = devices.map(d => {
                        let pct = 0;
                        let statusLabel = 'Offline';
                        let badgeClass = 'badge-offline';
                        
                        if (d.status === 'online') {
                            const hasTarget = d.target_version_id && d.target_version_id > 0;
                            const isMatch = !hasTarget || d.version === d.target_version_name;
                            if (isMatch) {
                                pct = 100;
                                statusLabel = 'Complete';
                                badgeClass = 'badge-online';
                            } else {
                                pct = 65; // simulated update progress
                                statusLabel = 'On Going';
                                badgeClass = 'badge-ongoing';
                            }
                        }
                        
                        const targetDesc = d.target_version_name ? `Target: v${d.target_version_name}` : 'Target: Global';
                        
                        return `
                            <div class="challenge-item">
                                <div class="challenge-header">
                                    <div>
                                        <div class="challenge-title">Upgrade ${d.name}</div>
                                        <div class="challenge-meta">v${d.version} &bull; ${targetDesc}</div>
                                    </div>
                                    <span class="badge ${badgeClass}">${statusLabel}</span>
                                </div>
                                <div class="challenge-progress-container">
                                    <div class="challenge-progress-bar">
                                        <div class="challenge-progress-fill" style="width: ${pct}%"></div>
                                    </div>
                                    <span class="challenge-progress-text">${pct}%</span>
                                </div>
                            </div>
                        `;
                    }).join('');
                } else {
                    tasksContainer.innerHTML = '<div class="empty-state"><p>No devices connected yet.</p></div>';
                }
            } else {
                tasksContainer.innerHTML = '<div class="empty-state"><p>Error connecting to devices database.</p></div>';
            }
        } catch (err) {
            tasksContainer.innerHTML = '<div class="empty-state"><p>Error loading tasks.</p></div>';
        }
    }
}

// Versions Management
async function loadVersions() {
    const res = await fetch('/api/versions');
    if (!res.ok) return;
    const data = await res.json();
    const tbody = document.querySelector('#versions-table tbody');
    const emptyState = document.getElementById('versions-empty');

    if (data.length === 0) {
        tbody.innerHTML = '';
        emptyState.style.display = 'block';
        return;
    }
    emptyState.style.display = 'none';

    tbody.innerHTML = data.map(v => `
        <tr>
            <td><strong>${v.version}</strong></td>
            <td><span class="file-size">${formatBytes(v.file_size)}</span></td>
            <td>${v.release_notes || '<span class="text-muted">None</span>'}</td>
            <td>${new Date(v.created_at).toLocaleString()}</td>
            <td><span class="badge ${v.is_active ? 'badge-active' : 'badge-inactive'}">${v.is_active ? 'Active' : 'Inactive'}</span></td>
            <td>
                <div class="action-group">
                    ${v.is_active ? '' : `<button class="btn btn-primary btn-sm" onclick="activateVersion(${v.id})">Activate</button>`}
                    <button class="btn btn-secondary btn-sm" onclick="editVersion(${v.id}, '${v.version}', '${v.release_notes ? v.release_notes.replace(/'/g, "\\'") : ''}')">Edit</button>
                    ${v.is_active ? '' : `<button class="btn btn-danger btn-sm" onclick="confirmDeleteVersion(${v.id}, '${v.version}')">Delete</button>`}
                </div>
            </td>
        </tr>
    `).join('');
}

async function activateVersion(id) {
    const res = await fetch(`/api/versions/activate/${id}`, { method: 'POST' });
    if (res.ok) {
        loadVersions();
        loadStats();
    }
}

function showUploadModal() {
    document.getElementById('upload-form').reset();
    showModal('upload-modal');
}

// Upload Handler
document.getElementById('upload-form').addEventListener('submit', async (e) => {
    e.preventDefault();
    const formData = new FormData(e.target);
    const res = await fetch('/api/versions/upload', {
        method: 'POST',
        body: formData
    });
    if (res.ok) {
        closeModal('upload-modal');
        loadVersions();
        loadStats();
    }
});

// Edit Version Handler
function editVersion(id, versionStr, notes) {
    document.getElementById('edit-version-id').value = id;
    document.getElementById('edit-version-string').value = versionStr;
    document.getElementById('edit-version-notes').value = notes;
    showModal('edit-version-modal');
}

document.getElementById('edit-version-form').addEventListener('submit', async (e) => {
    e.preventDefault();
    const id = document.getElementById('edit-version-id').value;
    const version_string = document.getElementById('edit-version-string').value;
    const release_notes = document.getElementById('edit-version-notes').value;

    const res = await fetch(`/api/versions/${id}`, {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ version_string, release_notes })
    });

    if (res.ok) {
        closeModal('edit-version-modal');
        loadVersions();
        loadStats();
    }
});

// Confirm delete helper
let currentDeleteAction = null;
function confirmAction(title, message, action) {
    document.getElementById('confirm-title').innerText = title;
    document.getElementById('confirm-message').innerText = message;
    currentDeleteAction = action;
    showModal('confirm-modal');
}

document.getElementById('confirm-action-btn').addEventListener('click', async () => {
    if (currentDeleteAction) {
        await currentDeleteAction();
        closeModal('confirm-modal');
    }
});

function confirmDeleteVersion(id, versionStr) {
    confirmAction(
        'Delete Firmware Version',
        `Are you sure you want to delete version ${versionStr}? This will also delete the physical binary file and target version assignments.`,
        async () => {
            const res = await fetch(`/api/versions/${id}`, { method: 'DELETE' });
            if (res.ok) {
                loadVersions();
                loadStats();
            }
        }
    );
}

// Devices Management
async function loadDevices() {
    const [devRes, verRes] = await Promise.all([
        fetch('/api/devices'),
        fetch('/api/versions')
    ]);
    if (!devRes.ok || !verRes.ok) return;
    
    const devices = await devRes.json();
    const versions = await verRes.json();
    
    const tbody = document.querySelector('#devices-table tbody');
    const emptyState = document.getElementById('devices-empty');
    
    if (devices.length === 0) {
        tbody.innerHTML = '';
        emptyState.style.display = 'block';
        document.getElementById('device-count-badge').innerText = '0 total';
        return;
    }
    emptyState.style.display = 'none';
    document.getElementById('device-count-badge').innerText = `${devices.length} total`;

    tbody.innerHTML = devices.map(d => {
        // Build the target version dropdown select options
        const globalLabel = 'Global (Default)';
        const isGlobal = !d.target_version_id;
        let options = `<option value="0" ${isGlobal ? 'selected' : ''}>${globalLabel}</option>`;
        versions.forEach(v => {
            const selected = (d.target_version_id === v.id) ? 'selected' : '';
            options += `<option value="${v.id}" ${selected}>${v.version}</option>`;
        });

        const statusClass = d.status === 'online' ? 'badge-online' : 'badge-offline';
        const statusLabel = d.status === 'online' ? 'Online' : 'Offline';

        return `
        <tr>
            <td><strong>${d.name}</strong></td>
            <td><code>${d.ip}</code></td>
            <td><code>${d.version}</code></td>
            <td>
                <select class="target-select" onchange="setTargetVersion('${d.id}', this.value)">
                    ${options}
                </select>
            </td>
            <td>${new Date(d.last_seen).toLocaleString()}</td>
            <td><span class="badge ${statusClass}">${statusLabel}</span></td>
            <td>
                <div class="action-group">
                    <button class="btn btn-ghost btn-sm" onclick="renameDevice('${d.id}', '${d.name.replace(/'/g, "\\'")}')">Rename</button>
                    <button class="btn btn-danger btn-sm" onclick="confirmDeleteDevice('${d.id}', '${d.name.replace(/'/g, "\\'")}')">Delete</button>
                </div>
            </td>
        </tr>
        `;
    }).join('');
}

async function setTargetVersion(deviceId, versionId) {
    const formData = new FormData();
    formData.append('target_version_id', versionId);

    const res = await fetch(`/api/devices/${deviceId}/config`, {
        method: 'POST',
        body: formData
    });

    if (res.ok) {
        loadDevices();
    }
}

// Rename Device Modal Handler
function renameDevice(id, name) {
    document.getElementById('rename-device-id').value = id;
    document.getElementById('rename-device-name').value = name;
    showModal('rename-device-modal');
}

document.getElementById('rename-device-form').addEventListener('submit', async (e) => {
    e.preventDefault();
    const id = document.getElementById('rename-device-id').value;
    const name = document.getElementById('rename-device-name').value;

    const formData = new FormData();
    formData.append('name', name);

    const res = await fetch(`/api/devices/${id}/config`, {
        method: 'POST',
        body: formData
    });

    if (res.ok) {
        closeModal('rename-device-modal');
        loadDevices();
        loadStats();
    }
});

function confirmDeleteDevice(id, name) {
    confirmAction(
        'Delete Device',
        `Are you sure you want to delete device ${name}? It will be removed from the dashboard database until its next check-in.`,
        async () => {
            const res = await fetch(`/api/devices/${id}`, { method: 'DELETE' });
            if (res.ok) {
                loadDevices();
                loadStats();
            }
        }
    );
}

function renderCalendar() {
    const daysOfWeek = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];
    const calendarStrip = document.getElementById('calendar-strip');
    if (!calendarStrip) return;
    
    const today = new Date();
    let html = '';
    
    // Generate last 7 days (Mon-Sun)
    for (let i = 6; i >= 0; i--) {
        const d = new Date();
        d.setDate(today.getDate() - i);
        const dayName = daysOfWeek[d.getDay()];
        const dayNum = d.getDate();
        const isToday = i === 0;
        // Mock activity: today has active status, other days have alternate pattern
        const hasActivity = isToday || (d.getDay() % 2 === 0);
        
        html += `
            <div class="calendar-day ${isToday ? 'active' : ''}">
                <span class="calendar-day-name">${dayName}</span>
                <span class="calendar-day-num">${dayNum}</span>
                <span class="calendar-day-status ${hasActivity ? 'has-activity' : 'no-activity'}"></span>
            </div>
        `;
    }
    calendarStrip.innerHTML = html;
}

// Initial Loading & Auto updates
renderCalendar();
loadStats();
setInterval(loadStats, 10000); // refresh stats/distribution/activity every 10s
