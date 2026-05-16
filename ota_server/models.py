from sqlalchemy import Column, Integer, String, DateTime, Boolean, ForeignKey
from sqlalchemy.ext.declarative import declarative_base
from sqlalchemy.orm import relationship
from datetime import datetime

Base = declarative_base()

class Version(Base):
    __tablename__ = "versions"
    id = Column(Integer, primary_key=True, index=True)
    version_string = Column(String, unique=True, index=True)
    filename = Column(String)
    release_notes = Column(String)
    created_at = Column(DateTime, default=datetime.utcnow)
    is_active = Column(Boolean, default=False)

class Device(Base):
    __tablename__ = "devices"
    id = Column(String, primary_key=True, index=True) # Usually MAC or unique ID
    device_name = Column(String)
    last_ip = Column(String)
    current_version = Column(String)
    last_seen = Column(DateTime, default=datetime.utcnow)
    status = Column(String, default="online") # online, offline, updating
