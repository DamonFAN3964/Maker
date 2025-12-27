"""
数据库模块
"""
from sqlalchemy import create_engine, Column, Integer, Float, String, Boolean, DateTime
from sqlalchemy.ext.declarative import declarative_base
from sqlalchemy.orm import sessionmaker
from datetime import datetime
from config import DATABASE_URL

# 数据库引擎
engine = create_engine(DATABASE_URL, pool_pre_ping=True)
SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=engine)
Base = declarative_base()


# ==================== 数据模型 ====================
class SensorLog(Base):
    """传感器日志"""
    __tablename__ = 'sensor_log'
    id = Column(Integer, primary_key=True, autoincrement=True)
    temperature = Column(Float, nullable=False)
    humidity = Column(Float, nullable=False)
    timestamp = Column(DateTime, default=datetime.now)


class DeviceCommand(Base):
    """设备命令"""
    __tablename__ = 'device_command'
    id = Column(Integer, primary_key=True, autoincrement=True)
    device = Column(String(50), nullable=False)
    command = Column(String(50), nullable=False)
    start_date = Column(String(10), nullable=True)
    start_time = Column(String(10), nullable=True)
    end_date = Column(String(10), nullable=True)
    end_time = Column(String(10), nullable=True)
    enabled = Column(Boolean, default=True)
    executed = Column(Boolean, default=False)
    created_at = Column(DateTime, default=datetime.now)


# 创建表
Base.metadata.create_all(bind=engine)


def get_db():
    """获取数据库会话"""
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()
