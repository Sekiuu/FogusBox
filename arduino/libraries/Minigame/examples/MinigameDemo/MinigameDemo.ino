#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Minigame.h>

// กำหนดหน้าจอ LCD (Address 0x27, 16 ตัวอักษร, 2 แถว)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// สร้างออบเจ็กต์ Minigame โดยส่ง lcd และขาปุ่มกด (Pin 2)
Minigame minigame(lcd, 2);

void setup() {
  Serial.begin(9600);
  
  // เริ่มต้นหน้าจอ LCD
  lcd.init();
  lcd.backlight();

  // เริ่มต้นเกม (true = แสดงหน้าจอเริ่มต้น FocusBox Offline 2 วินาที)
  minigame.begin(true);
}

void loop() {
  // เรียก update() ใน loop เพื่อให้เกมทำงานต่อเนื่อง
  minigame.update();
}
