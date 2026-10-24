import { defineNuxtPlugin } from '#app'
import { getMQTTClient } from '~/utils/mqtt'

export default defineNuxtPlugin(() => {
  const client = getMQTTClient()
  return {
    provide: {
      mqtt: client
    }
  }
})
