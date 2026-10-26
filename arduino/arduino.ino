// ============================================================================
// Fogus Box Timer  — Arduino (UNO R4) Pomodoro-style timer with Wi-Fi / MQTT
//
// OVERVIEW (flow of the sketch)
// ------------------------------
//  setup()  Load saved work/break times from Flash memory
//           Init LCD + buzzer
//           Connect to Wi-Fi
//           Register MQTT topics for this device
//           Show "PRESS BUTTON TO START" on LCD
//
//  loop()   [1] Pairing mode active?
//               yes -> reconnect if disconnected + publish a fresh pairing
//                    code every PAIRING_PUBLISH_INTERVAL_MS (30 s)
//               no, timer running? -> count down, refresh LCD, publish a
//                    status payload every 1 s
//               (2) Button scan:
//                    short press + release
//                       -> start the timer (if it is not yet started), or
//                          switch FOCUS <-> BREAK
//                    long press (hold >= 5 s)
//                       -> enter pairing mode (sends a 4-digit code)
//                    release during pairing
//                       -> toggle pairing paused / resumed
//
//  HARDWARE
//           Button  : D2  (active-low, pulled up)
//           Buzzer  : D8
//           LCD     : I2C 16x2 @ 0x27
//           Wi-Fi   : UNO R4 + WiFiS3
//           MQTT    : broker.hivemq.com:1883
//
//  BUTTON  LOW = pressed (pull-up resistor)
//  BUTTON  Pairing mode is entered by holding the button >= 5 s
//  BUTTON  Pairing is paused by a normal press while pairing is active
// ============================================================================

// ============================================================================
//  1. INCLUDES
// ============================================================================
#include <WiFiS3.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>  // บันทึกค่าเวลาลงใน Flash Memory

// ============================================================================
//  2. HARDWARE INIT & CONSTANTS
// ============================================================================
Preferences preferences;

// --- Button & buzzer ---
const int BUTTON_PIN = 2;  // ปุ่มกด D2 (Active LOW)
const int BUZZER     = 8;  // บัซเซอร์

// --- LCD (I2C 16x2) ---
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- Wi-Fi & MQTT ---
const char* ssid               = "ป๊อกโก้";
const char* password           = "1010101010";
const char* mqtt_server        = "broker.hivemq.com";
const int   mqtt_port          = 1883;

// Topic ที่ส่งรหัส pairing ไปหา (คงที่ทั้งโฆษณาและตอบกลับ)
const char* mqtt_pub_pairing_topic = "fogus-box/pairing";

// Topics ของอุปกรณ์แต่ละเครื่อง (สร้างอัตโนมัติจาก MAC address)
String mqtt_status_topic  = "";   // fogus-box/<MAC>/status
String mqtt_setting_topic = "";   // fogus-box/<MAC>/setting

WiFiClient espClient;
PubSubClient client(espClient);

// ============================================================================
//  3. TIMER CONFIG
// ============================================================================
// ค่าเริ่มต้น (จะถูกแทนที่ด้วยค่าจาก Flash memory หลังบูต)
const unsigned long DEFAULT_WORK_TIME_MS   = 5UL * 1000UL;    // 5 วิ
const unsigned long DEFAULT_BREAK_TIME_MS  = 5UL * 60UL * 1000UL; // 5 นิ

unsigned long FOCUS_TIME   = DEFAULT_WORK_TIME_MS;   // เวลาคลองโฟกัสปัจจุบัน
unsigned long BREAK_TIME   = DEFAULT_BREAK_TIME_MS;  // เวลาพักปัจจุบัน

enum State { FOCUS, BREAK };
State currentState = FOCUS;

unsigned long previousMillis = 0;   // จับเวลา cumulative (farthest singular)
unsigned long stateDuration  = FOCUS_TIME;  // ระยะเวลาของช่วงปัจจุบัน

// ============================================================================
//  4. STATE / PAIRING CONTROL
// ============================================================================
String deviceIdStr = "";  // MAC address ของบอร์ด (ใช้เป็น device ID)

const int PRESS_PARING_DURATION = 5000;//press 5 sec to pair
bool isStarted        = false;   // timer กำลังทำงาน
bool isPaired         = false;   // จับคู่สำเร็จแล้ว
bool isPairingActive  = false;   // กำลังทำงานในโหมด Pairing
bool isPairingPaused  = false;   // จุดประกับการ publish รหัส (true = หยุด publish)
const unsigned long PAIRING_PUBLISH_INTERVAL_MS = 30000UL; // ส่งทุก 30 s

char currentCode[5] = "0000";  // รหัส pairing ที่กำลังแจก

// แปลงสถานะการกดปุ่ม
unsigned long buttonPressStartTime = 0;   // เวลาเริ่มกด (ms)
bool isButtonPressed = false;
bool longPressTriggered = false;   // true เมื่อกดค้าง >= 5 s

unsigned long lastPairingPublishTime = 0;  // เวลาที่ส่งรหัส pairing ครั้งล่าสุด
unsigned long lastDataPublishTime  = 0;    // เวลาที่ส่ง state payload ครั้งล่าสุด

// ============================================================================
//  5. PERSISTENCE (Flash memory)
// ============================================================================
// บันทึกค่าเวลาลง Flash
void saveTimerSettings(unsigned long workMs, unsigned long breakMs) {
  preferences.begin("timer_cfg", false);
  preferences.putULong("work", workMs);
  preferences.putULong("break", breakMs);
  preferences.end();
  Serial.println(">>> บันทึกค่าเวลาใหม่ลงใน Flash Memory เรียบร้อยแล้ว! <<<");
}

// ดึงค่าเวลาจาก Flash memory (ถ้ามี) เมื่อเครื่องเปิดเครื่อง
void loadTimerSettings() {
  preferences.begin("timer_cfg", true);
  FOCUS_TIME   = preferences.getULong("work", 5000UL);
  BREAK_TIME   = preferences.getULong("break", 300000UL);
  preferences.end();

  Serial.println("\n-------------------------------------------");
  Serial.println(">>> โหลดค่าเวลาถาวรจาก Flash Memory สำเร็จ <<<");
  Serial.print("Focus Time ปัจจุบัน: "); Serial.print(FOCUS_TIME / 1000); Serial.println(" วินาที");
  Serial.print("Break Time ปัจจุบัน: "); Serial.print(BREAK_TIME / 1000); Serial.println(" วินาที");
  Serial.println("-------------------------------------------\n");
}

// ============================================================================
//  6. DEVICE ID & MQTT TOPICS
// ============================================================================
// ดึง MAC address ของบอร์ด R4
String getDeviceID() {
  byte mac[6];
  WiFi.macAddress(mac);
  char macBuf[18];
  snprintf(macBuf, sizeof(macBuf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macBuf);
}

// สร้าง MQTT topics อัตโนมัติจาก MAC address
void initMQTTTopics() {
  deviceIdStr = getDeviceID();
  mqtt_status_topic  = "fogus-box/" + deviceIdStr + "/status";
  mqtt_setting_topic = "fogus-box/" + deviceIdStr + "/setting";

  Serial.println("\n--- MQTT Topics Initialized ---");
  Serial.print("Device ID (MAC): "); Serial.println(deviceIdStr);
  Serial.print("Status Topic  : "); Serial.println(mqtt_status_topic);
  Serial.print("Setting Topic : "); Serial.println(mqtt_setting_topic);
  Serial.println("--------------------------------\n");
}

// ============================================================================
//  7. BUZZER (ไมโครโฟน/เสียง)
// ============================================================================
// ส่งเสียงผ่านบัซเซอร์ (count ครั้ง, 1 ครั้ง = 120 มิลลิวินาที สูง 1000 Hz)
void notifySound(int count) {
  for (int i = 0; i < count; i++) {
    tone(BUZZER, 1000);
    delay(120);
    noTone(BUZZER);
    delay(80);
  }
}

// ============================================================================
//  8. DISPLAY (LCD update helpers)
// ============================================================================
// แสดงผลหน้าจอ ready (standby) กดเพื่อเริ่ม
void updateLCDStandby() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("PRESS BUTTON    ");
  lcd.setCursor(0, 1);
  lcd.print("TO START TIMER  ");
}

// แสดงผลหน้าจอหลังได้รับ set time ใหม่ (รอการกดเพื่อเริ่ม)
void updateLCDReadyNewSetting() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SETTING UPDATED ");
  lcd.setCursor(0, 1);
  lcd.print("PRESS TO START  ");
}

// อัปเดตหน้าจอในโหมด Timer (แสดงเวลาคงเหลือ)
void updateLCDTimer(unsigned int minutes, unsigned int seconds) {
  lcd.setCursor(0, 1);
  lcd.print("Time Left: ");
  if (minutes < 10) lcd.print("0");
  lcd.print(minutes);
  lcd.print(":");
  if (seconds < 10) lcd.print("0");
  lcd.print(seconds);
  lcd.print(" ");
}

// อัปเดตหน้าจอในโหมด Pairing (แสดงรหัส / สถานะหยุด)
void updateLCDPairing() {
  lcd.clear();
  if (isPairingPaused) {
    lcd.setCursor(0, 0);
    lcd.print("PAIRING PAUSED ");
    lcd.setCursor(0, 1);
    lcd.print("Press 5s Reset  ");
  } else {
    lcd.setCursor(0, 0);
    lcd.print("STATE: PAIRING ");
    lcd.setCursor(0, 1);
    lcd.print("CODE: ");
    lcd.print(currentCode);
    lcd.print("      ");
  }
}

// ============================================================================
//  9. TIMER STATE MACHINE
// ============================================================================
// เริ่มช่วง Focus
void startFocusState() {
  currentState = FOCUS;
  stateDuration = FOCUS_TIME;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("STATE: FOCUS   ");

  notifySound(2);
}

// เริ่มช่วง Break
void startBreakState() {
  currentState = BREAK;
  stateDuration = BREAK_TIME;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("STATE: BREAK   ");

  notifySound(3);
}

// ============================================================================
//  10. PAIRING
// ============================================================================
// สุ่ม 4 หลัก + publish ไปยัง fogus-box/pairing
void sendPairingCode() {
  if (isPairingPaused) return;  // ไม่ส่งเมื่อหยุด (paused)

  int randomNum = random(0, 10000);
  snprintf(currentCode, sizeof(currentCode), "%04d", randomNum);

  StaticJsonDocument<256> doc;
  doc["code"]      = currentCode;
  doc["device_id"] = deviceIdStr;

  char jsonBuffer[256];
  serializeJson(doc, jsonBuffer);

  Serial.print("Publishing Pairing Code to [");
  Serial.print(mqtt_pub_pairing_topic);
  Serial.print("]: ");
  Serial.println(jsonBuffer);

  client.publish(mqtt_pub_pairing_topic, jsonBuffer);

  updateLCDPairing();  // อัปเดตหน้าจอให้ตรงกับรหัสที่ส่ง
}

// ============================================================================
//  11. MQTT PAYLOAD PUBLISH HELPERS
// ============================================================================
// ส่ง payload ยืนยัน connection สำเร็จ (status topic)
void sendConnectedStatusPayload() {
  StaticJsonDocument<384> doc;
  doc["status"] = "connected";

  JsonObject settings = doc.createNestedObject("settings");
  settings["work"]  = FOCUS_TIME / 1000;
  settings["break"] = BREAK_TIME / 1000;

  JsonObject runtime = doc.createNestedObject("runtime");
  runtime["time_remain"] = 0;

  char jsonBuffer[384];
  serializeJson(doc, jsonBuffer);

  Serial.print("Publishing Connected Response to [");
  Serial.print(mqtt_status_topic);
  Serial.print("]: ");
  Serial.println(jsonBuffer);

  client.publish(mqtt_status_topic.c_str(), jsonBuffer);
}

// ส่ง payload ยืนยันรับค่า time ตั้งใหม่สำเร็จ (status topic)
void sendSettingSuccessPayload() {
  StaticJsonDocument<256> doc;
  doc["status"] = "success";

  JsonObject setting = doc.createNestedObject("setting");
  setting["work"]  = FOCUS_TIME / 1000;
  setting["break"] = BREAK_TIME / 1000;

  char jsonBuffer[256];
  serializeJson(doc, jsonBuffer);

  Serial.print("Publishing Setting Response to [");
  Serial.print(mqtt_status_topic);
  Serial.print("]: ");
  Serial.println(jsonBuffer);

  client.publish(mqtt_status_topic.c_str(), jsonBuffer);
}

// ส่ง payload รายงานเวลาคงเหลือ + สถานะ (work/break) ออกไปยัง status topic
void sendTimerPayload(unsigned long totalSeconds) {
  StaticJsonDocument<384> doc;

  JsonObject config = doc.createNestedObject("config");
  config["work"]  = FOCUS_TIME / 1000;
  config["break"] = BREAK_TIME / 1000;

  JsonObject runtime = doc.createNestedObject("runtime");
  runtime["status"]    = (currentState == FOCUS) ? "work" : "break";
  runtime["remain_time"] = totalSeconds;

  char jsonBuffer[384];
  serializeJson(doc, jsonBuffer);

  client.publish(mqtt_status_topic.c_str(), jsonBuffer);
}

// ============================================================================
//  12. MQTT CALLBACK (รับข้อความเข้า)
// ============================================================================
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("Received MQTT Message [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);

  // --- 1. รับข้อความ "connected" -> จับคู่สำเร็จ ---
  if (message.equalsIgnoreCase("connected")) {
    isPaired = true;
    isPairingActive = false;
    isPairingPaused = false;  // reset ไม่ให้หยุด publish ระหว่างจับคู่

    Serial.println("\n-------------------------------------------");
    Serial.println(">>> จับคู่สำเร็จ (Paired)! ตอบกลับไปยัง status topic และรอรับตั้งค่าเวลา <<<");
    Serial.println("-------------------------------------------\n");

    notifySound(2);
    sendConnectedStatusPayload();
    updateLCDStandby();
    return;
  }

  // --- 2. รับข้อความ "disconnected" / "unpair" -> กลับมาสู่ Pairing ---
  if (message.equalsIgnoreCase("disconnected") ||
      message.equalsIgnoreCase("unpair")) {
    isPaired = false;
    isPairingActive = true;
    isPairingPaused = false;
    isStarted = false;

    Serial.println("\n-------------------------------------------");
    Serial.println(">>> ได้รับคำสั่ง Disconnected! กลับสู่สถานะ Pairing <<<");
    Serial.println("-------------------------------------------\n");

    notifySound(1);
    sendPairingCode();  // ส่งรหัสใหม่เพื่อให้ client ใหม่จับได้
    lastPairingPublishTime = millis();
    return;
  }

  // --- 3. รับค่า time ตั้งจาก mqtt_setting_topic -> อัพเดต ---
  if (String(topic) == mqtt_setting_topic) {
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (!error) {
      //stop timer
      bool updated = false;

      if (doc.containsKey("work_time")) {
        unsigned long workSec = doc["work_time"];
        FOCUS_TIME = workSec * 1000UL;
        updated = true;
      }

      if (doc.containsKey("break_time")) {
        unsigned long breakSec = doc["break_time"];
        BREAK_TIME = breakSec * 1000UL;
        updated = true;
      }

      if (updated) {
        // บันทึกค่าใหม่ลง Flash memory
        saveTimerSettings(FOCUS_TIME, BREAK_TIME);

        isPaired = true;
        isPairingActive = false;
        isStarted = false;

        Serial.println("\n-------------------------------------------");
        Serial.println(">>> รับและบันทึกเวลาตั้งค่าใหม่สำเร็จ! <<<");
        Serial.print("Focus Time ใหม่: "); Serial.print(FOCUS_TIME / 1000); Serial.println(" วินาที");
        Serial.print("Break Time ใหม่: "); Serial.print(BREAK_TIME / 1000); Serial.println(" วินาที");
        Serial.println("-------------------------------------------\n");

        notifySound(2);
        sendSettingSuccessPayload();  // ตอบกลับไปยัง status topic
        updateLCDReadyNewSetting();
      }
    } else {
      Serial.print("JSON Parse Failed: ");
      Serial.println(error.f_str());
    }
  }
}

// ============================================================================
//  13. WIFI & MQTT RECONNECT
// ============================================================================

// เชื่อมต่อ MQTT Broker ซ้ำไปเรื่อยๆ จนสำเร็จ
void reconnect() {
  while (!client.connected()) {
    Serial.print("กำลังเชื่อมต่อ MQTT Broker...");

    String clientId = "UNO_R4_Timer_";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println("เชื่อมต่อสำเร็จ!");
      client.subscribe(mqtt_setting_topic.c_str());
      client.subscribe(mqtt_status_topic.c_str());
    } else {
      Serial.print("ล้มเหลว (rc=");
      Serial.print(client.state());
      Serial.println(") ลองใหม่ใน 5 วินาที...");
      delay(5000);
    }
  }
}

// เชื่อมต่อ Wi-Fi + MQTT เฉพาะเมื่อต้องใช้เครือข่าย (ไม่เชื่อมต่อที่เปิดเครื่อง)
bool connectIfNeeded() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi ยังไม่ต่อ ขอเรียกใหม่...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("\nWi-Fi เชื่อมต่อสำเร็จ!");
  }

  if (!client.connected()) {
    reconnect();
  }
  return client.connected();
}

// ============================================================================
//  14. BUTTON HANDLING
// ============================================================================
void checkButton() {
  int reading = digitalRead(BUTTON_PIN);

  // การกด (ปุ่ม Active LOW + INPUT_PULLUP)
  if (reading == LOW) {
    if (!isButtonPressed) {
      // เริ่มจับเวลาเซ็นเซอร์กด
      isButtonPressed = true;
      buttonPressStartTime = millis();
      longPressTriggered = false;
    } else {
      // กดค้าง >= 5 วิ -> เข้า Pairing Mode
      if (!longPressTriggered && (millis() - buttonPressStartTime >= PRESS_PARING_DURATION)) {
        longPressTriggered = true;

        isPairingActive = true;
        isPairingPaused = false;
        isStarted = false;

        Serial.println("\n===========================================");
        Serial.println(">>> กดค้าง 5 วิ! เข้าสู่ Pairing Mode สุ่มส่งรหัสใหม่ <<<");
        Serial.println("===========================================\n");

        notifySound(1);
        sendPairingCode();  // ส่งรหัสครั้งแรกทันทีที่เข้า Pairing
        lastPairingPublishTime = millis();
      }
    }
  } else {
    if (isButtonPressed) {
      if (!longPressTriggered) {
        // ปล่อยปุ่ม: หนึ่งครั้ง (ไม่ใช่การกดค้าง)
        if (isPairingActive) {
          // Toggle: หยุด/ต่อการ publish รหัส Pairing
          isPairingPaused = !isPairingPaused;

          if (isPairingPaused) {
            Serial.println("\n>>> กดปุ่มสั่งหยุด Pairing! <<<\n");  // หยุด publish
          } else {
            Serial.println("\n>>> กดปุ่มเริ่ม Pairing ต่อ... <<<\n");
            sendPairingCode();  // ส่งรหัสใหม่เพื่อต่อไป
          }
          updateLCDPairing();
        } else if (!isStarted) {
          // เริ่มโหมด FOCUS ครั้งแรก
          isStarted = true;
          previousMillis = millis();
          startFocusState();
        } else {
          // กำลังทำงานอยู่ -> แกว่ง FOCUS <-> BREAK
          previousMillis = millis();
          if (currentState == FOCUS) {
            startBreakState();
          } else {
            startFocusState();
          }
        }
      }
      isButtonPressed = false;
    }
  }
}

// ============================================================================
//  15. SETUP & LOOP
// ============================================================================
void setup() {
  Serial.begin(115200);

  // โหลดค่าเวลาจาก Flash memory (ถ้ามี)
  loadTimerSettings();

  pinMode(BUZZER, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  randomSeed(analogRead(A0));

  lcd.init();
  lcd.backlight();

  // ---- เชื่อมต่อเครือข่ายเฉพาะเมื่อต้องใช้ (ไม่เชื่อมต่อทันทีหลังเปิดเครื่อง) ----
  initMQTTTopics();  // สร้าง topic จาก MAC address (ยังไม่เชื่อมต่อทันที)
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  updateLCDStandby();
}

void loop() {
  client.loop();  // อ่าน/push ต่อ MQTT + เรียก callback

  unsigned long currentMillis = millis();

  checkButton();  // สแกนปุ่ม

  // 1. โหมด Pairing: เชื่อมต่อเมื่อใช้งาน (connect on demand) ส่งรหัสทุก 30 s
  if (isPairingActive) {
    connectIfNeeded();
    if (!isPairingPaused && (currentMillis - lastPairingPublishTime >= PAIRING_PUBLISH_INTERVAL_MS)) {
      lastPairingPublishTime = currentMillis;
      sendPairingCode();
    }
  }
  // 2. เวลาเล่น: เชื่อมต่อเมื่อเริ่มทำงาน ส่ง payload ทุก 1 s
  else if (isStarted) {
    connectIfNeeded();
    unsigned long elapsedTime = currentMillis - previousMillis;

    if (elapsedTime >= stateDuration) {  // ระยะเวลาเต็มหมด
      previousMillis = currentMillis;
      if (currentState == FOCUS) {
        startBreakState();
      } else {
        startFocusState();
      }
      elapsedTime = 0;
    }

    unsigned long remainingMillis = stateDuration - elapsedTime;
    unsigned long totalSeconds    = remainingMillis / 1000;
    unsigned int minutes  = totalSeconds / 60;
    unsigned int seconds  = totalSeconds % 60;

    updateLCDTimer(minutes, seconds);

    // ส่ง state payload ออกไปทุก 1 s
    if (currentMillis - lastDataPublishTime >= 1000) {
      lastDataPublishTime = currentMillis;
      sendTimerPayload(totalSeconds);
    }
  }

  delay(1000);  // สำคัญ: ปรับ granularity ของการจับเวลาเป็น 1 s
}
