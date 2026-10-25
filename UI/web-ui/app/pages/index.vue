<script setup lang="ts">
//pairing fuction
const { lastMessage, pairingTopics, pair, onReviceMassage, deviceId } = usePairing()
const connected = ref(false)
//for display massages
const massages = ref<{ topic: string, msg: string, time: string }[]>([]);
//topics
const status_topic = ref("");
const setting_topic = ref("");
const subscribe_topics = ref<{ unsubscribe: () => void }[]>([])

//auto paring form saved deviceID
onMounted(() => {
  if (deviceId.value.length > 0)
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
  if(deviceId.length <= 0) return;
  const toast = useToast();
  toast.add({
    title: 'Connected',
    color: 'primary',
    description: `connected id is ${deviceId}`,
  })

  connected.value = true;
  const mqtt = useMQTT();
  const config = useRuntimeConfig();
  status_topic.value = config.public.mqtt_topics.status.replace("*", deviceId);
  setting_topic.value = config.public.mqtt_topics.setting.replace("*", deviceId)
  //send ack to deviceId/status
  console.log("send ack to topic : " + status_topic.value);
  mqtt.publish(status_topic.value, "connected");
  //subsbibe deviceId/status
  const status_chanel = mqtt.subscribe(status_topic.value, (msg, topic) => {
    const time = new Date().toTimeString()
    massages.value.push({ topic, msg, time });
  })
  //subsbibe deviceId/status
  const setting_chanel = mqtt.subscribe(setting_topic.value, (msg, topic) => {
    const time = new Date().toTimeString()
    massages.value.push({ topic, msg, time });
  })
  //memo the subscribed
  subscribe_topics.value.push(status_chanel, setting_chanel)
}

function disconnect() {
  const toast = useToast();
  toast.add({
    title: 'Disconnected',
    color: 'warning',
    description: `disconnected device id : ${deviceId.value}`,
  })

  //clear connect state
  connected.value = false;
  const mqtt = useMQTT();
  //clear cookie
  deviceId.value = ""
  //send disconnect to device
  console.log("send discconnet : " + status_topic.value);
  mqtt.publish(status_topic.value, "disconnect");
  //clear topic
  status_topic.value = status_topic.value = "";
  //unsubsbibe topics
  for (let index = 0; index < subscribe_topics.value.length; index++) {
    const topic = subscribe_topics.value[index];
    topic?.unsubscribe()
  }

}
</script>

<template>
  <UCard class="w-2/3 mx-auto p-8">
    <template #title> Fogus-box </template>
    <!-- settings -->
    <template #default>
      <div class="flex flex-col w-full justify-center items-center gap-4">
        <PairingForm v-if="!connected" :mqtt-massage="lastMessage" :on-pair="pairWithCode" />
        <UBadge v-else class="flex flex-col" color="primary">
          <p>Connected with</p>
          <span class="italic bg-info px-4 py-1 rounded-md text-xl">{{ deviceId }}</span>
          <UButton color="error" v-on:click="disconnect"> disconnected </UButton>
        </UBadge>
        <TimeSettingForm v-if="deviceId" :setting_topic="setting_topic" />
      </div>
    </template>
    <!-- display massage -->
    <template #footer>
      <h1> Massages </h1>
      <div class="grid grid-cols-4 gap-2 w-fit">
        <!-- make the last massage to top and use for loop to display each one -->
        <div v-if="massages.length > 0" v-for="msg, i in massages.toReversed()" :key="i"
          :class="`${i != 0 ? 'bg-primary-800' : 'bg-info-800 outline-1'} rounded-xs p-1`">
          <span class="text-natural-800">{{ msg.msg }}</span>
          <p class=" text-muted text-xs">{{ msg.topic }} : {{ msg.time }}</p>
        </div>
      </div>
    </template>
  </UCard>
</template>
