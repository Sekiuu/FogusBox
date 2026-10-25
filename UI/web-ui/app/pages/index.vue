<script setup lang="ts">
//pairing fuction
const { lastMessage, pairingTopics, pair, onReviceMassage, deviceId } = usePairing()
//for display massages
const massages = ref<{ topic: string, msg: string, time: string }[]>([]);
//topics
const status_topic = ref("");
const setting_topic = ref("");

//auto paring form saved deviceID
onMounted(() => {
  if (deviceId)
    connectToDevice(deviceId.value);
})

//----- paring fuctions -----
onReviceMassage((msg) => {
  //on recive massage form pairing topics
  const time = new Date().toTimeString()
  massages.value.push({ topic: pairingTopics, msg, time });
})

//use this fuction for pair (can add to form or button)
const pairWithCode = (code: string) => {
  const paired = pair(code);
  if (paired) {
    //success
    connectToDevice(deviceId.value)
  }
  return paired
}
const connectToDevice = (deviceId: string) => {
  const mqtt = useMQTT();
  const config = useRuntimeConfig();
  status_topic.value = config.public.mqtt_topics.status.replace("*", deviceId);
  setting_topic.value = config.public.mqtt_topics.setting.replace("*", deviceId)
  //send ack to deviceId/status
  sendConnectAck(status_topic.value);
  //subsbibe deviceId/status
  mqtt.subscribe(status_topic.value, (msg, topic) => {
    const time = new Date().toTimeString()
    massages.value.push({ topic, msg, time });
  })
  //subsbibe deviceId/status
  mqtt.subscribe(setting_topic.value, (msg, topic) => {
    const time = new Date().toTimeString()
    massages.value.push({ topic, msg, time });
  })
}
//send acknowledge affter connected
const sendConnectAck = (topic: string) => {
  const mqtt = useMQTT();
  console.log("send ack to topic : " + topic);
  mqtt.publish(topic, "connected");
  return topic;
}
</script>

<template>
  <UCard class="w-2/3 mx-auto p-8">
    <template #title> Fogus-box </template>
    <!-- settings -->
    <template #default>
      <div class="flex flex-col w-full justify-center items-center gap-4">
        <PairingForm :mqtt-massage="lastMessage" :on-pair="pairWithCode" />
        <TimeSettingForm v-if="deviceId" :setting_topic="setting_topic" />
      </div>
    </template>
    <!-- display massage -->
    <template #footer>
      <h1> Pairing Massages </h1>
      <div class="flex flex-col space-y-5 w-fit">
        <!-- make the last massage to top and use for loop to display each one -->
        <div v-if="massages.length > 0" v-for="msg, i in massages.toReversed()" :key="i"
          class=" bg-primary-100 rounded-xs p-1">
          <span class="text-secondary-800">{{ msg.msg }}</span>
          <p class=" text-muted text-xs">{{ msg.topic }} : {{ msg.time }}</p>
        </div>
      </div>
    </template>
  </UCard>
</template>
