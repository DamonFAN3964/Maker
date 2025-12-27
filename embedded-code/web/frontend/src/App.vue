<template>
  <div class="app" :class="{ dark: isDark }">
    <!-- 顶部导航 -->
    <header class="nav-bar" v-if="showNav">
      <h1 class="nav-title">智能寝室</h1>
      <div class="nav-actions">
        <button class="theme-btn" @click="isDark = !isDark">
          {{ isDark ? '☀️' : '🌙' }}
        </button>
        <button class="logout-btn" @click="handleLogout" v-if="authStore.isLoggedIn">
          退出
        </button>
      </div>
    </header>

    <!-- 路由视图 -->
    <main class="main-content" :class="{ 'no-nav': !showNav }">
      <router-view />
    </main>

    <!-- 底部导航 -->
    <nav class="tab-bar" v-if="showNav">
      <router-link to="/" class="tab-item" :class="{ active: $route.path === '/' }">
        <span class="tab-icon">🏠</span>
        <span class="tab-label">首页</span>
      </router-link>
      <router-link to="/control" class="tab-item" :class="{ active: $route.path === '/control' }">
        <span class="tab-icon">💨</span>
        <span class="tab-label">控制</span>
      </router-link>
      <router-link to="/history" class="tab-item" :class="{ active: $route.path === '/history' }">
        <span class="tab-icon">📊</span>
        <span class="tab-label">历史</span>
      </router-link>
    </nav>
  </div>
</template>

<script setup>
import { ref, computed } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'

const route = useRoute()
const router = useRouter()
const authStore = useAuthStore()

const isDark = ref(false)

const showNav = computed(() => route.path !== '/login')

async function handleLogout() {
  await authStore.logout()
  router.push('/login')
}
</script>
