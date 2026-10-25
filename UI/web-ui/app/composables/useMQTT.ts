import { getMQTTClient } from '~/utils/mqtt'

export const useMQTT = () => {
  const client = getMQTTClient()

  const subscribe = (topic: string, callback: (msg: string, topic: string) => void) => {
    client.subscribe(topic)
    const handler = (_t: string, message: Buffer) => {
      if (_t === topic) {
        callback(message.toString(), _t)
      }
    }
    client.on('message', handler)
    // Return an unsubscribe function so callers can clean up if needed.
    return () => {
      client.unsubscribe(topic)
      client.off('message', handler)
    }
  }

  const publish = (topic: string, message: string) => {
    client.publish(topic, message)
  }

  return { subscribe, publish }
}
