#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <FunctionalInterrupt.h>
#include <NeoPixelBus.h>
#include <esp8266node.h>
#include <Utils.h>
#include <time.h>

using namespace espnode;

/** Commands **/
#define TOPIC_DOOR_CONFIG "door/config"  // {"open": true:false}

#define TOPIC_LIGHT_CONFIG \
  "light/config"  // {"enabled": true|false, "color":
                  // "white"|"red"|"green"|"blue"}

/** Properties **/
#define TOPIC_LIGHT_STATE "light/state"
#define TOPIC_MOTION_STATE "motion/state"
#define TOPIC_SONAR_STATE "sonar/state"
#define TOPIC_DOOR_STATE "door/state"

//  GARAGE_DOOR_DISTANCE_OPEN_THRESHOLD_IN 130

constexpr uint8_t pirMotionPulsePin = D2;
constexpr uint8_t colorSaturation = 255;
constexpr uint8_t sonarPulseWidthPin = 9;
constexpr uint8_t ledDataPin = 3;
constexpr uint8_t ledCount = 16;
constexpr uint8_t doorRelayPulsePin = 2;

RgbColor red(colorSaturation, 0, 0);
RgbColor green(0, colorSaturation, 0);
RgbColor blue(0, 0, colorSaturation);
RgbColor white(colorSaturation);
RgbColor black(0);
StaticJsonDocument<64> valueDoc;

class PirTask : public NodeTask {
 public:
  PirTask(ConnectedNode& node)
      : NodeTask(node),
        motionActive(false),
        triggerCount(0),
        pirSampleRate(30000),
        pirPublishRate(1000) {}

  void init() override {
    pinMode(pirMotionPulsePin, INPUT);
    attachInterrupt(digitalPinToInterrupt(pirMotionPulsePin),
                    [this]() { pirIsr(); }, RISING);
  }

  void runCycle() override {
    if (pirSampleRate.checkDue()) {
      Serial.print("Trigger Count:");
      Serial.println(triggerCount);
      triggerCount = 0;
      //Serial.println(isMotionActive);
    }

    if (pirPublishRate.checkDue()) {
      valueDoc.clear();
      String value;
      valueDoc["active"] = motionActive;
      serializeJson(valueDoc, value);
      getNode().publish(TOPIC_MOTION_STATE, value);
    }
  }

  void pirIsr() {
    triggerCount++;
    Serial.print("PIR:");
    Serial.println(true);
  }

 private:
  volatile bool motionActive;
  volatile uint32_t triggerCount;
  Rate pirSampleRate;
  Rate pirPublishRate;
  // 5 motion active samples from PIR in the last 8 samples will be enough
  // trigger that motion active event
  TransientEvent<1, 3> pirMotionEvent;
};

class SonarTask : public NodeTask {
 public:
  SonarTask(ConnectedNode& node)
      : NodeTask(node), sonarSamplingRate(500), sonarPublishRate(1000) {}

  void init() override { pinMode(sonarPulseWidthPin, INPUT); }

  void runCycle() override {
    if (sonarSamplingRate.checkDue()) {
      uint32_t pwUs = pulseIn(sonarPulseWidthPin, HIGH);
      uint32_t pwIn = pwUs / 147;
      sonarFilter.sample(pwIn);
    }

    if (sonarPublishRate.checkDue()) {
      String pwInStr = String(sonarFilter.get(), 10);
      getNode().publish(TOPIC_SONAR_STATE, pwInStr);
    }
  }

 private:
  MedianFilter1D<uint32_t, 5> sonarFilter;
  Rate sonarSamplingRate;
  Rate sonarPublishRate;
  TransientEvent<10, 16> doorOpenEvent;
};

class LightTask : public NodeTask {
 public:
  LightTask(ConnectedNode& node)
      : NodeTask(node),
        // ESP8266 uses DMA on GPIO3, so the passed pin number is ignored
        strip(ledCount, ledDataPin) {}

  void init() override {
    getNode().subscribe(TOPIC_LIGHT_CONFIG,
                        [this](uint8_t* payload, unsigned int length) {
                          on_light_config_handler(payload, length);
                        });

    set_all_strip(black);
    strip.Begin();
    strip.Show();
  }
  void runCycle() override {}

 private:
  void set_all_strip(RgbwColor color) {
    for (int i = 0; i < ledCount; ++i) {
      strip.SetPixelColor(i, color);
    }
    strip.Show();
  }

  void on_light_config_handler(uint8_t* payload, unsigned int length) {
    String config = payload_to_str<64>(payload, length);
    getNode().log_P(PSTR("Light config (%d): %s"), length, config.c_str());
    if (deserializeJson(valueDoc, config)) {
      getNode().log_P("Failed to deserialize json value");
    } else {
      if (valueDoc["enabled"].as<bool>()) {
        String color = valueDoc["color"];
        if (color == "white") {
          set_all_strip(white);
        } else if (color == "red") {
          set_all_strip(red);
        } else if (color == "blue") {
          set_all_strip(blue);
        } else if (color == "green") {
          set_all_strip(green);
        }
      } else {
        set_all_strip(black);
      }
    }
  }

  NeoPixelBus<NeoRgbwFeature, NeoEsp8266Dma800KbpsMethod> strip;
};

class DoorTask : public NodeTask {
 public:
  DoorTask(ConnectedNode& node) : NodeTask(node) {}

  void init() override {
    pinMode(doorRelayPulsePin, OUTPUT);

    getNode().subscribe(TOPIC_DOOR_CONFIG,
                        [this](uint8_t* payload, unsigned int length) {
                          on_door_toggle_handler(payload, length);
                        });
  }

  void runCycle() {}

 private:
  void on_door_toggle_handler(uint8_t* payload, unsigned int length) {
    getNode().log_P(PSTR("Door toggle"));
    digitalWrite(doorRelayPulsePin, LOW);
    delay(250);
    digitalWrite(doorRelayPulsePin, HIGH);
  }
};

class GarageNode : public Esp8266Node {
 public:
  GarageNode()
      : Esp8266Node(),
        pirTask(*this),
        sonarTask(*this),
        lightTask(*this),
        doorTask(*this) {}

  void init() override {
    Esp8266Node::init();
    sonarTask.init();
    pirTask.init();
    lightTask.init();
    doorTask.init();
  }

  void runCycle() override {
    Esp8266Node::runCycle();
    sonarTask.runCycle();
    pirTask.runCycle();
    lightTask.runCycle();
    doorTask.runCycle();
  }

 private:
  PirTask pirTask;
  SonarTask sonarTask;
  LightTask lightTask;
  DoorTask doorTask;
};

GarageNode garageNode;

void setup() { garageNode.init(); }

void loop() { garageNode.runCycle(); }
