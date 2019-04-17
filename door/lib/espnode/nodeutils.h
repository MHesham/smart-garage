#pragma once

#include <cstdint>
#include <cstddef>

namespace espnode {

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

}