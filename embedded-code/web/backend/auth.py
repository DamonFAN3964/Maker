"""
认证模块
"""
import hashlib
import secrets
from datetime import datetime, timedelta
from fastapi import Depends, HTTPException
from fastapi.security import HTTPBearer, HTTPAuthorizationCredentials

# 初始用户配置
USERS = {
    "thethirdsilverbullet": hashlib.sha256("123123123".encode()).hexdigest()
}

# Token 存储 (生产环境建议用 Redis)
active_tokens = {}

security = HTTPBearer(auto_error=False)


def create_token(username: str) -> str:
    """生成登录 token"""
    token = secrets.token_urlsafe(32)
    active_tokens[token] = {
        "username": username,
        "created_at": datetime.now(),
        "expires_at": datetime.now() + timedelta(days=7)
    }
    return token


def verify_token(credentials: HTTPAuthorizationCredentials = Depends(security)):
    """验证 token（可选认证）"""
    if not credentials:
        return None
    
    token = credentials.credentials
    if token not in active_tokens:
        return None
    
    token_data = active_tokens[token]
    if datetime.now() > token_data["expires_at"]:
        del active_tokens[token]
        return None
    
    return token_data["username"]


def require_auth(credentials: HTTPAuthorizationCredentials = Depends(security)):
    """强制要求认证"""
    if not credentials:
        raise HTTPException(status_code=401, detail="未登录")
    
    token = credentials.credentials
    if token not in active_tokens:
        raise HTTPException(status_code=401, detail="Token 无效")
    
    token_data = active_tokens[token]
    if datetime.now() > token_data["expires_at"]:
        del active_tokens[token]
        raise HTTPException(status_code=401, detail="Token 已过期")
    
    return token_data["username"]
