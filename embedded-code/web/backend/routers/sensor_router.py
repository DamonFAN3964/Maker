"""
传感器数据路由
"""
from fastapi import APIRouter, HTTPException
from datetime import datetime
from database import SessionLocal, SensorLog
from schemas import (
    SensorData, SensorUploadResponse, SensorListResponse,
    HealthResponse, TimeResponse
)

router = APIRouter(prefix="/api", tags=["传感器"])


@router.get("/health", response_model=HealthResponse)
def health_check():
    """健康检查"""
    return {"status": "ok", "timestamp": datetime.now().isoformat()}


@router.get("/time", response_model=TimeResponse)
def get_current_time():
    """获取服务器当前时间，MCU 可用于同步本地时钟"""
    now = datetime.now()
    return {
        "success": True,
        "timestamp": now.isoformat(),
        "date": now.strftime('%Y-%m-%d'),
        "time": now.strftime('%H:%M:%S'),
        "unix": int(now.timestamp()),
        "weekday": now.weekday()
    }


@router.post("/upload", response_model=SensorUploadResponse)
def upload_sensor_data(data: SensorData):
    """MCU 上传传感器数据"""
    db = SessionLocal()
    try:
        log = SensorLog(temperature=data.temperature, humidity=data.humidity)
        db.add(log)
        db.commit()
        db.refresh(log)
        return {
            "success": True,
            "message": "数据上传成功",
            "data": {
                "id": log.id,
                "temperature": log.temperature,
                "humidity": log.humidity,
                "timestamp": log.timestamp.strftime('%Y-%m-%d %H:%M:%S')
            }
        }
    except Exception as e:
        db.rollback()
        raise HTTPException(status_code=500, detail=str(e))
    finally:
        db.close()


@router.get("/data", response_model=SensorListResponse)
def get_sensor_data(limit: int = 20):
    """获取历史数据"""
    db = SessionLocal()
    try:
        logs = db.query(SensorLog).order_by(SensorLog.timestamp.desc()).limit(limit).all()
        return {
            "success": True,
            "data": [{
                "id": log.id,
                "temperature": log.temperature,
                "humidity": log.humidity,
                "timestamp": log.timestamp.strftime('%Y-%m-%d %H:%M:%S')
            } for log in logs]
        }
    finally:
        db.close()
