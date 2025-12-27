"""
测试音频 API Demo
调用服务器 API 获取"欢迎"语音并保存/播放
"""

import requests
import subprocess
import sys

# 服务器地址
SERVER_URL = "http://localhost:8000"


def test_welcome_audio():
    """获取欢迎语音"""
    print("=" * 50)
    print("🎵 测试音频 API - 欢迎语音")
    print("=" * 50)
    
    url = f"{SERVER_URL}/api/audio/welcome"
    print(f"📡 请求: {url}")
    
    try:
        response = requests.get(url, timeout=30)
        
        if response.status_code == 200:
            pcm_data = response.content
            
            # 打印音频信息
            print(f"✅ 获取成功!")
            print(f"   数据大小: {len(pcm_data)} bytes")
            print(f"   采样率: {response.headers.get('X-Sample-Rate', '44100')} Hz")
            print(f"   声道数: {response.headers.get('X-Channels', '2')}")
            print(f"   位深度: {response.headers.get('X-Bits-Per-Sample', '16')} bit")
            
            # 保存文件
            output_file = "welcome.pcm"
            with open(output_file, "wb") as f:
                f.write(pcm_data)
            print(f"   已保存到: {output_file}")
            
            # 计算时长
            sample_rate = 44100
            channels = 2
            bits = 16
            duration = len(pcm_data) / (sample_rate * channels * (bits // 8))
            print(f"   音频时长: {duration:.2f} 秒")
            
            return pcm_data
        else:
            print(f"❌ 请求失败: {response.status_code}")
            print(f"   {response.text}")
            return None
            
    except requests.exceptions.ConnectionError:
        print("❌ 连接失败，请确保服务器已启动")
        print(f"   启动命令: cd backend && python main.py")
        return None
    except Exception as e:
        print(f"❌ 错误: {e}")
        return None


def test_custom_tts():
    """测试自定义文本 TTS"""
    print("\n" + "=" * 50)
    print("🎵 测试自定义 TTS")
    print("=" * 50)
    
    text = "你好，欢迎使用寝室智能终端"
    url = f"{SERVER_URL}/api/tts"
    
    print(f"📡 请求: {url}")
    print(f"   文本: {text}")
    
    try:
        response = requests.get(url, params={"text": text}, timeout=30)
        
        if response.status_code == 200:
            pcm_data = response.content
            print(f"✅ 获取成功! 数据大小: {len(pcm_data)} bytes")
            
            output_file = "custom_tts.pcm"
            with open(output_file, "wb") as f:
                f.write(pcm_data)
            print(f"   已保存到: {output_file}")
            return pcm_data
        else:
            print(f"❌ 请求失败: {response.status_code}")
            return None
    except Exception as e:
        print(f"❌ 错误: {e}")
        return None


def test_audio_list():
    """获取所有预设音频列表"""
    print("\n" + "=" * 50)
    print("🎵 预设音频列表")
    print("=" * 50)
    
    url = f"{SERVER_URL}/api/audio/list"
    
    try:
        response = requests.get(url, timeout=10)
        if response.status_code == 200:
            data = response.json()
            print("可用音频:")
            for item in data.get("audio", []):
                print(f"   - {item['id']}: {item['text']}")
        else:
            print(f"❌ 请求失败: {response.status_code}")
    except Exception as e:
        print(f"❌ 错误: {e}")


def play_pcm(filename):
    """使用 ffplay 播放 PCM 文件"""
    print(f"\n🔊 播放: {filename}")
    try:
        subprocess.run([
            "ffplay", "-f", "s16le", "-ar", "44100", "-ac", "2",
            "-nodisp", "-autoexit", filename
        ], check=True)
    except FileNotFoundError:
        print("⚠️  ffplay 未安装，请手动播放:")
        print(f"   ffplay -f s16le -ar 44100 -ac 2 {filename}")
    except Exception as e:
        print(f"❌ 播放失败: {e}")


if __name__ == "__main__":
    # 1. 获取预设音频列表
    test_audio_list()
    
    # 2. 获取欢迎语音
    pcm_data = test_welcome_audio()
    
    # 3. 测试自定义 TTS
    test_custom_tts()
    
    # 4. 播放（如果有 ffplay）
    if pcm_data and len(sys.argv) > 1 and sys.argv[1] == "--play":
        play_pcm("welcome.pcm")
    
    print("\n✨ 测试完成!")
    print("\n💡 播放命令:")
    print("   ffplay -f s16le -ar 44100 -ac 2 welcome.pcm")
