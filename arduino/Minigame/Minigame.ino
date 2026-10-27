#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Minigame.h>

// กำหนดหน้าจอ LCD (Address 0x27, 16 ตัวอักษร, 2 แถว)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// กำหนดขาปุ่มกด (Pin 2)
const int btnPin = 2;

// สร้างออบเจ็กต์ Minigame
Minigame minigame(lcd, btnPin);

void setup() {
  Serial.begin(9600);
  
  // เริ่มต้นจอ LCD
  lcd.init();
  lcd.backlight();
  
  // เริ่มต้นระบบมินิเกม (true = แสดง Splash Screen FocusBox)
  minigame.begin(true);
}

void loop() {
  // รันระบบมินิเกม (อัปเดตเมนูและลูปของเกม)
  minigame.update();
}
