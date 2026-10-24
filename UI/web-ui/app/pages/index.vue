<script setup lang="ts">
const massages = ref<string[]>([])
const lastMsg = ref<string>("");
onMounted(() => {
  const { subscribe } = useMQTT()
  subscribe('fogus-box/pairing', (msg) => {
    console.log('pairing:', msg)
    massages.value.push(msg)
    lastMsg.value = msg;
  })
})

</script>

<template>
  <UCard class="w-2/3 mx-auto p-8">
    <template #title> Pairing </template>
    <template #footer>
      <h1> Pairing Massages </h1>
      <div class="flex flex-col space-y-5 w-fit">
        <span v-for="(msg, i) in massages" :key="i" class=" bg-primary-200/50 text-secondary-800 rounded-xs p-1">
          {{ msg }}
        </span>
      </div>
    </template>
    <template #default>
      <PairingForm :mqtt-massage="lastMsg" />
    </template>
  </UCard>
</template>