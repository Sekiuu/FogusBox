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
    _lastFlipCheck(0) {
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
  }

  // ตั้งค่า Device ID และ MQTT Topics
  _deviceId = getMacAddress();
  _statusTopic  = "fogus-box/" + _deviceId + "/status";
  _settingTopic = "fogus-box/" + _deviceId + "/setting";
  Serial.println("Device ID: " + _deviceId);

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
  Serial.println(">>> บันทึกค่าเวลาใหม่ลงใน Flash Memory เรียบร้อยแล้ว! <<<");
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

void FogusBox::sendPairingCode() {
  if (_isPairingPaused) return;
  int randomNum = random(0, 10000);
  snprintf(_pairingCode, sizeof(_pairingCode), "%04d", randomNum);
  showPairing();
  if (!connectNetwork()) return;

  StaticJsonDocument<256> doc;
  doc["code"]      = _pairingCode;
  doc["device_id"] = _deviceId;
  char jsonBuffer[256];
  serializeJson(doc, jsonBuffer);
  _mqttClient.publish(_pairingTopic, jsonBuffer);
}

void FogusBox::sendConnectedStatus() {
  StaticJsonDocument<384> doc;
  doc["status"] = "connected";
  JsonObject settings = doc.createNestedObject("settings");
  settings["work"]  = _focusTimeMs / 1000UL;
  settings["break"] = _breakTimeMs / 1000UL;
  JsonObject runtime = doc.createNestedObject("runtime");
  runtime["time_remain"] = 0;
  char jsonBuffer[384];
  serializeJson(doc, jsonBuffer);
  _mqttClient.publish(_statusTopic.c_str(), jsonBuffer);
}

void FogusBox::sendSettingSuccess() {
  StaticJsonDocument<256> doc;
  doc["status"] = "success";
  JsonObject setting = doc.createNestedObject("setting");
  setting["work"]  = _focusTimeMs / 1000UL;
  setting["break"] = _breakTimeMs / 1000UL;
  char jsonBuffer[256];
  serializeJson(doc, jsonBuffer);
  _mqttClient.publish(_statusTopic.c_str(), jsonBuffer);
}

void FogusBox::sendTimerStatus(unsigned long remainingSeconds) {
  StaticJsonDocument<384> doc;
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
      _mqttClient.subscribe(_settingTopic.c_str());
      _mqttClient.subscribe(_statusTopic.c_str());
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

void FogusBox::handleMqttMessage(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  if (message.equalsIgnoreCase("connected")) {
    _isPaired = true;
    _isPairingActive = false;
    _isPairingPaused = false;
    beep(2);
    sendConnectedStatus();
    showStandby();
    return;
  }
  if (message.equalsIgnoreCase("disconnected") || message.equalsIgnoreCase("unpair")) {
    _isPaired = false;
    _isPairingActive = true;
    _isPairingPaused = false;
    _isStarted = false;
    beep(1);
    sendPairingCode();
    _lastPairingPublishTime = millis();
    return;
  }
  if (String(topic) == _settingTopic) {
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    if (!error) {
      bool updated = false;
      if (doc.containsKey("work_time")) { _focusTimeMs = doc["work_time"].as<unsigned long>() * 1000UL; updated = true; }
      else if (doc.containsKey("work")) { _focusTimeMs = doc["work"].as<unsigned long>() * 1000UL; updated = true; }
      if (doc.containsKey("break_time")) { _breakTimeMs = doc["break_time"].as<unsigned long>() * 1000UL; updated = true; }
      else if (doc.containsKey("break")) { _breakTimeMs = doc["break"].as<unsigned long>() * 1000UL; updated = true; }

      if (updated) {
        _lcd.clear();
        _lcd.setCursor(0, 0);
        _lcd.print("Applying Setting");
        _lcd.setCursor(0, 1);
        _lcd.print("Please wait...");
        delay(1000);

        saveSettings();
        _isPaired = true;
        _isPairingActive = false;
        _isStarted = false;

        beep(2);
        sendSettingSuccess();
        showSettingUpdated();
      }
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
  if (millis() - _lastFlipCheck < 200) return;
  _lastFlipCheck = millis();

  sensors_event_t a, g, temp;
  _mpu.getEvent(&a, &g, &temp);

  bool currentFlipped = (a.acceleration.z < -4.0);
  if (currentFlipped != _isFlippedUpper) {
    _isFlippedUpper = currentFlipped;
    _previousTimerMillis = millis();
    if (!_isStarted) {
      _isStarted = true;
      startFocus();
    } else {
      if (_currentState == BOX_STATE_FOCUS) startBreak();
      else startFocus();
    }
  }
}

void FogusBox::update() {
  if (_isPaired && !_mqttClient.connected()) connectNetwork();
  if (_mqttClient.connected()) _mqttClient.loop();

  unsigned long currentMillis = millis();

  checkButton();
  checkOrientation();

  // โหมด Pairing
  if (_isPairingActive) {
    if (!_isPairingPaused && (currentMillis - _lastPairingPublishTime >= 30000UL)) {
      _lastPairingPublishTime = currentMillis;
      sendPairingCode();
    }
  }
  // โหมดจับเวลา Pomodoro
  else if (_isStarted) {
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

    // ส่งข้อมูลสถานะ MQTT ทุกๆ 1 วินาที
    if (_mqttClient.connected() && (currentMillis - _lastDataPublishTime >= 1000)) {
      _lastDataPublishTime = currentMillis;
      unsigned long remaining = (_stateDurationMs - elapsedTime) / 1000;
      sendTimerStatus(remaining);
    }
  }

  delay(20);
}
