"""
加湿器控制路由
"""
from fastapi import APIRouter, HTTPException
from datetime import datetime
from database import SessionLocal, DeviceCommand
from schemas import (
    ScheduleData, ToggleData, LevelData, HumidifierConfig,
    MessageResponse, ScheduleResponse, LevelResponse, LevelSetResponse,
    HumidifierStatusResponse, ConfigResponse
)

router = APIRouter(prefix="/api/humidifier", tags=["加湿器"])


@router.get("/schedule", response_model=ScheduleResponse)
def get_schedule():
    """获取加湿器定时配置"""
    db = SessionLocal()
    try:
        cmd = db.query(DeviceCommand).filter_by(device='humidifier', command='schedule').first()
        if cmd:
            return {
                "success": True,
                "data": {
                    "id": cmd.id,
                    "start_date": cmd.start_date,
                    "start_time": cmd.start_time,
                    "end_date": cmd.end_date,
                    "end_time": cmd.end_time,
                    "enabled": cmd.enabled
                }
            }
        return {"success": True, "data": None}
    finally:
        db.close()


@router.post("/schedule", response_model=MessageResponse)
def set_schedule(data: ScheduleData):
    """设置加湿器定时"""
    db = SessionLocal()
    try:
        today = datetime.now().strftime('%Y-%m-%d')
        cmd = db.query(DeviceCommand).filter_by(device='humidifier', command='schedule').first()
        
        if cmd:
            cmd.start_date = data.start_date or today
            cmd.start_time = data.start_time
            cmd.end_date = data.end_date or today
            cmd.end_time = data.end_time
            cmd.enabled = data.enabled
            cmd.executed = False
        else:
            cmd = DeviceCommand(
                device='humidifier',
                command='schedule',
                start_date=data.start_date or today,
                start_time=data.start_time,
                end_date=data.end_date or today,
                end_time=data.end_time,
                enabled=data.enabled
            )
            db.add(cmd)
        
        db.commit()
        return {"success": True, "message": "定时设置成功"}
    except Exception as e:
        db.rollback()
        raise HTTPException(status_code=500, detail=str(e))
    finally:
        db.close()


@router.post("/toggle", response_model=MessageResponse)
def toggle_humidifier(data: ToggleData):
    """手动开关加湿器"""
    if data.action not in ['on', 'off']:
        raise HTTPException(status_code=400, detail="无效的操作")
    
    db = SessionLocal()
    try:
        cmd = DeviceCommand(
            device='humidifier',
            command=data.action,
            enabled=True,
            executed=False
        )
        db.add(cmd)
        db.commit()
        return {
            "success": True,
            "message": f'加湿器已{"开启" if data.action == "on" else "关闭"}'
        }
    except Exception as e:
        db.rollback()
        raise HTTPException(status_code=500, detail=str(e))
    finally:
        db.close()


@router.post("/level", response_model=LevelSetResponse)
def set_humidifier_level(data: LevelData):
    """设置加湿器档位 (1-3)"""
    if data.level < 1 or data.level > 3:
        raise HTTPException(status_code=400, detail="档位必须在 1-3 之间")
    
    db = SessionLocal()
    try:
        cmd = DeviceCommand(
            device='humidifier',
            command=f'level_{data.level}',
            enabled=True,
            executed=False
        )
        db.add(cmd)
        db.commit()
        return {
            "success": True,
            "message": f'加湿器已设置为 {data.level} 档',
            "level": data.level
        }
    except Exception as e:
        db.rollback()
        raise HTTPException(status_code=500, detail=str(e))
    finally:
        db.close()


@router.get("/level", response_model=LevelResponse)
def get_humidifier_level():
    """获取当前加湿器档位设置"""
    db = SessionLocal()
    try:
        cmd = db.query(DeviceCommand).filter(
            DeviceCommand.device == 'humidifier',
            DeviceCommand.command.like('level_%')
        ).order_by(DeviceCommand.created_at.desc()).first()
        
        if cmd:
            level = int(cmd.command.split('_')[1])
            return {"success": True, "level": level}
        return {"success": True, "level": 1}
    finally:
        db.close()


@router.get("/status", response_model=HumidifierStatusResponse)
def get_humidifier_status():
    """获取加湿器完整状态"""
    db = SessionLocal()
    try:
        # 获取最新开关状态
        power_cmd = db.query(DeviceCommand).filter(
            DeviceCommand.device == 'humidifier',
            DeviceCommand.command.in_(['on', 'off'])
        ).order_by(DeviceCommand.created_at.desc()).first()
        
        # 获取最新档位
        level_cmd = db.query(DeviceCommand).filter(
            DeviceCommand.device == 'humidifier',
            DeviceCommand.command.like('level_%')
        ).order_by(DeviceCommand.created_at.desc()).first()
        
        # 获取定时配置
        schedule_cmd = db.query(DeviceCommand).filter_by(
            device='humidifier', command='schedule'
        ).first()
        
        power = power_cmd.command == 'on' if power_cmd else False
        level = int(level_cmd.command.split('_')[1]) if level_cmd else 1
        
        status = {
            "power": power,
            "level": level,
            "schedule": None
        }
        
        if schedule_cmd:
            now = datetime.now()
            start_date = schedule_cmd.start_date or now.strftime('%Y-%m-%d')
            end_date = schedule_cmd.end_date or now.strftime('%Y-%m-%d')
            
            try:
                start_dt = datetime.strptime(f'{start_date} {schedule_cmd.start_time}', '%Y-%m-%d %H:%M')
                end_dt = datetime.strptime(f'{end_date} {schedule_cmd.end_time}', '%Y-%m-%d %H:%M')
                is_active = start_dt <= now <= end_dt
            except:
                is_active = False
            
            status["schedule"] = {
                "enabled": schedule_cmd.enabled,
                "start_date": schedule_cmd.start_date,
                "start_time": schedule_cmd.start_time,
                "end_date": schedule_cmd.end_date,
                "end_time": schedule_cmd.end_time,
                "is_active": is_active and schedule_cmd.enabled
            }
        
        return {"success": True, "status": status}
    finally:
        db.close()


@router.post("/config", response_model=ConfigResponse)
def set_humidifier_config(config: HumidifierConfig):
    """设置加湿器完整配置"""
    db = SessionLocal()
    try:
        # 1. 设置开关状态
        power_cmd = DeviceCommand(
            device='humidifier',
            command='on' if config.power else 'off',
            enabled=True,
            executed=False
        )
        db.add(power_cmd)
        
        # 2. 设置档位
        if 1 <= config.level <= 3:
            level_cmd = DeviceCommand(
                device='humidifier',
                command=f'level_{config.level}',
                enabled=True,
                executed=False
            )
            db.add(level_cmd)
        
        # 3. 设置定时
        if config.schedule_enabled and config.start_time and config.end_time:
            today = datetime.now().strftime('%Y-%m-%d')
            schedule_cmd = db.query(DeviceCommand).filter_by(
                device='humidifier', command='schedule'
            ).first()
            
            if schedule_cmd:
                schedule_cmd.start_date = config.start_date or today
                schedule_cmd.start_time = config.start_time
                schedule_cmd.end_date = config.end_date or today
                schedule_cmd.end_time = config.end_time
                schedule_cmd.enabled = True
                schedule_cmd.executed = False
            else:
                schedule_cmd = DeviceCommand(
                    device='humidifier',
                    command='schedule',
                    start_date=config.start_date or today,
                    start_time=config.start_time,
                    end_date=config.end_date or today,
                    end_time=config.end_time,
                    enabled=True
                )
                db.add(schedule_cmd)
        
        db.commit()
        return {"success": True, "message": "配置已更新", "config": config.dict()}
    except Exception as e:
        db.rollback()
        raise HTTPException(status_code=500, detail=str(e))
    finally:
        db.close()
