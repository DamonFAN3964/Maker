<template>
  <div class="login-page">
    <div class="login-card">
      <div class="login-header">
        <div class="logo">🏠</div>
        <h1>智能寝室</h1>
        <p>请登录以继续</p>
      </div>

      <form @submit.prevent="handleLogin" class="login-form">
        <div class="input-group">
          <label>用户名</label>
          <input 
            type="text" 
            v-model="username" 
            placeholder="请输入用户名"
            autocomplete="username"
          />
        </div>

        <div class="input-group">
          <label>密码</label>
          <input 
            type="password" 
            v-model="password" 
            placeholder="请输入密码"
            autocomplete="current-password"
          />
        </div>

        <button type="submit" class="login-btn" :disabled="loading">
          {{ loading ? '登录中...' : '登录' }}
        </button>

        <p class="error-msg" v-if="error">{{ error }}</p>
      </form>
    </div>
  </div>
</template>

<script setup>
import { ref } from 'vue'
import { useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'

const router = useRouter()
const authStore = useAuthStore()

const username = ref('')
const password = ref('')
const loading = ref(false)
const error = ref('')

async function handleLogin() {
  if (!username.value || !password.value) {
    error.value = '请输入用户名和密码'
    return
  }

  loading.value = true
  error.value = ''

  try {
    await authStore.login(username.value, password.value)
    router.push('/')
  } catch (e) {
    error.value = e.response?.data?.detail || '登录失败'
  } finally {
    loading.value = false
  }
}
</script>

<style scoped>
.login-page {
  min-height: 100vh;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 20px;
  position: relative;
  overflow: hidden;
}

/* 彩虹背景 */
.login-page::before {
  content: '';
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: 
    linear-gradient(125deg, 
      #ff6b6b 0%, 
      #feca57 14%, 
      #48dbfb 28%, 
      #00d9a5 42%, 
      #54a0ff 57%, 
      #ff9ff3 71%, 
      #667eea 85%, 
      #ff6b6b 100%
    );
  background-size: 400% 400%;
  animation: rainbowWave 12s ease infinite;
  z-index: -2;
}

.login-page::after {
  content: '';
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: 
    radial-gradient(ellipse at 20% 30%, rgba(255, 255, 255, 0.3) 0%, transparent 50%),
    radial-gradient(ellipse at 80% 70%, rgba(255, 255, 255, 0.25) 0%, transparent 50%),
    radial-gradient(ellipse at 50% 50%, rgba(255, 255, 255, 0.2) 0%, transparent 60%);
  animation: floatGlow 15s ease-in-out infinite;
  pointer-events: none;
  z-index: -1;
}

@keyframes rainbowWave {
  0% { background-position: 0% 50%; }
  25% { background-position: 50% 100%; }
  50% { background-position: 100% 50%; }
  75% { background-position: 50% 0%; }
  100% { background-position: 0% 50%; }
}

@keyframes floatGlow {
  0%, 100% { transform: scale(1); opacity: 0.8; }
  50% { transform: scale(1.1); opacity: 1; }
}

.login-card {
  width: 100%;
  max-width: 380px;
  background: rgba(255, 255, 255, 0.3);
  backdrop-filter: blur(25px);
  -webkit-backdrop-filter: blur(25px);
  border: 2px solid rgba(255, 255, 255, 0.5);
  border-radius: 28px;
  padding: 40px 28px;
  box-shadow: 
    0 25px 50px rgba(0, 0, 0, 0.15),
    inset 0 1px 1px rgba(255, 255, 255, 0.6);
  position: relative;
  z-index: 1;
}

.login-header {
  text-align: center;
  margin-bottom: 36px;
}

.logo {
  font-size: 64px;
  margin-bottom: 16px;
  animation: bounce 2s ease-in-out infinite;
  filter: drop-shadow(0 4px 8px rgba(0,0,0,0.2));
}

@keyframes bounce {
  0%, 100% { transform: translateY(0); }
  50% { transform: translateY(-10px); }
}

.login-header h1 {
  font-size: 30px;
  font-weight: 800;
  margin-bottom: 10px;
  background: linear-gradient(135deg, #ff6b6b, #feca57, #48dbfb, #ff9ff3, #54a0ff, #667eea);
  background-size: 300% 300%;
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  background-clip: text;
  animation: rainbow 4s ease infinite;
}

@keyframes rainbow {
  0% { background-position: 0% 50%; }
  50% { background-position: 100% 50%; }
  100% { background-position: 0% 50%; }
}

.login-header p {
  color: rgba(255, 255, 255, 0.9);
  font-size: 15px;
  font-weight: 600;
  text-shadow: 0 1px 2px rgba(0,0,0,0.1);
}

.login-form {
  display: flex;
  flex-direction: column;
  gap: 22px;
}

.input-group {
  display: flex;
  flex-direction: column;
  gap: 10px;
}

.input-group label {
  font-size: 14px;
  font-weight: 700;
  color: rgba(255, 255, 255, 0.95);
  text-shadow: 0 1px 2px rgba(0,0,0,0.1);
}

.input-group input {
  padding: 16px 18px;
  border: 2px solid rgba(255, 255, 255, 0.4);
  border-radius: 14px;
  font-size: 16px;
  background: rgba(255, 255, 255, 0.25);
  color: #333;
  transition: all 0.3s ease;
  font-weight: 500;
}

.input-group input::placeholder {
  color: rgba(100, 100, 100, 0.6);
}

.input-group input:focus {
  outline: none;
  border-color: rgba(255, 255, 255, 0.8);
  background: rgba(255, 255, 255, 0.4);
  box-shadow: 0 0 0 4px rgba(255, 255, 255, 0.2);
}

.login-btn {
  padding: 16px;
  background: linear-gradient(135deg, #ff6b6b, #feca57, #48dbfb);
  background-size: 200% 200%;
  color: white;
  border: none;
  border-radius: 14px;
  font-size: 17px;
  font-weight: 700;
  cursor: pointer;
  transition: all 0.3s ease;
  margin-top: 10px;
  box-shadow: 0 8px 25px rgba(255, 107, 107, 0.4);
  position: relative;
  overflow: hidden;
  animation: btnGradient 3s ease infinite;
}

@keyframes btnGradient {
  0% { background-position: 0% 50%; }
  50% { background-position: 100% 50%; }
  100% { background-position: 0% 50%; }
}

.login-btn::before {
  content: '';
  position: absolute;
  top: 0;
  left: -100%;
  width: 100%;
  height: 100%;
  background: linear-gradient(90deg, transparent, rgba(255,255,255,0.4), transparent);
  transition: left 0.5s ease;
}

.login-btn:hover:not(:disabled)::before {
  left: 100%;
}

.login-btn:hover:not(:disabled) {
  transform: translateY(-3px) scale(1.02);
  box-shadow: 0 12px 35px rgba(255, 107, 107, 0.5);
}

.login-btn:disabled {
  opacity: 0.6;
  cursor: not-allowed;
  transform: none;
}

.error-msg {
  color: white;
  font-size: 14px;
  text-align: center;
  font-weight: 600;
  background: rgba(255, 107, 107, 0.8);
  padding: 12px;
  border-radius: 12px;
  box-shadow: 0 4px 15px rgba(255, 107, 107, 0.3);
}
</style>
