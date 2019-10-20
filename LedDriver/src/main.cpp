#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <FunctionalInterrupt.h>
#include <NeoPixelBus.h>
#include <Utils.h>
#include <esp8266node.h>
#include <time.h>

using namespace espnode;

constexpr uint8_t pirMotionPulsePin = 2;
constexpr uint8_t colorSaturation = 255;
constexpr uint8_t ledDataPin = 3;
constexpr uint8_t ledCount = 30;
constexpr uint8_t doorRelayPulsePin = 2;
constexpr uint8_t potPin = A0;

const RgbColor red(colorSaturation, 0, 0);
const RgbColor green(0, colorSaturation, 0);
const RgbColor blue(0, 0, colorSaturation);
const RgbColor white(colorSaturation);
const RgbColor black(0);

class LedDriverNode : public Esp8266Node {
public:
  LedDriverNode()
      : Esp8266Node(),
        // ESP8266 uses DMA on GPIO3, so the passed pin number is
        // ignored
        strip(ledCount, ledDataPin), potSamplingRate(100) {}

  void init() override {
    Esp8266Node::init();
    pinMode(A0, INPUT);
    strip.Begin();
    strip.Show();
  }

  void runCycle() override {
    Esp8266Node::runCycle();
    if (potSamplingRate.checkDue()) {
      int ledBrightness = analogRead(A0);
      uint8_t brightnessW =
          static_cast<uint8_t>(map(ledBrightness, 0, 1024, 0, 255));
      set_all_strip(brightnessW);
    }
  }

private:
  void set_all_strip(uint8_t brightness) {
    if (brightness < 8) {
      brightness = 0;
    }
    for (int i = 0; i < ledCount; ++i) {
      strip.SetPixelColor(
          i, RgbwColor(brightness, brightness, brightness, brightness));
    }
    strip.Show();
  }
  NeoPixelBus<NeoRgbwFeature, NeoEsp8266Dma800KbpsMethod> strip;
  Rate potSamplingRate;
};

LedDriverNode ledDriverNode;

void setup() { ledDriverNode.init(); }

void loop() { ledDriverNode.runCycle(); }
