#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

namespace readingmusic {

class Arpeggiator {
public:
    void setSampleRate(float sampleRate) { sampleRate_ = sampleRate; }

    void setScale(const std::vector<float>& frequencies) {
        scale_ = frequencies;
        if (scale_.empty()) {
            scale_ = {261.63f, 293.66f, 329.63f, 392.00f, 440.00f};
        }
    }

    void setNoteInterval(float minSeconds, float maxSeconds) {
        minInterval_ = minSeconds;
        maxInterval_ = maxSeconds;
        scheduleNextNote();
    }

    void setVelocity(float velocity) { velocity_ = velocity; }

    void reset(uint32_t seed) {
        rngState_ = seed == 0 ? 0xABCDEF01u : seed;
        scheduleNextNote();
        samplesUntilNextNote_ = 0;
    }

    bool tick() {
        if (samplesUntilNextNote_ > 0) {
            samplesUntilNextNote_--;
            return false;
        }
        currentNote_ = pickNote();
        scheduleNextNote();
        return true;
    }

    float currentNote() const { return currentNote_; }
    float velocity() const { return velocity_; }

private:
    void scheduleNextNote() {
        float interval = minInterval_ + randomFloat() * (maxInterval_ - minInterval_);
        samplesUntilNextNote_ = static_cast<int>(interval * sampleRate_);
    }

    float pickNote() {
        if (scale_.empty()) return 440.0f;
        int index = static_cast<int>(randomFloat() * scale_.size());
        if (index >= static_cast<int>(scale_.size())) index = static_cast<int>(scale_.size()) - 1;
        return scale_[index];
    }

    float randomFloat() {
        rngState_ = rngState_ * 1664525u + 1013904223u;
        return static_cast<float>(rngState_) / static_cast<float>(UINT32_MAX);
    }

    float sampleRate_ = 48000.0f;
    float minInterval_ = 1.5f;
    float maxInterval_ = 4.0f;
    float velocity_ = 0.35f;
    float currentNote_ = 440.0f;
    int samplesUntilNextNote_ = 0;
    uint32_t rngState_ = 0xABCDEF01u;
    std::vector<float> scale_;
};

} // namespace readingmusic
