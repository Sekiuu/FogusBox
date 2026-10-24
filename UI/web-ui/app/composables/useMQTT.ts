import { getMQTTClient } from '~/utils/mqtt'

export const useMQTT = () => {
  const client = getMQTTClient()

  const subscribe = (topic: string, callback: (msg: string) => void) => {
    client.subscribe(topic)
    client.on('message', (t: string, message: Buffer) => {
      if (t === topic) {
        callback(message.toString())
      }
    })
  }

  return { subscribe }
}
