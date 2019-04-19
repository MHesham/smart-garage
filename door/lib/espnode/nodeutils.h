#pragma once

#include <cstddef>
#include <cstdint>

namespace espnode {

template <size_t MAX_SZ = 32>
static inline String payload_to_str(uint8_t* payload, size_t length) {
  static_assert(MAX_SZ > 0, "invalid max size");
  char buff[MAX_SZ];
  size_t sz = std::min(MAX_SZ - 1, length);
  strncpy(buff, reinterpret_cast<char*>(payload), sz);
  buff[MAX_SZ - 1] = 0;
  return String(buff);
}

class Rate {
 public:
  Rate(uint32_t periodMs) : startMs(0), periodMs(periodMs) {}

  bool checkDue() {
    uint32_t currentMs = millis();
    bool isDue = false;
    if (startMs == 0 || (currentMs - startMs >= periodMs)) {
      startMs = currentMs;
      isDue = true;
    }
    return isDue;
  }

 private:
  uint32_t startMs;
  uint32_t periodMs;
};

template <class T, size_t SIZE>
class MedianFilter1D {
 public:
  static_assert(SIZE % 2 != 0, "median filter window must be odd sized");
  MedianFilter1D() : nextIn(0) { std::fill(window.begin(), window.end(), 0); }
  void sample(const T& data) {
    window[nextIn] = data;
    nextIn = (nextIn + 1) % SIZE;

    median[0] = window[0];
    int i = 1;
    while (i < SIZE) {
      const T& x = window[i];
      int j = i - 1;
      while (j >= 0 && median[j] > x) {
        median[j + 1] = median[j];
        j--;
      }
      median[j + 1] = x;
      i++;
    }
  }

  const T& get() const { return median[SIZE / 2]; }

 private:
  size_t nextIn;
  std::array<T, SIZE> median;
  std::array<T, SIZE> window;
};

template <int THRESHOLD, size_t WINDOW_SIZE>
class TransientEvent {
 public:
  static_assert(THRESHOLD <= WINDOW_SIZE, "threshold out of bounds");
  TransientEvent() : nextInOut(0), count(0) {
    std::fill(window.begin(), window.end(), false);
  }
  void sample(bool state) {
    if (window[nextInOut]) {
      count--;
    }
    if (state) {
      count++;
    }
    window[nextInOut] = state;
    nextInOut = (nextInOut + 1) % WINDOW_SIZE;
  }

  bool isTriggered() const { return count >= THRESHOLD; }

 private:
  size_t nextInOut;
  std::array<bool, WINDOW_SIZE> window;
  int count;
};

}  // namespace espnode