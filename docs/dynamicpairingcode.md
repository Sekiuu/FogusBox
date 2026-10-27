# Physical Computing Project 2026 - IT KMITL
## ระบบ Dynamic Pairing Code & Communication Protocol (FocusBox)

เอกสารนี้ระบุข้อกำหนดและโปรโตคอลการสื่อสารผ่าน **MQTT** ระหว่างอุปกรณ์ **FocusBox (Arduino UNO R4 WiFi)** และ **Web Application (Web-UI)**

---

### 1. โครงสร้าง MQTT Topic ทั้งหมด

ค่าเริ่มต้น `{project_name}` = `fogus-box` และ `{device_id}` = MAC Address ของบอร์ด (เช่น `F4:12:FA:AB:CD:EF`)

| Topic | ทิศทาง | ประเภท | รายละเอียด |
| :--- | :--- | :--- | :--- |
| **`{project_name}/pairing`** | Device $\rightarrow$ Web | Public | บอร์ดประกาศรหัส 4 หลักและ Device ID ออกมาเป็นระยะตอนเปิดโหมด Pairing |
| **`{project_name}/{device_id}/status`** | Bi-directional | Device Channel | ใช้ส่งสถานะ, Heartbeat (Ping/Pong) และข้อความยืนยันการทำงาน (ACK) |
| **`{project_name}/{device_id}/settings`** | Web $\rightarrow$ Device | Device Channel | หน้าเว็บส่งค่าการตั้งค่าเวลา (Work/Break) ไปยังบอร์ด |

---

### 2. ขั้นตอนการทำงานและโปรโตคอล (Step-by-Step Flow)

```mermaid
sequenceDiagram
    autonumber
    actor User as ผู้ใช้งาน
    participant Box as FocusBox (Device)
    participant Web as Web-UI (Frontend)
    participant Broker as MQTT Broker

    Note over Box,Web: ขั้นตอนที่ 1: การ Pairing (จับคู่)
    User->>Box: กดปุ่ม D2 ค้างเพื่อเปิดโหมด Pairing
    Box->>Broker: Publish {project_name}/pairing (ทุก 30 วินาที)<br/>{"device_id":"...", "code":"8492"}
    Web->>Broker: Subscribe {project_name}/pairing
    User->>Web: กรอกรหัส 4 หลักที่เห็นบนจอ LCD
    Web->>Web: ตรวจสอบรหัสตรงกัน -> บันทึก device_id ลง LocalStorage
    Web->>Broker: Unsubscribe {project_name}/pairing
    Web->>Broker: Publish {project_name}/{device_id}/status<br/>"connected" หรือ {"status":"connected"}
    Broker->>Box: Forward "connected"
    Box->>Box: บันทึกสถานะ Paired, ส่งเสียงบี๊บ 2 ครั้ง, จอขึ้น Standby
    Box->>Broker: Publish {project_name}/{device_id}/status<br/>{"status":"success", "state":"connected"}

    Note over Box,Web: ขั้นตอนที่ 2: Heartbeat (Ping / Pong)
    loop ทุกๆ 5 วินาที
        Box->>Broker: Publish {project_name}/{device_id}/status<br/>{"state":"ping", "runtime":{...}}
        Broker->>Web: Forward Ping
        Web->>Broker: Publish {project_name}/{device_id}/status<br/>"pong" หรือ {"state":"pong"}
        Broker->>Box: Forward Pong (รีเซ็ตตัวจับเวลา Timeout)
    end
    Note over Box: หากไม่ได้รับ Pong เกิน 25 วินาที -> ยกเลิกการเชื่อมต่ออัตโนมัติ

    Note over Box,Web: ขั้นตอนที่ 3: การตั้งค่าเวลา (Settings & ACK)
    User->>Web: ปรับเวลา Work/Break แล้วกดบันทึก
    Web->>Broker: Publish {project_name}/{device_id}/settings<br/>{"work_time": 25, "break_time": 5}
    Broker->>Box: Forward Settings
    Box->>Box: บันทึกค่าลง Flash Memory (Preferences)
    Box->>Broker: Publish {project_name}/{device_id}/status (ACK ทันที)<br/>{"status":"success", "setting":{"work_time":25, "break_time":5}}
    Broker->>Web: Forward ACK -> หน้าเว็บแสดงผลสำเร็จ
    Box->>Box: จอ LCD ขึ้น "SETTING SAVED!" และส่งเสียงบี๊บ 2 ครั้ง
```

---

### 3. รายละเอียดและโครงสร้าง Payload ในแต่ละขั้นตอน

#### ขั้นตอนที่ 1: การ Pairing (จับคู่)
1. **ฝั่ง Device:** เมื่อผู้ใช้กดปุ่ม D2 เพื่อเข้าสู่โหมด Pairing บอร์ดจะสุ่มรหัส 4 หลัก และส่ง Payload ไปยัง `{project_name}/pairing` ทุกๆ **30 วินาที**
   - **Topic:** `fogus-box/pairing`
   - **Payload ตัวอย่าง:**
     ```json
     {
       "device_id": "F4:12:FA:AB:CD:EF",
       "code": "8492"
     }
     ```
   - **หน้าจอ LCD:** แสดงรหัส 4 หลัก เช่น `CODE: 8492`
2. **ฝั่ง Web-UI:** หน้าเว็บ `Subscribe` รอไว้ที่ `fogus-box/pairing` เมื่อผู้ใช้กรอกรหัสตรงกับที่บอร์ดประกาศ หน้าเว็บจะดึง `device_id` บันทึกลงเบราว์เซอร์ และสั่ง `Unsubscribe` จาก topic pairing ทันที
3. **การยืนยันการเชื่อมต่อ:** หน้าเว็บส่งข้อความไปยัง `{project_name}/{device_id}/status`
   - **Payload:** `"connected"` หรือ `{"status": "connected"}`
4. **ฝั่ง Device ตอบรับ:** บอร์ดปรับสถานะเป็นเชื่อมต่อแล้ว (`_isPaired = true`), ส่งเสียงบี๊บ 2 ครั้ง, แสดงหน้า Standby บน LCD และส่งข้อความยืนยันกลับไปยัง status:
   ```json
   {
     "status": "success",
     "state": "connected",
     "settings": {
       "work_time": 25,
       "break_time": 5
     }
   }
   ```

---

#### ขั้นตอนที่ 2: การเช็คสถานะการเชื่อมต่อ (Status & Heartbeat)
1. **ฝั่ง Device ส่ง Ping:** ส่งข้อมูลสถานะไปยัง `{project_name}/{device_id}/status` ทุกๆ **5 วินาที**
   - **Payload ตัวอย่าง:**
     ```json
     {
       "state": "ping"
     }
     ```
   - *หมายเหตุ:* หากกล่องอยู่ในระหว่างจับเวลา ระบบจะแนบสถานะเวลานับถอยหลังไปด้วย:
     ```json
     {
       "state": "ping",
       "runtime": {
         "status": "work",
         "remain_time": 1420
       }
     }
     ```
2. **ฝั่ง Web-UI ตอบ Pong:** หน้าเว็บรอรับข้อความ ping แล้วตอบกลับทาง `{project_name}/{device_id}/status` ทันที
   - **Payload:** `"pong"` หรือ `{"state": "pong"}`
3. **การตรวจสอบ Online / Offline:**
   - **ฝั่ง Web-UI:** หากไม่ได้รับ `ping` นานเกิน 15 วินาที หน้าเว็บจะแสดงสถานะ **Offline / Disconnected**
   - **ฝั่ง Device:** หากไม่ได้รับ `pong` ตอบกลับนานเกิน **25 วินาที** บอร์ดจะยกเลิกการเชื่อมต่อทั้งหมด ตัดกลับสู่สถานะก่อน Pairing และส่งเสียงแจ้งเตือน 1 ครั้ง

---

#### ขั้นตอนที่ 3: การตั้งค่าและการตอบกลับ (Settings & ACK)
1. **ฝั่ง Web-UI ส่งค่าตั้งค่า:** เมื่อผู้ใช้เปลี่ยนค่าเวลาทำงาน/เวลาพัก แล้วกดบันทึก หน้าเว็บจะส่ง JSON ไปยัง Topic `settings`
   - **Topic:** `fogus-box/{device_id}/settings`
   - **Payload ตัวอย่าง:**
     ```json
     {
       "work_time": 25,
       "break_time": 5
     }
     ```
     *(รองรับทั้งหน่วยวินาที และหน่วยนาทีตามที่ระบบกำหนด)*
2. **ฝั่ง Device บันทึกลง Storage:** บอร์ดรับ JSON นำค่าไปบันทึกลง **Flash Memory (Preferences)** เพื่อให้ค่าคงอยู่แม้ปิดเครื่อง
3. **การตอบกลับยืนยัน (ACK):** หลังบันทึกสำเร็จ บอร์ดส่งข้อมูลยืนยันกลับไปยัง `{project_name}/{device_id}/status` **ทันที** เพื่อให้หน้าเว็บทราบว่าการตั้งค่าสำเร็จสมบูรณ์
   - **Topic:** `fogus-box/{device_id}/status`
   - **Payload ตัวอย่าง:**
     ```json
     {
       "status": "success",
       "message": "ตั้งค่าเสร็จแล้ว",
       "setting": {
         "work_time": 25,
         "break_time": 5
       }
     }
     ```
4. **การตอบสนองบนอุปกรณ์:** บอร์ดส่งเสียงแจ้งเตือน 2 ครั้ง และหน้าจอ LCD แสดงข้อความ:
   ```text
   SETTING SAVED!
   W:25s B:5s
   ```
   ค้างไว้ 1.5 วินาที ก่อนเปลี่ยนเป็น `FLIP TO START` เพื่อให้พร้อมเริ่มจับเวลาด้วยการพลิกกล่อง
