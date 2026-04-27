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

// Stats
async function loadStats() {
    const res = await fetch('/api/dashboard/stats');
    const data = await res.json();
    document.getElementById('stat-devices').innerText = data.total_devices;
    document.getElementById('stat-version').innerText = data.active_version;
    document.getElementById('stat-date').innerText = data.last_upload ? new Date(data.last_upload).toLocaleDateString() : '--';
}

// Versions
async function loadVersions() {
    const res = await fetch('/api/versions');
    const data = await res.json();
    const tbody = document.querySelector('#versions-table tbody');
    tbody.innerHTML = data.map(v => `
        <tr>
            <td><strong>${v.version}</strong></td>
            <td>${new Date(v.created_at).toLocaleString()}</td>
            <td><span class="badge ${v.is_active ? 'badge-active' : 'badge-inactive'}">${v.is_active ? 'Active' : 'Inactive'}</span></td>
            <td>
                ${v.is_active ? '' : `<button class="btn btn-primary" onclick="activateVersion(${v.id})">Activate</button>`}
            </td>
        </tr>
    `).join('');
}

async function activateVersion(id) {
    await fetch(`/api/versions/activate/${id}`, { method: 'POST' });
    loadVersions();
    loadStats();
}

// Devices
async function loadDevices() {
    const res = await fetch('/api/devices');
    const data = await res.json();
    const tbody = document.querySelector('#devices-table tbody');
    tbody.innerHTML = data.map(d => `
        <tr>
            <td>${d.name}</td>
            <td>${d.ip}</td>
            <td>${d.version}</td>
            <td>${new Date(d.last_seen).toLocaleString()}</td>
            <td><span class="status-dot"></span> Online</td>
        </tr>
    `).join('');
}

// Modal
function showUploadModal() {
    document.getElementById('upload-modal').style.display = 'block';
}

function closeModal() {
    document.getElementById('upload-modal').style.display = 'none';
}

// Upload Form
document.getElementById('upload-form').addEventListener('submit', async (e) => {
    e.preventDefault();
    const formData = new FormData(e.target);
    
    const res = await fetch('/api/versions/upload', {
        method: 'POST',
        body: formData
    });
    
    if (res.ok) {
        closeModal();
        loadVersions();
    }
});

// Initial Load
loadStats();
setInterval(loadStats, 5000); // Poll stats every 5s
