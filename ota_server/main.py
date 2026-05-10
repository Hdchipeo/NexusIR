import os
import shutil
from typing import List
from fastapi import FastAPI, UploadFile, File, Form, Depends, HTTPException, Request
from fastapi.responses import HTMLResponse, FileResponse, PlainTextResponse
from fastapi.staticfiles import StaticFiles
from sqlalchemy.orm import Session
from datetime import datetime
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
<<<<<<< HEAD
def get_version_txt(device_id: str = "Unknown", db: Session = Depends(get_db)):
    # Check for device-specific version
    device = db.query(Device).filter(Device.id == device_id).first()
    if device and device.target_version_id:
        target_v = db.query(Version).filter(Version.id == device.target_version_id).first()
        if target_v:
            return target_v.version_string

    # Fallback to global active version
=======
def get_version_txt(db: Session = Depends(get_db)):
>>>>>>> 23262fa7d5edab1511d7550405a5120c98d1e31d
    active_version = db.query(Version).filter(Version.is_active == True).order_by(Version.created_at.desc()).first()
    if active_version:
        return active_version.version_string
    return "0.0.0"

@app.get("/nexus-ir.bin")
<<<<<<< HEAD
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
    
=======
def download_firmware(db: Session = Depends(get_db)):
    active_version = db.query(Version).filter(Version.is_active == True).order_by(Version.created_at.desc()).first()
    if active_version:
        file_path = os.path.join(UPLOAD_DIR, active_version.filename)
        if os.path.exists(file_path):
            return FileResponse(file_path, media_type="application/octet-stream", filename="nexus-ir.bin")
>>>>>>> 23262fa7d5edab1511d7550405a5120c98d1e31d
    raise HTTPException(status_code=404, detail="Firmware not found")

# --- Dashboard APIs ---

@app.get("/", response_class=HTMLResponse)
def read_root():
    with open(os.path.join(STATIC_DIR, "index.html"), "r") as f:
        return f.read()

@app.get("/api/dashboard/stats")
def get_stats(db: Session = Depends(get_db)):
    total_devices = db.query(Device).count()
    active_version = db.query(Version).filter(Version.is_active == True).first()
    return {
        "total_devices": total_devices,
        "active_version": active_version.version_string if active_version else "None",
        "last_upload": active_version.created_at if active_version else None
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
<<<<<<< HEAD
            "target_version_id": d.target_version_id,
            "target_version_name": d.target_version.version_string if d.target_version else "Global",
=======
>>>>>>> 23262fa7d5edab1511d7550405a5120c98d1e31d
            "last_seen": d.last_seen.isoformat(),
            "status": d.status
        } for d in devices
    ]

<<<<<<< HEAD
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

=======
>>>>>>> 23262fa7d5edab1511d7550405a5120c98d1e31d
@app.get("/api/versions", response_model=List[dict])
def list_versions(db: Session = Depends(get_db)):
    versions = db.query(Version).order_by(Version.created_at.desc()).all()
    return [
        {
            "id": v.id,
            "version": v.version_string,
            "filename": v.filename,
            "created_at": v.created_at.isoformat(),
            "is_active": v.is_active
        } for v in versions
    ]

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

# Middleware to track devices (simple check-in)
@app.middleware("http")
async def track_device_middleware(request: Request, call_next):
    if request.url.path == "/version.txt":
        device_id = request.query_params.get("device_id", "Unknown")
        current_ver = request.query_params.get("v", "Unknown")
        client_host = request.client.host
        
        # Background: Update device info
        db = SessionLocal()
        device = db.query(Device).filter(Device.id == device_id).first()
        if not device:
            device = Device(id=device_id, device_name=f"Device-{device_id[-4:]}")
        
        device.last_ip = client_host
        device.current_version = current_ver
        device.last_seen = datetime.utcnow()
        device.status = "online"
        
        db.merge(device)
        db.commit()
        db.close()
        
    response = await call_next(request)
    return response
