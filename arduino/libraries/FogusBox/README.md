# FogusBox Core Library

ไลบรารีหลักสำหรับโปรเจกต์ FogusBox บนบอร์ด **Arduino UNO R4 WiFi** จัดการการทำงานเบื้องหลังทั้งหมด:
- การเชื่อมต่อ **Wi-Fi** และ **MQTT Broker** (HiveMQ)
- การสร้างรหัส Pairing 4 หลัก และการจับคู่อุปกรณ์ผ่าน MQTT Topic
- การบันทึกและโหลดการตั้งค่าเวลา Focus / Break ลง **Flash Memory (Preferences)**
- ตรวจจับการพลิกกล่องด้วยเซนเซอร์ **MPU6050** เพื่อสลับโหมด Focus/Break
- จัดการหน้าจอ **LCD 16x2 (I2C)** และเสียงเตือน **Buzzer**
- ทำงานร่วมกับไลบรารี `Minigame` อย่างไร้รอยต่อ

---

## ตัวอย่างการใช้งานร่วมกับ Minigame ในไฟล์ `arduino.ino`

```cpp
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Minigame.h>
#include <FogusBox.h>

// ขาเชื่อมต่อฮาร์ดแวร์
const int BUTTON_PIN   = 2;  // ปุ่มหลัก (Pairing)
const int GAME_BTN_PIN = 3;  // ปุ่มมินิเกม (Break)
const int BUZZER_PIN   = 8;  // ลำโพง Buzzer

// สร้างออบเจ็กต์
LiquidCrystal_I2C lcd(0x27, 16, 2);
Minigame minigame(lcd, GAME_BTN_PIN);
FogusBox fogusBox(lcd, BUZZER_PIN, BUTTON_PIN);

bool lastGameBtnState = HIGH;

void setup() {
  Serial.begin(115200);

  // เริ่มต้นระบบหลัก (Wi-Fi, MQTT, MPU6050, Timer)
  fogusBox.begin("HikKy", "Solunaa030", "broker.hivemq.com", 1883);

  // เริ่มต้นมินิเกม
  minigame.begin(false);
}

void loop() {
  // รันระบบหลัก (Network, Timer, MPU6050, Buttons)
  fogusBox.update();

  // ตรวจจับปุ่ม D3 เพื่อเปิดเมนูมินิเกมตอนพัก
  bool gameBtnState = digitalRead(GAME_BTN_PIN);
  if (gameBtnState == LOW && lastGameBtnState == HIGH) {
    if (fogusBox.isStarted() && fogusBox.isBreak() && !minigame.isActive()) {
      minigame.openMenu();
    }
  }
  lastGameBtnState = gameBtnState;

  // ควบคุมการแสดงผลของมินิเกม
  if (minigame.isActive()) {
    if (fogusBox.isBreak() && !fogusBox.isPairingActive()) {
      fogusBox.suppressTimerDisplay(true); // ปิดการวาด Timer ทับจอ
      minigame.update();
    } else {
      minigame.stop();
      fogusBox.suppressTimerDisplay(false);
    }
  } else {
    fogusBox.suppressTimerDisplay(false);
  }
}
```
