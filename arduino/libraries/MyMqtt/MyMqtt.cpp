#include "MyMqtt.h"

// Initialize static pointer
MyMqtt* MyMqtt::instance = nullptr;

MyMqtt::MyMqtt(const char* ssid, const char* pass, const char* server, int port, const char* id)
  : ssid(ssid), password(pass), mqttServer(server), mqttPort(port), clientId(id),
    mqttClient(wifiClient), lastReconnectAttempt(0), wasConnected(false) {
  instance = this;
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(callbackWrapper);
}

void MyMqtt::begin() {
  connectWiFi();
}

void MyMqtt::connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi Connected! IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi Connection Failed. Retrying later...");
  }
}

void MyMqtt::loop() {
  // 1. Maintain Wi-Fi connection
  if (WiFi.status() != WL_CONNECTED) {
    if (wasConnected) {
      wasConnected = false;
      onDisconnected();
    }
    connectWiFi();
    return;
  }

  // 2. Maintain MQTT connection
  if (!mqttClient.connected()) {
    if (wasConnected) {
      wasConnected = false;
      onDisconnected();
    }

    unsigned long now = millis();
    if (now - lastReconnectAttempt > 5000) { // Try reconnecting every 5 seconds
      lastReconnectAttempt = now;
      if (reconnectMQTT()) {
        wasConnected = true;
        onConnected();
      }
    }
  } else {
    mqttClient.loop();
  }
}

bool MyMqtt::reconnectMQTT() {
  Serial.print("Attempting MQTT connection...");
  if (mqttClient.connect(clientId)) {
    Serial.println("connected");
  } else {
    Serial.print("failed, rc=");
    Serial.print(mqttClient.state());
    Serial.println(" try again in 5 seconds");
  }
  return mqttClient.connected();
}

void MyMqtt::publish(const char* topic, const char* message) {
  if (mqttClient.connected()) {
    mqttClient.publish(topic, message);
  } else {
    Serial.println("Publish failed: MQTT not connected.");
  }
}

void MyMqtt::subscribe(const char* topic) {
  if (mqttClient.connected()) {
    mqttClient.subscribe(topic);
    Serial.print("Subscribed to topic: ");
    Serial.println(topic);
  }
}

// Static wrapper to bridge PubSubClient C-style callback to C++ object
void MyMqtt::callbackWrapper(char* topic, byte* payload, unsigned int length) {
  if (instance != nullptr) {
    String topicStr = String(topic);
    String messageStr = "";
    for (unsigned int i = 0; i < length; i++) {
      messageStr += (char)payload[i];
    }
    instance->onMessage(topicStr, messageStr);
  }
}