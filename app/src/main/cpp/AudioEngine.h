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
        float targetGain = 0.0f;
        float pan = 0.0f;
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
    void startMelodyPhrase();
    void triggerNextPhraseNote();
    void morphHarmony();
    void fireOneShot(SampleId id, float midiNote, float gain, float pan);
    float nextRandom();
    float midiToHz(int midi) const;
    std::vector<int> buildScaleMidi(int root, const std::vector<int>& intervals, int octaves) const;
    float osc(float phase, int wave) const;
    float softMaskNoise();
    int nearestScaleIndex(int midi) const;

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
    static constexpr int kMaxShots = 10;
    PadVoice pads_[kMaxPads]{};
    int activePads_ = 3;
    OneShot shots_[kMaxShots]{};
    LoopPlayer loopA_{};
    LoopPlayer loopB_{};

    std::vector<int> chordRoots_;
    std::vector<int> chordIntervals_;
    std::vector<int> scaleMidi_;
    int chordIndex_ = 0;
    int chordRootMidi_ = 48;
    int harmonySamplesLeft_ = 0;
    int nextVoiceToMorph_ = 0;
    float padGlide_ = 0.00004f;

    // Melodic phrases (connected line, not isolated hits)
    int phraseNotesRemaining_ = 0;
    int phraseNoteSamplesLeft_ = 0;
    int phraseRestSamplesLeft_ = 0;
    int lastMelodyIndex_ = -1;
    float phraseDirection_ = 1.0f;
    SampleId melodySample_ = SampleId::SoftPiano;
    float melodyGain_ = 0.28f;
    float noteGapMin_ = 0.38f;
    float noteGapMax_ = 0.72f;
    float phraseRestMin_ = 3.5f;
    float phraseRestMax_ = 7.0f;
    int phraseLenMin_ = 4;
    int phraseLenMax_ = 7;
    float maskGain_ = 0.04f;
    float padLevel_ = 0.7f;

    float lfo1Phase_ = 0.0f;
    float lfo2Phase_ = 0.0f;
    float lfo1Rate_ = 0.03f;
    float lfo2Rate_ = 0.05f;
    float pinkB_[7]{};
    float brown_ = 0.0f;
};

} // namespace readingmusic
