# FogusBox Web UI

Web dashboard and companion interface for the **FogusBox** Pomodoro timer device built with Nuxt 3 / Nuxt UI and MQTT.

## Overview

- **Dynamic Device Pairing:** Pair with a FogusBox device using a 4-digit OTP code shown on the physical display over MQTT.
- **Heartbeat & Connection Monitoring:** Listens for periodic `ping` messages from the device, responds with `pong`, and automatically disconnects if the device goes offline or fails to ping before timeout.
- **Remote Configuration:** Configure Pomodoro work time and break time durations and sync them with the device.

## Environment Variables

Configure connection settings in `.env` or system environment variables:

```bash
# MQTT Broker URL (WebSockets)
NUXT_PUBLIC_MQTT_URL=wss://mqtt-dashboard.com:8884/mqtt

# MQTT Client ID
NUXT_PUBLIC_MQTT_CLIENT_ID=fg-server

# MQTT Topics
NUXT_PUBLIC_MQTT_PAIRING_TOPIC=fogus-box/pairing
NUXT_PUBLIC_MQTT_STATUS_TOPIC=fogus-box/*/status
NUXT_PUBLIC_MQTT_SETTING_TOPIC=fogus-box/*/setting

# Timeouts (in milliseconds)
SETTING_ACK_TIMEOUT=60000
CONNECT_ACK_TIMEOUT=5000
PING_TIMEOUT=15000
```

## Setup & Development

Install dependencies:

```bash
npm install
```

Start the development server on `http://localhost:3000`:

```bash
npm run dev
```

Build for production:

```bash
npm run build
npm run preview
```

## Documentation

- [Composables & MQTT Architecture](app/composables/README.md)
  - `usePairing`: Handles code matching and device ID persistence.
  - `useDeviceConnection`: Manages connection lifecycle, ping/pong heartbeat, timeout disconnection, and MQTT publish/subscribe.
  - `useDeviceSettings`: Validates and sends timer settings to the device.
