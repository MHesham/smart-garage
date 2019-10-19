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

class Task {
 public:
  virtual ~Task() {}
  virtual void init() = 0;
  virtual void runCycle() = 0;
};

class ConnectedNode {
 public:
  typedef std::function<void(uint8_t *, unsigned int)> TopicHandler;
  virtual ~ConnectedNode() {}

  virtual void subscribe(const String &topic, TopicHandler handler) = 0;
  virtual void publish(const String &topic, const String &value) = 0;
  virtual void log_P(PGM_P formatP, ...)
      __attribute__((format(printf, 2, 3))) = 0;
};

class NodeTask : public Task {
 public:
  NodeTask(ConnectedNode &node) : node(node) {}
  ConnectedNode &getNode() { return node; }

 private:
  ConnectedNode &node;
};

class Esp8266Node : public ConnectedNode, public Task {
 public:
  Esp8266Node();
  virtual void init() override;
  virtual void runCycle() override;
  void subscribe(const String &topic, TopicHandler handler) override;
  void publish(const String &topic, const String &value) override;
  void log_P(PGM_P formatP, ...) override __attribute__((format(printf, 2, 3)));
  void mqttCallback(char *topic, uint8_t *payload, unsigned int length);

 protected:
  void fail();
  void onMqttConnect();
  void logTime();
  PubSubClient &getMqttClient() { return mqttClient; }

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

  PubSubClient mqttClient;
  BearSSL::WiFiClientSecure wifiClient;
  BearSSL::X509List caCert;
  NodeConfig config;
  TopicRegistryNode *registryHead;
  char logTopic[32];
};
}  // namespace espnode