// https://nuxt.com/docs/api/configuration/nuxt-config
export default defineNuxtConfig({
  modules: ['@nuxt/ui'],
  css: ['~/assets/css/main.css'],
  compatibilityDate: '2025-07-15',
  devtools: { enabled: true },
  runtimeConfig: {
    public: {
      mqtt: {
        url: process.env.NUXT_PUBLIC_MQTT_URL ?? 'wss://mqtt-dashboard.com:8884/mqtt',
        clientId: process.env.NUXT_PUBLIC_MQTT_CLIENT_ID ?? 'fg-server'
      },
      mqtt_topics:{
        piaring: process.env.NUXT_PUBLIC_MQTT_PAIRING_TOPIC?? "fogus-box/pairing",
        status: process.env.NUXT_PUBLIC_MQTT_STATUS_TOPIC?? "fogus-box/*/status",
        setting: process.env.NUXT_PUBLIC_MQTT_SETTING_TOPIC?? "fogus-box/*/setting"
      }
    }
  },
  vite: {
    optimizeDeps: {
      include: [
        '@vue/devtools-core',
        'mqtt'
      ]
    }
  }
})
