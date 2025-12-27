"""
配置模块
"""
import os

# 数据库配置
DB_USER = os.environ.get('DB_USER', 'root')
DB_PASSWORD = os.environ.get('DB_PASSWORD', 'Zero0ts87')
DB_HOST = os.environ.get('DB_HOST', 'localhost')
DB_PORT = os.environ.get('DB_PORT', '3306')
DB_NAME = os.environ.get('DB_NAME', 'smart_dorm')

DATABASE_URL = f'mysql+pymysql://{DB_USER}:{DB_PASSWORD}@{DB_HOST}:{DB_PORT}/{DB_NAME}'

# TTS 配置
TTS_APP_ID = os.environ.get('TTS_APP_ID', '3961637282')
TTS_TOKEN = os.environ.get('TTS_TOKEN', '_WQPPl8SFZYpUgzf29piJq7DlWJ93KlA')
TTS_VOICE = os.environ.get('TTS_VOICE', 'BV700_streaming')
TTS_API_URL = "https://openspeech.bytedance.com/api/v1/tts"

# MCU 音频参数
MCU_SAMPLE_RATE = 44100
MCU_CHANNELS = 2

# 音频目录
AUDIO_DIR = os.path.join(os.path.dirname(__file__), 'audio')

# 预设音频
PRESET_AUDIO = {
    'welcome': '欢迎回到寝室',
    'goodbye': '再见，注意安全',
    'alert_temp_high': '警告，室内温度过高，请注意通风',
    'alert_temp_low': '警告，室内温度过低，请注意保暖',
    'alert_humidity_high': '警告，室内湿度过高',
    'alert_humidity_low': '警告，室内湿度过低，建议开启加湿器',
    'humidifier_on': '加湿器已开启',
    'humidifier_off': '加湿器已关闭',
    'morning': '早上好，祝你今天心情愉快',
    'night': '晚安，祝你好梦'
}
