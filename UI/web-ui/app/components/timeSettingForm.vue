<script setup lang="ts">

const props = defineProps<{
  setting_topic: string
}>()

type DeviceTimeSetting = {
  work_time: number
  break_time: number
}

//เอาไว้บันทึกข้อมูลที่กรอกใน form
const timeSettingState = reactive({
  work_time: {
    minute: 25,
    sec: 0
  },
  break_time: {
    minute: 5,
    sec: 0
  }
})

async function onSubmit() {
  //convert minute to secs for work and break time
  const workTime = timeSettingState.work_time;
  const breakTime = timeSettingState.break_time;
  const totalWorkTime = convertToSec(workTime.minute) + workTime.sec;
  const totalBreakTime = convertToSec(breakTime.minute) + breakTime.sec;
  //create payload for mqtt
  const settingPayload: DeviceTimeSetting = { work_time: totalWorkTime, break_time: totalBreakTime }

  //send payload
  const mqtt = useMQTT();
  mqtt.publish(props.setting_topic, JSON.stringify(settingPayload));
}

//convert time from minute to secconds
function convertToSec(minute: number) {
  const sec = minute * 60
  return sec;
}

</script>

<template>
  <UForm v-on:submit="onSubmit" :state="timeSettingState" class="flex flex-col gap-4 justify-center items-center">
    <!-- timer setting group -->
    <div class="flex gap-2">
      <!-- work time setting -->
      <UFieldGroup class="flex flex-col items-center outline-1 rounded-md">
        <label>Work Time</label>
        <div class="flex">
          <UFormField class="flex flex-col items-center" label="นาที (Min)">
            <UInput type="number" min="0" :model-value="timeSettingState.work_time.minute"
              class="w-20 p-2 text-center text-xl rounded-xl" />
          </UFormField>
          <UFormField class="flex flex-col items-center" label="วินาที (Sec)">
            <UInput type="number" :model-value="timeSettingState.work_time.sec" min="0" max="59"
              class="w-20 p-2 text-center text-xl rounded-xl" />
          </UFormField>
        </div>
      </UFieldGroup>
      
      <!-- break time setting -->
      <UFieldGroup class="flex flex-col  items-center outline-1 rounded-md">
        <label>Break Time</label>
        <div class="flex">
          <UFormField class="flex flex-col items-center" label="นาที (Min)">
            <UInput type="number" min="0" :model-value="timeSettingState.break_time.minute"
              class="w-20 p-2 text-center text-xl rounded-xl" />
          </UFormField>
          <UFormField class="flex flex-col items-center" label="วินาที (Sec)">
            <UInput type="number" :model-value="timeSettingState.break_time.sec" min="0" max="59"
              class="w-20 p-2 text-center text-xl rounded-xl" />
          </UFormField>
        </div>
      </UFieldGroup>
    </div>
    <!-- --- -->
    <UButton type="submit">
      Submit
    </UButton>
  </UForm>
</template>
