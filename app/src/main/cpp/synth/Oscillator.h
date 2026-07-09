#pragma once

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace readingmusic {

enum class WaveType { Sine, Triangle, Saw };

class Oscillator {
public:
    void setSampleRate(float sampleRate) {
        sampleRate_ = sampleRate;
        updatePhaseIncrement();
    }

    void setFrequency(float frequency) {
        frequency_ = frequency;
        updatePhaseIncrement();
    }

    void setWaveType(WaveType type) { waveType_ = type; }

    void setDetuneCents(float cents) {
        detuneRatio_ = std::pow(2.0f, cents / 1200.0f);
        updatePhaseIncrement();
    }

    float process() {
        float sample = 0.0f;
        switch (waveType_) {
            case WaveType::Sine:
                sample = std::sin(phase_ * 2.0f * static_cast<float>(M_PI));
                break;
            case WaveType::Triangle:
                sample = 2.0f * std::abs(2.0f * (phase_ - std::floor(phase_ + 0.5f))) - 1.0f;
                break;
            case WaveType::Saw:
                sample = 2.0f * (phase_ - std::floor(phase_ + 0.5f));
                break;
        }
        phase_ += phaseIncrement_;
        if (phase_ >= 1.0f) phase_ -= 1.0f;
        return sample;
    }

    void reset() { phase_ = 0.0f; }

private:
    void updatePhaseIncrement() {
        if (sampleRate_ > 0.0f) {
            phaseIncrement_ = (frequency_ * detuneRatio_) / sampleRate_;
        }
    }

    float sampleRate_ = 48000.0f;
    float frequency_ = 440.0f;
    float detuneRatio_ = 1.0f;
    float phase_ = 0.0f;
    float phaseIncrement_ = 0.0f;
    WaveType waveType_ = WaveType::Sine;
};

} // namespace readingmusic
