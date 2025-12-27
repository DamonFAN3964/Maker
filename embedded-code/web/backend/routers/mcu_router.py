"""
MCU 命令路由 - 拆分为3个独立API
"""
from fastapi import APIRouter
from datetime import datetime
from database import SessionLocal, DeviceCommand
from schemas import McuPowerResponse, McuLevelResponse, McuScheduleResponse, CommandsResponse

router = APIRouter(prefix="/api/mcu", tags=["MCU命令"])


@router.get("/power", response_model=McuPowerResponse)
def get_mcu_power_command():
    """
    MCU 获取开关命令
    
    返回最新的未执行开关命令，获取后自动标记为已执行
    
    返回:
    - has_command: 是否有待执行命令
    - action: 'on' 或 'off'
    """
    db = SessionLocal()
    try:
        # 查找最新的未执行开关命令
        cmd = db.query(DeviceCommand).filter(
            DeviceCommand.device == 'humidifier',
            DeviceCommand.command.in_(['on', 'off']),
            DeviceCommand.executed == False,
            DeviceCommand.enabled == True
        ).order_by(DeviceCommand.created_at.desc()).first()
        
        if cmd:
            action = cmd.command
            cmd.executed = True
            db.commit()
            return {
                "success": True,
                "has_command": True,
                "action": action
            }
        
        return {
            "success": True,
            "has_command": False,
            "action": None
        }
    finally:
        db.close()


@router.get("/level", response_model=McuLevelResponse)
def get_mcu_level_command():
    """
    MCU 获取档位命令
    
    返回最新的未执行档位命令，获取后自动标记为已执行
    
    返回:
    - has_command: 是否有待执行命令
    - level: 档位 1-4
    """
    db = SessionLocal()
    try:
        # 查找最新的未执行档位命令
        cmd = db.query(DeviceCommand).filter(
            DeviceCommand.device == 'humidifier',
            DeviceCommand.command.like('level_%'),
            DeviceCommand.executed == False,
            DeviceCommand.enabled == True
        ).order_by(DeviceCommand.created_at.desc()).first()
        
        if cmd:
            level = int(cmd.command.split('_')[1])
            cmd.executed = True
            db.commit()
            return {
                "success": True,
                "has_command": True,
                "level": level
            }
        
        return {
            "success": True,
            "has_command": False,
            "level": None
        }
    finally:
        db.close()


@router.get("/schedule", response_model=McuScheduleResponse)
def get_mcu_schedule():
    """
    MCU 获取定时配置
    
    返回当前定时配置及是否应该运行
    此接口不会标记为已执行，MCU 应持续轮询
    
    返回:
    - enabled: 定时是否启用
    - should_run: 当前时间是否在定时范围内（应该开启）
    - start_date/start_time: 开始时间
    - end_date/end_time: 结束时间
    """
    db = SessionLocal()
    try:
        cmd = db.query(DeviceCommand).filter_by(
            device='humidifier',
            command='schedule'
        ).first()
        
        if not cmd or not cmd.enabled:
            return {
                "success": True,
                "enabled": False,
                "should_run": False,
                "start_date": None,
                "start_time": None,
                "end_date": None,
                "end_time": None
            }
        
        # 计算当前是否应该运行
        now = datetime.now()
        start_date = cmd.start_date or now.strftime('%Y-%m-%d')
        end_date = cmd.end_date or now.strftime('%Y-%m-%d')
        
        try:
            start_dt = datetime.strptime(f'{start_date} {cmd.start_time}', '%Y-%m-%d %H:%M')
            end_dt = datetime.strptime(f'{end_date} {cmd.end_time}', '%Y-%m-%d %H:%M')
            should_run = start_dt <= now <= end_dt
        except:
            should_run = False
        
        return {
            "success": True,
            "enabled": cmd.enabled,
            "should_run": should_run,
            "start_date": cmd.start_date,
            "start_time": cmd.start_time,
            "end_date": cmd.end_date,
            "end_time": cmd.end_time
        }
    finally:
        db.close()


@router.get("/commands", response_model=CommandsResponse)
def get_mcu_commands():
    """
    MCU 轮询获取所有命令（兼容旧版）

    返回格式:
    {
        "success": true,
        "commands": [
            {"device": "humidifier", "action": "on", "type": "manual"},
            {"device": "humidifier", "action": "level_2", "type": "level"},
            {"device": "humidifier", "action": "on", "type": "schedule"}
        ]
    }
    """
    db = SessionLocal()
    try:
        commands = db.query(DeviceCommand).filter_by(executed=False, enabled=True).all()
        result = []

        for cmd in commands:
            if cmd.command == 'schedule':
                now = datetime.now()
                start_date = cmd.start_date or now.strftime('%Y-%m-%d')
                end_date = cmd.end_date or now.strftime('%Y-%m-%d')

                try:
                    start_dt = datetime.strptime(f'{start_date} {cmd.start_time}', '%Y-%m-%d %H:%M')
                    end_dt = datetime.strptime(f'{end_date} {cmd.end_time}', '%Y-%m-%d %H:%M')
                    action = 'on' if start_dt <= now <= end_dt else 'off'
                except Exception:
                    action = 'off'

                result.append({'device': cmd.device, 'action': action, 'type': 'schedule'})
            elif cmd.command.startswith('level_'):
                result.append({'device': cmd.device, 'action': cmd.command, 'type': 'level'})
                cmd.executed = True
            else:
                result.append({'device': cmd.device, 'action': cmd.command, 'type': 'manual'})
                cmd.executed = True

        db.commit()
        return {"success": True, "commands": result}
    finally:
        db.close()
