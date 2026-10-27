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
const int BUTTON_PIN   = 2;  // ปุ่มหลัก (Pairing/ตั้งค่า)
const int GAME_BTN_PIN = 3;  // ปุ่มสำหรับเล่นเกมตอนพัก
const int BUZZER       = 8;  

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

// สถานะปุ่มกดหลัก
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
//  5. MINIGAME VARIABLES
// ============================================================================
bool isPlayingGame = false;
int gameState = 0; // 0 = หน้าเมนู, 1 = เล่นเกม Cave, 2 = เล่นเกม Star
int selectedGame = 1; 

unsigned long menuTimer = 0;
bool lastGameBtnState = HIGH;
bool gameBtnClicked = false;

// --- เกมที่ 1 (Cave Escape) ---
int playerRow1 = 1, obstacleX = 15, obstacleRow = 1, score1 = 0;
unsigned long lastTick1 = 0;
int oldPlayerRow1 = 1, oldObstacleX = 15, oldObstacleRow = 1;

// --- เกมที่ 2 (Star Catcher) ---
int playerX2 = 7, itemX2 = 0, itemRow2 = 0, itemType2 = 0; 
int score2 = 0, highScore2 = 0;
unsigned long dropTimer2 = 0, moveTimer2 = 0;
int oldPlayerX2 = 7, oldItemX2 = 0, oldItemRow2 = 0, oldScore2 = -1;

void resetGame(int gameId);
void saveHighScore();

// ============================================================================
//  6. PERSISTENCE (Flash memory)
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
  highScore2   = preferences.getInt("highscore", 0); // โหลด Highscore เกม
  preferences.end();
}

void saveHighScore() {
  preferences.begin("timer_cfg", false);
  preferences.putInt("highscore", highScore2);
  preferences.end();
}

// ============================================================================
//  7. DEVICE ID & MQTT TOPICS
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
//  8. BUZZER & DISPLAY 
// ============================================================================
void notifySound(int count) {
  for (int i = 0; i < count; i++) {
    tone(BUZZER, 1000);
    delay(120);
    noTone(BUZZER);
    delay(80);
  }
}

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
  isPlayingGame = false; // หมดเวลาพัก บังคับออกจากเกมทันที
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
  lcd.print("BREAK  D3=Game "); // แนะนำปุ่มเข้าเกม
  notifySound(3);
}

// ============================================================================
//  10. PAIRING & MQTT 
// ============================================================================
void sendPairingCode() {
  if (isPairingPaused) return;
  int randomNum = random(0, 10000);
  snprintf(currentCode, sizeof(currentCode), "%04d", randomNum);
  updateLCDPairing();
  if (!connectIfNeeded()) return;

  StaticJsonDocument<256> doc;
  doc["code"]      = currentCode;
  doc["device_id"] = deviceIdStr;
  char jsonBuffer[256];
  serializeJson(doc, jsonBuffer);
  client.publish(mqtt_pub_pairing_topic, jsonBuffer);
}

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
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Applying Setting");
        lcd.setCursor(0, 1);
        lcd.print("Please wait...");
        delay(1000); 

        saveTimerSettings(FOCUS_TIME, BREAK_TIME);
        isPaired = true; 
        isPairingActive = false;
        isStarted = false; 

        notifySound(2);
        sendSettingSuccessPayload();
        updateLCDReadyNewSetting();
      }
    }
  }
}

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
//  11. HARDWARE SENSING (MPU & BUTTONS)
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
          isPlayingGame = false; // ปิดเกมถ้าจะเข้า Pairing
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
//  12. MINIGAME LOGIC 
// ============================================================================
void runMenu() {
  // รับค่าการกดปุ่มมาจาก loop หลัก
  if (gameBtnClicked) {
    selectedGame = (selectedGame == 1) ? 2 : 1; 
    menuTimer = millis(); 
    lcd.clear(); 
  }

  lcd.setCursor(0, 0);
  lcd.print("Select Game:");
  lcd.setCursor(0, 1);
  if (selectedGame == 1) {
    lcd.print("> 1.Cave Escape ");
  } else {
    lcd.print("> 2.Star Catcher");
  }

  // ปรับเวลาตรงนี้ได้ครับ ถ้า 3000 (3 วินาที) เร็วไป ให้แก้เป็น 5000 
  if (millis() - menuTimer > 3000) {
    gameState = selectedGame;
    resetGame(gameState);
  }
}

void resetGame(int gameId) {
  lcd.clear();
  if (gameId == 1) {
    lcd.print("Starting Cave...");
    playerRow1 = 1; obstacleX = 15; score1 = 0;
    oldPlayerRow1 = 1; oldObstacleX = 15;
  } else {
    lcd.print("Star Catcher");
    lcd.setCursor(0, 1);
    lcd.print("High: "); lcd.print(highScore2);
    delay(1500);
    playerX2 = 7; score2 = 0; itemX2 = random(0, 13); itemRow2 = 0;
    oldPlayerX2 = 7; oldItemX2 = itemX2; oldItemRow2 = 0; oldScore2 = -1;
  }
  delay(1000);
  lcd.clear();
}

void runCaveEscape() {
  if (digitalRead(GAME_BTN_PIN) == LOW) playerRow1 = 0; 
  else playerRow1 = 1; 

  if (millis() - lastTick1 > 300) {
    lastTick1 = millis();
    obstacleX--;

    if (obstacleX < 0) {
      lcd.setCursor(0, obstacleRow);
      lcd.print(" "); 
      obstacleX = 15;
      obstacleRow = random(0, 2);
      score1++;
    }

    if (obstacleX == 1 && playerRow1 == obstacleRow) {
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("GAME OVER");
      lcd.setCursor(0, 1); lcd.print("Score: "); lcd.print(score1);
      delay(2000); 
      gameState = 0; menuTimer = millis(); lcd.clear();
      return;
    }

    if (oldPlayerRow1 != playerRow1) { lcd.setCursor(1, oldPlayerRow1); lcd.print(" "); }
    lcd.setCursor(1, playerRow1); lcd.print("P");

    if (oldObstacleX != obstacleX || oldObstacleRow != obstacleRow) {
      lcd.setCursor(oldObstacleX, oldObstacleRow); lcd.print(" ");
    }
    lcd.setCursor(obstacleX, obstacleRow); lcd.print("#");

    oldPlayerRow1 = playerRow1; oldObstacleX = obstacleX; oldObstacleRow = obstacleRow;
  }
}

void runStarCatcher() {
  if (millis() - moveTimer2 > 150) {
    moveTimer2 = millis();
    if (digitalRead(GAME_BTN_PIN) == LOW) { if (playerX2 < 15) playerX2++; } 
    else { if (playerX2 > 0) playerX2--; }
  }

  if (millis() - dropTimer2 > 400) {
    dropTimer2 = millis();
    itemRow2++;
    
    if (itemRow2 > 1) {
      if (itemX2 == playerX2) { 
        if (itemType2 == 0) { 
          score2++;
          if (score2 > highScore2) {
            highScore2 = score2;
            saveHighScore(); // บันทึก High score ลง Flash memory
          }
        } else { 
          lcd.clear();
          lcd.setCursor(0, 0); lcd.print("BOOM! GAME OVER");
          lcd.setCursor(0, 1); lcd.print("Score: "); lcd.print(score2);
          delay(2000);
          gameState = 0; menuTimer = millis(); lcd.clear();
          return;
        }
      }
      lcd.setCursor(itemX2, 1); lcd.print(" ");
      itemX2 = random(0, 13); itemType2 = (random(0, 4) == 0) ? 1 : 0; itemRow2 = 0;
    }
  }

  if (oldItemX2 != itemX2 || oldItemRow2 != itemRow2) {
    lcd.setCursor(oldItemX2, oldItemRow2); lcd.print(" ");
  }
  lcd.setCursor(itemX2, itemRow2); lcd.print(itemType2 == 0 ? "*" : "X");

  if (oldPlayerX2 != playerX2) {
    lcd.setCursor(oldPlayerX2, 1); lcd.print(" ");
  }
  lcd.setCursor(playerX2, 1); lcd.print("O");

  if (oldScore2 != score2) {
    lcd.setCursor(13, 0); if (score2 < 10) lcd.print("  "); 
    lcd.setCursor(13, 0); lcd.print(score2);
  }

  oldItemX2 = itemX2; oldItemRow2 = itemRow2; oldPlayerX2 = playerX2; oldScore2 = score2;
}

// ============================================================================
//  13. SETUP & LOOP
// ============================================================================
void setup() {
  Serial.begin(115200);

  loadTimerSettings();

  pinMode(BUZZER, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(GAME_BTN_PIN, INPUT_PULLUP); // ตั้งค่าปุ่มเกม

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
  if (isPaired && !client.connected()) connectIfNeeded();
  if (client.connected()) client.loop();

  unsigned long currentMillis = millis();

  checkButton();       
  checkOrientation();  

  // ตรวจจับปุ่มกด D3 (เช็คแค่จุดเดียว)
  bool gameBtnState = digitalRead(GAME_BTN_PIN);
  gameBtnClicked = (gameBtnState == LOW && lastGameBtnState == HIGH); // เช็คว่าเพิ่งกดปุ่มใช่ไหม
  lastGameBtnState = gameBtnState; // อัปเดตสถานะทันที

  // หากอยู่ในสถานะ Break เพื่อเริ่มเข้าเกมเมนู
  if (isStarted && currentState == BREAK && !isPlayingGame) {
    if (gameBtnClicked) {
      isPlayingGame = true;
      gameState = 0;
      selectedGame = 1;
      menuTimer = millis();
      lcd.clear();
      
      gameBtnClicked = false; // เคลียร์สถานะปุ่มทิ้ง ป้องกันไม่ให้เมนูสลับเกมเองตอนเพิ่งกดเข้าเกม
    }
  }

  // 3. โหมด Pairing
  if (isPairingActive) {
    if (!isPairingPaused && (currentMillis - lastPairingPublishTime >= PAIRING_PUBLISH_INTERVAL_MS)) {
      lastPairingPublishTime = currentMillis;
      sendPairingCode();
    }
  }
  // 4. โหมดจับเวลา 
  else if (isStarted) {
    unsigned long elapsedTime = currentMillis - previousMillis;

    // ----- ตรวจสอบหมดเวลา -----
    // หากหมดเวลาพัก ระบบจะกระโดดไป startFocusState() ทันที
    // และในนั้นจะสั่ง isPlayingGame = false ให้อัตโนมัติ (ตัดเกมออก)
    if (elapsedTime >= stateDuration) {
      previousMillis = currentMillis;
      if (currentState == FOCUS) startBreakState();
      else startFocusState();
      elapsedTime = 0;
    }

    // ----- แสดงผล Timer หรือ Minigame -----
    if (isPlayingGame && currentState == BREAK) {
      // เล่นเกมแทนการโชว์ Timer
      if (gameState == 0) runMenu();
      else if (gameState == 1) runCaveEscape();
      else if (gameState == 2) runStarCatcher();
    } else {
      // โชว์เวลา Timer ตามปกติ
      unsigned long remainingMillis = stateDuration - elapsedTime;
      unsigned long totalSeconds    = remainingMillis / 1000;
      unsigned int minutes  = totalSeconds / 60;
      unsigned int seconds  = totalSeconds % 60;

      updateLCDTimer(minutes, seconds);
    }

    // ----- ส่ง MQTT Data (เบื้องหลังทำงานต่อเสมอ) -----
    if (client.connected() && (currentMillis - lastDataPublishTime >= 1000)) {
      lastDataPublishTime = currentMillis;
      unsigned long remaining = (stateDuration - elapsedTime) / 1000;
      sendTimerPayload(remaining);
    }
  }

  delay(20); 
}
