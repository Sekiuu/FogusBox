<script setup lang="ts">
import * as v from 'valibot'
import type { FormSubmitEvent } from '@nuxt/ui'

/**
 * Props
 * @property mqttMassage - Latest raw MQTT payload from the pairing topic.
 * @property onPair - Callback invoked with the submitted code; returns whether pairing succeeded.
 */
const props = defineProps<{
  mqttMassage: string
  onPair: (code: string) => any
}>()

/**
 * Form schema: pairing code must be exactly 4 digits.
 */
const pairing_schema = v.object({
  code: v.pipe(
    v.string(),
    v.length(4, "must be 4 digits")
  ),
})
type PairingSchema = v.InferOutput<typeof pairing_schema>

/** Local form state for the code input */
const pairingCode = reactive({
  code: ""
})

const toast = useToast()

/**
 * Submit handler: delegates pairing logic to the parent via `onPair`.
 *
 * On failure, the input is cleared so the user can retry.
 */
async function onSubmit(event: FormSubmitEvent<PairingSchema>) {
  const result = props.onPair(event.data.code)
  if (!result) {
    pairingCode.code = ''
  }
}

</script>

<template>
  <UForm v-on:submit="onSubmit" :state="pairingCode" :schema="pairing_schema"
    class=" space-y-4">
    <h1>PAIRING</h1>
    <UFormField label="Code" name="code">
      <UInput v-model="pairingCode.code" />
    </UFormField>
    <UButton type="submit">
      Submit
    </UButton>
  </UForm>
</template>
