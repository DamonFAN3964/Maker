"""
MCU 模拟脚本 - FastAPI 版本
"""

import requests
import random
import time

# 服务器地址（FastAPI 默认端口 8000）
BASE_URL = 'http://localhost:8000'
UPLOAD_URL = f'{BASE_URL}/api/upload'
COMMAND_URL = f'{BASE_URL}/api/mcu/commands'

humidifier_on = False


def upload_sensor_data():
    """上传传感器数据"""
    try:
        data = {
            'temperature': round(random.uniform(18.0, 32.0), 1),
            'humidity': round(random.uniform(40.0, 80.0), 1)
        }
        response = requests.post(UPLOAD_URL, json=data, timeout=5)
        if response.status_code == 200:
            print(f'📤 上传成功 | 温度: {data["temperature"]}℃ | 湿度: {data["humidity"]}%')
        else:
            print(f'❌ 上传失败 | {response.status_code}')
    except requests.exceptions.ConnectionError:
        print('⚠️  连接失败')
    except Exception as e:
        print(f'❌ 错误: {e}')


def poll_commands():
    """轮询控制命令"""
    global humidifier_on
    try:
        response = requests.get(COMMAND_URL, timeout=5)
        if response.status_code == 200:
            result = response.json()
            for cmd in result.get('commands', []):
                if cmd['device'] == 'humidifier':
                    action = cmd['action']
                    if action == 'on' and not humidifier_on:
                        humidifier_on = True
                        print(f'💨 加湿器已开启 [{cmd["type"]}]')
                    elif action == 'off' and humidifier_on:
                        humidifier_on = False
                        print(f'🔇 加湿器已关闭 [{cmd["type"]}]')
    except:
        pass


def main():
    print('=' * 50)
    print('🔌 MCU 模拟器 (FastAPI)')
    print(f'📡 服务器: {BASE_URL}')
    print('=' * 50)
    
    last_upload = 0
    last_poll = 0
    
    while True:
        now = time.time()
        if now - last_upload >= 5:
            upload_sensor_data()
            last_upload = now
        if now - last_poll >= 2:
            poll_commands()
            last_poll = now
        time.sleep(0.5)


if __name__ == '__main__':
    main()
