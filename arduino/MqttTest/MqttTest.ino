#include "ProjectMqtt.h"

// กำหนดค่าโปรเจกต์
const char* WIFI_SSID = "HY9P";
const char* WIFI_PASSWORD = "1010101010";
const char* MQTT_SERVER = "broker.hivemq.com";
const int MQTT_PORT = 1883;

ProjectMqtt mqttClient(WIFI_SSID, WIFI_PASSWORD, MQTT_SERVER, MQTT_PORT, "fogus-box", "pbox_001");

unsigned long lastHeartbeat = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  mqttClient.begin();

  // จำลองว่ายังไม่จับคู่ตอนเปิดเครื่อง (สามารถเปลี่ยนเป็นอ่านค่าจาก EEPROM ได้)
  mqttClient.setPaired(false);
}

void loop() {
  // รันระบบ MQTT หลักเชื่อมต่อ Wi-Fi และฟังข้อความ
  mqttClient.loop();

  // ถ้ายังไม่จับคู่ ให้ส่ง Pairing Code ออกไปเรื่อยๆ (ขั้นที่ 1)
  // (คุณสามารถเช็คปุ่มกดบนบอร์ดเพื่อสลับสถานะ isPaired เป็น true ได้ เมื่อผู้ใช้กดปุ่มยืนยัน)
  mqttClient.handlePairingMode();

  // ถ้าจับคู่แล้ว ส่ง Heartbeat สถานะเครื่องทุกๆ 10 วินาที (ขั้นที่ 2)
  // if (mqttClient.isPaired) { ... }
  unsigned long now = millis();
  if (now - lastHeartbeat > 10000) {
    lastHeartbeat = now;
    mqttClient.sendStatus("idle", 25, 5);
  }
}