<template>
  <div class="history">
    <div class="card">
      <div class="card-header">
        <span class="card-icon">📊</span>
        <span class="card-title">历史记录</span>
        <button class="refresh-btn" @click="store.fetchData" :disabled="store.loading">
          <span :class="{ spinning: store.loading }">↻</span>
        </button>
      </div>

      <div class="list">
        <div class="list-item" v-for="item in store.sensorData" :key="item.id">
          <span class="item-time">{{ item.timestamp }}</span>
          <span class="item-temp">{{ item.temperature }}°C</span>
          <span class="item-humidity">{{ item.humidity }}%</span>
        </div>
        <div class="empty" v-if="!store.loading && store.sensorData.length === 0">
          暂无数据
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { onMounted } from 'vue'
import { useSensorStore } from '@/stores/sensor'

const store = useSensorStore()

onMounted(() => {
  store.fetchData()
})
</script>
