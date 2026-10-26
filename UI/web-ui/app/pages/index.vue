<script setup lang="ts">
const mqtt = useMQTT()
const pairing = usePairing(mqtt)
const connection = useDeviceConnection(mqtt,pairing.deviceId)

const messages = ref<{ topic: string; msg: string; time: string }[]>([])

onMounted(async () => {
  if (connection.deviceId.value && !connection.isConnected.value) {
    await connection.connect()
  }
})

//msg reciving
mqtt.client.on("message",(topic,payload) => {
  const time = new Date().toLocaleTimeString()
  const msg = payload.toString();
  messages.value.push({topic,msg,time})
})

const pairWithCode = async (code: string) => {
  const paired = pairing.pair(code)
  if (paired) {
    await connection.connect()
  }
  return paired
}

const disconnect = () => {
  connection.disconnect()
  messages.value = []
}
</script>

<template>
  <UCard class="w-2/3 mx-auto p-8">
    <template #title> Fogus-box </template>
    <template #default>
      <div class="flex flex-col w-full justify-center items-center gap-4">
        <PairingForm v-if="!connection.isConnected.value" :mqtt-massage="pairing.lastMessage.value"
          :on-pair="pairWithCode" />

        <UBadge v-else class="flex flex-col" color="primary">
          <p>Connected with</p>
          <span class="italic bg-info px-4 py-1 rounded-md text-xl">{{ connection.deviceId }}</span>
          <UButton color="error" @click="disconnect"> disconnected </UButton>
        </UBadge>
        <TimeSettingForm v-if="connection.isConnected" :connection="connection" />
      </div>
    </template>

    <template #footer>
      <h1>Messages</h1>
      <div class="flex flex-col w-fit">
        <div v-if="messages.length > 0" v-for="msg, i in messages.toReversed()" :key="i"
          :class="`${i !== 0 ? 'bg-primary-800' : 'bg-info-800 outline-1'} rounded-xs p-1`">
          <span class="text-natural-800">{{ msg.msg }}</span>
          <p class="text-muted text-xs">{{ msg.topic }} : {{ msg.time }}</p>
        </div>
      </div>
    </template>
  </UCard>
</template>
