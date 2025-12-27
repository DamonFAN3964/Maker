import { defineStore } from 'pinia'
import { ref, reactive } from 'vue'
import api from '@/api'

export const useHumidifierStore = defineStore('humidifier', () => {
  const loading = ref(false)
  const message = ref('')
  
  const schedule = reactive({
    startDate: new Date().toISOString().split('T')[0],
    startTime: '08:00',
    endDate: new Date().toISOString().split('T')[0],
    endTime: '22:00',
    enabled: false
  })

  async function fetchSchedule() {
    try {
      const res = await api.getSchedule()
      if (res.data.success && res.data.data) {
        const data = res.data.data
        schedule.startDate = data.start_date || schedule.startDate
        schedule.startTime = data.start_time || '08:00'
        schedule.endDate = data.end_date || schedule.endDate
        schedule.endTime = data.end_time || '22:00'
        schedule.enabled = data.enabled
      }
    } catch (e) {
      console.error('获取配置失败:', e)
    }
  }

  async function saveSchedule() {
    loading.value = true
    try {
      const res = await api.setSchedule({
        start_date: schedule.startDate,
        start_time: schedule.startTime,
        end_date: schedule.endDate,
        end_time: schedule.endTime,
        enabled: schedule.enabled
      })
      showMessage(res.data.success ? '设置已保存' : '保存失败')
    } catch {
      showMessage('保存失败')
    } finally {
      loading.value = false
    }
  }

  async function toggle(action) {
    loading.value = true
    try {
      const res = await api.toggleHumidifier(action)
      showMessage(res.data.message || (action === 'on' ? '已开启' : '已关闭'))
    } catch {
      showMessage('操作失败')
    } finally {
      loading.value = false
    }
  }

  function showMessage(msg) {
    message.value = msg
    setTimeout(() => { message.value = '' }, 2000)
  }

  return { schedule, loading, message, fetchSchedule, saveSchedule, toggle }
})
