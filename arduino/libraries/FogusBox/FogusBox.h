#ifndef FOGUSBOX_H
#define FOGUSBOX_H

#include <Arduino.h>
#include <WiFiS3.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

enum BoxState {
  BOX_STATE_FOCUS,
  BOX_STATE_BREAK
};

class FogusBox {
public:
  /**
   * @brief คอนสตรัคเตอร์ของ FogusBox
   * @param lcd Reference ของจอ LCD I2C 16x2
   * @param buzzerPin ขาดิจิทัลสำหรับ Buzzer (default = 8)
   * @param buttonPin ขาดิจิทัลสำหรับปุ่มหลัก Pairing (default = 2)
   */
  FogusBox(LiquidCrystal_I2C &lcd, uint8_t buzzerPin = 8, uint8_t buttonPin = 2);

  /**
   * @brief เริ่มต้นระบบฮาร์ดแวร์ โหลดการตั้งค่าจาก Flash และเชื่อมต่อ Network
   */
  void begin(const char* ssid, const char* password, const char* mqttServer = "broker.hivemq.com", int mqttPort = 1883);

  /**
   * @brief ฟังก์ชันหลักที่ต้องเรียกใน loop()
   */
  void update();

  // สถานะของระบบ
  bool isStarted() const;
  bool isPaired() const;
  bool isPairingActive() const;
  bool isBreak() const;
  bool isFocus() const;
  BoxState getState() const;

  // ควบคุมสถานะ
  void startFocus();
  void startBreak();
  void stopTimer();
  void startPairing();
  void stopPairing();

  // ควบคุมเสียงและการแสดงผล
  void beep(int count = 1);
  void showStandby();
  void showFocus();
  void showBreak();
  void showTimer(unsigned int minutes, unsigned int seconds);
  void showPairing();
  void showSettingUpdated();

  // ตั้งค่าปิดการแสดงผลหน้าจอชั่วคราว (เช่น เมื่อเล่นมินิเกม)
  void suppressTimerDisplay(bool suppress);
  bool isTimerDisplaySuppressed() const;

  // การตั้งค่าเวลา
  unsigned long getWorkTime() const;
  unsigned long getBreakTime() const;
  void setWorkTime(unsigned long ms);
  void setBreakTime(unsigned long ms);
  void saveSettings();
  void loadSettings();

  // Network & MQTT
  bool connectNetwork();
  void sendPairingCode();
  void sendConnectedStatus();
  void sendSettingSuccess();
  void sendTimerStatus(unsigned long remainingSeconds);

  // MQTT Callback
  void handleMqttMessage(char* topic, byte* payload, unsigned int length);

private:
  LiquidCrystal_I2C &_lcd;
  uint8_t _buzzerPin;
  uint8_t _buttonPin;

  Preferences _preferences;
  Adafruit_MPU6050 _mpu;
  WiFiClient _espClient;
  PubSubClient _mqttClient;

  const char* _ssid;
  const char* _password;
  const char* _mqttServer;
  int _mqttPort;

  String _deviceId;
  String _statusTopic;
  String _settingTopic;
  const char* _pairingTopic;

  char _pairingCode[5];
  bool _isPaired;
  bool _isPairingActive;
  bool _isPairingPaused;
  unsigned long _lastPairingPublishTime;

  unsigned long _focusTimeMs;
  unsigned long _breakTimeMs;
  unsigned long _stateDurationMs;
  unsigned long _previousTimerMillis;
  unsigned long _lastDataPublishTime;
  BoxState _currentState;
  bool _isStarted;
  bool _displaySuppressed;

  int _buttonState;
  int _lastButtonState;
  unsigned long _lastDebounceTime;
  const unsigned long _debounceDelay;

  bool _isFlippedUpper;
  unsigned long _lastFlipCheck;

  void checkButton();
  void checkOrientation();
  void reconnectMqtt();
  String getMacAddress();
};

#endif // FOGUSBOX_H
