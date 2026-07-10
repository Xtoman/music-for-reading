#pragma once

#include "SampleBank.h"

#include <atomic>
#include <cstdint>
#include <vector>

namespace readingmusic {

enum class MusicStyle : int {
    Fantasy = 0,
    SciFi = 1,
    Noir = 2,
    Classical = 3,
    Nature = 4,
    Lofi = 5,
    Meditation = 6
};

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    bool start();
    void stop();
    bool isRunning() const { return running_.load(); }

    void setStyle(MusicStyle style);
    void setVolume(float volume);
    void setFadeSeconds(float seconds);
    void requestSleepFade(float seconds);

    void render(float* output, int numFrames, int numChannels);

private:
    struct PadVoice {
        float phase = 0.0f;
        float frequency = 110.0f;
        float targetFrequency = 110.0f;
        float amplitude = 0.1f;
        float detune = 0.0f;
        int wave = 0;
    };

    struct OneShot {
        bool active = false;
        SampleId sample = SampleId::SoftPiano;
        float pos = 0.0f;
        float rate = 1.0f;
        float gain = 0.0f;
        float pan = 0.0f; // -1..1
    };

    struct LoopPlayer {
        SampleId sample = SampleId::RainLoop;
        float pos = 0.0f;
        float gain = 0.0f;
        float rate = 1.0f;
    };

    void applyStyle(MusicStyle style);
    void reseed(MusicStyle style);
    void renderStyle(float* output, int numFrames, int numChannels);
    void triggerMelodyNote();
    void advanceChord();
    void fireOneShot(SampleId id, float midiNote, float gain, float pan);
    float nextRandom();
    float midiToHz(int midi) const;
    std::vector<int> buildScaleMidi(int root, const std::vector<int>& intervals, int octaves) const;
    float osc(float phase, int wave) const;
    float softMaskNoise();

    std::atomic<bool> running_{false};
    std::atomic<float> targetVolume_{0.8f};
    std::atomic<float> fadeSeconds_{2.5f};
    std::atomic<bool> sleepFadeRequested_{false};
    std::atomic<float> sleepFadeSeconds_{0.0f};

    float sampleRate_ = 48000.0f;
    float currentGain_ = 0.0f;
    float sleepFadeGain_ = 1.0f;
    float sleepFadeStep_ = 0.0f;
    int sleepFadeSamplesRemaining_ = 0;

    MusicStyle currentStyle_ = MusicStyle::Fantasy;
    uint32_t rngState_ = 0xC0FFEE01u;

    SampleBank samples_;

    static constexpr int kMaxPads = 5;
    static constexpr int kMaxShots = 8;
    PadVoice pads_[kMaxPads]{};
    int activePads_ = 3;
    OneShot shots_[kMaxShots]{};
    LoopPlayer loopA_{};
    LoopPlayer loopB_{};

    // Harmony / melody
    std::vector<int> chordRoots_;
    std::vector<int> chordIntervals_;
    std::vector<int> scaleMidi_;
    int chordIndex_ = 0;
    int chordRootMidi_ = 48;
    int chordSamplesLeft_ = 0;
    int melodySamplesLeft_ = 0;
    int lastMelodyIndex_ = -1;
    SampleId melodySample_ = SampleId::SoftPiano;
    float melodyGain_ = 0.35f;
    float padGainScale_ = 1.0f;
    float maskGain_ = 0.04f;

    // Phrase shaping
    float melodyGapMin_ = 1.2f;
    float melodyGapMax_ = 2.8f;
    float chordSecondsMin_ = 8.0f;
    float chordSecondsMax_ = 14.0f;

    // LFOs + noise
    float lfo1Phase_ = 0.0f;
    float lfo2Phase_ = 0.0f;
    float lfo1Rate_ = 0.03f;
    float lfo2Rate_ = 0.05f;
    float pinkB_[7]{};
    float brown_ = 0.0f;
};

} // namespace readingmusic
