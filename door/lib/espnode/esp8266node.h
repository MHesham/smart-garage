#pragma once

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <stdint.h>
#include <functional>

namespace espnode {

#define CA_CERT_FILENAME "/ca.crt.pem"
#define MQTT_CONFIG_FILENAME "/mqtt.config"
#define WIFI_CONFIG_FILENAME "/wifi.config"
#define CONFIG_FILENAME "/config.json"

#define LOG_FMT_HELPER(FMT, ...) PSTR(FMT), __VA_ARGS__

#define LOG(...) log_P(LOG_FMT_HELPER(__VA_ARGS__, ""))

struct NodeConfig {
  char ssid[16];
  char wifiPassword[16];
  char hostname[16];
  uint16_t otaPort;
  char otaPasswordMd5Hash[34];
  char mqttBrokerHostname[32];
  uint16_t mqttPort;
  char mqttUser[16];
  char mqttPassword[16];
};

class Esp8266Node {
 public:
  Esp8266Node();
  virtual ~Esp8266Node() {}
  virtual void setup();
  virtual void loop();
  void mqttCallback(char *topic, uint8_t *payload, unsigned int length);

  typedef std::function<void(uint8_t *, unsigned int)> TopicHandler;

 protected:
  void fail();
  void onMqttConnect();
  void subscribe(const String &topic, TopicHandler handler);
  PubSubClient &getMqttClient() { return mqttClient; }

  template <class... T>
  void log_P(const char *fmtP, T... args) {
    char buffer[64];
    sprintf_P(buffer, fmtP, args...);
    Serial.println(buffer);
    if (mqttClient.connected()) {
      mqttClient.publish(logTopic, buffer, false);
    }
  }
  void logTime();

 private:
  void connectWiFi();
  void connectMqtt();
  void loadConfig();
  void setupOTA();
  void setClock();
  void loadDataFromFlash();
  struct TopicRegistryNode {
    TopicRegistryNode(const String &t, TopicHandler h)
        : topic(t), handler(h), next(nullptr) {}
    String topic;
    TopicHandler handler;
    TopicRegistryNode *next;
  };

  BearSSL::WiFiClientSecure wifiClient;
  BearSSL::X509List caCert;
  PubSubClient mqttClient;
  NodeConfig config;
  TopicRegistryNode *registryHead;
  char logTopic[32];
};
}  // namespace espnode