"""
寝室智能终端 - FastAPI 后端

拆分后的入口文件：负责创建 app、配置中间件、挂载各业务路由。
"""

import os
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from config import AUDIO_DIR
from routers.auth_router import router as auth_router
from routers.sensor_router import router as sensor_router
from routers.humidifier_router import router as humidifier_router
from routers.mcu_router import router as mcu_router
from routers.audio_router import router as audio_router

app = FastAPI(title="寝室智能终端 API", version="2.0.0")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  # 生产环境请改为具体域名
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(auth_router)
app.include_router(sensor_router)
app.include_router(humidifier_router)
app.include_router(mcu_router)
app.include_router(audio_router)


if __name__ == '__main__':
    import uvicorn

    os.makedirs(AUDIO_DIR, exist_ok=True)
    port = int(os.environ.get('PORT', 8000))
    uvicorn.run("main:app", host="0.0.0.0", port=port, reload=True)

