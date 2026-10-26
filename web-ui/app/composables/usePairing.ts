/**
 * Pairing composable
 *
 * Manages the device pairing flow over MQTT:
 *  - subscribes to `fogus-box/pairing`
 *  - exposes the latest raw MQTT message
 *  - validates a user-submitted code against the latest message
 *  - persists the matched `device_id` into the `device_id` cookie
 */

import type { MqttPairingMessage, UseMQTT } from '~/types/mqtt'

export const usePairing = (mqtt: UseMQTT) => {
  const config = useRuntimeConfig()
  const pairingTopics = config.public.mqtt_topics.pairing

  const lastMessage = ref<string>('')

  const deviceId = useCookie<string>('device_id')

  const toast = useToast()

  let handlePairingMessage: (message: string) => void = (msg) => {};
  onMounted(() => {
    mqtt.subscribe(pairingTopics, (msg) => {
      lastMessage.value = msg.toString()
      handlePairingMessage(lastMessage.value)
    })
  })
  const onReceiveMessage = (callback: (message: string) => void) => {
    handlePairingMessage = callback;
  }

  const pair = (code: string): boolean => {
    if (lastMessage.value.length <= 0) {
      toast.add({
        title: "Can't Find Pairing Request",
        color: 'warning',
      })
      return false
    }

    try {
      const msg = JSON.parse(lastMessage.value) as MqttPairingMessage
      if (msg.code === code) {
        deviceId.value = msg.device_id
        return true
      }

      toast.add({
        title: "Can't Find Device",
        color: 'warning',
      })
      return false
    } catch (e) {
      const err = e as Error
      toast.add({
        title: `Error : ${err.name}`,
        color: 'error',
        description: err.message,
      })
      console.error(e)
      return false
    }
  }

  return { lastMessage, pairingTopics, deviceId, pair, onReceiveMessage }
}
