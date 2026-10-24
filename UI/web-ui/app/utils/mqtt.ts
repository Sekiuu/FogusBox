import mqtt from 'mqtt'
import { useRuntimeConfig } from '#app'

let client: ReturnType<typeof mqtt.connect> | null = null

export const getMQTTClient = () => {
  if (client) return client
  const config = useRuntimeConfig().public.mqtt as { url?: string; clientId?: string }
  if (!config.url) {
    throw new Error('MQTT broker URL is not defined in the runtime config (public.mqtt.url).')
  }

  client = mqtt.connect(config.url, {
    clientId: config.clientId || 'nuxt_' + Math.random().toString(16).slice(2),
    ...config
  })

  client.on('connect', () => console.log('✅ MQTT connected'))
  client.on('reconnect', () => console.log('🔄 MQTT reconnecting'))
  client.on('error', (err) => console.error('❌ MQTT error', err))

  return client
}
