#include <WiFiS3.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>  // บันทึกข้อมูลถาวร (Flash Memory)

// --- Preferences Object ---
Preferences preferences;

// --- Hardware Pins ---
const int BUTTON_PIN = 2;  // ปุ่มกด D2 (Active LOW)
const int BUZZER     = 8;  // บัซเซอร์

// --- LCD Config ---
LiquidCrystal_I2C lcd(0x27, 16, 2); 

// --- Wi-Fi & MQTT Config ---
const char* ssid               = "ป๊อกโก้";
const char* password           = "1010101010";
const char* mqtt_server         = "broker.hivemq.com";
const int   mqtt_port           = 1883;

const char* mqtt_pub_pairing   = "fogus-box/pairing"; 

String mqtt_status_topic  = ""; 
String mqtt_setting_topic = ""; 

WiFiClient espClient;
PubSubClient client(espClient);

// --- Timer Config & Variables ---
unsigned long FOCUS_TIME = 5UL * 1000UL;        // Work เริ่มต้น 5 วินาที
unsigned long BREAK_TIME = 5UL * 60UL * 1000UL; // Break เริ่มต้น 5 นาที

enum State { FOCUS, BREAK };
State currentState = FOCUS;

unsigned long previousMillis = 0;
unsigned long stateDuration = FOCUS_TIME;

// --- State Control Variables ---
String deviceIdStr = "";
bool isStarted = false;          
bool isPaired = false;           
bool isPairingActive = false;    
bool isPairingStopped = false;   
const int PARING_INTERVAL_TIME = 30000;
char currentCode[5] = "0000";

// ตัวแปรตรวจจับการกดปุ่ม (Short Press / Long Press 5s)
unsigned long buttonPressStartTime = 0;
bool isButtonPressed = false;
bool longPressTriggered = false;

unsigned long lastPairingPublishTime = 0;
unsigned long lastDataPublishTime = 0;

// --- ฟังก์ชัน บันทึกค่าเวลาลงใน Flash Memory ---
void saveTimerSettings(unsigned long workMs, unsigned long breakMs) {
  preferences.begin("timer_cfg", false);
  preferences.putULong("work", workMs);
  preferences.putULong("break", breakMs);
  preferences.end();
  Serial.println(">>> บันทึกค่าเวลาใหม่ลงใน Flash Memory เรียบร้อยแล้ว! <<<");
}

// --- ฟังก์ชัน ดึงค่าเวลาจาก Flash Memory เมื่อเปิดเครื่อง ---
void loadTimerSettings() {
  preferences.begin("timer_cfg", true);
  FOCUS_TIME = preferences.getULong("work", 5000UL);
  BREAK_TIME = preferences.getULong("break", 300000UL);
  preferences.end();

  Serial.println("\n-------------------------------------------");
  Serial.println(">>> โหลดค่าเวลาถาวรจาก Flash Memory สำเร็จ <<<");
  Serial.print("Focus Time ปัจจุบัน: "); Serial.print(FOCUS_TIME / 1000); Serial.println(" วินาที");
  Serial.print("Break Time ปัจจุบัน: "); Serial.print(BREAK_TIME / 1000); Serial.println(" วินาที");
  Serial.println("-------------------------------------------\n");
}

// ฟังก์ชันดึง MAC Address ของบอร์ด R4
String getDeviceID() {
  byte mac[6];
  WiFi.macAddress(mac);
  char macBuf[18];
  snprintf(macBuf, sizeof(macBuf), "%02X:%02X:%02X:%02X:%02X:%02X", 
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macBuf);
}

// ฟังก์ชันสร้าง MQTT Topics อัตโนมัติจาก MAC Address
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

// ฟังก์ชันส่งเสียง Buzzer
void notifySound(int count) {
  for (int i = 0; i < count; i++) {
    tone(BUZZER, 1000);
    delay(120);
    noTone(BUZZER);
    delay(80);
  }
}

// แสดงผลหน้าจอพร้อมกดเริ่มงาน (Standby)
void updateLCDStandby() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("PRESS BUTTON    ");
  lcd.setCursor(0, 1);
  lcd.print("TO START TIMER  ");
}

// แสดงผลหน้าจอเมื่อได้รับตั้งเวลาใหม่ (รอการกดปุ่มเพื่อเริ่ม)
void updateLCDReadyNewSetting() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SETTING UPDATED ");
  lcd.setCursor(0, 1);
  lcd.print("PRESS TO START  ");
}

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

// อัปเดตหน้าจอ LCD สำหรับโหมด Timer
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

// อัปเดตหน้าจอ LCD สำหรับโหมด Pairing
void updateLCDPairing() {
  lcd.clear();
  if (isPairingStopped) {
    lcd.setCursor(0, 0);
    lcd.print("PAIRING STOPPED ");
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

// สุ่มส่งรหัส Pairing
void sendPairingCode() {
  if (isPairingStopped) return;

  int randomNum = random(0, 10000);
  snprintf(currentCode, sizeof(currentCode), "%04d", randomNum);

  StaticJsonDocument<256> doc;
  doc["code"] = currentCode;
  doc["device_id"] = deviceIdStr;

  char jsonBuffer[256];
  serializeJson(doc, jsonBuffer);

  Serial.print("Publishing Pairing Code to [");
  Serial.print(mqtt_pub_pairing);
  Serial.print("]: ");
  Serial.println(jsonBuffer);

  client.publish(mqtt_pub_pairing, jsonBuffer);

  updateLCDPairing();
}

// ส่ง Payload รายงานสถานะตอบกลับเมื่อเชื่อมต่อสำเร็จ
void sendConnectedStatusPayload() {
  StaticJsonDocument<384> doc;

  doc["status"] = "connected";

  JsonObject settings = doc.createNestedObject("settings");
  settings["work"] = FOCUS_TIME / 1000;
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

// ส่ง Payload ตอบกลับเมื่อตั้งค่า Setting สำเร็จ ไปยัง fogus-box/<MAC_ADDRESS>/status
void sendSettingSuccessPayload() {
  StaticJsonDocument<256> doc;

  doc["status"] = "success";

  JsonObject setting = doc.createNestedObject("setting");
  setting["work"] = FOCUS_TIME / 1000;
  setting["break"] = BREAK_TIME / 1000;

  char jsonBuffer[256];
  serializeJson(doc, jsonBuffer);

  Serial.print("Publishing Setting Response to [");
  Serial.print(mqtt_status_topic);
  Serial.print("]: ");
  Serial.println(jsonBuffer);

  client.publish(mqtt_status_topic.c_str(), jsonBuffer);
}

// ส่ง Payload รายงานเวลาคงเหลือไปยัง status topic
void sendTimerPayload(unsigned long totalSeconds) {
  StaticJsonDocument<384> doc;

  JsonObject config = doc.createNestedObject("config");
  config["work"] = FOCUS_TIME / 1000;
  config["break"] = BREAK_TIME / 1000;

  JsonObject runtime = doc.createNestedObject("runtime");
  runtime["status"] = (currentState == FOCUS) ? "work" : "break";
  runtime["remain_time"] = totalSeconds;

  char jsonBuffer[384];
  serializeJson(doc, jsonBuffer);

  client.publish(mqtt_status_topic.c_str(), jsonBuffer);
}

// ฟังก์ชัน Callback รับข้อความจาก MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("Received MQTT Message [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);

  // 1. เมื่อได้รับคำว่า "connected"
  if (message.equalsIgnoreCase("connected")) {
    isPaired = true;
    isPairingActive = false;
    isPairingStopped = false;
    
    Serial.println("\n-------------------------------------------");
    Serial.println(">>> จับคู่สำเร็จ (Paired)! ตอบกลับไปยัง status topic และรอรับตั้งค่าเวลา <<<");
    Serial.println("-------------------------------------------\n");
    
    notifySound(2);
    sendConnectedStatusPayload();
    updateLCDStandby();
    return;
  }

  // 2. เมื่อได้รับคำสั่ง "disconnected" หรือ "unpair"
  if (message.equalsIgnoreCase("disconnected") || message.equalsIgnoreCase("unpair")) {
    isPaired = false;
    isPairingActive = true;
    isPairingStopped = false;
    isStarted = false;
    
    Serial.println("\n-------------------------------------------");
    Serial.println(">>> ได้รับคำสั่ง Disconnected! กลับสู่สถานะ Pairing <<<");
    Serial.println("-------------------------------------------\n");
    
    notifySound(1);
    sendPairingCode();
    lastPairingPublishTime = millis();
    return;
  }

  // 3. เมื่อได้รับค่าเวลาตั้งค่าจาก mqtt_setting_topic
  if (String(topic) == mqtt_setting_topic) {
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (!error) {
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
        // บันทึกค่าใหม่ลง Flash Memory
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
        sendSettingSuccessPayload(); // ส่ง Payload ตอบกลับไปยัง fogus-box/<MAC_ADDRESS>/status
        updateLCDReadyNewSetting();
      }
    } else {
      Serial.print("JSON Parse Failed: ");
      Serial.println(error.f_str());
    }
  } 
}

void setup_wifi() {
  delay(10);
  Serial.print("กำลังเชื่อมต่อ Wi-Fi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi เชื่อมต่อสำเร็จ!");
  
  initMQTTTopics();
}

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

// ตรวจสอบการกดปุ่ม
void checkButton() {
  int reading = digitalRead(BUTTON_PIN);

  if (reading == LOW) {
    if (!isButtonPressed) {
      isButtonPressed = true;
      buttonPressStartTime = millis();
      longPressTriggered = false;
    } else {
      if (!longPressTriggered && (millis() - buttonPressStartTime >= 5000)) {
        longPressTriggered = true;
        
        isPairingActive = true;
        isPairingStopped = false;
        isStarted = false;
        
        Serial.println("\n===========================================");
        Serial.println(">>> กดค้าง 5 วิ! เข้าสู่ Pairing Mode สุ่มส่งรหัสใหม่ <<<");
        Serial.println("===========================================\n");
        
        notifySound(1);
        sendPairingCode();
        lastPairingPublishTime = millis();
      }
    }
  } else {
    if (isButtonPressed) {
      if (!longPressTriggered) {
        if (isPairingActive) {
          isPairingStopped = !isPairingStopped;
          if (isPairingStopped) {
            Serial.println("\n>>> กดปุ่มสั่งหยุด Pairing! <<<\n");
          } else {
            Serial.println("\n>>> กดปุ่มเริ่ม Pairing ต่อ... <<<\n");
            sendPairingCode();
          }
          updateLCDPairing();
        } else if (!isStarted) {
          isStarted = true;
          previousMillis = millis();
          startFocusState();
        } else {
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

void setup() {
  Serial.begin(115200);

  // ดึงค่าเวลาจาก Flash Memory
  loadTimerSettings();

  pinMode(BUZZER, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  randomSeed(analogRead(A0));

  lcd.init();
  lcd.backlight();

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  updateLCDStandby();
}

void loop() {
  if (isPairingActive && !client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long currentMillis = millis();

  checkButton();

  if (isPairingActive) {
    if (!isPairingStopped && (currentMillis - lastPairingPublishTime >= PARING_INTERVAL_TIME)) {
      lastPairingPublishTime = currentMillis;
      sendPairingCode();
    }
  } else if (isStarted) {
    unsigned long elapsedTime = currentMillis - previousMillis;

    if (elapsedTime >= stateDuration) {
      previousMillis = currentMillis;
      if (currentState == FOCUS) {
        startBreakState();
      } else {
        startFocusState();
      }
      elapsedTime = 0;
    }

    unsigned long remainingMillis = stateDuration - elapsedTime;
    unsigned long totalSeconds = remainingMillis / 1000;
    unsigned int minutes = totalSeconds / 60;
    unsigned int seconds = totalSeconds % 60;

    updateLCDTimer(minutes, seconds);

    if (currentMillis - lastDataPublishTime >= 1000) {
      lastDataPublishTime = currentMillis;
      sendTimerPayload(totalSeconds);
    }
  }
  delay(1000);
}