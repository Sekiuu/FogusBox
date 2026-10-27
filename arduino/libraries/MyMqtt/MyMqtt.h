#ifndef MY_MQTT_H
#define MY_MQTT_H

#include <Arduino.h>
#include <WiFiS3.h>
#include <PubSubClient.h>

class MyMqtt {
  private:
    const char* ssid;
    const char* password;
    const char* mqttServer;
    int mqttPort;
    String clientId;

    WiFiClient wifiClient;
    PubSubClient mqttClient;

    unsigned long lastReconnectAttempt;
    bool wasConnected;

    // Static instance pointer to handle global callback routing
    static MyMqtt* instance;
    static void callbackWrapper(char* topic, byte* payload, unsigned int length);

  public:
    MyMqtt(const char* ssid, const char* pass, const char* server, int port, String id);

    void begin();
    void loop();
    void connectWiFi();
    bool reconnectMQTT();
    bool connected();

    void publish(const char* topic, const char* message);

    void subscribe(const char* topic);

    // Event hooks to be overridden in your custom subclass or main code
    virtual void onConnected() {}
    virtual void onDisconnected() {}
    virtual void onMessage(String topic, String message) {}
};

#endif