"""
豆包 TTS 语音合成 Demo
使用火山引擎语音合成 API
文档: https://www.volcengine.com/docs/6561/79823
"""

import base64
import uuid
import requests

# ==================== 配置区域 ====================
APP_ID = "3961637282"
ACCESS_TOKEN = "_WQPPl8SFZYpUgzf29piJq7DlWJ93KlA"

# TTS 配置
VOICE_TYPE = "BV700_streaming"

# 音频格式配置
AUDIO_FORMAT = "pcm"  # 可选: pcm, mp3, wav, ogg_opus
SAMPLE_RATE = 16000   # PCM 采样率: 8000, 16000, 24000

API_URL = "https://openspeech.bytedance.com/api/v1/tts"


# ==================== TTS 函数 ====================
def text_to_speech(text, output_file="output.pcm", encoding="pcm", sample_rate=16000):
    """
    将文本转换为语音
    
    Args:
        text: 要合成的文本
        output_file: 输出音频文件路径
        encoding: 音频格式 (pcm, mp3, wav, ogg_opus)
        sample_rate: 采样率 (仅 pcm 格式有效)
    
    Returns:
        bool: 是否成功
        bytes: 音频数据 (成功时返回)
    """
    
    headers = {
        "Content-Type": "application/json",
        "Authorization": f"Bearer; {ACCESS_TOKEN}"
    }
    
    # 构建音频配置
    audio_config = {
        "voice_type": VOICE_TYPE,
        "encoding": encoding,
        "speed_ratio": 1.0,
        "volume_ratio": 1.0,
        "pitch_ratio": 1.0
    }
    
    # PCM 格式需要指定采样率
    if encoding == "pcm":
        audio_config["sample_rate"] = sample_rate
    
    payload = {
        "app": {
            "appid": APP_ID,
            "token": ACCESS_TOKEN,
            "cluster": "volcano_tts"
        },
        "user": {
            "uid": "demo_user"
        },
        "audio": audio_config,
        "request": {
            "reqid": str(uuid.uuid4()),
            "text": text,
            "text_type": "plain",
            "operation": "query"
        }
    }
    
    try:
        print(f"🎤 正在合成: {text[:30]}...")
        print(f"   格式: {encoding}, 采样率: {sample_rate}Hz")
        
        response = requests.post(API_URL, headers=headers, json=payload, timeout=30)
        result = response.json()
        
        if result.get("code") == 3000:
            audio_data = base64.b64decode(result["data"])
            
            with open(output_file, "wb") as f:
                f.write(audio_data)
            
            print(f"✅ 合成成功! 已保存到: {output_file}")
            print(f"   文件大小: {len(audio_data)} bytes")
            return True, audio_data
        else:
            print(f"❌ 合成失败: {result.get('message', '未知错误')}")
            print(f"   错误码: {result.get('code')}")
            return False, None
            
    except Exception as e:
        print(f"❌ 错误: {e}")
        return False, None


def text_to_pcm(text, output_file="output.pcm", sample_rate=16000):
    """
    将文本转换为 PCM 格式音频
    
    Args:
        text: 要合成的文本
        output_file: 输出文件路径
        sample_rate: 采样率 (8000, 16000, 24000)
    
    Returns:
        bool: 是否成功
        bytes: PCM 音频数据
    """
    return text_to_speech(text, output_file, encoding="pcm", sample_rate=sample_rate)


def get_pcm_bytes(text, sample_rate=16000):
    """
    获取 PCM 音频字节数据（不保存文件）
    适合直接发送给 MCU 或音频设备
    
    Args:
        text: 要合成的文本
        sample_rate: 采样率
    
    Returns:
        bytes: PCM 音频数据，失败返回 None
    """
    headers = {
        "Content-Type": "application/json",
        "Authorization": f"Bearer; {ACCESS_TOKEN}"
    }
    
    payload = {
        "app": {
            "appid": APP_ID,
            "token": ACCESS_TOKEN,
            "cluster": "volcano_tts"
        },
        "user": {
            "uid": "demo_user"
        },
        "audio": {
            "voice_type": VOICE_TYPE,
            "encoding": "pcm",
            "sample_rate": sample_rate,
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
    
    try:
        response = requests.post(API_URL, headers=headers, json=payload, timeout=30)
        result = response.json()
        
        if result.get("code") == 3000:
            return base64.b64decode(result["data"])
        else:
            print(f"❌ TTS 失败: {result.get('message')}")
            return None
    except Exception as e:
        print(f"❌ 错误: {e}")
        return None


# ==================== 测试入口 ====================
if __name__ == "__main__":
    print("=" * 50)
    print("🔊 豆包 TTS 语音合成 Demo (PCM 格式)")
    print("=" * 50)
    
    # 测试 1: PCM 格式 16kHz
    print("\n📝 测试 1: PCM 格式 (16kHz)")
    text_to_pcm(
        "你好，我是寝室智能终端的语音助手。",
        "test_16k.pcm",
        sample_rate=16000
    )
    
    # 测试 2: PCM 格式 8kHz
    print("\n📝 测试 2: PCM 格式 (8kHz)")
    text_to_pcm(
        "当前室内温度25度，湿度60%。",
        "test_8k.pcm",
        sample_rate=8000
    )
    
    # 测试 3: 获取 PCM 字节数据
    print("\n📝 测试 3: 获取 PCM 字节数据")
    pcm_data = get_pcm_bytes("环境舒适", sample_rate=16000)
    if pcm_data:
        print(f"✅ 获取成功，数据长度: {len(pcm_data)} bytes")
    
    print("\n✨ 测试完成!")
    print("\n💡 PCM 播放方法:")
    print("   ffplay -f s16le -ar 16000 -ac 1 test_16k.pcm")
    print("   或转换为 WAV:")
    print("   ffmpeg -f s16le -ar 16000 -ac 1 -i test_16k.pcm output.wav")
