#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <time.h>

#include <esp8266node.h>

using namespace smartgarage;

#define TOPIC_GARAGE_DOOR_ON "/garage/door/on"
#define TOPIC_GARAGE_DOOR_OFF "/garage/door/off"
#define TOPIC_GARAGE_SONAR_DISTANCE_IN "/garage/sonar/distance_in"

class GarageDoor : public Esp8266Node {
 public:
  static constexpr uint8_t sonarPulsePin = 5;
  static constexpr uint32_t samplingRatePeriodMs = 1000;

  GarageDoor() : lastUpdateMs(0) {}
  virtual void setup() override {
    Esp8266Node::setup();
    subscribe(TOPIC_GARAGE_DOOR_ON,
              [this](uint8_t* payload, unsigned int length) {
                on_handler(payload, length);
              });
    subscribe(TOPIC_GARAGE_DOOR_OFF,
              [this](uint8_t* payload, unsigned int length) {
                on_handler(payload, length);
              });
    pinMode(sonarPulsePin, INPUT);
    lastUpdateMs = millis();
  }

  virtual void loop() override {
    Esp8266Node::loop();
    uint32_t timeMs = millis();
    if (timeMs - lastUpdateMs >= samplingRatePeriodMs) {
      lastUpdateMs = millis();
      sampleSonar();
    }
  }

 private:
  void sampleSonar() {
    uint32_t pwUs = pulseIn(sonarPulsePin, HIGH);
    uint32_t pwIn = pwUs / 147;
    Serial.print(F("PW (in): "));
    Serial.println(pwIn);
    String pwInStr = String(pwIn, 10);
    if (!getMqttClient().publish(TOPIC_GARAGE_SONAR_DISTANCE_IN, pwInStr.c_str())) {
      Serial.println("Failed to publish topic");
    }
  }
  void on_handler(uint8_t* payload, unsigned int length) {
    Serial.println("Door ON");
  }

  void off_handler(uint8_t* payload, unsigned int length) {
    Serial.println("Door OFF");
  }

  uint32_t lastUpdateMs;
};

GarageDoor node;

void setup() { node.setup(); }

void loop() { node.loop(); }
