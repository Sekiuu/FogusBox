#include "MyMqtt.h"
#include <ArduinoJson.h> // สำหรับจัดการ JSON Payload

class ProjectMqtt : public MyMqtt
{
private:
  String projectName;
  String deviceId;
  bool isPaired;
  String pairingCode;

  unsigned long lastPairingPublish;
  unsigned long lastStatusPublish;

public:
  ProjectMqtt(const char *ssid, const char *pass, const char *server, int port, String projName, String devId)
      : MyMqtt(ssid, pass, server, port, devId),
        projectName(projName), deviceId(devId), isPaired(false),
        lastPairingPublish(0), lastStatusPublish(0) {}

  void setPaired(bool paired)
  {
    isPaired = paired;
    if (isPaired)
    {
      // เมื่อจับคู่สำเร็จ ให้ Subscribe รอรับ Settings ทันที
      String settingsTopic = projectName + "/" + deviceId + "/settings";
      subscribe(settingsTopic.c_str());
      Serial.println("Device is now Paired! Subscribed to settings.");
    }
  }

  void generatePairingCode()
  {
    // สุ่มรหัส 4 หลัก (0000 - 9999)
    int codeInt = random(1000, 10000);
    pairingCode = String(codeInt);
    // เติม 0 ด้านหน้าให้ครบ 4 หลักกรณีเลขน้อยกว่า 1000
    while (pairingCode.length() < 4)
    {
      pairingCode = "0" + pairingCode;
    }
  }

  String getPairingCode()
  {
    return pairingCode;
  }

  void onConnected() override
  {
    Serial.println("MQTT Connected!");
    setPaired(true);
  }

  void onDisconnected() override
  {
    Serial.println("MQTT Disconnected!");
  }

  // Callback เมื่อมีข้อความส่งมา (เช่น Web ส่ง Settings มา)
  void onMessage(String topic, String message) override
  {
    Serial.print("Received on [");
    Serial.print(topic);
    Serial.print("]: ");
    Serial.println(message);
  }

  JsonDocument ParseJson(String message)
  {
    // Parse JSON Settings จากเว็บ
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, message);
    if (error)
    {
      doc.clear();
      doc["state"] = "error";
      doc["message"] = "JSON parse failed!";
    }
    return doc;
  }

  // ฟังก์ชันส่ง Status & Heartbeat (ขั้นตอนที่ 2)
  void sendStatus(String state, int workTime, int breakTime)
  {
    if (!connected())
      return;

    String statusTopic = projectName + "/" + deviceId + "/status";

    JsonDocument doc;
    doc["state"] = state;

    JsonObject settings = doc["settings"].to<JsonObject>();
    settings["work_time"] = workTime;
    settings["break_time"] = breakTime;

    char buffer[256];
    serializeJson(doc, buffer);

    publish(statusTopic.c_str(), buffer);
  }

  // รันลูปจัดการ Pairing แยกต่างหากตอนยังไม่จับคู่
  void handlePairingMode()
  {
    if (isPaired)
      return;

    unsigned long now = millis();
    if (now - lastPairingPublish > 2500)
    { // ทุกๆ 2.5 วินาที
      lastPairingPublish = now;

      if (pairingCode == "")
      {
        generatePairingCode();
      }

      String pairingTopic = projectName + "/pairing";
      String payload = "{\"device_id\": \"" + deviceId + "\", \"code\": \"" + pairingCode + "\"}";

      publish(pairingTopic.c_str(), payload.c_str());
      Serial.print("Publishing Pairing Code: ");
      Serial.println(pairingCode);
    }
  }
};