<script setup lang="ts">
const props = defineProps<{
  connection:DeviceConnection,
}>()
const { save } = useDeviceSettings(props.connection)
const toast = useToast()
const state = reactive<TimeSettingFrom>({
  work_time: { minute: 25, sec: 0 },
  break_time: { minute: 5, sec: 0 },
})
async function onSubmit() {
  const ok = await save(state)
  if (ok) {
    toast.add({ title: 'Setting saved', color: 'success' })
  } else {
    toast.add({ title: 'Save failed', color: 'error' })
  }
}
</script>

<template>
  <UForm @submit="onSubmit" :state="state" class="flex flex-col gap-4 justify-center items-center">
    <div class="flex gap-2">
      <UFieldGroup class="flex flex-col items-center outline-1 rounded-md">
        <label>Work Time</label>
        <div class="flex">
          <UFormField class="flex flex-col items-center" label="นาที (Min)">
            <UInput type="number" min="0" v-model="state.work_time.minute"
              class="w-20 p-2 text-center text-xl rounded-xl" />
          </UFormField>
          <UFormField class="flex flex-col items-center" label="วินาที (Sec)">
            <UInput type="number" min="0" v-model="state.work_time.sec" max="59"
              class="w-20 p-2 text-center text-xl rounded-xl" />
          </UFormField>
        </div>
      </UFieldGroup>

      <UFieldGroup class="flex flex-col items-center outline-1 rounded-md">
        <label>Break Time</label>
        <div class="flex">
          <UFormField class="flex flex-col items-center" label="นาที (Min)">
            <UInput type="number" min="0" v-model="state.break_time.minute"
              class="w-20 p-2 text-center text-xl rounded-xl" />
          </UFormField>
          <UFormField class="flex flex-col items-center" label="วินาที (Sec)">
            <UInput type="number" min="0" v-model="state.break_time.sec" max="59"
              class="w-20 p-2 text-center text-xl rounded-xl" />
          </UFormField>
        </div>
      </UFieldGroup>
    </div>

    <UButton type="submit">
      Submit
    </UButton>
  </UForm>
</template>
