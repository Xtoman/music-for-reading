#pragma once

#include <cmath>

namespace readingmusic {

class SoftLimiter {
public:
  float process(float input) {
    return std::tanh(input * drive_) * outputGain_;
  }

  void setDrive(float drive) { drive_ = drive; }
  void setOutputGain(float gain) { outputGain_ = gain; }

private:
  float drive_ = 1.2f;
  float outputGain_ = 0.85f;
};

class SimpleLfo {
public:
  void setSampleRate(float sampleRate) { sampleRate_ = sampleRate; }
  void setRate(float hz) { rate_ = hz; }

  float process() {
    float value = std::sin(phase_ * 2.0f * 3.14159265f);
    phase_ += rate_ / sampleRate_;
    if (phase_ >= 1.0f) phase_ -= 1.0f;
    return value;
  }

private:
  float sampleRate_ = 48000.0f;
  float rate_ = 0.05f;
  float phase_ = 0.0f;
};

} // namespace readingmusic
