<template>
  <div class="home">
    <!-- 环境数据卡片 -->
    <div class="card-group">
      <div class="env-card">
        <div class="env-icon">🌡️</div>
        <div class="env-value">
          {{ latestData?.temperature ?? '--' }}<span>°C</span>
        </div>
        <div class="env-label">温度</div>
      </div>
      <div class="env-card">
        <div class="env-icon">💧</div>
        <div class="env-value">
          {{ latestData?.humidity ?? '--' }}<span>%</span>
        </div>
        <div class="env-label">湿度</div>
      </div>
    </div>

    <!-- 快捷操作 -->
    <div class="card">
      <div class="card-header">
        <span class="card-title">快捷操作</span>
      </div>
      <div class="quick-actions">
        <button class="action-btn" @click="humidifierStore.toggle('on')">
          <span class="action-icon">💨</span>
          <span>开启加湿器</span>
        </button>
        <button class="action-btn" @click="humidifierStore.toggle('off')">
          <span class="action-icon">🔇</span>
          <span>关闭加湿器</span>
        </button>
      </div>
    </div>

    <!-- 最近数据 -->
    <div class="card">
      <div class="card-header">
        <span class="card-title">最近记录</span>
        <button class="refresh-btn" @click="sensorStore.fetchData" :disabled="loading">
          <span :class="{ spinning: loading }">↻</span>
        </button>
      </div>
      <div class="recent-list">
        <div class="list-item" v-for="item in recentData" :key="item.id">
          <span class="item-time">{{ item.timestamp }}</span>
          <span class="item-temp">{{ item.temperature }}°C</span>
          <span class="item-humidity">{{ item.humidity }}%</span>
        </div>
        <div class="empty" v-if="!loading && recentData.length === 0">暂无数据</div>
      </div>
    </div>

    <!-- Toast -->
    <div class="toast" v-if="humidifierStore.message">{{ humidifierStore.message }}</div>
  </div>
</template>

<script setup>
import { computed, onMounted } from 'vue'
import { useSensorStore } from '@/stores/sensor'
import { useHumidifierStore } from '@/stores/humidifier'

const sensorStore = useSensorStore()
const humidifierStore = useHumidifierStore()

const latestData = computed(() => sensorStore.latestData)
const recentData = computed(() => sensorStore.sensorData.slice(0, 5))
const loading = computed(() => sensorStore.loading)

onMounted(() => {
  sensorStore.fetchData()
})
</script>
