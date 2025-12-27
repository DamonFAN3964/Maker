"""
音频/TTS 路由
"""
import os
from fastapi import APIRouter, HTTPException, Query
from fastapi.responses import Response
from database import SessionLocal, SensorLog
from schemas import AudioListResponse
from config import AUDIO_DIR, PRESET_AUDIO, MCU_SAMPLE_RATE, MCU_CHANNELS
from tts import get_tts_pcm, resample_mono_to_stereo_44100

router = APIRouter(prefix="/api", tags=["音频"])


@router.get("/tts")
def tts_endpoint(text: str = Query(..., description="要合成的文本")):
    """
    TTS 语音合成 API - 返回裸 PCM 数据
    
    返回格式:
    - 16-bit little-endian
    - 立体声 (2 channels)
    - 44100 Hz 采样率
    """
    if not text or len(text) > 500:
        raise HTTPException(status_code=400, detail="文本长度需在 1-500 字符之间")
    
    try:
        raw_pcm = get_tts_pcm(text, source_rate=24000)
        output_pcm = resample_mono_to_stereo_44100(raw_pcm, source_rate=24000)
        
        return Response(
            content=output_pcm,
            media_type="application/octet-stream",
            headers={
                "Content-Length": str(len(output_pcm)),
                "X-Sample-Rate": str(MCU_SAMPLE_RATE),
                "X-Channels": str(MCU_CHANNELS),
                "X-Bits-Per-Sample": "16"
            }
        )
    except HTTPException:
        raise
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))


@router.get("/tts/announce")
def tts_announce():
    """播报当前环境状态"""
    db = SessionLocal()
    try:
        log = db.query(SensorLog).order_by(SensorLog.timestamp.desc()).first()
        
        if log:
            text = f"当前室内温度{log.temperature:.1f}度，湿度{log.humidity:.1f}%"
        else:
            text = "暂无环境数据"
        
        raw_pcm = get_tts_pcm(text, source_rate=24000)
        output_pcm = resample_mono_to_stereo_44100(raw_pcm, source_rate=24000)
        
        return Response(
            content=output_pcm,
            media_type="application/octet-stream",
            headers={
                "Content-Length": str(len(output_pcm)),
                "X-Sample-Rate": str(MCU_SAMPLE_RATE),
                "X-Channels": str(MCU_CHANNELS),
                "X-Bits-Per-Sample": "16"
            }
        )
    finally:
        db.close()


@router.get("/audio/list", response_model=AudioListResponse)
def list_audio():
    """获取所有预设音频列表"""
    return {
        "success": True,
        "audio": [{"id": k, "text": v} for k, v in PRESET_AUDIO.items()]
    }


@router.get("/audio/{audio_id}")
def play_preset_audio(audio_id: str):
    """播放预设音频"""
    if audio_id not in PRESET_AUDIO:
        raise HTTPException(status_code=404, detail=f"音频 '{audio_id}' 不存在")
    
    text = PRESET_AUDIO[audio_id]
    cache_file = os.path.join(AUDIO_DIR, f'{audio_id}.pcm')
    
    if os.path.exists(cache_file):
        with open(cache_file, 'rb') as f:
            pcm_data = f.read()
    else:
        try:
            raw_pcm = get_tts_pcm(text, source_rate=24000)
            pcm_data = resample_mono_to_stereo_44100(raw_pcm, source_rate=24000)
            
            os.makedirs(AUDIO_DIR, exist_ok=True)
            with open(cache_file, 'wb') as f:
                f.write(pcm_data)
        except Exception as e:
            raise HTTPException(status_code=500, detail=str(e))
    
    return Response(
        content=pcm_data,
        media_type="application/octet-stream",
        headers={
            "Content-Length": str(len(pcm_data)),
            "X-Sample-Rate": str(MCU_SAMPLE_RATE),
            "X-Channels": str(MCU_CHANNELS),
            "X-Bits-Per-Sample": "16",
            "X-Audio-Id": audio_id
        }
    )


@router.get("/audio/file/{filename}")
def play_audio_file(filename: str):
    """播放指定的 PCM 音频文件"""
    if '..' in filename or filename.startswith('/'):
        raise HTTPException(status_code=400, detail="无效的文件名")
    
    file_path = os.path.join(AUDIO_DIR, filename)
    
    if not os.path.exists(file_path):
        raise HTTPException(status_code=404, detail=f"文件 '{filename}' 不存在")
    
    with open(file_path, 'rb') as f:
        pcm_data = f.read()
    
    return Response(
        content=pcm_data,
        media_type="application/octet-stream",
        headers={
            "Content-Length": str(len(pcm_data)),
            "X-Sample-Rate": str(MCU_SAMPLE_RATE),
            "X-Channels": str(MCU_CHANNELS),
            "X-Bits-Per-Sample": "16"
        }
    )
