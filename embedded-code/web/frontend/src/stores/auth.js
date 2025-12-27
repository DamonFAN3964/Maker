import { defineStore } from 'pinia'
import { ref, computed } from 'vue'
import api from '@/api'

export const useAuthStore = defineStore('auth', () => {
  const token = ref(localStorage.getItem('token') || '')
  const username = ref(localStorage.getItem('username') || '')

  const isLoggedIn = computed(() => !!token.value)

  async function login(user, pass) {
    const res = await api.login(user, pass)
    if (res.data.success) {
      token.value = res.data.token
      username.value = res.data.username
      localStorage.setItem('token', res.data.token)
      localStorage.setItem('username', res.data.username)
    }
    return res.data
  }

  async function logout() {
    try {
      await api.logout()
    } catch {
      // 忽略错误
    }
    token.value = ''
    username.value = ''
    localStorage.removeItem('token')
    localStorage.removeItem('username')
  }

  async function checkAuth() {
    if (!token.value) return false
    try {
      await api.getMe()
      return true
    } catch {
      logout()
      return false
    }
  }

  return { token, username, isLoggedIn, login, logout, checkAuth }
})
