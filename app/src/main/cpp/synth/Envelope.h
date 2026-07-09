#pragma once

namespace readingmusic {

class Envelope {
public:
    void setSampleRate(float sampleRate) { sampleRate_ = sampleRate; }

    void setAttack(float seconds) { attackSamples_ = static_cast<int>(seconds * sampleRate_); }
    void setDecay(float seconds) { decaySamples_ = static_cast<int>(seconds * sampleRate_); }
    void setSustain(float level) { sustainLevel_ = level; }
    void setRelease(float seconds) { releaseSamples_ = static_cast<int>(seconds * sampleRate_); }

    void noteOn() {
        state_ = State::Attack;
        counter_ = 0;
        currentLevel_ = 0.0f;
    }

    void noteOff() {
        if (state_ != State::Idle && state_ != State::Release) {
            state_ = State::Release;
            counter_ = 0;
            releaseStartLevel_ = currentLevel_;
        }
    }

    bool isActive() const { return state_ != State::Idle; }

    float process() {
        switch (state_) {
            case State::Attack:
                if (attackSamples_ <= 0) {
                    currentLevel_ = 1.0f;
                    state_ = State::Decay;
                    counter_ = 0;
                } else {
                    currentLevel_ = static_cast<float>(counter_) / static_cast<float>(attackSamples_);
                    counter_++;
                    if (counter_ >= attackSamples_) {
                        currentLevel_ = 1.0f;
                        state_ = State::Decay;
                        counter_ = 0;
                    }
                }
                break;
            case State::Decay:
                if (decaySamples_ <= 0) {
                    currentLevel_ = sustainLevel_;
                    state_ = State::Sustain;
                } else {
                    float t = static_cast<float>(counter_) / static_cast<float>(decaySamples_);
                    currentLevel_ = 1.0f + (sustainLevel_ - 1.0f) * t;
                    counter_++;
                    if (counter_ >= decaySamples_) {
                        currentLevel_ = sustainLevel_;
                        state_ = State::Sustain;
                    }
                }
                break;
            case State::Sustain:
                currentLevel_ = sustainLevel_;
                break;
            case State::Release:
                if (releaseSamples_ <= 0) {
                    currentLevel_ = 0.0f;
                    state_ = State::Idle;
                } else {
                    float t = static_cast<float>(counter_) / static_cast<float>(releaseSamples_);
                    currentLevel_ = releaseStartLevel_ * (1.0f - t);
                    counter_++;
                    if (counter_ >= releaseSamples_) {
                        currentLevel_ = 0.0f;
                        state_ = State::Idle;
                    }
                }
                break;
            case State::Idle:
                currentLevel_ = 0.0f;
                break;
        }
        return currentLevel_;
    }

private:
    enum class State { Idle, Attack, Decay, Sustain, Release };

    float sampleRate_ = 48000.0f;
    int attackSamples_ = 0;
    int decaySamples_ = 0;
    int releaseSamples_ = 0;
    float sustainLevel_ = 0.7f;
    float currentLevel_ = 0.0f;
    float releaseStartLevel_ = 0.0f;
    int counter_ = 0;
    State state_ = State::Idle;
};

} // namespace readingmusic
