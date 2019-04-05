#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <time.h>

#include "app_config.h"
#include "topics.h"

ConnectionConfig connConfig;
#if MQTT_SECURE
BearSSL::WiFiClientSecure wifiClient;
BearSSL::X509List caCert;
#else
WiFiClient wifiClient;
#endif
PubSubClient mqttClient(wifiClient);

bool loadConfigfromSpiffs() {
  if (!SPIFFS.begin()) {
    Serial.println(F("SPIFFS init failed"));
    return false;
  }

  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    Serial.print(dir.fileName());
    Serial.print("\t");
    Serial.println(dir.fileSize());
  }
#if MQTT_SECURE
  {
    if (!SPIFFS.exists(CA_CERT_FILENAME)) {
      Serial.println(F(CA_CERT_FILENAME " doesn't exist"));
      return false;
    }
    File file = SPIFFS.open(CA_CERT_FILENAME, "r");
    if (!file) {
      Serial.println(F(CA_CERT_FILENAME " open failed"));
      return false;
    }
    String str = file.readString();
    if (str.length() == 0) {
      Serial.println(F(CA_CERT_FILENAME " empty"));
      return false;
    }
    caCert.append(str.c_str());
    file.close();
  }
#endif

  memset(&connConfig, 0, sizeof(connConfig));

  {
    if (!SPIFFS.exists(WIFI_CONFIG_FILENAME)) {
      Serial.println(WIFI_CONFIG_FILENAME " doesn't exist");
      return false;
    }
    File file = SPIFFS.open(WIFI_CONFIG_FILENAME, "r");
    if (!file) {
      Serial.println(WIFI_CONFIG_FILENAME " open failed");
      return false;
    }
    {
      String str = file.readStringUntil('\n');
      strlcpy(connConfig.ssid, str.c_str(), sizeof(connConfig.ssid));
    }
    {
      String str = file.readStringUntil('\n');
      strlcpy(connConfig.wifiPassword, str.c_str(),
              sizeof(connConfig.wifiPassword));
    }
  }

  {
    if (!SPIFFS.exists(MQTT_CONFIG_FILENAME)) {
      Serial.println(F(MQTT_CONFIG_FILENAME " doesn't exist"));
      return false;
    }
    File file = SPIFFS.open(MQTT_CONFIG_FILENAME, "r");
    if (!file) {
      Serial.println(F(MQTT_CONFIG_FILENAME "open failed"));
      return false;
    }
    {
      String str = file.readStringUntil('\n');
      strlcpy(connConfig.mqttBrokerHostname, str.c_str(),
              sizeof(connConfig.mqttBrokerHostname));
    }
    {
      String str = file.readStringUntil('\n');
      connConfig.mqttPort = static_cast<uint16_t>(str.toInt());
    }
    {
      String str = file.readStringUntil('\n');
      strlcpy(connConfig.mqttUser, str.c_str(), sizeof(connConfig.mqttUser));
    }
    {
      String str = file.readStringUntil('\n');
      strlcpy(connConfig.mqttPassword, str.c_str(),
              sizeof(connConfig.mqttPassword));
    }
    {
      String str = file.readStringUntil('\n');
      strlcpy(connConfig.mqttClientId, str.c_str(),
              sizeof(connConfig.mqttClientId));
    }
  }

  SPIFFS.end();
  return true;
}

bool loadConfig() {
  if (!loadConfigfromSpiffs()) {
    return false;
  }
  Serial.println(F("connConfig received:"));
  Serial.println(F("WiFi config:"));
  Serial.println(connConfig.ssid);
  Serial.println(connConfig.wifiPassword);
  Serial.println(F("MQTT config:"));
  Serial.println(connConfig.mqttBrokerHostname);
  Serial.println(connConfig.mqttPort);
  Serial.println(connConfig.mqttUser);
  Serial.println(connConfig.mqttPassword);
  Serial.println(connConfig.mqttClientId);
  return true;
}

void connectWiFi() {
  WiFi.hostname(NODE_HOSTNAME);
  WiFi.mode(WIFI_STA);
  WiFi.begin(connConfig.ssid, connConfig.wifiPassword);
  Serial.print(F("SSID="));
  Serial.println(connConfig.ssid);
  Serial.print(F("connecting to WiFi "));
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(F("[done]"));
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void on_handler() { Serial.println("Door ON"); }

void off_handler() { Serial.println("Door OFF"); }

void mqttCallback(char *topic, uint8_t *payload, unsigned int length) {
  if (strcmp(topic, TOPIC_GARAGE_DOOR_OFF) == 0) {
    off_handler();
  } else if (strcmp(topic, TOPIC_GARAGE_DOOR_ON) == 0) {
    on_handler();
  } else {
    Serial.print(F("Unexpected topic "));
    Serial.println(topic);
  }
}

void printTime() {
  tm timeinfo;
  time_t now = time(nullptr);
  gmtime_r(&now, &timeinfo);
  Serial.print(F("GMT time: "));
  Serial.print(asctime(&timeinfo));
}
void connectMqtt() {
  if (!mqttClient.connected()) {
    printTime();
    Serial.print(F("connecting to MQTT broker "));
    bool connected;
#if MQTT_SECURE
    connected = mqttClient.connect(NODE_HOSTNAME, connConfig.mqttUser,
                                   connConfig.mqttPassword);
#else
    connected = mqttClient.connect(clientId.c_str());
#endif
    if (connected) {
      Serial.println(F("[done]"));
      mqttClient.subscribe(TOPIC_GARAGE_DOOR_ON);
      Serial.print(F("subscribed to "));
      Serial.println(F(TOPIC_GARAGE_DOOR_ON));

      mqttClient.subscribe(TOPIC_GARAGE_DOOR_OFF);
      Serial.print(F("subscribed to "));
      Serial.println(F(TOPIC_GARAGE_DOOR_OFF));
      digitalWrite(LED_BUILTIN, LOW);
    } else {
      Serial.println(F("[failed]"));
      Serial.print(F("MQTT error:"));
      Serial.print(mqttClient.state());
      char buff[128];
      wifiClient.getLastSSLError(buff, sizeof(buff));
      Serial.print(F(", SSL error: "));
      Serial.println(buff);
      Serial.println(F(" try again in 5 seconds"));
      delay(5000);
    }
  }
}

// Set time via NTP, as required for x.509 validation
void setClock() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  Serial.print(F("Waiting for NTP time sync "));
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("[done]");
}

void setupOTA() {
  ArduinoOTA.onStart([]() { Serial.println(F("OTA Start")); });
  ArduinoOTA.onEnd([]() { Serial.println(F("\nOTA End")); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR)
      Serial.println("End Failed");
  });
  ArduinoOTA.setHostname(NODE_HOSTNAME);
  ArduinoOTA.begin(true);
}

void setup(void) {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.begin(74880);
  while (!Serial) {
  }
  Serial.println(F("smart-garage door starting"));

  if (!loadConfig()) {
    Serial.println(F("FATAL: config loading failed"));
    for (;;) {
    }
  }
  connectWiFi();
  setClock();
  randomSeed(micros());
  setupOTA();

#if MQTT_SECURE
  wifiClient.setTrustAnchors(&caCert);
#endif
  mqttClient.setServer(connConfig.mqttBrokerHostname, connConfig.mqttPort);
  mqttClient.setCallback(mqttCallback);
}

void loop(void) {
  ArduinoOTA.handle();
  if (!mqttClient.connected()) {
    digitalWrite(LED_BUILTIN, HIGH);
    connectMqtt();
    if (!mqttClient.connected()) {
      return;
    }
  }
  mqttClient.loop();
}
