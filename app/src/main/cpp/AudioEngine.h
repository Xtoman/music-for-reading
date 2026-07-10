#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

namespace readingmusic {

enum class MusicStyle : int {
    DeepAmbient = 0,
    SoftPiano = 1,
    RainPad = 2,
    ZenGarden = 3,
    LofiHaze = 4,
    NightForest = 5
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
    void applyStyle(MusicStyle style);
    void renderStyle(float* output, int numFrames, int numChannels);
    void reseed(MusicStyle style);
    void triggerMelodyNote();
    void advanceChord();
    float nextRandom();
    float sampleTextureNoise();
    std::vector<float> buildScale(int rootMidi, const std::vector<int>& intervals, int octaves) const;
    float midiToHz(int midi) const;

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

    MusicStyle currentStyle_ = MusicStyle::DeepAmbient;
    uint32_t rngState_ = 0xC0FFEE01u;

    struct Voice {
        float phase = 0.0f;
        float frequency = 220.0f;
        float amplitude = 0.2f;
        float targetFrequency = 220.0f;
        int waveType = 0;
        float detune = 0.0f;
    };

    static constexpr int kMaxVoices = 6;
    Voice voices_[kMaxVoices]{};
    int activeVoices_ = 3;

    // Soft texture (not a masking wall)
    float pinkB_[7]{};
    float brown_ = 0.0f;
    float midLp_ = 0.0f;
    float textureGain_ = 0.03f;
    float rainGain_ = 0.0f;

    // Melody / arp
    float melodyPhase_ = 0.0f;
    float melodyFrequency_ = 440.0f;
    float melodyEnvelope_ = 0.0f;
    float melodyVelocity_ = 0.35f;
    float melodyDecay_ = 0.9995f;
    int melodySamplesUntilNext_ = 0;
    int melodyWave_ = 0;
    int lastMelodyIndex_ = -1;

    // Chord progression
    int chordRootMidi_ = 48;
    int chordIndex_ = 0;
    int chordSamplesUntilNext_ = 0;
    std::vector<int> chordRoots_;
    std::vector<int> chordIntervals_;

    // LFOs
    float lfo1Phase_ = 0.0f;
    float lfo2Phase_ = 0.0f;
    float lfo1Rate_ = 0.03f;
    float lfo2Rate_ = 0.05f;

    std::vector<float> scale_;
};

} // namespace readingmusic
