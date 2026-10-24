<script setup lang="ts">
const { lastMessage, pair, onReviceMassage, deviceId } = usePairing()
const massages = ref<string[]>([]);
const status_topic = ref("");
const setting_topic = ref("");
onReviceMassage((msg) => {
  massages.value.push(msg);

})
const pairDevice = (code: string) => {
  const paired = pair(code);
  if (paired) {
    const mqtt = useMQTT();
    //success
    const config = useRuntimeConfig();
    status_topic.value = config.public.mqtt_topics.status.replace("*", deviceId.value);
    setting_topic.value = config.public.mqtt_topics.setting.replace("*", deviceId.value)
    //send ack to deviceId/status
    sendConnectAck(status_topic.value);
    //subsbibe deviceId/status
    mqtt.subscribe(status_topic.value, (msg) => {
      massages.value.push(msg);

    })
    //subsbibe deviceId/status
    mqtt.subscribe(setting_topic.value, (msg) => {
      massages.value.push(msg);
    })
  }
  return paired
}

const sendConnectAck = (topic: string) => {
  const mqtt = useMQTT();
  console.log("send ack to topic : " + topic);
  mqtt.publish(topic, "connected");
  return topic;
}
</script>

<template>
  <UCard class="w-2/3 mx-auto p-8">
    <template #title> Pairing </template>
    <template #default>
      <PairingForm :mqtt-massage="lastMessage" :on-pair="pairDevice" />
      <TimeSettingForm v-if="deviceId" :setting_topic="setting_topic" />
    </template>
    <template #footer>
      <h1> Pairing Massages </h1>
      <div class="flex flex-col space-y-5 w-fit">
        <span v-if="massages.length > 0" v-for="msg, i in massages" :key="i"
          class=" bg-primary-200/50 text-secondary-800 rounded-xs p-1">
          {{ msg }}
        </span>
      </div>
    </template>
  </UCard>
</template>
