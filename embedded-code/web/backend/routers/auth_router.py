"""
认证路由
"""
import hashlib
from fastapi import APIRouter, HTTPException, Depends
from schemas import LoginData, LoginResponse, MessageResponse, UserResponse
from auth import USERS, active_tokens, create_token, require_auth

router = APIRouter(prefix="/api/auth", tags=["认证"])


@router.post("/login", response_model=LoginResponse)
def login(data: LoginData):
    """用户登录"""
    password_hash = hashlib.sha256(data.password.encode()).hexdigest()
    
    if data.username not in USERS or USERS[data.username] != password_hash:
        raise HTTPException(status_code=401, detail="用户名或密码错误")
    
    token = create_token(data.username)
    return {
        "success": True,
        "message": "登录成功",
        "token": token,
        "username": data.username,
        "expires_in": 7 * 24 * 3600
    }


@router.post("/logout", response_model=MessageResponse)
def logout(username: str = Depends(require_auth)):
    """退出登录"""
    tokens_to_remove = [t for t, d in active_tokens.items() if d["username"] == username]
    for t in tokens_to_remove:
        del active_tokens[t]
    
    return {"success": True, "message": "已退出登录"}


@router.get("/me", response_model=UserResponse)
def get_current_user(username: str = Depends(require_auth)):
    """获取当前登录用户信息"""
    return {"success": True, "username": username}
