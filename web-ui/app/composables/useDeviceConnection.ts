import type { Ref } from 'vue'
import type { MqttStatusMessage, UseMQTT } from '~/types/mqtt'

export interface DeviceConnection {
  deviceId: Ref<string | undefined>
  statusTopic: Ref<string>
  settingTopic: Ref<string>
  statusMessage: Ref<MqttStatusMessage | null>
  settingsMessage: Ref<string | null>
  isPaired: Ref<boolean>,
  onRecivedStatus: (callback: (msg: string, statusMsg: MqttStatusMessage) => void) => void,
  onRecivedSetting: (callback: (msg: string) => void) => void,
  connect: () => Promise<void>,
  disconnect: () => void,
  publishSettings: (payload: unknown) => Promise<void>
}

export const useDeviceConnection = (deviceId: Ref<string | undefined>): DeviceConnection => {
  const mqtt = useMQTT();
  const config = useRuntimeConfig()
  const toast = useToast()
  const CONNECTION_TIMEOUT = parseInt(config.public.connect_timeout);
  const SETTING_TIMEOUT = parseInt(config.public.setting_timeout);
  const onRecivedStatus = (callback: (msg: string, statusMsg: MqttStatusMessage) => void) => {
    statusRecived = callback;
  }
  const onRecivedSetting = (callback: (msg: string) => void) => {
    settingRecived = callback;
  }
  let statusRecived: (msg: string, statusMsg: MqttStatusMessage) => void;
  let settingRecived: (msg: string) => void;
  const statusTopic = computed(() =>
    config.public.mqtt_topics.status.replace('*', deviceId.value ?? ''),
  )
  const settingTopic = computed(() =>
    config.public.mqtt_topics.setting.replace('*', deviceId.value ?? ''),
  )

  let statusMessage = ref<MqttStatusMessage | null>(null)
  let settingsMessage = ref<string | null>(null)
  const isPaired = ref(false)

  let statusUnsub: { unsubscribe: () => void } | null = null
  let settingUnsub: { unsubscribe: () => void } | null = null

  const handleStatusMessage = (msg: string, statusMsg: MqttStatusMessage) => {
    statusMessage.value = statusMsg
    if (statusRecived) statusRecived(msg, statusMsg)
  }

  const connect = async () => {
    if (isPaired.value || !deviceId.value) return

    mqtt.publish(statusTopic.value, 'connected')

    const onConnected = () => {
      isPaired.value = true
      toast.add({
        title: 'Connected',
        color: 'primary',
        description: `connected id is ${deviceId.value}`,
      })
      statusUnsub = mqtt.subscribe(statusTopic.value, (msg) => {
        try {
          const parsed = JSON.parse(msg) as MqttStatusMessage
          handleStatusMessage(msg, parsed)
          //pingpong
          if (parsed.status = "ping") {
            const pongObject = { status: "pong" }
            mqtt.publish(statusTopic.value, JSON.stringify(pongObject));
          }
        } catch (e) {
          console.error(e)
        }
      })

      settingUnsub = mqtt.subscribe(settingTopic.value, (msg) => {
        settingsMessage.value = msg
        if (settingRecived) settingRecived(msg)
      })
    }
    try {
      //wait for device to ack
      const msg = await mqtt.waitForMassage(statusTopic.value, CONNECTION_TIMEOUT)
      const statusMsg = JSON.parse(msg) as MqttStatusMessage
      if (statusMsg.status == "connected") {
        onConnected()
      }
    } catch (e) {
      console.warn('connect ack not received:', e)
      toast.add({
        title: 'Can not connect',
        color: 'warning',
        description: `disconnected device id : ${deviceId.value}`,
      })
    }
  }

  const disconnect = () => {
    toast.add({
      title: 'Disconnected',
      color: 'warning',
      description: `disconnected device id : ${deviceId.value}`,
    })

    mqtt.publish(statusTopic.value, 'disconnect')
    statusUnsub?.unsubscribe()
    settingUnsub?.unsubscribe()
    statusUnsub = null
    settingUnsub = null

    deviceId.value = ''
    isPaired.value = false
    statusMessage.value = null
    settingsMessage.value = null
  }

  const publishSettings = async (payload: unknown) => {
    mqtt.publish(settingTopic.value, JSON.stringify(payload))

    try {
      const msg = await mqtt.waitForMassage(statusTopic.value, SETTING_TIMEOUT)
      if (msg == "success") {
        toast.add({
          title: 'Setting success!',
          color: 'primary',
        })
      }
    } catch (e) {
      console.warn('publishSettings ack not received:', e)
      throw e
    }
  }

  onUnmounted(() => {
    statusUnsub?.unsubscribe()
    settingUnsub?.unsubscribe()
  })

  return {
    deviceId,
    statusTopic,
    settingTopic,
    statusMessage,
    settingsMessage,
    isPaired: isPaired,
    onRecivedStatus,
    onRecivedSetting,
    connect,
    disconnect,
    publishSettings,
  }
}
