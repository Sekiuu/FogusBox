# Physical Computing Project 2026 - IT KMITL

## Project Title: FocusBox 📦

โปรเจกต์นี้เป็นส่วนหนึ่งในรายวิชา PHYSICAL COMPUTING 06016409 ภาคเรียนที่ 1 ปีการศึกษา 2569 คณะเทคโนโลยีสารสนเทศ สาขาเทคโนโลยีสารสนเทศ สถาบันเทคโนโลยีพระจอมเกล้าเจ้าคุณทหารลาดกระบัง

---

## บทคัดย่อ (Abstract)

โปรเจกต์นี้เป็นการพัฒนาอุปกรณ์ไมโครคอนโทรลเลอร์ **FocusBox** เพื่อช่วยบริหารเวลาและเพิ่มประสิทธิภาพการทำงานด้วยเทคนิค Pomodoro ในรูปแบบกล่องสี่เหลี่ยมพกพา โดยใช้บอร์ด Arduino UNO R4 WiFi ร่วมกับเซนเซอร์ GY-521 MPU6050 สำหรับตรวจจับการพลิกด้านของกล่องเพื่อเปลี่ยนโหมดจับเวลาโดยอัตโนมัติ นอกจากนี้ ในช่วงเวลาพักผ่อน หน้าจอแสดงผลจะเปลี่ยนเป็นมินิเกมที่ผู้ใช้สามารถควบคุมด้วยการเอียงกล่องเพื่อหลบสิ่งกีดขวาง ช่วยผ่อนคลายความตึงเครียดและเสริมสร้างวินัยในการทำงานได้อย่างมีประสิทธิภาพ

---

## Hardware

- Arduino UNO R4 WiFi
- Buzzer
- GY-521 MPU6050
- Button
- 0.96 Inch OLED I2C Display (128 × 64)

---

## Libraries

- WiFiS3.h
- PubSubClient.h
- ArduinoJson.h
- Wire.h
- Adafruit_BusIO
- Adafruit_GFX_Library
- Adafruit_MPU6050
- Adafruit_SSD1306
- Adafruit_Unified_Sensor
- ArduinoJson
- Button
- IntervalTimer
- LiquidCrystal_I2C
- Minigame
- MyMqtt
- PubSubClient

---

## Project Website

[[FocusBox Website](https://sekiuu.github.io/FocusBox/)
](https://sekiuu.github.io/FogusBox/)
---

## Project Demo Video

*Coming soon*

---

## Project Poster

[Fogus Box.pdf](https://github.com/user-attachments/files/34006768/Fogus.Box.pdf)

---

## Tools

- Visual Studio Code

---

## Source Code


#### 1. usePairing

Device pairing is handled via MQTT. The user enters a 4-digit code displayed on the device. When the code matches, the device ID is saved in a cookie so the rest of the application can communicate with the paired device.

**Data Flow**

1. The device publishes a JSON payload to the `fogus-box/pairing` topic.

   ```json
   {
     "code": "1234",
     "device_id": "abc-123"
   }
   ```

2. `usePairing` subscribes to the topic and stores the latest raw message.
3. `PairingForm` collects a 4-digit code and calls `pair(code)`.
4. If the code matches, `device_id` is saved in the `device_id` cookie.
5. If pairing fails, the form is cleared and a warning toast is displayed.

**Files**

- `app/composables/usePairing.ts` — Handles the MQTT subscription, code matching, and cookie persistence.
- `app/components/pairingForm.vue` — Form component that delegates pairing to a parent callback.
- `app/pages/index.vue` — Connects the composable to the pairing form.

#### 2. useDeviceConnection

This composable manages the MQTT connection to a paired device, including the status and acknowledgment channel, settings channel, connection lifecycle, and request-response handling.

**Interface**

```typescript
interface DeviceConnection {
  deviceId: Ref<string | undefined>
  statusTopic: Ref<string>
  settingTopic: Ref<string>
  statusMessage: Ref<MqttStatusMessage | null>
  settingsMessage: Ref<string | null>
  isConnected: Ref<boolean>
  onRecivedStatus: (
    callback: (msg: string, statusMsg: MqttStatusMessage) => void
  ) => void
  onRecivedSetting: (callback: (msg: string) => void) => void
  waitForMassage: (
    topic: string,
    timeoutMs?: number
  ) => Promise<string>
  connect: () => Promise<void>
  disconnect: () => void
  publishSettings: (payload: unknown) => Promise<void>
}
```

**Methods**

- `connect()` — Publishes `"connected"` to `statusTopic`, subscribes to the status and settings topics, and waits for the device acknowledgment `{"status":"success"}`.
- `disconnect()` — Publishes `"disconnect"`, unsubscribes from both topics, and clears the connection state.
- `publishSettings(payload)` — Publishes a JSON payload to `settingTopic` and waits for the device acknowledgment.
- `waitForMassage(topic, timeoutMs = 5000)` — Subscribes to a topic and resolves with the received message string, or rejects if the operation times out.
- `onRecivedStatus(callback)` — Registers a listener for every message received on `statusTopic`.
- `onRecivedSetting(callback)` — Registers a listener for every message received on `settingTopic`.

**Device Contract**

- After receiving `"connected"` on `statusTopic`, the device must publish `{"status":"success"}` back to `statusTopic`.
- After receiving a settings payload on `settingTopic`, the device must publish `{"status":"success"}` back to `statusTopic`.

**Files**

- `app/composables/useDeviceConnection.ts` — Manages connection state, MQTT subscriptions, and acknowledgment logic.
- `app/composables/useDeviceSettings.ts` — Validates, converts, and saves time settings through `publishSettings`.
- `app/components/timeSettingForm.vue` — Form component connected to `useDeviceSettings.save()`.

---

## Contributors

| Student ID | Name | Responsibility |
|---|---|---|
| 65070143 | นายภูริ งาดีสงวนนาม (ภูริ) | UI Development and Mini-game |
| 65070149 | นายมุนินทร ดรลิเคน (เซก้า) | Web Application and System Connectivity |
| 65070152 | นายยศกร ประหารภาพ (นิว) | Hardware Code and Firmware Development |
| 65070166 | นางสาววธูพิตราพร เดชชัง (โฟร์) | Hardware and Device Structure |

---

## นำเสนอ

**รศ.ดร. ปานวิทย์ ธุวะนุติ**

อาจารย์ประจำคณะเทคโนโลยีสารสนเทศ สถาบันเทคโนโลยีพระจอมเกล้าเจ้าคุณทหารลาดกระบัง

**Email:** panwit@it.kmitl.ac.th
