# Pairing Flow

Device pairing is handled via MQTT. The user enters a 4-digit code shown on the device; on match the device id is saved to a cookie so the rest of the app can target that device.

## Data Flow

1. Device publishes a JSON payload to `fogus-box/pairing`:
   `{ "code": "1234", "device_id": "abc-123" }`
2. `usePairing` subscribes to that topic and stores the latest raw message.
3. `PairingForm` collects a 4-digit code and calls `pair(code)`.
4. On match, `device_id` is written to the `device_id` cookie.
5. On failure, the form clears and shows a warning toast.

## Files

- `app/composables/usePairing.ts` - composable owning MQTT subscription, matching, and cookie persistence.
- `app/components/pairingForm.vue` - dumb form component; delegates pairing to a parent callback.
- `app/pages/index.vue` - page that wires the composable to the form.
