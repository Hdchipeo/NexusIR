import os
import shutil
from typing import List, Optional
from fastapi import FastAPI, UploadFile, File, Form, Depends, HTTPException, Request
from fastapi.responses import HTMLResponse, FileResponse, PlainTextResponse
from fastapi.staticfiles import StaticFiles
from sqlalchemy.orm import Session
from sqlalchemy import func
from datetime import datetime, timedelta
from pydantic import BaseModel
from contextlib import asynccontextmanager
import socket
from zeroconf import IPVersion, ServiceInfo, Zeroconf

try:
    from .models import Base, Version, Device
except ImportError:
    from models import Base, Version, Device

from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker

# Paths relative to this script
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
SQLALCHEMY_DATABASE_URL = f"sqlite:///{os.path.join(BASE_DIR, 'ota.db')}"
UPLOAD_DIR = os.path.join(BASE_DIR, "uploads")
STATIC_DIR = os.path.join(BASE_DIR, "static")

engine = create_engine(SQLALCHEMY_DATABASE_URL, connect_args={"check_same_thread": False})
SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=engine)
Base.metadata.create_all(bind=engine)

from zeroconf.asyncio import AsyncZeroconf

def get_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(('10.255.255.255', 1))
        IP = s.getsockname()[0]
    except Exception:
        IP = '127.0.0.1'
    finally:
        s.close()
    return IP

@asynccontextmanager
async def lifespan(app: FastAPI):
    # Startup: Register mDNS service
    desc = {'version': '1.4.2'}
    local_ip = get_ip()
    port = 5555 
    
    info = ServiceInfo(
        "_nexusir-ota._tcp.local.",
        "NexusIR-OTA._nexusir-ota._tcp.local.",
        addresses=[socket.inet_aton(local_ip)],
        port=port,
        properties=desc,
        server=f"{socket.gethostname()}.local.",
    )
    
    aiozc = AsyncZeroconf(ip_version=IPVersion.V4Only)
    await aiozc.zeroconf.async_register_service(info)
    print(f"[*] mDNS Service registered: {socket.gethostname()}.local:{port} ({local_ip})")
    
    yield
    
    # Shutdown: Unregister
    await aiozc.zeroconf.async_unregister_service(info)
    await aiozc.async_close()

app = FastAPI(title="NexusIR OTA Server", lifespan=lifespan)

# Ensure uploads directory exists
os.makedirs(UPLOAD_DIR, exist_ok=True)

# Mount static files
app.mount("/static", StaticFiles(directory=STATIC_DIR), name="static")

# Dependency
def get_db():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()

# --- Device APIs (Protocols for ESP32) ---

@app.get("/version.txt", response_class=PlainTextResponse)
def get_version_txt(device_id: str = "Unknown", db: Session = Depends(get_db)):
    # Check for device-specific version
    device = db.query(Device).filter(Device.id == device_id).first()
    if device and device.target_version_id:
        target_v = db.query(Version).filter(Version.id == device.target_version_id).first()
        if target_v:
            return target_v.version_string

    # Fallback to global active version
    active_version = db.query(Version).filter(Version.is_active == True).order_by(Version.created_at.desc()).first()
    if active_version:
        return active_version.version_string
    return "0.0.0"

@app.get("/nexus-ir.bin")
def download_firmware(device_id: str = "Unknown", db: Session = Depends(get_db)):
    target_v = None
    # Check for device-specific version
    device = db.query(Device).filter(Device.id == device_id).first()
    if device and device.target_version_id:
        target_v = db.query(Version).filter(Version.id == device.target_version_id).first()
    
    # Fallback to global active version
    if not target_v:
        target_v = db.query(Version).filter(Version.is_active == True).order_by(Version.created_at.desc()).first()

    if target_v:
        file_path = os.path.join(UPLOAD_DIR, target_v.filename)
        if os.path.exists(file_path):
            return FileResponse(file_path, media_type="application/octet-stream", filename="nexus-ir.bin")
    
    raise HTTPException(status_code=404, detail="Firmware not found")

# --- Dashboard APIs ---

@app.get("/", response_class=HTMLResponse)
def read_root():
    with open(os.path.join(STATIC_DIR, "index.html"), "r") as f:
        return f.read()

@app.get("/api/dashboard/stats")
def get_stats(db: Session = Depends(get_db)):
    total_devices = db.query(Device).count()
    online_threshold = datetime.utcnow() - timedelta(minutes=5)
    online_devices = db.query(Device).filter(
        Device.status == "online",
        Device.last_seen >= online_threshold
    ).count()
    active_version = db.query(Version).filter(Version.is_active == True).first()
    total_versions = db.query(Version).count()
    last_upload = db.query(func.max(Version.created_at)).scalar()

    # Version distribution: how many devices run each firmware version
    dist_rows = db.query(
        Device.current_version, func.count(Device.id)
    ).group_by(Device.current_version).all()
    version_distribution = [
        {"version": row[0] or "Unknown", "count": row[1]} for row in dist_rows
    ]

    # Recent activity: last 10 devices by last_seen
    recent_devices = db.query(Device).order_by(Device.last_seen.desc()).limit(10).all()
    recent_activity = [
        {
            "name": d.device_name,
            "version": d.current_version,
            "last_seen": d.last_seen.isoformat() if d.last_seen else None,
            "status": d.status
        } for d in recent_devices
    ]

    return {
        "total_devices": total_devices,
        "online_devices": online_devices,
        "active_version": active_version.version_string if active_version else "None",
        "total_versions": total_versions,
        "last_upload": last_upload.isoformat() if last_upload else None,
        "version_distribution": version_distribution,
        "recent_activity": recent_activity
    }

@app.get("/api/devices", response_model=List[dict])
def list_devices(db: Session = Depends(get_db)):
    devices = db.query(Device).all()
    return [
        {
            "id": d.id,
            "name": d.device_name,
            "ip": d.last_ip,
            "version": d.current_version,
            "target_version_id": d.target_version_id,
            "target_version_name": d.target_version.version_string if d.target_version else "Global",
            "last_seen": d.last_seen.isoformat(),
            "status": d.status
        } for d in devices
    ]

@app.post("/api/devices/{device_id}/config")
def config_device(device_id: str, name: str = Form(None), target_version_id: int = Form(None), db: Session = Depends(get_db)):
    device = db.query(Device).filter(Device.id == device_id).first()
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")
    
    if name is not None:
        device.device_name = name
    
    # target_version_id = 0 or -1 could mean "Back to Global"
    if target_version_id is not None:
        if target_version_id <= 0:
            device.target_version_id = None
        else:
            device.target_version_id = target_version_id
            
    db.commit()
    return {"status": "success", "device_id": device_id}

@app.get("/api/versions", response_model=List[dict])
def list_versions(db: Session = Depends(get_db)):
    versions = db.query(Version).order_by(Version.created_at.desc()).all()
    result = []
    for v in versions:
        file_path = os.path.join(UPLOAD_DIR, v.filename) if v.filename else ""
        try:
            file_size = os.path.getsize(file_path) if file_path and os.path.exists(file_path) else 0
        except OSError:
            file_size = 0
        result.append({
            "id": v.id,
            "version": v.version_string,
            "filename": v.filename,
            "created_at": v.created_at.isoformat(),
            "is_active": v.is_active,
            "release_notes": v.release_notes,
            "file_size": file_size
        })
    return result

@app.post("/api/versions/upload")
async def upload_version(
    version: str = Form(...), 
    release_notes: str = Form(""), 
    file: UploadFile = File(...),
    db: Session = Depends(get_db)
):
    filename = f"{version}_{file.filename}"
    file_path = os.path.join(UPLOAD_DIR, filename)
    
    with open(file_path, "wb") as buffer:
        shutil.copyfileobj(file.file, buffer)
    
    new_version = Version(
        version_string=version,
        filename=filename,
        release_notes=release_notes,
        is_active=False
    )
    db.add(new_version)
    db.commit()
    return {"status": "success", "version": version}

@app.post("/api/versions/activate/{version_id}")
def activate_version(version_id: int, db: Session = Depends(get_db)):
    # Deactivate all
    db.query(Version).update({Version.is_active: False})
    # Activate specific
    version = db.query(Version).filter(Version.id == version_id).first()
    if not version:
        raise HTTPException(status_code=404, detail="Version not found")
    version.is_active = True
    db.commit()
    return {"status": "success"}

# --- Pydantic models for request bodies ---

class VersionUpdateRequest(BaseModel):
    version_string: Optional[str] = None
    release_notes: Optional[str] = None

# --- Additional CRUD APIs ---

@app.delete("/api/versions/{version_id}")
def delete_version(version_id: int, db: Session = Depends(get_db)):
    version = db.query(Version).filter(Version.id == version_id).first()
    if not version:
        raise HTTPException(status_code=404, detail="Version not found")
    if version.is_active:
        raise HTTPException(status_code=400, detail="Cannot delete the active version")

    # Delete the physical firmware file
    file_path = os.path.join(UPLOAD_DIR, version.filename)
    if os.path.exists(file_path):
        os.remove(file_path)

    # Clear target_version_id on any device referencing this version
    db.query(Device).filter(Device.target_version_id == version_id).update(
        {Device.target_version_id: None}
    )

    db.delete(version)
    db.commit()
    return {"status": "success", "detail": f"Version {version.version_string} deleted"}

@app.put("/api/versions/{version_id}")
def update_version(version_id: int, body: VersionUpdateRequest, db: Session = Depends(get_db)):
    version = db.query(Version).filter(Version.id == version_id).first()
    if not version:
        raise HTTPException(status_code=404, detail="Version not found")

    if body.version_string is not None:
        version.version_string = body.version_string
    if body.release_notes is not None:
        version.release_notes = body.release_notes

    db.commit()
    return {"status": "success", "detail": f"Version {version_id} updated"}

@app.delete("/api/devices/{device_id}")
def delete_device(device_id: str, db: Session = Depends(get_db)):
    device = db.query(Device).filter(Device.id == device_id).first()
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")

    db.delete(device)
    db.commit()
    return {"status": "success", "detail": f"Device {device_id} deleted"}

# Middleware to track devices (simple check-in)
@app.middleware("http")
async def track_device_middleware(request: Request, call_next):
    if request.url.path == "/version.txt":
        device_id = request.query_params.get("device_id", "Unknown")
        device_name = request.query_params.get("device_name", None)
        current_ver = request.query_params.get("v", "Unknown")
        client_host = request.client.host
        
        # Background: Update device info
        db = SessionLocal()
        device = db.query(Device).filter(Device.id == device_id).first()
        if not device:
            name = device_name if device_name else f"Device-{device_id[-4:]}"
            device = Device(id=device_id, device_name=name)
        elif device_name:
            device.device_name = device_name
        
        device.last_ip = client_host
        device.current_version = current_ver
        device.last_seen = datetime.utcnow()
        device.status = "online"
        
        db.merge(device)
        db.commit()
        db.close()
        
    response = await call_next(request)
    return response

if __name__ == "__main__":
    import uvicorn
    uvicorn.run("main:app", host="0.0.0.0", port=5555, reload=True)

