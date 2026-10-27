import { getMQTTClient } from '~/utils/mqtt'

export const useMQTT = () => {
  const client = getMQTTClient()

  const subscribe = (topic: string, callback: (msg: string, topic: string) => void) => {
    client.subscribe(topic)
    console.log("Subscribe Topic : " + topic)
    const handler = (_t: string, message: Buffer) => {
      if (_t === topic) {
        callback(message.toString(), _t)
      }
      console.log("Revice Massage From \nTopic : " + topic + "\nmassage : " + message.toString())
    }
    client.on('message', handler)
    // Return an unsubscribe function so callers can clean up if needed.
    function unsubscribe() {
      client.unsubscribe(topic)
      client.off('message', handler)
    }
    return { unsubscribe }
  }
  const publish = (topic: string, message: string) => {
    console.log("publish Topic : " + topic + "\nmassage :" + message)
    client.publish(topic, message)
  }
  const waitForMassage = (topic: string, timeoutMs = 5000): Promise<string> => {
    return new Promise((resolve, reject) => {
      const sub = subscribe(topic, (msg) => {
        clearTimeout(timer)
        sub.unsubscribe()
        resolve(msg)
      })
      const timer = setTimeout(() => {
        sub.unsubscribe()
        reject(new Error(`waitForMassage timeout on ${topic}`))
      }, timeoutMs)
    })
  }

  return { subscribe, publish, waitForMassage, client }
}
