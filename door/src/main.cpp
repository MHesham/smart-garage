#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <NeoPixelBus.h>
#include <esp8266node.h>
#include <nodeutils.h>
#include <time.h>

using namespace espnode;

#define TOPIC_GARAGE_DOOR_TOGGLE "/garage/door/toggle"
#define TOPIC_GARAGE_MOTION_STATE "/garage/motion/state"  // {active, inactive}
#define TOPIC_GARAGE_DOOR_STATE "/garage/door/state"      // {open, closed}
#define TOPIC_GARAGE_DOOR_DISTANCE_IN "/garage/door/distance_in"
#define TOPIC_GARAGE_LIGHT_STATE "/garage/light/state"  // {on, off}
#define GARAGE_DOOR_DISTANCE_OPEN_THRESHOLD_IN 130

#define colorSaturation 255
RgbColor red(colorSaturation, 0, 0);
RgbColor green(0, colorSaturation, 0);
RgbColor blue(0, 0, colorSaturation);
RgbColor white(colorSaturation);
RgbColor black(0);

class GarageNode : public Esp8266Node {
 public:
  static constexpr uint8_t sonarPulseWidthPin = 9;
  static constexpr uint8_t pirMotionPulsePin = 5;
  static constexpr uint8_t doorRelayPulsePin = 4;
  static constexpr uint8_t ledDataPin = 3;
  static constexpr uint8_t ledCount = 16;

  GarageNode()
      : sonarSamplingRate(500),
        sonarPublishRate(1000),
        pirSampleRate(1000),
        pirPublishRate(1000),
        // ESP8266 uses DMA on GPIO3, so the passed pin number is ignored
        strip(ledCount, ledDataPin) {}
  virtual void setup() override {
    Esp8266Node::setup();
    subscribe(TOPIC_GARAGE_DOOR_TOGGLE,
              [this](uint8_t* payload, unsigned int length) {
                on_door_toggle_handler(payload, length);
              });
    subscribe(TOPIC_GARAGE_LIGHT_STATE,
              [this](uint8_t* payload, unsigned int length) {
                on_light_state_handler(payload, length);
              });
    pinMode(sonarPulseWidthPin, INPUT);
    pinMode(pirMotionPulsePin, INPUT);
    pinMode(doorRelayPulsePin, OUTPUT);

    strip.Begin();
    strip.Show();

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
      bool isDoorOpen =
          (sonarFilter.get() >= GARAGE_DOOR_DISTANCE_OPEN_THRESHOLD_IN);
      doorOpenEvent.sample(isDoorOpen);
    }

    if (sonarPublishRate.checkDue()) {
      const char* doorState = "closed";
      if (doorOpenEvent.isTriggered()) {
        doorState = "open";
      }
      if (!getMqttClient().publish(TOPIC_GARAGE_DOOR_STATE, doorState)) {
        LOG("Failed to publish " TOPIC_GARAGE_DOOR_STATE);
      }

      String pwInStr = String(sonarFilter.get(), 10);
      if (!getMqttClient().publish(TOPIC_GARAGE_DOOR_DISTANCE_IN,
                                   pwInStr.c_str())) {
        LOG("Failed to publish " TOPIC_GARAGE_DOOR_DISTANCE_IN);
      }
    }
  }

  void pirTask() {
    if (pirSampleRate.checkDue()) {
      bool isMotionActive = (digitalRead(pirMotionPulsePin) == HIGH);
      pirMotionEvent.sample(isMotionActive);
    }

    if (pirPublishRate.checkDue()) {
      const char* motionState = "inactive";
      if (pirMotionEvent.isTriggered()) {
        motionState = "active";
      }
      if (!getMqttClient().publish(TOPIC_GARAGE_MOTION_STATE, motionState)) {
        LOG("Failed to publish " TOPIC_GARAGE_MOTION_STATE);
      }
    }
  }

  void on_door_toggle_handler(uint8_t* payload, unsigned int length) {
    LOG("Door toggle");
    digitalWrite(doorRelayPulsePin, LOW);
    delay(250);
    digitalWrite(doorRelayPulsePin, HIGH);
  }

  void on_light_state_handler(uint8_t* payload, unsigned int length) {
    String state = payload_to_str(payload, length);
    LOG("Light state %s", state.c_str());
    if (state == "on") {
      strip.SetPixelColor(0, red);
      strip.SetPixelColor(1, green);
      strip.SetPixelColor(2, blue);
      strip.SetPixelColor(3, white);
      strip.Show();
    } else if (state == "off") {
      strip.SetPixelColor(0, black);
      strip.SetPixelColor(1, black);
      strip.SetPixelColor(2, black);
      strip.SetPixelColor(3, black);
      strip.Show();
    }
  }

  MedianFilter1D<uint32_t, 5> sonarFilter;
  Rate sonarSamplingRate;
  Rate sonarPublishRate;
  Rate pirSampleRate;
  Rate pirPublishRate;
  // 5 motion active samples from PIR in the last 8 samples will be enough
  // trigger that motion active event
  TransientEvent<5, 8> pirMotionEvent;
  TransientEvent<10, 16> doorOpenEvent;
  NeoPixelBus<NeoRgbwFeature, NeoEsp8266Dma800KbpsMethod> strip;
};

GarageNode garageNode;

void setup() { garageNode.setup(); }

void loop() { garageNode.loop(); }
