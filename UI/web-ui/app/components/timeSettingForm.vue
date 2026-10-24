<script setup lang="ts">
import * as v from 'valibot'
import type { FormSubmitEvent } from '@nuxt/ui'

const props = defineProps<{
  setting_topic:string
}>()

type DeviceTimeSetting = {
  work_time : number
  break_time : number
}
async function onSubmit() {
  const mqtt = useMQTT();

}

</script>

<template>
  <UForm v-on:submit="onSubmit" class="flex gap-4 w-full justify-center">
    <div class="flex flex-col items-center">
      <label class="text-sm text-gray-500 mb-1">นาที (Min)</label>
      <UInput type="number"  min="0" class="w-20 p-2 text-center text-xl rounded-xl" />
    </div>
    <div class="flex flex-col items-center">
      <label class="text-sm text-gray-500 mb-1">วินาที (Sec)</label>
      <UInput type="number"  min="0" max="59" class="w-20 p-2 text-center text-xl rounded-xl" />
    </div>
    <UButton type="submit">
      Submit
    </UButton>
  </UForm>
</template>
