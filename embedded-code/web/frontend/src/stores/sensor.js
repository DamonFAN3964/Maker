import { defineStore } from 'pinia'
import { ref, computed } from 'vue'
import api from '@/api'

export const useSensorStore = defineStore('sensor', () => {
  const sensorData = ref([])
  const loading = ref(false)
  const error = ref(null)

  const latestData = computed(() => sensorData.value[0] || null)

  async function fetchData() {
    loading.value = true
    error.value = null
    try {
      const res = await api.getSensorData()
      if (res.data.success) {
        sensorData.value = res.data.data
      }
    } catch (e) {
      error.value = e.message
    } finally {
      loading.value = false
    }
  }

  return { sensorData, loading, error, latestData, fetchData }
})
