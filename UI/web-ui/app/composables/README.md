# Composables

## usePairing

Device pairing is handled via MQTT. The user enters a 4-digit code shown on the device; on match the device id is saved to a cookie so the rest of the app can target that device.

### Data Flow

1. Device publishes a JSON payload to `fogus-box/pairing`:
   `{ "code": "1234", "device_id": "abc-123" }`
2. `usePairing` subscribes to that topic and stores the latest raw message.
3. `PairingForm` collects a 4-digit code and calls `pair(code)`.
4. On match, `device_id` is written to the `device_id` cookie.
5. On failure, the form clears and shows a warning toast.

### Files

- `app/composables/usePairing.ts` - composable owning MQTT subscription, matching, and cookie persistence.
- `app/components/pairingForm.vue` - dumb form component; delegates pairing to a parent callback.
- `app/pages/index.vue` - page that wires the composable to the form.

---

## useDeviceConnection

Owns the MQTT connection to a paired device: status/ack channel, settings channel, connect/disconnect, and a request/response helper.

### Data Flow

1. After pairing, `device_id` is available.
2. `connect()` publishes `"connected"` to `statusTopic`, subscribes to `statusTopic` and `settingTopic`, and waits for the device to reply with `{"status":"success"}` on `statusTopic`.
3. `publishSettings(payload)` publishes to `settingTopic` and waits for `{"status":"success"}` on `statusTopic`.
4. `waitForMassage(topic, timeoutMs)` subscribes to an arbitrary topic and resolves with the first message, or rejects on timeout.

### Interface

```ts
interface DeviceConnection {
  deviceId: Ref<string | undefined>
  statusTopic: Ref<string>
  settingTopic: Ref<string>
  statusMessage: Ref<MqttStatusMessage | null>
  settingsMessage: Ref<string | null>
  isConnected: Ref<boolean>
  onRecivedStatus: (callback: (msg: string, statusMsg: MqttStatusMessage) => void) => void
  onRecivedSetting: (callback: (msg: string) => void) => void
  waitForMassage: (topic: string, timeoutMs?: number) => Promise<string>
  connect: () => Promise<void>
  disconnect: () => void
  publishSettings: (payload: unknown) => Promise<void>
}
```

### Methods

- `connect()` - publishes `"connected"` to statusTopic, subscribes to status/setting topics, awaits device ack `{"status":"success"}`.
- `disconnect()` - publishes `"disconnect"`, unsubscribes both topics, clears state.
- `publishSettings(payload)` - publishes JSON payload to settingTopic, awaits device ack.
- `waitForMassage(topic, timeoutMs=5000)` - subscribes to `topic`, resolves with the message string, or rejects on timeout.
- `onRecivedStatus(callback)` - registers a listener for every statusTopic message.
- `onRecivedSetting(callback)` - registers a listener for every settingTopic message.

### Device Contract

- After receiving `"connected"` on `statusTopic`, device must publish `{"status":"success"}` back to `statusTopic`.
- After receiving a settings payload on `settingTopic`, device must publish `{"status":"success"}` back to `statusTopic`.

### Files

- `app/composables/useDeviceConnection.ts` - composable owning connection state, MQTT subscriptions, and ack logic.
- `app/composables/useDeviceSettings.ts` - helper for validating/converting/saving time settings via `publishSettings`.
- `app/components/timeSettingForm.vue` - form bound to `useDeviceSettings.save()`.
