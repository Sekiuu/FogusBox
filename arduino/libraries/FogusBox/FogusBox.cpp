#include "FogusBox.h"

static FogusBox* s_fogusBoxInstance = nullptr;

static void globalMqttCallback(char* topic, byte* payload, unsigned int length) {
  if (s_fogusBoxInstance != nullptr) {
    s_fogusBoxInstance->handleMqttMessage(topic, payload, length);
  }
}

FogusBox::FogusBox(LiquidCrystal_I2C &lcd, uint8_t buzzerPin, uint8_t buttonPin)
  : _lcd(lcd),
    _buzzerPin(buzzerPin),
    _buttonPin(buttonPin),
    _mqttClient(_espClient),
    _ssid(""),
    _password(""),
    _mqttServer("broker.hivemq.com"),
    _mqttPort(1883),
    _deviceId(""),
    _statusTopic(""),
    _settingTopic(""),
    _pairingTopic("fogus-box/pairing"),
    _isPaired(false),
    _isPairingActive(false),
    _isPairingPaused(false),
    _lastPairingPublishTime(0),
    _lastPingTime(0),
    _lastPongTime(0),
    _focusTimeMs(5000UL),
    _breakTimeMs(300000UL),
    _stateDurationMs(5000UL),
    _previousTimerMillis(0),
    _lastDataPublishTime(0),
    _currentState(BOX_STATE_FOCUS),
    _isStarted(false),
    _displaySuppressed(false),
    _buttonState(HIGH),
    _lastButtonState(HIGH),
    _lastDebounceTime(0),
    _debounceDelay(50),
    _isFlippedUpper(false),
    _pendingFlipped(false),
    _lastFlipCheck(0),
    _flipDebounceStartTime(0),
    _lastFlipTriggerTime(0),
    _flipDebounceMs(350UL),
    _flipCooldownMs(2000UL) {
  strcpy(_pairingCode, "0000");
}

void FogusBox::begin(const char* ssid, const char* password, const char* mqttServer, int mqttPort) {
  s_fogusBoxInstance = this;
  _ssid = ssid;
  _password = password;
  _mqttServer = mqttServer;
  _mqttPort = mqttPort;

  pinMode(_buzzerPin, OUTPUT);
  pinMode(_buttonPin, INPUT_PULLUP);

  randomSeed(analogRead(A0));
  _lcd.init();
  _lcd.backlight();

  loadSettings();

  // ตั้งค่า MPU6050
  if (!_mpu.begin()) {
    Serial.println("ไม่พบ MPU6050");
  } else {
    _mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    sensors_event_t a, g, temp;
    _mpu.getEvent(&a, &g, &temp);
    _isFlippedUpper = (a.acceleration.z < -4.0);
    _pendingFlipped = _isFlippedUpper;
    _flipDebounceStartTime = millis();
  }

  // กำหนด Topic ตามข้อกำหนดใน docs/dynamicpairingcode.md
  // {project_name}/{device_id}/status และ {project_name}/{device_id}/settings
  _deviceId = getMacAddress();
  _statusTopic  = "fogus-box/" + _deviceId + "/status";
  _settingTopic = "fogus-box/" + _deviceId + "/settings";
  Serial.println("Device ID: " + _deviceId);
  Serial.println("Status Topic:  " + _statusTopic);
  Serial.println("Setting Topic: " + _settingTopic);

  _mqttClient.setServer(_mqttServer, _mqttPort);
  _mqttClient.setCallback(globalMqttCallback);

  showStandby();
}

String FogusBox::getMacAddress() {
  byte mac[6];
  WiFi.macAddress(mac);
  char macBuf[18];
  snprintf(macBuf, sizeof(macBuf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macBuf);
}

void FogusBox::saveSettings() {
  _preferences.begin("timer_cfg", false);
  _preferences.putULong("work", _focusTimeMs);
  _preferences.putULong("break", _breakTimeMs);
  _preferences.end();
  _stateDurationMs = _focusTimeMs;
  Serial.println(">>> บันทึกค่าเวลาใหม่ลงใน Flash Memory เรียบร้อยแล้ว! (Work: " + String(_focusTimeMs / 1000UL) + "s, Break: " + String(_breakTimeMs / 1000UL) + "s) <<<");
}

void FogusBox::loadSettings() {
  _preferences.begin("timer_cfg", true);
  _focusTimeMs = _preferences.getULong("work", 5000UL);
  _breakTimeMs = _preferences.getULong("break", 300000UL);
  _preferences.end();
  _stateDurationMs = _focusTimeMs;
}

void FogusBox::beep(int count) {
  for (int i = 0; i < count; i++) {
    tone(_buzzerPin, 1000);
    delay(120);
    noTone(_buzzerPin);
    delay(80);
  }
}

void FogusBox::showStandby() {
  _lcd.clear();
  _lcd.setCursor(0, 0);
  _lcd.print("FLIP BOX TO     ");
  _lcd.setCursor(0, 1);
  _lcd.print("START TIMER     ");
}

void FogusBox::showSettingUpdated() {
  _lcd.clear();
  _lcd.setCursor(0, 0);
  _lcd.print("SETTING UPDATED ");
  _lcd.setCursor(0, 1);
  _lcd.print("FLIP TO START   ");
}

void FogusBox::showTimer(unsigned int minutes, unsigned int seconds) {
  _lcd.setCursor(0, 1);
  _lcd.print("Time Left: ");
  if (minutes < 10) _lcd.print("0");
  _lcd.print(minutes);
  _lcd.print(":");
  if (seconds < 10) _lcd.print("0");
  _lcd.print(seconds);
  _lcd.print(" ");
}

void FogusBox::showPairing() {
  _lcd.clear();
  if (_isPairingPaused) {
    _lcd.setCursor(0, 0);
    _lcd.print("PAIRING PAUSED ");
    _lcd.setCursor(0, 1);
    _lcd.print("Press To Resume ");
  } else {
    _lcd.setCursor(0, 0);
    _lcd.print("STATE: PAIRING ");
    _lcd.setCursor(0, 1);
    _lcd.print("CODE: ");
    _lcd.print(_pairingCode);
    _lcd.print("      ");
  }
}

void FogusBox::showFocus() {
  _lcd.clear();
  _lcd.setCursor(0, 0);
  _lcd.print("STATE: FOCUS   ");
}

void FogusBox::showBreak() {
  _lcd.clear();
  _lcd.setCursor(0, 0);
  _lcd.print("BREAK  D3=Game ");
}

void FogusBox::suppressTimerDisplay(bool suppress) {
  _displaySuppressed = suppress;
}

bool FogusBox::isTimerDisplaySuppressed() const {
  return _displaySuppressed;
}

void FogusBox::startFocus() {
  _currentState = BOX_STATE_FOCUS;
  _stateDurationMs = _focusTimeMs;
  showFocus();
  beep(2);
}

void FogusBox::startBreak() {
  _currentState = BOX_STATE_BREAK;
  _stateDurationMs = _breakTimeMs;
  showBreak();
  beep(3);
}

void FogusBox::stopTimer() {
  _isStarted = false;
  showStandby();
}

void FogusBox::startPairing() {
  _isPairingActive = true;
  _isPairingPaused = false;
  _isStarted = false;
  Serial.println("\n>>> เข้าสู่ Pairing Mode <<<");
  beep(1);
  sendPairingCode();
  _lastPairingPublishTime = millis();
}

void FogusBox::stopPairing() {
  _isPairingActive = false;
  _isPairingPaused = false;
  Serial.println("\n>>> ออกจาก Pairing Mode <<<");
  beep(1);
  showStandby();
}

bool FogusBox::isStarted() const { return _isStarted; }
bool FogusBox::isPaired() const { return _isPaired; }
bool FogusBox::isPairingActive() const { return _isPairingActive; }
bool FogusBox::isBreak() const { return _currentState == BOX_STATE_BREAK; }
bool FogusBox::isFocus() const { return _currentState == BOX_STATE_FOCUS; }
BoxState FogusBox::getState() const { return _currentState; }

unsigned long FogusBox::getWorkTime() const { return _focusTimeMs; }
unsigned long FogusBox::getBreakTime() const { return _breakTimeMs; }
void FogusBox::setWorkTime(unsigned long ms) { _focusTimeMs = ms; }
void FogusBox::setBreakTime(unsigned long ms) { _breakTimeMs = ms; }

void FogusBox::setFlipDelay(unsigned long ms) { _flipCooldownMs = ms; }
unsigned long FogusBox::getFlipDelay() const { return _flipCooldownMs; }

void FogusBox::setFlipCooldown(unsigned long ms) { _flipCooldownMs = ms; }
unsigned long FogusBox::getFlipCooldown() const { return _flipCooldownMs; }

void FogusBox::setFlipDebounce(unsigned long ms) { _flipDebounceMs = ms; }
unsigned long FogusBox::getFlipDebounce() const { return _flipDebounceMs; }

// ============================================================================
// ขั้นตอนที่ 1: การ Pairing (จับคู่) ตาม docs/dynamicpairingcode.md
// ส่ง Payload: {"device_id": "mac_address", "code": "8492"} เข้า {project_name}/pairing ทุก 30 วินาที
// ============================================================================
void FogusBox::sendPairingCode() {
  if (_isPairingPaused) return;
  int randomNum = random(0, 10000);
  snprintf(_pairingCode, sizeof(_pairingCode), "%04d", randomNum);
  showPairing();
  if (!connectNetwork()) return;

  StaticJsonDocument<256> doc;
  doc["device_id"] = _deviceId;
  doc["code"]      = _pairingCode;
  char jsonBuffer[256];
  serializeJson(doc, jsonBuffer);
  _mqttClient.publish(_pairingTopic, jsonBuffer);
  Serial.println(">>> ประกาศ Pairing: " + String(jsonBuffer) + " บน " + String(_pairingTopic) + " <<<");
}

void FogusBox::sendConnectedStatus() {
  StaticJsonDocument<384> doc;
  doc["status"] = "success";
  doc["state"]  = "connected";
  JsonObject settings = doc.createNestedObject("settings");
  settings["work_time"]  = _focusTimeMs / 1000UL;
  settings["break_time"] = _breakTimeMs / 1000UL;
  settings["work"]       = _focusTimeMs / 1000UL;
  settings["break"]      = _breakTimeMs / 1000UL;
  JsonObject runtime = doc.createNestedObject("runtime");
  runtime["time_remain"] = 0;
  char jsonBuffer[384];
  serializeJson(doc, jsonBuffer);
  _mqttClient.publish(_statusTopic.c_str(), jsonBuffer);
}

// ============================================================================
// ขั้นตอนที่ 3: การยืนยัน (ACK) หลังการตั้งค่า ตาม docs/dynamicpairingcode.md
// ส่งกลับไปที่ {project_name}/{device_id}/status
// โครงสร้าง: {"status": "success", "setting": {"work_time": 25, "break_time": 5}}
// ============================================================================
void FogusBox::sendSettingSuccess() {
  StaticJsonDocument<384> doc;
  doc["status"]  = "success";
  doc["message"] = "ตั้งค่าเสร็จแล้ว";

  JsonObject setting = doc.createNestedObject("setting");
  setting["work_time"]  = _focusTimeMs / 1000UL;
  setting["break_time"] = _breakTimeMs / 1000UL;
  setting["work"]       = _focusTimeMs / 1000UL;
  setting["break"]      = _breakTimeMs / 1000UL;

  JsonObject settings = doc.createNestedObject("settings");
  settings["work_time"]  = _focusTimeMs / 1000UL;
  settings["break_time"] = _breakTimeMs / 1000UL;
  settings["work"]       = _focusTimeMs / 1000UL;
  settings["break"]      = _breakTimeMs / 1000UL;

  char jsonBuffer[384];
  serializeJson(doc, jsonBuffer);

  // ส่งกลับไปยัง {project_name}/{device_id}/status ตามข้อกำหนดใน docs/dynamicpairingcode.md
  _mqttClient.publish(_statusTopic.c_str(), jsonBuffer);

  Serial.println(">>> [ACK] ตอบกลับว่าตั้งค่าเสร็จแล้ว ไปยัง " + _statusTopic + ": " + String(jsonBuffer) + " <<<");
}

void FogusBox::sendTimerStatus(unsigned long remainingSeconds) {
  StaticJsonDocument<384> doc;
  doc["state"] = "ping";
  JsonObject config = doc.createNestedObject("config");
  config["work"]  = _focusTimeMs / 1000UL;
  config["break"] = _breakTimeMs / 1000UL;
  JsonObject runtime = doc.createNestedObject("runtime");
  runtime["status"]      = (_currentState == BOX_STATE_FOCUS) ? "work" : "break";
  runtime["remain_time"] = remainingSeconds;
  char jsonBuffer[384];
  serializeJson(doc, jsonBuffer);
  _mqttClient.publish(_statusTopic.c_str(), jsonBuffer);
}

void FogusBox::reconnectMqtt() {
  while (!_mqttClient.connected()) {
    _lcd.clear();
    _lcd.setCursor(0, 0);
    _lcd.print("MQTT Connecting");
    String clientId = "UNO_R4_Timer_";
    clientId += String(random(0xffff), HEX);
    if (_mqttClient.connect(clientId.c_str())) {
      _lcd.setCursor(0, 1);
      _lcd.print("MQTT Connected!");
      delay(1000);

      // Subscribe topics ตามข้อกำหนดใน docs/dynamicpairingcode.md
      _mqttClient.subscribe(_settingTopic.c_str()); // fogus-box/{device_id}/settings
      String settingSingular = "fogus-box/" + _deviceId + "/setting";
      _mqttClient.subscribe(settingSingular.c_str()); // fogus-box/{device_id}/setting
      _mqttClient.subscribe(_statusTopic.c_str());  // fogus-box/{device_id}/status

      Serial.println("Subscribed to:");
      Serial.println(" - " + _settingTopic);
      Serial.println(" - " + settingSingular);
      Serial.println(" - " + _statusTopic);
    } else {
      _lcd.setCursor(0, 1);
      _lcd.print("MQTT Error!");
      delay(3000);
    }
  }
}

bool FogusBox::connectNetwork() {
  if (WiFi.status() != WL_CONNECTED) {
    _lcd.clear();
    _lcd.setCursor(0, 0);
    _lcd.print("WIFI Connecting");
    _lcd.setCursor(0, 1);
    WiFi.begin(_ssid, _password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      _lcd.print(".");
      delay(500);
      attempts++;
    }
    if (WiFi.status() != WL_CONNECTED) {
      _lcd.clear();
      _lcd.setCursor(0, 0);
      _lcd.print("WIFI Error!");
      delay(2000);
      return false;
    }
    _lcd.clear();
    _lcd.setCursor(0, 0);
    _lcd.print("WIFI Connected!");
    delay(1000);
  }
  if (!_mqttClient.connected()) {
    reconnectMqtt();
  }
  return _mqttClient.connected();
}

// ============================================================================
// จัดการข้อความ MQTT ขาเข้า (Callback)
// ============================================================================
void FogusBox::handleMqttMessage(char* topic, byte* payload, unsigned int length) {
  String topicStr = String(topic);
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  // --------------------------------------------------------------------------
  // 1. ข้อความใน Topic: {project_name}/{device_id}/status
  // --------------------------------------------------------------------------
  if (topicStr == _statusTopic) {
    // ขั้นตอนที่ 1 (ข้อ 5-6): Web ส่ง "connected" เพื่อบอก device ว่าจับคู่สำเร็จแล้ว
    if (message.equalsIgnoreCase("connected") || message.indexOf("\"status\":\"connected\"") >= 0 || message.indexOf("connected") >= 0) {
      _isPaired = true;
      _isPairingActive = false;
      _isPairingPaused = false;
      _lastPongTime = millis(); // เริ่มนับ Heartbeat
      beep(2);
      sendConnectedStatus();
      showStandby();
      Serial.println(">>> [Pairing] หน้าเว็บเชื่อมต่อสำเร็จแล้ว! (Status: Connected) <<<");
      return;
    }

    // ขั้นตอนที่ 2 (ข้อ 3): หน้าเว็บตอบกลับ pong ใน status topic
    if (message.equalsIgnoreCase("pong") || message.indexOf("pong") >= 0) {
      _lastPongTime = millis(); // บันทึกเวลาที่ได้รับ pong ล่าสุด
      Serial.println(">>> [Heartbeat] ได้รับ pong จากหน้าเว็บเรียบร้อย <<<");
      return;
    }

    // ข้อความสั่ง Disconnected หรือ Unpair
    if (message.equalsIgnoreCase("disconnected") || message.equalsIgnoreCase("unpair") || message.indexOf("disconnect") >= 0) {
      _isPaired = false;
      _isPairingActive = false;
      _isPairingPaused = false;
      _isStarted = false;
      beep(1);
      showStandby();
      Serial.println(">>> ได้รับคำสั่ง Disconnected จาก Web <<<");
      return;
    }
  }

  // --------------------------------------------------------------------------
  // 2. ข้อความใน Topic: {project_name}/{device_id}/settings (ขั้นตอนที่ 3)
  // Web-UI ส่ง JSON เช่น {"work_time": 25, "break_time": 5}
  // --------------------------------------------------------------------------
  String settingSingular = "fogus-box/" + _deviceId + "/setting";
  if (topicStr == _settingTopic || topicStr == settingSingular) {
    Serial.println(">>> [Settings] ได้รับคำสั่งตั้งค่าจาก Web: " + message + " <<<");

    StaticJsonDocument<384> doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    if (!error) {
      // ป้องกันการวนลูป หากข้อความเป็นข้อความ ACK ที่มี status: success
      if (doc.containsKey("status") && doc["status"] == "success") {
        return;
      }

      bool updated = false;

      // ดึงค่าจาก JSON (รองรับทั้งแบบ nested object และแบบ flat key)
      JsonObject root = doc.containsKey("setting") ? doc["setting"].as<JsonObject>() :
                        (doc.containsKey("settings") ? doc["settings"].as<JsonObject>() : doc.as<JsonObject>());

      if (root.containsKey("work_time")) {
        _focusTimeMs = root["work_time"].as<unsigned long>() * 1000UL;
        updated = true;
      } else if (root.containsKey("work")) {
        _focusTimeMs = root["work"].as<unsigned long>() * 1000UL;
        updated = true;
      }

      if (root.containsKey("break_time")) {
        _breakTimeMs = root["break_time"].as<unsigned long>() * 1000UL;
        updated = true;
      } else if (root.containsKey("break")) {
        _breakTimeMs = root["break"].as<unsigned long>() * 1000UL;
        updated = true;
      }

      if (updated) {
        // 1. บันทึกลง Storage (Flash Memory) ทันที
        saveSettings();
        _isPaired = true;
        _isPairingActive = false;
        _isStarted = false;
        _lastPongTime = millis();

        // 2. ส่ง ACK ตอบกลับทันทีว่า "ตั้งค่าเสร็จแล้ว" (status: success) ตามข้อกำหนด
        sendSettingSuccess();

        // 3. แสดงผลยืนยันบนหน้าจอ LCD และส่งเสียงแจ้งเตือน
        _lcd.clear();
        _lcd.setCursor(0, 0);
        _lcd.print("SETTING SAVED!  ");
        _lcd.setCursor(0, 1);
        _lcd.print("W:");
        _lcd.print(_focusTimeMs / 1000UL);
        _lcd.print("s B:");
        _lcd.print(_breakTimeMs / 1000UL);
        _lcd.print("s   ");

        beep(2);
        delay(1500);

        showSettingUpdated();
      }
    } else {
      Serial.println(">>> เกิดข้อผิดพลาดในการ Deserialize JSON Settings <<<");
    }
  }
}

void FogusBox::checkButton() {
  int reading = digitalRead(_buttonPin);
  if (reading != _lastButtonState) {
    _lastDebounceTime = millis();
  }
  if ((millis() - _lastDebounceTime) > _debounceDelay) {
    if (reading != _buttonState) {
      _buttonState = reading;
      if (_buttonState == LOW) {
        if (!_isPairingActive) {
          startPairing();
        } else {
          stopPairing();
        }
      }
    }
  }
  _lastButtonState = reading;
}

void FogusBox::checkOrientation() {
  if (_isPairingActive) return;

  unsigned long currentMillis = millis();

  // 1. ตรวจสอบ Cooldown: ถ้าเพิ่งพลิกไปไม่ถึง _flipCooldownMs (เช่น 2 วินาที) ให้ล็อกไว้ไม่ให้พลิกซ้ำ
  if (currentMillis - _lastFlipTriggerTime < _flipCooldownMs) {
    _lastFlipCheck = currentMillis;
    return;
  }

  if (currentMillis - _lastFlipCheck < 50) return;
  _lastFlipCheck = currentMillis;

  sensors_event_t a, g, temp;
  _mpu.getEvent(&a, &g, &temp);

  // 2. ใช้ Hysteresis เพื่อตัดการแกว่งของเซนเซอร์ขณะกำลังหมุนกล่องในอากาศ
  // คว่ำ: Z < -3.5 m/s²
  // หงาย: Z > 3.5 m/s²
  // ระหว่าง -3.5 ถึง 3.5 คือช่วงกำลังหมุนกล่อง (Deadband) จะคงทิศทางเดิมไว้
  bool currentFlipped = _pendingFlipped;
  if (a.acceleration.z < -3.5) {
    currentFlipped = true;
  } else if (a.acceleration.z > 3.5) {
    currentFlipped = false;
  }

  // 3. หากทิศทางเปลี่ยนไป ให้เริ่มนับเวลา Debounce
  if (currentFlipped != _pendingFlipped) {
    _pendingFlipped = currentFlipped;
    _flipDebounceStartTime = currentMillis;
  }

  // 4. ต้องนิ่งอยู่ในทิศทางใหม่ต่อเนื่องอย่างน้อย _flipDebounceMs (350 ms)
  // และทิศทางนั้นต้องต่างจากทิศทางปัจจุบัน (_isFlippedUpper)
  if ((currentMillis - _flipDebounceStartTime >= _flipDebounceMs) && (_pendingFlipped != _isFlippedUpper)) {
    _isFlippedUpper = _pendingFlipped;
    _lastFlipTriggerTime = currentMillis; // เริ่มล็อก Cooldown ป้องกันการเด้งซ้ำ
    _previousTimerMillis = currentMillis;

    Serial.println(">>> ตรวจพบการพลิกกล่องที่เสถียร (Cooldown " + String(_flipCooldownMs) + "ms) สลับโหมดเวลา <<<");

    if (!_isStarted) {
      _isStarted = true;
      startFocus();
    } else {
      if (_currentState == BOX_STATE_FOCUS) {
        startBreak();
      } else {
        startFocus();
      }
    }
  }
}

void FogusBox::update() {
  if (_isPaired && !_mqttClient.connected()) connectNetwork();
  if (_mqttClient.connected()) _mqttClient.loop();

  unsigned long currentMillis = millis();

  checkButton();
  checkOrientation();

  // --------------------------------------------------------------------------
  // ขั้นตอนที่ 1: โหมด Pairing (ส่งรหัส 4 หลักเข้า {project_name}/pairing ทุก 30 วิ)
  // --------------------------------------------------------------------------
  if (_isPairingActive) {
    if (!_isPairingPaused && (currentMillis - _lastPairingPublishTime >= 30000UL)) {
      _lastPairingPublishTime = currentMillis;
      sendPairingCode();
    }
  }

  // --------------------------------------------------------------------------
  // ขั้นตอนที่ 2: การเช็คสถานะการเชื่อมต่อ (Status & Heartbeat) เมื่อเชื่อมต่อแล้ว
  // 1. ส่ง {"state": "ping"} ไปที่ status topic ทุกๆ 5 วินาที
  // 2. ถ้าไม่ได้รับ "pong" ตอบกลับนานเกิน 25 วินาที ให้ตัดการเชื่อมต่อ
  // --------------------------------------------------------------------------
  if (_isPaired) {
    if (currentMillis - _lastPingTime >= 5000UL) {
      _lastPingTime = currentMillis;

      StaticJsonDocument<256> pingDoc;
      pingDoc["state"] = "ping";

      // หากอยู่ในช่วงจับเวลา แนบสถานะเวลาถอยหลังไปด้วย
      if (_isStarted) {
        unsigned long elapsedTime = currentMillis - _previousTimerMillis;
        unsigned long remaining = (_stateDurationMs > elapsedTime) ? (_stateDurationMs - elapsedTime) / 1000UL : 0;
        JsonObject runtime = pingDoc.createNestedObject("runtime");
        runtime["status"] = (_currentState == BOX_STATE_FOCUS) ? "work" : "break";
        runtime["remain_time"] = remaining;
      }

      char pingBuf[256];
      serializeJson(pingDoc, pingBuf);
      _mqttClient.publish(_statusTopic.c_str(), pingBuf);
    }

    // ตรวจสอบ Timeout ของ Pong (นานเกิน 25 วินาที)
    if (_lastPongTime > 0 && (currentMillis - _lastPongTime > 25000UL)) {
      Serial.println(">>> ไม่ได้รับ Pong จาก Web นานเกินกำหนด! ยกเลิกการเชื่อมต่อและกลับสู่สถานะก่อน Pairing <<<");
      _isPaired = false;
      _isStarted = false;
      _isPairingActive = false;
      _lastPongTime = 0;
      beep(1);
      showStandby();
    }
  }

  // --------------------------------------------------------------------------
  // โหมดจับเวลา Pomodoro
  // --------------------------------------------------------------------------
  if (_isStarted) {
    unsigned long elapsedTime = currentMillis - _previousTimerMillis;

    // ตรวจสอบหมดเวลา
    if (elapsedTime >= _stateDurationMs) {
      _previousTimerMillis = currentMillis;
      if (_currentState == BOX_STATE_FOCUS) startBreak();
      else startFocus();
      elapsedTime = 0;
    }

    // แสดงผลเวลานับถอยหลัง (หากไม่ได้ถูกสั่งปิดการแสดงผล เช่น ตอนกำลังเล่นเกม)
    if (!_displaySuppressed) {
      unsigned long remainingMillis = _stateDurationMs - elapsedTime;
      unsigned long totalSeconds    = remainingMillis / 1000;
      unsigned int minutes  = totalSeconds / 60;
      unsigned int seconds  = totalSeconds % 60;
      showTimer(minutes, seconds);
    }
  }

  delay(20);
}
