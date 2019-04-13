#pragma once

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <stdint.h>
#include <functional>

namespace smartgarage {

#define CA_CERT_FILENAME "/ca.crt.pem"
#define MQTT_CONFIG_FILENAME "/mqtt.config"
#define WIFI_CONFIG_FILENAME "/wifi.config"

struct ConnectionConfig {
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
  void fail() {}
  void mqttCallback(char *topic, uint8_t *payload, unsigned int length);
  static void printTime();

  typedef std::function<void(uint8_t *, unsigned int)> TopicHandler;

 protected:
  void onMqttConnect();
  void connectWiFi();
  void connectMqtt();
  void loadConfig();
  void setupOTA();
  void subscribe(const String &topic, TopicHandler handler);
  PubSubClient &getMqttClient() { return mqttClient; }

 private:
  void setClock();
  void loadConfigfromSpiffs();

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
  ConnectionConfig config;
  TopicRegistryNode *registryHead;
};
}  // namespace smartgarage