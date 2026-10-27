Physical Computing Project 2026 - IT KMITL

---
ระบบ **Dynamic Pairing Code** 
---

### 1. โครงสร้าง Topic ทั้งหมด

* **`{project_name}/pairing`** (Public Topic): ใช้สำหรับให้บอร์ดประกาศ Code และ Device ID ออกมาเป็นระยะตอนเปิดโหมด Pairing
* **`{project_name}/{device_id}/status`** (Device $\rightarrow$ Web): ใช้ส่งสถานะ, Heartbeat และ ACK กลับไปที่หน้าเว็บ
* **`{project_name}/{device_id}/settings`** (Web $\rightarrow$ Device): ใช้ส่งค่าตั้งค่าจากหน้าเว็บไปที่บอร์ด

---

### 2. ขั้นตอนการทำงาน (Step-by-Step)

#### ขั้นตอนที่ 1: การ Pairing (จับคู่)

1. **ฝั่ง Device:** เมื่อผู้ใช้เปิดโหมด Pairing บนกล่อง บอร์ดจะสุ่มรหัส 4 หลัก และส่ง Payload เข้าไปที่ `{project_name}/pairing` ทุกๆ 30 วินาที
* *ตัวอย่าง Payload:* `{"device_id": "mac_address", "code": "8492"}`


2. **ฝั่ง Web-UI:** หน้าเว็บจะสั่ง `Subscribe` ไปที่ `{project_name}/pairing` รอไว้
3. **การจับคู่:** ผู้ใช้ดูรหัส 4 หลักจากหน้าจอ OLED ของกล่อง แล้วมากรอกลงใน Web UI
4. **บันทึก:** เมื่อหน้าเว็บเจอข้อความใน Topic ที่มี `code` ตรงกับที่ผู้ใช้พิมพ์ มันจะดึง `device_id` ออกมา, บันทึกลง `localStorage` ของเบราว์เซอร์ และทำการ `Unsubscribe` จาก topic pairing ทันที เพื่อความปลอดภัย
5. ส่งข้อมูล status : connected เพื่อบอก device ว่าเชื่อมต่อมาแล้ว
6. device เปลี่ยนสถานนะเป็นเชื่อมเต่อแล้ว

---

#### ขั้นตอนที่ 2: การเช็คสถานะการเชื่อมต่อ (Status & Heartbeat)

1. **ฝั่ง Device:** ส่งข้อมูลสถานะไปยัง `{project_name}/{device_id}/status` เป็นระยะ (เช่น ทุกๆ 5-10 วินาที)
* *ตัวอย่าง Payload:* `{"state": "ping",}`


2. **ฝั่ง Web-UI:** ทำการ `Subscribe` ไว้ที่ `{project_name}/{device_id}/status`
3. **การเช็ค Online/Offline:** หน้าเว็บใช้รอฟัง ping แล้วตอบกลับใน status ว่า pong
ถ้าไม่มี ping มานานเกินจะ **"Disconnected / ออฟไลน์"** ทันที
4. device ถ้าไม่ได้รับ pong จาก status นานเกินกำหนดจะยกเลิกการเชื่อมต่อนั้งหมดและกลับไปสถานะ ไม่เชื่อมต่อ (ก่อน paring)

---

#### ขั้นตอนที่ 3: การตั้งค่าและการตอบกลับ (Settings & ACK)

1. **ฝั่ง Web-UI:** เมื่อผู้ใช้เปลี่ยนค่าเวลา แล้วกดบันทึก ระบบจะส่ง JSON ไปที่ `{project_name}/{device_id}/settings`
* *ตัวอย่าง Payload:* `{"work_time": 25, "break_time": 5}`


2. **ฝั่ง Device:** บอร์ดซึ่ง `Subscribe` รออยู่ที่ topic settings อยู่แล้ว จะได้รับข้อความนี้นำไปบันทึกลง storage
3. **การยืนยัน (ACK):** หลังจากบันทึกเสร็จ บอร์ดจะส่งสถานะอัปเดตกลับมาทาง `{project_name}/{device_id}/status` ทันที {status : success, setting : {work_time, break_time}} เพื่อให้หน้าเว็บรู้ว่าบอร์ดได้รับค่าและอัปเดตเรียบร้อยแล้ว