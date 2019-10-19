#include "esp8266node.h"

#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <time.h>
#include <cstdarg>

using namespace espnode;

Esp8266Node::Esp8266Node()
    : mqttClient(wifiClient), registryHead(nullptr) {}

void Esp8266Node::init() {
  Serial.begin(115200);
  while (!Serial) {
  }
  Serial.flush();

  loadConfig();
  connectWiFi();
  setClock();
  randomSeed(micros());
  setupOTA();

  wifiClient.setTrustAnchors(&caCert);
  getMqttClient().setServer(config.mqttBrokerHostname, config.mqttPort);
  getMqttClient().setCallback(
      [this](char *topic, uint8_t *payload, unsigned int length) {
        this->mqttCallback(topic, payload, length);
      });
  Serial.print(config.hostname);
  Serial.println(F(" node starting"));
}

void Esp8266Node::runCycle() {
  ArduinoOTA.handle();
  if (!getMqttClient().connected()) {
    connectMqtt();
  }
  if (getMqttClient().connected()) {
    getMqttClient().loop();
  }
}

void Esp8266Node::log_P(PGM_P formatP, ...) {
  va_list vl;
  static char buffer[96];
  va_start(vl, formatP);
  vsnprintf_P(buffer, sizeof(buffer), formatP, vl);
  buffer[sizeof(buffer) - 1] = '\0';
  va_end(vl);
  Serial.println(buffer);
  Serial.flush();
  if (getMqttClient().connected()) {
    getMqttClient().publish(logTopic, buffer);
  }
}

void Esp8266Node::subscribe(const String &topic, TopicHandler handler) {
  TopicRegistryNode *newReg = new TopicRegistryNode(topic, handler);
  newReg->next = registryHead;
  registryHead = newReg;
}

void Esp8266Node::publish(const String &topic, const String &value) {
  if (!getMqttClient().publish(topic.c_str(), value.c_str())) {
    Serial.printf_P(PSTR("Failed to publish %s"), topic.c_str());
  }
}

void Esp8266Node::logTime() {
  tm timeinfo;
  time_t now = time(nullptr);
  gmtime_r(&now, &timeinfo);
  char timeBuff[32];
  log_P(PSTR("GMT time: %s"), asctime_r(&timeinfo, timeBuff));
}

// Set time via NTP, as required for x.509 validation
void Esp8266Node::setClock() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  Serial.print(F("Waiting for NTP time sync "));
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(F("."));
    now = time(nullptr);
  }
  Serial.println(F("[done]"));
}

void Esp8266Node::setupOTA() {
  ArduinoOTA.onStart([]() { Serial.println(F("OTA Start")); });
  ArduinoOTA.onEnd([]() { Serial.println(F("\nOTA End")); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf_P(PSTR("Progress: %u%%\r"), (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf_P(PSTR("Error[%u]: "), error);
    if (error == OTA_AUTH_ERROR)
      Serial.println(F("Auth Failed"));
    else if (error == OTA_BEGIN_ERROR)
      Serial.println(F("Begin Failed"));
    else if (error == OTA_CONNECT_ERROR)
      Serial.println(F("Connect Failed"));
    else if (error == OTA_RECEIVE_ERROR)
      Serial.println(F("Receive Failed"));
    else if (error == OTA_END_ERROR)
      Serial.println(F("End Failed"));
  });
  ArduinoOTA.setHostname(config.hostname);
  ArduinoOTA.setPasswordHash(config.otaPasswordMd5Hash);
  ArduinoOTA.setPort(config.otaPort);
  ArduinoOTA.begin(true);
}

void Esp8266Node::loadDataFromFlash() {
  if (!SPIFFS.begin()) {
    Serial.println(F("SPIFFS init failed"));
    fail();
  }

  {
    if (!SPIFFS.exists(CA_CERT_FILENAME)) {
      Serial.println(F(CA_CERT_FILENAME " doesn't exist"));
      fail();
    }
    File file = SPIFFS.open(CA_CERT_FILENAME, "r");
    if (!file) {
      Serial.println(F(CA_CERT_FILENAME " open failed"));
      fail();
    }
    String str = file.readString();
    if (str.length() == 0) {
      Serial.println(F(CA_CERT_FILENAME " empty"));
      fail();
    }
    caCert.append(str.c_str());
    file.close();
  }

  if (!SPIFFS.exists(CONFIG_FILENAME)) {
    Serial.println(F(CONFIG_FILENAME " doesn't exist"));
    fail();
  }
  File file = SPIFFS.open(CONFIG_FILENAME, "r");
  if (!file) {
    Serial.println(F(CONFIG_FILENAME " open failed"));
    fail();
  }

  static StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.println(F("Failed to deserialize json file"));
    fail();
  }

  strlcpy(config.hostname, doc["hostname"], sizeof(config.hostname));
  strlcpy(config.mqttBrokerHostname, doc["mqttBrokerHostname"],
          sizeof(config.mqttBrokerHostname));
  strlcpy(config.mqttPassword, doc["mqttPassword"],
          sizeof(config.mqttPassword));
  config.mqttPort = doc["mqttPort"].as<uint16_t>();
  strlcpy(config.mqttUser, doc["mqttUser"], sizeof(config.mqttUser));
  strlcpy(config.otaPasswordMd5Hash, doc["otaPasswordMd5Hash"],
          sizeof(config.otaPasswordMd5Hash));
  config.otaPort = doc["otaPort"].as<uint16_t>();
  strlcpy(config.ssid, doc["ssid"], sizeof(config.ssid));
  strlcpy(config.wifiPassword, doc["wifiPassword"],
          sizeof(config.wifiPassword));

  SPIFFS.end();
}

void Esp8266Node::loadConfig() {
  loadDataFromFlash();
  Serial.println(F("config received:"));
  Serial.println(F("WiFi config:"));
  Serial.println(config.ssid);
  Serial.println(config.wifiPassword);
  Serial.println(config.hostname);
  Serial.println(config.otaPort);
  Serial.println(config.otaPasswordMd5Hash);
  Serial.println(F("MQTT config:"));
  Serial.println(config.mqttBrokerHostname);
  Serial.println(config.mqttPort);
  Serial.println(config.mqttUser);
  Serial.println(config.mqttPassword);

  sprintf_P(logTopic, PSTR("%s/log"), config.hostname);
}

void Esp8266Node::connectWiFi() {
  WiFi.hostname(config.hostname);
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.ssid, config.wifiPassword);
  Serial.print(F("SSID="));
  Serial.println(config.ssid);
  Serial.print(F("connecting to WiFi "));
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(F("[done]"));
  Serial.print(F("IP: "));
  Serial.println(WiFi.localIP());
}

void Esp8266Node::connectMqtt() {
  if (!getMqttClient().connected()) {
    Serial.printf_P(PSTR("connecting to MQTT broker..."));
    Serial.flush();
    bool connected;
    connected = getMqttClient().connect(config.hostname, config.mqttUser,
                                        config.mqttPassword);
    if (connected) {
      log_P(PSTR("connected to MQTT broker"));
      onMqttConnect();
    } else {
      char sslBuff[128];
      wifiClient.getLastSSLError(sslBuff, sizeof(sslBuff));
      log_P(PSTR("connecting to MQTT broker failed. MQTT error=%d SSL "
                 "error='%s'"),
            getMqttClient().state(), sslBuff);
    }
  }
}

void Esp8266Node::mqttCallback(char *topic, uint8_t *payload,
                               unsigned int length) {
  TopicRegistryNode *curr = registryHead;
  while (curr) {
    if (curr->topic == topic) {
      curr->handler(payload, length);
      return;
    }
    curr = curr->next;
  }
  log_P(PSTR("Received unknown topic %s"), topic);
}

void Esp8266Node::onMqttConnect() {
  TopicRegistryNode *curr = registryHead;
  while (curr) {
    log_P(PSTR("subscribing to %s"), curr->topic.c_str());
    getMqttClient().subscribe(curr->topic.c_str());
    curr = curr->next;
  }
}

void Esp8266Node::fail() {
  Serial.println(F("!!Critical Failure!!"));
  // Let the watchdog reset the board
  for (;;) {
  }
}
