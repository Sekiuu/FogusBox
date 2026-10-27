// ============================================================================
// Fogus Box Timer — Arduino (UNO R4) Pomodoro-style timer with Wi-Fi / MQTT
// Modularized using FogusBox and Minigame libraries
// ============================================================================

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Minigame.h>
#include <FogusBox.h>

// ============================================================================
//  HARDWARE PINS & CONFIG
// ============================================================================
const int BUTTON_PIN   = 2;  // ปุ่มหลัก (Pairing/ตั้งค่า)
const int GAME_BTN_PIN = 3;  // ปุ่มสำหรับเล่นเกมตอนพัก (Break)
const int BUZZER_PIN   = 8;  // ลำโพง Buzzer

// Wi-Fi & MQTT Configuration
const char* WIFI_SSID     = "HikKy";
const char* WIFI_PASSWORD = "Solunaa030";
const char* MQTT_SERVER   = "broker.hivemq.com";
const int   MQTT_PORT     = 1883;

// ============================================================================
//  OBJECT INITIALIZATION
// ============================================================================
LiquidCrystal_I2C lcd(0x27, 16, 2);
Minigame minigame(lcd, GAME_BTN_PIN);
FogusBox fogusBox(lcd, BUZZER_PIN, BUTTON_PIN);

bool lastGameBtnState = HIGH;

// ============================================================================
//  SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);

  // เริ่มต้นระบบหลัก (โหลด Flash, MPU6050, Wi-Fi, MQTT, LCD, Buzzer)
  fogusBox.begin(WIFI_SSID, WIFI_PASSWORD, MQTT_SERVER, MQTT_PORT);

  // เริ่มต้นระบบมินิเกม (false = ไม่แสดง Splash Screen ซ้ำตอนบูต)
  minigame.begin(false);
}

// ============================================================================
//  MAIN LOOP
// ============================================================================
void loop() {
  // รันระบบหลัก (จัดการ Network, MQTT, เช็คการพลิกกล่อง MPU6050, จับเวลา Pomodoro)
  fogusBox.update();

  // ตรวจจับปุ่มกด D3 เพื่อเปิดเมนูมินิเกมตอนอยู่ในโหมด Break
  bool gameBtnState = digitalRead(GAME_BTN_PIN);
  if (gameBtnState == LOW && lastGameBtnState == HIGH) {
    if (fogusBox.isStarted() && fogusBox.isBreak() && !minigame.isActive()) {
      minigame.openMenu();
    }
  }
  lastGameBtnState = gameBtnState;

  // ควบคุมการทำงานของมินิเกม
  if (minigame.isActive()) {
    // ให้เล่นเกมได้เฉพาะช่วงเวลาพัก (Break) และไม่ได้อยู่ในโหมด Pairing
    if (fogusBox.isBreak() && !fogusBox.isPairingActive()) {
      fogusBox.suppressTimerDisplay(true); // ปิดการแสดงผลเวลานับถอยหลัง เพื่อให้มินิเกมแสดงผลบนจอ LCD ได้เต็มที่
      minigame.update();                   // อัปเดตเฟรมของเกม
    } else {
      minigame.stop();                     // หมดเวลาพัก หรือกดเข้า Pairing สั่งหยุดเกมทันที
      fogusBox.suppressTimerDisplay(false);
    }
  } else {
    fogusBox.suppressTimerDisplay(false);  // แสดงผลเวลานับถอยหลังตามปกติ
  }
}
