#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <time.h>

#include <esp8266node.h>
#include <nodeutils.h>

using namespace espnode;

// {open, close}
#define TOPIC_GARAGE_DOOR "/garage/door"
// {active, inactive}
#define TOPIC_GARAGE_MOTION "/garage/motion"
// value in inch
#define TOPIC_GARAGE_SONAR_DISTANCE_IN "/garage/sonar/distance_in"

class GarageNode : public Esp8266Node {
 public:
  static constexpr uint8_t sonarPulseWidthPin = 9;
  static constexpr uint8_t pirMotionPulsePin = 5;
  static constexpr uint8_t doorRelayPulsePin = 4;

  GarageNode()
      : sonarSamplingRate(200), sonarPublishRate(1000), pirPublishRate(1000) {}
  virtual void setup() override {
    Esp8266Node::setup();
    subscribe(TOPIC_GARAGE_DOOR, [this](uint8_t* payload, unsigned int length) {
      on_door_handler(payload, length);
    });
    pinMode(sonarPulseWidthPin, INPUT);
    pinMode(pirMotionPulsePin, INPUT);
    pinMode(doorRelayPulsePin, OUTPUT);

    LOG("GarageNode starting");
  }

  virtual void loop() override {
    Esp8266Node::loop();
    sonarTask();
    pirTask();
  }

 private:
  void sonarTask() {
    if (sonarSamplingRate.checkDue()) {
      uint32_t pwUs = pulseIn(sonarPulseWidthPin, HIGH);
      uint32_t pwIn = pwUs / 147;
      sonarFilter.sample(pwIn);
    }

    if (sonarPublishRate.checkDue()) {
      String pwInStr = String(sonarFilter.get(), 10);
      if (!getMqttClient().publish(TOPIC_GARAGE_SONAR_DISTANCE_IN,
                                   pwInStr.c_str())) {
        LOG("Failed to publish " TOPIC_GARAGE_SONAR_DISTANCE_IN);
      }
    }
  }

  void pirTask() {
    if (pirPublishRate.checkDue()) {
      const char* motionState = "inactive";
      if (digitalRead(pirMotionPulsePin) == HIGH) {
        motionState = "active";
      }
      if (!getMqttClient().publish(TOPIC_GARAGE_MOTION, motionState)) {
        LOG("Failed to publish " TOPIC_GARAGE_MOTION);
      }
    }
  }

  void on_door_handler(uint8_t* payload, unsigned int length) {
    LOG("Door %s", payload);
  }

  MedianFilter1D<uint32_t, 5> sonarFilter;
  Rate sonarSamplingRate;
  Rate sonarPublishRate;
  Rate pirPublishRate;
};

GarageNode garageNode;

void setup() { garageNode.setup(); }

void loop() { garageNode.loop(); }
