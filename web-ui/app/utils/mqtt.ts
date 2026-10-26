import mqtt from 'mqtt'
import { useRuntimeConfig } from '#app'

let client: ReturnType<typeof mqtt.connect> | null = null

export const getMQTTClient = () => {
  if (client) return client
  const config = useRuntimeConfig().public.mqtt as { url?: string; clientId?: string }
  if (!config.url) {
    throw new Error('MQTT broker URL is not defined in the runtime config (public.mqtt.url).')
  }
  const uniqueClientId = config.clientId
    ? `${config.clientId}-${Math.random().toString(36).slice(2, 8)}`
    : `nuxt_${Math.random().toString(16).slice(2)}`

  client = mqtt.connect(config.url, {
    clientId: uniqueClientId,
    keepalive: 60,
    connectTimeout: 10_000,
    reconnectPeriod: 5_000,
    clean: true,
  })

  client.on('connect', () => console.log('✅ MQTT connected', uniqueClientId))
  client.on('reconnect', () => console.log('🔄 MQTT reconnecting', uniqueClientId))
  client.on('error', (err) => console.error('❌ MQTT error', err))
  client.on('disconnect', (packet) => console.log('🔌 MQTT disconnected', packet))

  return client
}
