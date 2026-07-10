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

struct StyleConfig {
    MusicStyle style;
    uint32_t seed;
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
    float nextRandom();
    float sampleMaskNoise();
    std::vector<float> buildScale(int rootMidi, const std::vector<int>& intervals, int octaves) const;
    float midiToHz(int midi) const;

    std::atomic<bool> running_{false};
    std::atomic<float> targetVolume_{0.85f};
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

    static constexpr int kMaxVoices = 8;
    Voice voices_[kMaxVoices]{};
    int activeVoices_ = 4;

    // Broadband masking bed (pink + brown + mid-band)
    float pinkB_[7]{};
    float brown_ = 0.0f;
    float midLp_ = 0.0f;
    float midHp_ = 0.0f;
    float maskNoiseGain_ = 0.18f;
    float padNoiseGain_ = 0.0f;
    int noiseType_ = 1;

    // Arp / piano
    float arpFrequency_ = 440.0f;
    float arpPhase_ = 0.0f;
    float arpEnvelope_ = 0.0f;
    int arpSamplesUntilNext_ = 0;
    float arpVelocity_ = 0.3f;

    // LFOs
    float lfo1Phase_ = 0.0f;
    float lfo2Phase_ = 0.0f;
    float lfo1Rate_ = 0.03f;
    float lfo2Rate_ = 0.05f;

    // Night forest sparkles
    float sparkleEnvelope_ = 0.0f;
    float sparkleFrequency_ = 1200.0f;
    int sparkleSamplesUntilNext_ = 0;

    std::vector<float> scale_;
    float layerGains_[4]{0.4f, 0.3f, 0.2f, 0.1f};
};

} // namespace readingmusic
