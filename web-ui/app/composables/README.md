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
- `app/components/pairingForm.vue` - form component; delegates pairing to a parent callback.
- `app/pages/index.vue` - page that wires the composable to the form.

---

## useDeviceConnection

Owns the MQTT connection to a paired device: status/ack channel, settings channel, heartbeat (ping/pong) monitoring with automatic timeout disconnection, and request/response helpers.

### Data Flow

1. After pairing, `device_id` is available.
2. `connect()` publishes `"connected"` to `statusTopic`, subscribes to `statusTopic` and `settingTopic`, and waits for the device to reply with `{"status":"connected"}` on `statusTopic`.
3. **Heartbeat & Ping/Pong:**
   - Once connected, the Web UI starts a ping timeout timer (`PING_TIMEOUT`, default 15s).
   - The device periodically sends a ping (raw `"ping"`, `{"state":"ping"}`, or `{"status":"ping"}`) to `statusTopic`.
   - On receiving a ping, the Web UI publishes `"pong"` back to `statusTopic`, updates `lastPing`, and resets the ping timeout timer.
   - If no ping or device activity is received within `PING_TIMEOUT`, the Web UI automatically calls `disconnect()`.
4. `publishSettings(payload)` publishes to `settingTopic` and waits for `{"status":"success"}` or `"success"` on `statusTopic`.

### Interface

```ts
interface DeviceConnection {
  deviceId: Ref<string | undefined>
  statusTopic: Ref<string>
  settingTopic: Ref<string>
  statusMessage: Ref<MqttStatusMessage | null>
  settingsMessage: Ref<string | null>
  isPaired: Ref<boolean>
  lastPing: Ref<number>
  onRecivedStatus: (callback: (msg: string, statusMsg: MqttStatusMessage) => void) => void
  onRecivedSetting: (callback: (msg: string) => void) => void
  connect: () => Promise<void>
  disconnect: () => void
  publishSettings: (payload: unknown) => Promise<void>
}
```

### Methods & State

- `connect()` - publishes `"connected"` to `statusTopic`, subscribes to status/setting topics, awaits device ack `{"status":"connected"}`, and starts the ping timeout heartbeat monitor.
- `disconnect()` - cancels the ping timeout timer, publishes `"disconnect"`, unsubscribes from both topics, and clears connection state.
- `publishSettings(payload)` - publishes JSON payload to `settingTopic`, awaits device ack.
- `onRecivedStatus(callback)` - registers a listener for every `statusTopic` message.
- `onRecivedSetting(callback)` - registers a listener for every `settingTopic` message.
- `lastPing` - reactive timestamp (ms) of the last received ping from the device (0 when disconnected).

### Device & Heartbeat Contract

- **Connect ACK:** After receiving `"connected"` on `statusTopic`, device must publish `{"status":"connected"}` back to `statusTopic`.
- **Heartbeat (Ping/Pong):**
  - Device sends `ping` (`"ping"` or `{"state":"ping"}`) to `statusTopic` every 5–10 seconds.
  - Web UI responds with `"pong"` to `statusTopic` and resets the timer.
  - If no ping arrives within `PING_TIMEOUT` (default: `15000` ms, configurable via `process.env.PING_TIMEOUT`), Web UI calls `disconnect()`.
  - If device receives no `"pong"`, device cancels the connection and returns to pairing mode.
- **Settings ACK:** After receiving a settings payload on `settingTopic`, device must publish `{"status":"success"}` or `"success"` back to `statusTopic`.

### Files

- `app/composables/useDeviceConnection.ts` - composable owning connection state, ping/pong heartbeat, timeout disconnection, and MQTT subscriptions.
- `app/composables/useDeviceSettings.ts` - helper for validating/converting/saving time settings via `publishSettings`.
- `app/components/timeSettingForm.vue` - form bound to `useDeviceSettings.save()`.
