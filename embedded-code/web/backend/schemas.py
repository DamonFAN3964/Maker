"""
Pydantic 数据模型
"""
from pydantic import BaseModel, Field
from typing import Optional, List


# ==================== 请求模型 ====================
class SensorData(BaseModel):
    temperature: float
    humidity: float


class ScheduleData(BaseModel):
    start_date: Optional[str] = None
    start_time: str
    end_date: Optional[str] = None
    end_time: str
    enabled: bool = True


class ToggleData(BaseModel):
    action: str  # 'on' or 'off'


class LevelData(BaseModel):
    level: int  # 档位 1-3


class LoginData(BaseModel):
    username: str
    password: str


class HumidifierConfig(BaseModel):
    """加湿器完整配置"""
    power: bool = True
    level: int = 1
    start_date: Optional[str] = None
    start_time: Optional[str] = None
    end_date: Optional[str] = None
    end_time: Optional[str] = None
    schedule_enabled: bool = False


# ==================== 响应模型 ====================
class HealthResponse(BaseModel):
    status: str
    timestamp: str


class TimeResponse(BaseModel):
    success: bool
    timestamp: str
    date: str
    time: str
    unix: int
    weekday: int = Field(..., ge=0, le=6)


class MessageResponse(BaseModel):
    success: bool
    message: str


class SensorDataItem(BaseModel):
    id: int
    temperature: float
    humidity: float
    timestamp: str


class SensorUploadResponse(BaseModel):
    success: bool
    message: str
    data: SensorDataItem


class SensorListResponse(BaseModel):
    success: bool
    data: List[SensorDataItem]


class ScheduleDataItem(BaseModel):
    id: int
    start_date: Optional[str]
    start_time: Optional[str]
    end_date: Optional[str]
    end_time: Optional[str]
    enabled: bool


class ScheduleResponse(BaseModel):
    success: bool
    data: Optional[ScheduleDataItem]


class LevelResponse(BaseModel):
    success: bool
    level: int


class LevelSetResponse(BaseModel):
    success: bool
    message: str
    level: int


class LoginResponse(BaseModel):
    success: bool
    message: str
    token: str
    username: str
    expires_in: int


class UserResponse(BaseModel):
    success: bool
    username: str


class ScheduleStatus(BaseModel):
    enabled: bool
    start_date: Optional[str]
    start_time: Optional[str]
    end_date: Optional[str]
    end_time: Optional[str]
    is_active: bool


class HumidifierStatusData(BaseModel):
    power: bool
    level: int
    schedule: Optional[ScheduleStatus]


class HumidifierStatusResponse(BaseModel):
    success: bool
    status: HumidifierStatusData


class ConfigResponse(BaseModel):
    success: bool
    message: str
    config: dict


class AudioItem(BaseModel):
    id: str
    text: str


class AudioListResponse(BaseModel):
    success: bool
    audio: List[AudioItem]


class CommandItem(BaseModel):
    device: str
    action: str
    type: str


class CommandsResponse(BaseModel):
    success: bool
    commands: List[CommandItem]


# ==================== MCU 独立命令响应模型 ====================
class McuPowerResponse(BaseModel):
    """MCU 开关命令响应"""
    success: bool
    has_command: bool
    action: Optional[str] = None  # 'on' or 'off'


class McuLevelResponse(BaseModel):
    """MCU 档位命令响应"""
    success: bool
    has_command: bool
    level: Optional[int] = None  # 1-3


class McuScheduleResponse(BaseModel):
    """MCU 定时命令响应"""
    success: bool
    enabled: bool
    should_run: bool  # 当前时间是否应该运行
    start_date: Optional[str] = None
    start_time: Optional[str] = None
    end_date: Optional[str] = None
    end_time: Optional[str] = None
