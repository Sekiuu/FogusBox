<script setup lang="ts">
import * as v from 'valibot'
import type { FormSubmitEvent } from '@nuxt/ui'

type MqttPairingMassage = {
    code: String,
    device_id: String
}
const props = defineProps<{ mqttMassage: string }>()
//form validation
const pairing_schema = v.object({
    code: v.pipe(
        v.string(),
        v.length(4, "must be 4 digits")
    ),
});
type PairingSchema = v.InferOutput<typeof pairing_schema>
const pairingCode = reactive({
    code: ""
})
//-----

//submit actions
const toast = useToast()
async function onSubmit(event: FormSubmitEvent<PairingSchema>) {
    //try to match code
    console.log(event.data)
    const data = event.data;
    try {
        if (props.mqttMassage.length <= 0) {
            toast.add({
                title: "Can't Find Pairing Request",
                color: "warning",
            })
            return;
        }
        const mqttMassage = JSON.parse(props.mqttMassage) as MqttPairingMassage;
        //try match code
        if (mqttMassage.code == data.code) {
            toast.add({
                title: "Success",
                color: "primary",
                description: `connected id is ${data.code}`
            })
        } else {
            toast.add({
                title: "Can't Find Device",
                color: "warning",
            })
        }
    } catch (e) {
        const err = (e as Error);
        toast.add({
            title: "Error : " + err.name,
            color:"error",
            description: err.message
        })
        console.error(e);
    }
}

</script>
<template>
    <UForm v-on:submit="onSubmit" :state="pairingCode" :schema="pairing_schema">
        <UFormField label="Code" name="code">
            <UInput v-model="pairingCode.code" />
        </UFormField>
        <UButton type="submit">
            Submit
        </UButton>
    </UForm>
</template>