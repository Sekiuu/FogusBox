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
#include <Preferences.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// ============================================================================
//  2. HARDWARE INIT & CONSTANTS
// ============================================================================
Preferences preferences;
Adafruit_MPU6050 mpu;

// --- Button & buzzer ---
const int BUTTON_PIN = 2;  
const int BUZZER     = 8;  

// --- LCD (I2C 16x2) ---
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- Wi-Fi & MQTT ---
const char* ssid               = "HikKy";
const char* password           = "Solunaa030";
const char* mqtt_server        = "broker.hivemq.com";
const int   mqtt_port          = 1883;

// Topic ที่ส่งรหัส pairing ไปหา
const char* mqtt_pub_pairing_topic = "fogus-box/pairing";

// Topics ของอุปกรณ์แต่ละเครื่อง
String mqtt_status_topic  = "";   
String mqtt_setting_topic = "";   

WiFiClient espClient;
PubSubClient client(espClient);

bool connectIfNeeded();

// ============================================================================
//  3. TIMER CONFIG
// ============================================================================
const unsigned long DEFAULT_WORK_TIME_MS   = 5UL * 1000UL;        
const unsigned long DEFAULT_BREAK_TIME_MS  = 5UL * 60UL * 1000UL; 

unsigned long FOCUS_TIME   = DEFAULT_WORK_TIME_MS;   
unsigned long BREAK_TIME   = DEFAULT_BREAK_TIME_MS;  

enum State { FOCUS, BREAK };
State currentState = FOCUS;

unsigned long previousMillis = 0;             
unsigned long stateDuration  = FOCUS_TIME;    

// ============================================================================
//  4. STATE / PAIRING / MPU CONTROL
// ============================================================================
String deviceIdStr = "";  

bool isStarted        = false;   
bool isPaired         = false;   
bool isPairingActive  = false;   
bool isPairingPaused  = false;   
const unsigned long PAIRING_PUBLISH_INTERVAL_MS = 30000UL; 

char currentCode[5] = "0000";  

// สถานะปุ่มกด 
int buttonState = HIGH;             
int lastButtonState = HIGH;         
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50; 

// สถานะการพลิกกล่อง (MPU6050)
bool isFlippedUpper = false;     
unsigned long lastFlipCheck = 0; 

unsigned long lastPairingPublishTime = 0;  
unsigned long lastDataPublishTime  = 0;    

// ============================================================================
//  5. PERSISTENCE (Flash memory)
// ============================================================================
void saveTimerSettings(unsigned long workMs, unsigned long breakMs) {
  preferences.begin("timer_cfg", false);
  preferences.putULong("work", workMs);
  preferences.putULong("break", breakMs);
  preferences.end();
  Serial.println(">>> บันทึกค่าเวลาใหม่ลงใน Flash Memory เรียบร้อยแล้ว! <<<");
}

void loadTimerSettings() {
  preferences.begin("timer_cfg", true);
  FOCUS_TIME   = preferences.getULong("work", 5000UL);
  BREAK_TIME   = preferences.getULong("break", 300000UL);
  preferences.end();
}

// ============================================================================
//  6. DEVICE ID & MQTT TOPICS
// ============================================================================
String getDeviceID() {
  byte mac[6];
  WiFi.macAddress(mac);
  char macBuf[18];
  snprintf(macBuf, sizeof(macBuf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macBuf);
}

void initMQTTTopics() {
  deviceIdStr = getDeviceID();
  mqtt_status_topic  = "fogus-box/" + deviceIdStr + "/status";
  mqtt_setting_topic = "fogus-box/" + deviceIdStr + "/setting";
  Serial.println("Device ID: " + deviceIdStr);
}

// ============================================================================
//  7. BUZZER
// ============================================================================
void notifySound(int count) {
  for (int i = 0; i < count; i++) {
    tone(BUZZER, 1000);
    delay(120);
    noTone(BUZZER);
    delay(80);
  }
}

// ============================================================================
//  8. DISPLAY 
// ============================================================================
void updateLCDStandby() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("FLIP BOX TO     ");
  lcd.setCursor(0, 1);
  lcd.print("START TIMER     ");
}

void updateLCDReadyNewSetting() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SETTING UPDATED ");
  lcd.setCursor(0, 1);
  lcd.print("FLIP TO START   ");
}

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

void updateLCDPairing() {
  lcd.clear();
  if (isPairingPaused) {
    lcd.setCursor(0, 0);
    lcd.print("PAIRING PAUSED ");
    lcd.setCursor(0, 1);
    lcd.print("Press To Resume ");
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
void startFocusState() {
  currentState = FOCUS;
  stateDuration = FOCUS_TIME;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("STATE: FOCUS   ");
  notifySound(2);
}

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
void sendPairingCode() {
  if (isPairingPaused) return;

  int randomNum = random(0, 10000);
  snprintf(currentCode, sizeof(currentCode), "%04d", randomNum);
  updateLCDPairing();

  if (!connectIfNeeded()) {
    Serial.println("Network Error");
    return;
  }

  StaticJsonDocument<256> doc;
  doc["code"]      = currentCode;
  doc["device_id"] = deviceIdStr;
  char jsonBuffer[256];
  serializeJson(doc, jsonBuffer);
  client.publish(mqtt_pub_pairing_topic, jsonBuffer);
}

// ============================================================================
//  11. MQTT PAYLOAD PUBLISH HELPERS
// ============================================================================
void sendConnectedStatusPayload() {
  StaticJsonDocument<384> doc;
  doc["status"] = "connected";
  JsonObject settings = doc.createNestedObject("settings");
  settings["work"]  = FOCUS_TIME / 1000UL;
  settings["break"] = BREAK_TIME / 1000UL;
  JsonObject runtime = doc.createNestedObject("runtime");
  runtime["time_remain"] = 0;
  char jsonBuffer[384];
  serializeJson(doc, jsonBuffer);
  client.publish(mqtt_status_topic.c_str(), jsonBuffer);
}

void sendSettingSuccessPayload() {
  StaticJsonDocument<256> doc;
  doc["status"] = "success";
  JsonObject setting = doc.createNestedObject("setting");
  setting["work"]  = FOCUS_TIME / 1000UL;
  setting["break"] = BREAK_TIME / 1000UL;
  char jsonBuffer[256];
  serializeJson(doc, jsonBuffer);
  client.publish(mqtt_status_topic.c_str(), jsonBuffer);
}

void sendTimerPayload(unsigned long totalSeconds) {
  StaticJsonDocument<384> doc;
  JsonObject config = doc.createNestedObject("config");
  config["work"]  = FOCUS_TIME / 1000UL;
  config["break"] = BREAK_TIME / 1000UL;
  JsonObject runtime = doc.createNestedObject("runtime");
  runtime["status"]      = (currentState == FOCUS) ? "work" : "break";
  runtime["remain_time"] = totalSeconds;
  char jsonBuffer[384];
  serializeJson(doc, jsonBuffer);
  client.publish(mqtt_status_topic.c_str(), jsonBuffer);
}

// ============================================================================
//  12. MQTT CALLBACK
// ============================================================================
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  if (message.equalsIgnoreCase("connected")) {
    isPaired = true;
    isPairingActive = false;
    isPairingPaused = false;
    notifySound(2);
    sendConnectedStatusPayload();
    updateLCDStandby();
    return;
  }

  if (message.equalsIgnoreCase("disconnected") || message.equalsIgnoreCase("unpair")) {
    isPaired = false;
    isPairingActive = true;
    isPairingPaused = false;
    isStarted = false;
    notifySound(1);
    sendPairingCode();
    lastPairingPublishTime = millis();
    return;
  }

  if (String(topic) == mqtt_setting_topic) {
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (!error) {
      bool updated = false;
      if (doc.containsKey("work_time")) { FOCUS_TIME = doc["work_time"].as<unsigned long>() * 1000UL; updated = true; }
      else if (doc.containsKey("work")) { FOCUS_TIME = doc["work"].as<unsigned long>() * 1000UL; updated = true; }
      if (doc.containsKey("break_time")) { BREAK_TIME = doc["break_time"].as<unsigned long>() * 1000UL; updated = true; }
      else if (doc.containsKey("break")) { BREAK_TIME = doc["break"].as<unsigned long>() * 1000UL; updated = true; }

      if (updated) {
        // ---- แสดงสถานะกำลังตั้งค่าบนหน้าจอ LCD ----
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Applying Setting");
        lcd.setCursor(0, 1);
        lcd.print("Please wait...");
        delay(1000); // หน่วงเวลา 1 วินาทีให้เห็นข้อความ
        // -------------------------------------------

        saveTimerSettings(FOCUS_TIME, BREAK_TIME);
        
        // รับค่าเสร็จถือว่าระบบจับคู่อยู่ (เพื่อรักษาเน็ตไว้รับค่าต่อๆ ไป)
        isPaired = true; 
        isPairingActive = false;
        
        // เราตั้งใจให้มันไม่หยุดจับเวลาถ้าทำงานอยู่ หรือจะหยุดก็ได้ตามต้องการ
        // โค้ดเดิมให้ isStarted = false; ผมคงไว้เพื่อให้มันกลับหน้าสแตนด์บายก่อน
        isStarted = false; 

        notifySound(2);
        sendSettingSuccessPayload();
        
        // แสดงข้อความบนจอ LCD ว่าได้รับ Setting ใหม่และพร้อมใช้งานแล้ว
        updateLCDReadyNewSetting();
      }
    }
  }
}

// ============================================================================
//  13. WIFI & MQTT RECONNECT (ใส่ระบบโชว์จอ LCD)
// ============================================================================
void reconnect() {
  while (!client.connected()) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MQTT Connecting");
    
    String clientId = "UNO_R4_Timer_";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      lcd.setCursor(0, 1);
      lcd.print("MQTT Connected!");
      delay(1000); 
      
      client.subscribe(mqtt_setting_topic.c_str());
      client.subscribe(mqtt_status_topic.c_str());
    } else {
      lcd.setCursor(0, 1);
      lcd.print("MQTT Error!");
      delay(3000);
    }
  }
}

bool connectIfNeeded() {
  if (WiFi.status() != WL_CONNECTED) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WIFI Connecting");
    lcd.setCursor(0, 1);
    
    WiFi.begin(ssid, password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      lcd.print("."); 
      delay(500);
      attempts++;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("WIFI Error!");
      delay(2000);
      return false;
    }
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WIFI Connected!");
    delay(1000); 
  }
  
  if (!client.connected()) {
    reconnect();
  }
  
  return client.connected();
}

// ============================================================================
//  14. MPU6050 SENSING 
// ============================================================================
void checkOrientation() {
  if (isPairingActive) return;
  if (millis() - lastFlipCheck < 200) return;
  lastFlipCheck = millis();

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  bool currentFlipped = (a.acceleration.z < -4.0);

  if (currentFlipped != isFlippedUpper) {
    isFlippedUpper = currentFlipped;

    if (!isStarted) {
      isStarted = true;
      previousMillis = millis();
      startFocusState();
    } else {
      previousMillis = millis();
      if (currentState == FOCUS) startBreakState();
      else startFocusState();
    }
  }
}

// ============================================================================
//  15. BUTTON HANDLING 
// ============================================================================
void checkButton() {
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading; 

      if (buttonState == LOW) {
        if (!isPairingActive) {
          isPairingActive = true;
          isPairingPaused = false;
          isStarted = false; 

          Serial.println("\n>>> เข้าสู่ Pairing Mode <<<");
          notifySound(1);
          sendPairingCode(); 
          lastPairingPublishTime = millis();
        } else {
          isPairingActive = false;
          isPairingPaused = false;
          
          Serial.println("\n>>> ออกจาก Pairing Mode <<<");
          notifySound(1);
          updateLCDStandby();
        }
      }
    }
  }
  lastButtonState = reading; 
}

// ============================================================================
//  16. SETUP & LOOP
// ============================================================================
void setup() {
  Serial.begin(115200);

  loadTimerSettings();

  pinMode(BUZZER, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  randomSeed(analogRead(A0));
  lcd.init();
  lcd.backlight();

  if (!mpu.begin()) {
    Serial.println("ไม่พบ MPU6050");
  } else {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    isFlippedUpper = (a.acceleration.z < -4.0);
  }

  initMQTTTopics();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  updateLCDStandby();
}

void loop() {
  // 1. ถ้าระบบจับคู่กับแอปแล้ว (isPaired) ให้คอยเช็คและรักษาการเชื่อมต่อเน็ตไว้ตลอดเวลา
  // เพื่อรอรับคำสั่ง Setting ได้ตลอดแม้ไม่ได้อยู่ในโหมด Pairing
  if (isPaired) {
    if (!client.connected()) {
      connectIfNeeded();
    }
  }

  // 2. ถ้าเชื่อมต่อเน็ตอยู่ ให้ฟังก์ชัน MQTT ทำงานเพื่อรอรับข้อความ
  if (client.connected()) {
    client.loop();
  }

  unsigned long currentMillis = millis();

  checkButton();       
  checkOrientation();  

  // 3. โหมด Pairing: วนรอบส่งรหัส ทุกๆ 30 วินาที
  if (isPairingActive) {
    if (!isPairingPaused && (currentMillis - lastPairingPublishTime >= PAIRING_PUBLISH_INTERVAL_MS)) {
      lastPairingPublishTime = currentMillis;
      sendPairingCode();
    }
  }
  // 4. โหมดจับเวลา: นับถอยหลัง และส่งข้อมูลเวลาผ่าน MQTT ทุกๆ 1 วินาที (ถ้าเน็ตต่ออยู่)
  else if (isStarted) {
    unsigned long elapsedTime = currentMillis - previousMillis;

    if (elapsedTime >= stateDuration) {
      previousMillis = currentMillis;
      if (currentState == FOCUS) startBreakState();
      else startFocusState();
      elapsedTime = 0;
    }

    unsigned long remainingMillis = stateDuration - elapsedTime;
    unsigned long totalSeconds    = remainingMillis / 1000;
    unsigned int minutes  = totalSeconds / 60;
    unsigned int seconds  = totalSeconds % 60;

    updateLCDTimer(minutes, seconds);

    if (client.connected() && (currentMillis - lastDataPublishTime >= 1000)) {
      lastDataPublishTime = currentMillis;
      sendTimerPayload(totalSeconds);
    }
  }

  delay(20); 
}
