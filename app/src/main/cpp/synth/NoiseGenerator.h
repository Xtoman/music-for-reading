#pragma once

#include <cmath>
#include <cstdint>

namespace readingmusic {

class NoiseGenerator {
public:
    void setSampleRate(float sampleRate) { sampleRate_ = sampleRate; }

    void setType(int type) { type_ = type; } // 0 = white, 1 = pink, 2 = brown

    float process() {
        float white = randomFloat() * 2.0f - 1.0f;
        switch (type_) {
            case 1: return processPink(white);
            case 2: return processBrown(white);
            default: return white;
        }
    }

private:
    float randomFloat() {
        state_ = state_ * 1664525u + 1013904223u;
        return static_cast<float>(state_) / static_cast<float>(UINT32_MAX);
    }

    float processPink(float white) {
        b0_ = 0.99886f * b0_ + white * 0.0555179f;
        b1_ = 0.99332f * b1_ + white * 0.0750759f;
        b2_ = 0.96900f * b2_ + white * 0.1538520f;
        b3_ = 0.86650f * b3_ + white * 0.3104856f;
        b4_ = 0.55000f * b4_ + white * 0.5329522f;
        b5_ = -0.7616f * b5_ - white * 0.0168980f;
        float pink = b0_ + b1_ + b2_ + b3_ + b4_ + b5_ + b6_ + white * 0.5362f;
        b6_ = white * 0.115926f;
        return pink * 0.11f;
    }

    float processBrown(float white) {
        brown_ = (brown_ + (0.02f * white)) / 1.02f;
        return brown_ * 3.5f;
    }

    float sampleRate_ = 48000.0f;
    int type_ = 1;
    uint32_t state_ = 0x12345678u;

    float b0_ = 0, b1_ = 0, b2_ = 0, b3_ = 0, b4_ = 0, b5_ = 0, b6_ = 0;
    float brown_ = 0.0f;
};

} // namespace readingmusic
