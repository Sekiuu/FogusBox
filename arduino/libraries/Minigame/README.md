# Minigame Library for FogusBox

ไลบรารีรวมมินิเกมบนหน้าจอ LCD 16x2 (I2C) ควบคุมด้วยปุ่มกด 1 ปุ่ม (One-Button Control) พัฒนาสำหรับโปรเจกต์ FogusBox

## คุณสมบัติ (Features)
- รองรับจอ **LCD 16x2 ผ่าน I2C** (ใช้ไลบรารี `LiquidCrystal_I2C`)
- ใช้ปุ่มกดเพียง **1 ปุ่ม** (Internal Pull-Up)
- มี 2 เกมในตัว:
  1. **Cave Escape**: บังคับยานหลบสิ่งกีดขวาง (กดปุ่ม = บินขึ้น, ปล่อย = ลงล่าง)
  2. **Star Catcher**: เดินเก็บดาว `*` และหลบระเบิด `X` พร้อมระบบ High Score บันทึกลง EEPROM
- มีหน้า Main Menu นับถอยหลังเข้าเกมอัตโนมัติ (Timeout 3 วินาที)
- โค้ดถูกออกแบบให้เรียกใช้ง่ายและนำไปผสานเข้ากับโค้ดหลักของ FogusBox ได้ทันที

---

## โครงสร้างไฟล์ (File Structure)
```text
arduino/libraries/Minigame/
├── Minigame.h              # Header file นิยามคลาส Minigame
├── Minigame.cpp            # ซอร์สโค้ดการทำงานของเกม
├── library.properties      # ข้อมูลเมทาดาทาของไลบรารี
├── keywords.txt            # ไฮไลต์สีคำสั่งสำหรับ Arduino IDE
├── README.md               # เอกสารแนะนำการใช้งาน
└── examples/
    └── MinigameDemo/
        └── MinigameDemo.ino # ตัวอย่างโค้ดเรียกใช้งาน
```

---

## การเชื่อมต่อฮาร์ดแวร์ (Hardware Wiring)
| อุปกรณ์ (Component) | ขาบน Arduino Uno/Nano | รายละเอียด |
| :--- | :--- | :--- |
| **LCD 16x2 I2C (SDA)** | A4 | I2C Data |
| **LCD 16x2 I2C (SCL)** | A5 | I2C Clock |
| **LCD 16x2 I2C (VCC)** | 5V | ไฟเลี้ยง |
| **LCD 16x2 I2C (GND)** | GND | กราวด์ |
| **Push Button** | Pin 2 และ GND | ใช้วงจร Internal Pull-Up ภายใน |

---

## ตัวอย่างการใช้งานเบื้องต้น (Quick Start)

```cpp
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Minigame.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
Minigame minigame(lcd, 2); // กำหนด LCD และขา Button Pin 2

void setup() {
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();

  // เริ่มต้นเกม (แสดงข้อความ FocusBox ตอนเปิดเครื่อง)
  minigame.begin(true);
}

void loop() {
  // รันลูปเกมอย่างต่อเนื่อง
  minigame.update();
}
```

---

## รายละเอียดคำสั่ง (API Reference)

### Constructor
```cpp
Minigame(LiquidCrystal_I2C &lcd, uint8_t buttonPin = 2, int eepromAddr = 100);
```
- `lcd`: Reference ของจอ LCD I2C
- `buttonPin`: ขาดิจิทัลสำหรับปุ่มกด (ค่าเริ่มต้น: 2)
- `eepromAddr`: ตำแหน่งใน EEPROM ที่ใช้บันทึก High Score ของเกม Star Catcher (ค่าเริ่มต้น: 100)

### เมธอดหลัก
- `void begin(bool showSplash = true)`: กำหนดค่าเริ่มต้นให้กับปุ่มกด, โหลด High Score และเปิดหน้าเมนู
- `void update()`: ฟังก์ชัน Tick หลักที่ต้องวางไว้ใน `loop()`
- `void openMenu()`: สั่งเปิดหน้าเมนูเลือกเกม
- `void startCaveEscape()`: สั่งเริ่มเกม Cave Escape ทันที
- `void startStarCatcher()`: สั่งเริ่มเกม Star Catcher ทันที
- `void stop()`: หยุดการเล่นเกมและเคลียร์หน้าจอ

### ตรวจสอบสถานะและคะแนน
- `GameState getState()`: คืนค่าสถานะ (`GAME_STATE_IDLE`, `GAME_STATE_MENU`, `GAME_STATE_CAVE_ESCAPE`, `GAME_STATE_STAR_CATCHER`)
- `bool isPlaying()`: คืนค่า `true` หากกำลังเล่นเกมใดเกมหนึ่งอยู่
- `int getScore()`: ดึงคะแนนรอบปัจจุบัน
- `int getHighScore()`: ดึงคะแนน High Score ของเกม Star Catcher
- `void resetHighScore()`: รีเซ็ต High Score ใน EEPROM เป็น 0
