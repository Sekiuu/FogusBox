/**
 * Pairing composable
 *
 * Manages the device pairing flow over MQTT:
 *  - subscribes to `fogus-box/pairing`
 *  - exposes the latest raw MQTT message
 *  - validates a user-submitted code against the latest message
 *  - persists the matched `device_id` into the `device_id` cookie
 */

import { useMQTT } from "#imports"

/**
 * Shape of an incoming MQTT pairing message.
 */
export type MqttPairingMessage = {
  /** 4-digit pairing code broadcast by the device */
  code: string
  /** Stable device identifier written to cookie on successful match */
  device_id: string
}

export const usePairing = () => {
  const config = useRuntimeConfig()
  const pairingTopics = config.public.mqtt_topics.piaring

  /** Latest raw MQTT payload on the `fogus-box/pairing` topic */
  const lastMessage = ref<string>('')

  /**
   * Current paired device identifier.
   *
   * Backed by a cookie so it survives page reloads.
   * Empty when the device is not yet paired.
   */
  const deviceId = useCookie<string>('device_id')

  const toast = useToast()

  // Subscribe once on mount so the page always receives pairing broadcasts.
  let msgCallBack: (massage: string) => void;
  const onReviceMassage = (callback: (massage: string) => void) => {
    msgCallBack = callback;
  }
  onMounted(() => {
    const mqtt = useMQTT()
    mqtt.subscribe(pairingTopics, (msg) => {
      lastMessage.value = msg.toString()
      msgCallBack(msg);
    })
  })

  /**
   * Attempt to pair using the submitted code.
   * @returns `true` if pairing succeeded and `device_id` was saved
   */
  const pair = (code: string): boolean => {
    // A pairing request must already be broadcasting before we can match it.
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
        // Persist the paired device so the rest of the app can target it.
        deviceId.value = msg.device_id
        toast.add({
          title: 'Success',
          color: 'primary',
          description: `connected id is ${deviceId.value}`,
        })
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

  return { lastMessage,pairingTopics, deviceId, pair, onReviceMassage }
}
