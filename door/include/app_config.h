#pragma once

#include <stdint.h>

#define MQTT_SECURE 1
#define CA_CERT_FILENAME "/ca.crt.pem"
#define MQTT_CONFIG_FILENAME "/mqtt.config"
#define WIFI_CONFIG_FILENAME "/wifi.config"
#define NODE_HOSTNAME "garage-door"

struct ConnectionConfig {
  char ssid[16];
  char wifiPassword[16];
  char mqttBrokerHostname[32];
  uint16_t mqttPort;
  char mqttUser[16];
  char mqttPassword[16];
  char mqttClientId[16];
};