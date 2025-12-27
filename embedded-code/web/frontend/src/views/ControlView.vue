<template>
  <div class="control">
    <div class="card">
      <div class="card-header">
        <span class="card-icon">💨</span>
        <span class="card-title">加湿器控制</span>
        <div class="status-badge" :class="{ active: status.power }">
          {{ status.power ? '运行中' : '已关闭' }}
        </div>
      </div>

      <!-- 电源开关 -->
      <div class="control-row">
        <span class="row-label">电源</span>
        <div class="seg-control">
          <button 
            class="seg-btn" 
            :class="{ active: status.power }"
            @click="togglePower(true)"
          >开启</button>
          <button 
            class="seg-btn" 
            :class="{ active: !status.power }"
            @click="togglePower(false)"
          >关闭</button>
        </div>
      </div>

      <!-- 档位选择 -->
      <div class="control-row" :class="{ disabled: !status.power }">
        <span class="row-label">档位</span>
        <div class="level-selector">
          <button 
            v-for="l in 3" 
            :key="l"
            class="level-btn"
            :class="{ active: status.level === l }"
            :disabled="!status.power"
            @click="setLevel(l)"
          >
            {{ l }}
          </button>
        </div>
      </div>

      <div class="level-desc" :class="{ disabled: !status.power }">
        <span v-if="status.level === 1">💧 轻柔模式 - 静音运行</span>
        <span v-else-if="status.level === 2">💧💧 标准模式 - 日常使用</span>
        <span v-else-if="status.level === 3">💧💧💧 强力模式 - 快速加湿</span>
      </div>

      <!-- 定时开关 -->
      <div class="control-row" :class="{ disabled: !status.power }">
        <span class="row-label">定时运行</span>
        <label class="ios-switch">
          <input type="checkbox" v-model="schedule.enabled" :disabled="!status.power" @change="saveSchedule" />
          <span class="slider"></span>
        </label>
      </div>

      <!-- 时间设置 -->
      <div class="datetime-section" v-if="schedule.enabled && status.power">
        <div class="picker-row">
          <span class="picker-label">开始日期</span>
          <input type="date" v-model="schedule.startDate" class="native-input" />
        </div>
        <div class="picker-row">
          <span class="picker-label">开始时间</span>
          <input type="time" v-model="schedule.startTime" class="native-input" />
        </div>
        <div class="picker-row">
          <span class="picker-label">结束日期</span>
          <input type="date" v-model="schedule.endDate" class="native-input" />
        </div>
        <div class="picker-row">
          <span class="picker-label">结束时间</span>
          <input type="time" v-model="schedule.endTime" class="native-input" />
        </div>

        <button class="save-btn" @click="saveSchedule" :disabled="loading">
          保存定时设置
        </button>

        <!-- 定时状态 -->
        <div class="remaining-box" v-if="status.schedule?.is_active">
          <div class="remaining-icon">⏱️</div>
          <div class="remaining-text">
            <span class="remaining-label">定时运行中</span>
            <span class="remaining-value">{{ remainingTime }}</span>
          </div>
        </div>
      </div>
    </div>

    <!-- Toast -->
    <div class="toast" :class="{ error: isError }" v-if="message">{{ message }}</div>
  </div>
</template>

<script setup>
import { ref, reactive, computed, onMounted, onUnmounted } from 'vue'
import api from '@/api'

const loading = ref(false)
const message = ref('')
const currentTime = ref(new Date())
let timer = null

// 获取默认时间（当前时间 + 4小时后结束）
function getDefaultSchedule() {
  const now = new Date()
  const endTime = new Date(now.getTime() + 4 * 60 * 60 * 1000) // 4小时后
  
  const formatDate = (d) => d.toISOString().split('T')[0]
  const formatTime = (d) => d.toTimeString().slice(0, 5) // HH:MM
  
  return {
    startDate: formatDate(now),
    startTime: formatTime(now),
    endDate: formatDate(endTime),
    endTime: formatTime(endTime)
  }
}

const defaultSchedule = getDefaultSchedule()

// 加湿器状态
const status = reactive({
  power: false,
  level: 1,
  schedule: null
})

// 定时配置
const schedule = reactive({
  enabled: false,
  startDate: defaultSchedule.startDate,
  startTime: defaultSchedule.startTime,
  endDate: defaultSchedule.endDate,
  endTime: defaultSchedule.endTime
})

// 剩余时间计算
const remainingTime = computed(() => {
  if (!status.schedule?.is_active) return ''
  const now = currentTime.value
  const end = new Date(`${status.schedule.end_date}T${status.schedule.end_time}`)
  let diff = end - now
  if (diff < 0) return '已结束'
  
  const hours = Math.floor(diff / (1000 * 60 * 60))
  const minutes = Math.floor((diff % (1000 * 60 * 60)) / (1000 * 60))
  return hours > 0 ? `${hours}小时${minutes}分钟` : `${minutes}分钟`
})

// 获取状态
async function fetchStatus() {
  try {
    const res = await api.getHumidifierStatus()
    if (res.data.success) {
      const s = res.data.status
      status.power = s.power
      status.level = s.level
      status.schedule = s.schedule
      
      if (s.schedule && s.schedule.enabled) {
        // 只有当服务器有启用的定时配置时才使用服务器的值
        schedule.enabled = s.schedule.enabled
        schedule.startDate = s.schedule.start_date || schedule.startDate
        schedule.startTime = s.schedule.start_time || schedule.startTime
        schedule.endDate = s.schedule.end_date || schedule.endDate
        schedule.endTime = s.schedule.end_time || schedule.endTime
      } else {
        // 否则重置为当前时间的默认值
        const defaults = getDefaultSchedule()
        schedule.enabled = false
        schedule.startDate = defaults.startDate
        schedule.startTime = defaults.startTime
        schedule.endDate = defaults.endDate
        schedule.endTime = defaults.endTime
      }
    }
  } catch (e) {
    console.error('获取状态失败:', e)
  }
}

// 开关电源
async function togglePower(on) {
  loading.value = true
  try {
    await api.toggleHumidifier(on ? 'on' : 'off')
    status.power = on
    showMessage(on ? '已开启' : '已关闭')
  } catch {
    showMessage('操作失败')
  } finally {
    loading.value = false
  }
}

// 设置档位
async function setLevel(level) {
  loading.value = true
  try {
    await api.setHumidifierLevel(level)
    status.level = level
    showMessage(`已设置为 ${level} 档`)
  } catch {
    showMessage('设置失败')
  } finally {
    loading.value = false
  }
}

// 验证时间是否在过去
function validateScheduleTime() {
  const now = new Date()
  const startDateTime = new Date(`${schedule.startDate}T${schedule.startTime}`)
  const endDateTime = new Date(`${schedule.endDate}T${schedule.endTime}`)
  
  // 允许1分钟的误差
  const tolerance = 60 * 1000
  
  if (startDateTime.getTime() < now.getTime() - tolerance) {
    return { valid: false, error: '开始时间不能是过去的时间' }
  }
  
  if (endDateTime.getTime() <= startDateTime.getTime()) {
    return { valid: false, error: '结束时间必须晚于开始时间' }
  }
  
  return { valid: true }
}

// 保存定时
async function saveSchedule() {
  // 如果启用定时，先验证时间
  if (schedule.enabled) {
    const validation = validateScheduleTime()
    if (!validation.valid) {
      showMessage(validation.error, true)
      return
    }
  }
  
  loading.value = true
  try {
    await api.setSchedule({
      start_date: schedule.startDate,
      start_time: schedule.startTime,
      end_date: schedule.endDate,
      end_time: schedule.endTime,
      enabled: schedule.enabled
    })
    showMessage('定时设置已保存')
    fetchStatus()
  } catch {
    showMessage('保存失败', true)
  } finally {
    loading.value = false
  }
}

const isError = ref(false)

function showMessage(msg, error = false) {
  message.value = msg
  isError.value = error
  setTimeout(() => { message.value = ''; isError.value = false }, 2500)
}

onMounted(() => {
  fetchStatus()
  timer = setInterval(() => { currentTime.value = new Date() }, 1000)
})

onUnmounted(() => {
  if (timer) clearInterval(timer)
})
</script>

<style scoped>
.level-selector {
  display: flex;
  gap: 10px;
}

.level-btn {
  width: 50px;
  height: 50px;
  border: 2px solid var(--glass-border);
  background: var(--glass-bg);
  backdrop-filter: blur(10px);
  border-radius: 14px;
  font-size: 20px;
  font-weight: 700;
  color: var(--text-primary);
  cursor: pointer;
  transition: all 0.3s ease;
  position: relative;
  overflow: hidden;
}

.level-btn:nth-child(1).active {
  border-color: transparent;
  background: linear-gradient(135deg, #48dbfb, #54a0ff);
  color: white;
  box-shadow: 0 6px 20px rgba(72, 219, 251, 0.5);
}

.level-btn:nth-child(2).active {
  border-color: transparent;
  background: linear-gradient(135deg, #feca57, #ff9f43);
  color: white;
  box-shadow: 0 6px 20px rgba(254, 202, 87, 0.5);
}

.level-btn:nth-child(3).active {
  border-color: transparent;
  background: linear-gradient(135deg, #ff6b6b, #ee5a24);
  color: white;
  box-shadow: 0 6px 20px rgba(255, 107, 107, 0.5);
}

.level-btn:active:not(:disabled) {
  transform: scale(0.9);
}

.level-btn:hover:not(:disabled):not(.active) {
  transform: translateY(-2px);
  border-color: var(--accent-solid);
}

.level-btn:disabled {
  opacity: 0.4;
  cursor: not-allowed;
}

.level-desc {
  padding: 14px 20px;
  font-size: 15px;
  color: var(--text-secondary);
  border-bottom: 1px solid var(--separator);
  background: linear-gradient(90deg, rgba(102, 126, 234, 0.03), transparent);
  font-weight: 500;
}

.status-badge {
  font-size: 12px;
  padding: 6px 14px;
  border-radius: 20px;
  background: var(--glass-bg);
  border: 1px solid var(--glass-border);
  color: var(--text-secondary);
  font-weight: 600;
  transition: all 0.3s ease;
}

.status-badge.active {
  background: linear-gradient(135deg, #00d9a5, #00b894);
  border-color: transparent;
  color: white;
  box-shadow: 0 4px 15px rgba(0, 217, 165, 0.4);
  animation: pulse 2s ease-in-out infinite;
}

@keyframes pulse {
  0%, 100% { transform: scale(1); }
  50% { transform: scale(1.05); }
}

.toast.error {
  background: linear-gradient(135deg, #ff6b6b, #ee5a24);
  box-shadow: 0 10px 30px rgba(255, 107, 107, 0.5);
}

.control-row.disabled,
.level-desc.disabled {
  opacity: 0.4;
  pointer-events: none;
}

.ios-switch input:disabled + .slider {
  opacity: 0.4;
  cursor: not-allowed;
}
</style>
