import axios from 'axios'

const instance = axios.create({
  baseURL: import.meta.env.VITE_API_URL || '/api',
  timeout: 10000,
  headers: {
    'Content-Type': 'application/json'
  }
})

// 请求拦截器：自动添加 token
instance.interceptors.request.use(config => {
  const token = localStorage.getItem('token')
  if (token) {
    config.headers.Authorization = `Bearer ${token}`
  }
  return config
})

// 响应拦截器：处理 401 错误
instance.interceptors.response.use(
  response => response,
  error => {
    if (error.response?.status === 401) {
      localStorage.removeItem('token')
      localStorage.removeItem('username')
      window.location.href = '/login'
    }
    return Promise.reject(error)
  }
)

export default {
  // 认证
  login: (username, password) => instance.post('/auth/login', { username, password }),
  logout: () => instance.post('/auth/logout'),
  getMe: () => instance.get('/auth/me'),
  
  // 传感器数据
  getSensorData: (limit = 20) => instance.get(`/data?limit=${limit}`),
  
  // 加湿器控制
  getSchedule: () => instance.get('/humidifier/schedule'),
  setSchedule: (data) => instance.post('/humidifier/schedule', data),
  toggleHumidifier: (action) => instance.post('/humidifier/toggle', { action }),
  setHumidifierLevel: (level) => instance.post('/humidifier/level', { level }),
  getHumidifierLevel: () => instance.get('/humidifier/level'),
  getHumidifierStatus: () => instance.get('/humidifier/status'),
  setHumidifierConfig: (config) => instance.post('/humidifier/config', config),
  
  // 健康检查
  healthCheck: () => instance.get('/health')
}
