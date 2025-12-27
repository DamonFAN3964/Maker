"""
TTS 语音合成模块
"""
import base64
import uuid
import struct
import requests as http_requests
from fastapi import HTTPException
from config import TTS_APP_ID, TTS_TOKEN, TTS_VOICE, TTS_API_URL, MCU_SAMPLE_RATE


def get_tts_pcm(text: str, source_rate: int = 24000) -> bytes:
    """
    调用火山引擎 TTS API 获取 PCM 数据
    返回: 单声道 16-bit PCM，采样率为 source_rate
    """
    headers = {
        "Content-Type": "application/json",
        "Authorization": f"Bearer; {TTS_TOKEN}"
    }
    
    payload = {
        "app": {
            "appid": TTS_APP_ID,
            "token": TTS_TOKEN,
            "cluster": "volcano_tts"
        },
        "user": {"uid": "mcu_client"},
        "audio": {
            "voice_type": TTS_VOICE,
            "encoding": "pcm",
            "sample_rate": source_rate,
            "speed_ratio": 1.0,
            "volume_ratio": 1.0,
            "pitch_ratio": 1.0
        },
        "request": {
            "reqid": str(uuid.uuid4()),
            "text": text,
            "text_type": "plain",
            "operation": "query"
        }
    }
    
    response = http_requests.post(TTS_API_URL, headers=headers, json=payload, timeout=30)
    result = response.json()
    
    if result.get("code") == 3000:
        return base64.b64decode(result["data"])
    else:
        raise HTTPException(status_code=500, detail=f"TTS 失败: {result.get('message')}")


def resample_mono_to_stereo_44100(pcm_data: bytes, source_rate: int = 24000) -> bytes:
    """
    将单声道 PCM 重采样为 44100Hz 立体声
    输入: 16-bit little-endian 单声道
    输出: 16-bit little-endian 立体声 44100Hz
    """
    # 解析输入的 16-bit 样本
    sample_count = len(pcm_data) // 2
    samples = struct.unpack(f'<{sample_count}h', pcm_data)
    
    # 计算重采样比例
    ratio = MCU_SAMPLE_RATE / source_rate
    new_sample_count = int(sample_count * ratio)
    
    # 线性插值重采样
    resampled = []
    for i in range(new_sample_count):
        src_idx = i / ratio
        idx_low = int(src_idx)
        idx_high = min(idx_low + 1, sample_count - 1)
        frac = src_idx - idx_low
        
        # 线性插值
        sample = int(samples[idx_low] * (1 - frac) + samples[idx_high] * frac)
        resampled.append(sample)
    
    # 转换为立体声（左右声道相同）
    stereo_samples = []
    for sample in resampled:
        stereo_samples.append(sample)  # 左声道
        stereo_samples.append(sample)  # 右声道
    
    # 打包为 16-bit little-endian
    return struct.pack(f'<{len(stereo_samples)}h', *stereo_samples)
