#include "AudioEngine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace readingmusic {

namespace {

float oscSample(float phase, int waveType) {
    switch (waveType) {
        case 1:
            return 2.0f * std::abs(2.0f * (phase - std::floor(phase + 0.5f))) - 1.0f;
        case 2:
            return 2.0f * (phase - std::floor(phase + 0.5f));
        default:
            return std::sin(phase * 2.0f * static_cast<float>(M_PI));
    }
}

float pinkNoise(float white, float* b) {
    b[0] = 0.99886f * b[0] + white * 0.0555179f;
    b[1] = 0.99332f * b[1] + white * 0.0750759f;
    b[2] = 0.96900f * b[2] + white * 0.1538520f;
    b[3] = 0.86650f * b[3] + white * 0.3104856f;
    b[4] = 0.55000f * b[4] + white * 0.5329522f;
    b[5] = -0.7616f * b[5] - white * 0.0168980f;
    float pink = b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6] + white * 0.5362f;
    b[6] = white * 0.115926f;
    return pink * 0.11f;
}

} // namespace

AudioEngine::AudioEngine() {
    applyStyle(MusicStyle::DeepAmbient);
}

AudioEngine::~AudioEngine() {
    stop();
}

bool AudioEngine::start() {
    // Fresh time-based seed so each playback session is unique
    applyStyle(currentStyle_);
    running_.store(true);
    currentGain_ = 0.0f;
    sleepFadeGain_ = 1.0f;
    sleepFadeRequested_.store(false);
    return true;
}

void AudioEngine::stop() {
    running_.store(false);
}

void AudioEngine::setStyle(MusicStyle style) {
    applyStyle(style);
}

void AudioEngine::setVolume(float volume) {
    targetVolume_.store(std::clamp(volume, 0.0f, 1.0f));
}

void AudioEngine::setFadeSeconds(float seconds) {
    fadeSeconds_.store(std::max(0.1f, seconds));
}

void AudioEngine::requestSleepFade(float seconds) {
    sleepFadeSeconds_.store(std::max(0.5f, seconds));
    sleepFadeRequested_.store(true);
}

float AudioEngine::midiToHz(int midi) const {
    return 440.0f * std::pow(2.0f, (midi - 69) / 12.0f);
}

std::vector<float> AudioEngine::buildScale(int rootMidi, const std::vector<int>& intervals, int octaves) const {
    std::vector<float> notes;
    for (int o = 0; o < octaves; ++o) {
        for (int interval : intervals) {
            notes.push_back(midiToHz(rootMidi + interval + o * 12));
        }
    }
    return notes;
}

float AudioEngine::nextRandom() {
    rngState_ = rngState_ * 1664525u + 1013904223u;
    return static_cast<float>(rngState_) / static_cast<float>(UINT32_MAX);
}

void AudioEngine::reseed(MusicStyle style) {
    using clock = std::chrono::steady_clock;
    const auto now = clock::now().time_since_epoch();
    const auto nanos = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
    // Mix time with style so parallel style switches still diverge
    rngState_ = static_cast<uint32_t>(nanos ^ (nanos >> 32))
        ^ (0xA5A5A5A5u + static_cast<uint32_t>(style) * 0x9E3779B9u);
    if (rngState_ == 0) {
        rngState_ = 0xC0FFEE01u;
    }
}

float AudioEngine::sampleMaskNoise() {
    float white = nextRandom() * 2.0f - 1.0f;
    float pink = pinkNoise(white, pinkB_);
    brown_ = (brown_ + 0.02f * white) / 1.02f;
    float brown = brown_ * 3.5f;

    // Mid-band emphasis (~300–3000 Hz) to mask speech / cabin chatter
    midLp_ += 0.12f * (white - midLp_);
    midHp_ += 0.02f * (midLp_ - midHp_);
    float mid = (midLp_ - midHp_) * 1.8f;

    return pink * 0.55f + brown * 0.35f + mid * 0.45f;
}

void AudioEngine::applyStyle(MusicStyle style) {
    currentStyle_ = style;
    reseed(style);

    for (auto& voice : voices_) {
        voice.phase = nextRandom();
        voice.amplitude = 0.0f;
        voice.detune = 0.0f;
        voice.waveType = 0;
    }

    padNoiseGain_ = 0.0f;
    noiseType_ = 1;
    lfo1Rate_ = 0.03f;
    lfo2Rate_ = 0.05f;
    arpVelocity_ = 0.25f;
    activeVoices_ = 4;
    // Continuous masking bed present in every style
    maskNoiseGain_ = 0.16f;

    switch (style) {
        case MusicStyle::DeepAmbient: {
            // Dense low–mid drones: strong against traffic rumble
            int root = 36; // C2
            std::vector<int> chord = {0, 7, 12, 16, 19, 24};
            activeVoices_ = 6;
            for (int i = 0; i < 6; ++i) {
                voices_[i].frequency = midiToHz(root + chord[i]);
                voices_[i].targetFrequency = voices_[i].frequency;
                voices_[i].waveType = 0;
                voices_[i].amplitude = 0.22f - i * 0.02f;
                voices_[i].detune = (nextRandom() - 0.5f) * 0.006f;
            }
            maskNoiseGain_ = 0.20f;
            lfo1Rate_ = 0.02f;
            lfo2Rate_ = 0.035f;
            break;
        }
        case MusicStyle::SoftPiano: {
            scale_ = buildScale(55, {0, 2, 4, 7, 9}, 2);
            arpSamplesUntilNext_ = static_cast<int>(sampleRate_ * (1.2f + nextRandom()));
            arpFrequency_ = scale_[static_cast<int>(nextRandom() * scale_.size()) % scale_.size()];
            arpEnvelope_ = 0.0f;
            activeVoices_ = 3;
            for (int i = 0; i < 3; ++i) {
                voices_[i].frequency = midiToHz(43 + i * 7);
                voices_[i].waveType = 0;
                voices_[i].amplitude = 0.16f - i * 0.03f;
            }
            maskNoiseGain_ = 0.17f;
            arpVelocity_ = 0.28f;
            break;
        }
        case MusicStyle::RainPad: {
            activeVoices_ = 3;
            for (int i = 0; i < 3; ++i) {
                voices_[i].frequency = midiToHz(48 + i * 5);
                voices_[i].waveType = 0;
                voices_[i].amplitude = 0.18f - i * 0.03f;
            }
            padNoiseGain_ = 0.14f;
            maskNoiseGain_ = 0.12f;
            noiseType_ = 1;
            lfo1Rate_ = 0.018f;
            break;
        }
        case MusicStyle::ZenGarden: {
            activeVoices_ = 3;
            voices_[0].frequency = midiToHz(48);
            voices_[0].amplitude = 0.26f;
            voices_[1].frequency = midiToHz(55);
            voices_[1].amplitude = 0.18f;
            voices_[2].frequency = midiToHz(60);
            voices_[2].amplitude = 0.12f;
            maskNoiseGain_ = 0.18f;
            lfo1Rate_ = 0.1f;
            lfo2Rate_ = 0.07f;
            break;
        }
        case MusicStyle::LofiHaze: {
            int root = 40;
            std::vector<int> chord = {0, 3, 7, 10, 14, 17};
            activeVoices_ = 6;
            for (int i = 0; i < 6; ++i) {
                voices_[i].frequency = midiToHz(root + chord[i]);
                voices_[i].waveType = 1;
                voices_[i].amplitude = 0.14f - i * 0.012f;
                voices_[i].detune = (nextRandom() - 0.5f) * 0.018f;
            }
            padNoiseGain_ = 0.06f;
            maskNoiseGain_ = 0.15f;
            noiseType_ = 2;
            lfo1Rate_ = 0.012f;
            break;
        }
        case MusicStyle::NightForest: {
            activeVoices_ = 3;
            voices_[0].frequency = midiToHz(36);
            voices_[0].amplitude = 0.28f;
            voices_[1].frequency = midiToHz(43);
            voices_[1].amplitude = 0.18f;
            voices_[2].frequency = midiToHz(50);
            voices_[2].amplitude = 0.12f;
            sparkleSamplesUntilNext_ = static_cast<int>(sampleRate_ * (12.0f + nextRandom() * 12.0f));
            sparkleEnvelope_ = 0.0f;
            sparkleFrequency_ = midiToHz(84 + static_cast<int>(nextRandom() * 12));
            maskNoiseGain_ = 0.19f;
            lfo1Rate_ = 0.01f;
            break;
        }
    }
}

void AudioEngine::renderStyle(float* output, int numFrames, int numChannels) {
    const float fadeStep = 1.0f / (fadeSeconds_.load() * sampleRate_);

    for (int frame = 0; frame < numFrames; ++frame) {
        if (sleepFadeRequested_.load()) {
            float seconds = sleepFadeSeconds_.load();
            sleepFadeSamplesRemaining_ = static_cast<int>(seconds * sampleRate_);
            sleepFadeStep_ = 1.0f / static_cast<float>(sleepFadeSamplesRemaining_);
            sleepFadeRequested_.store(false);
        }

        if (sleepFadeSamplesRemaining_ > 0) {
            sleepFadeGain_ -= sleepFadeStep_;
            sleepFadeSamplesRemaining_--;
            if (sleepFadeGain_ <= 0.0f) {
                sleepFadeGain_ = 0.0f;
                running_.store(false);
            }
        }

        float lfo1 = std::sin(lfo1Phase_ * 2.0f * static_cast<float>(M_PI));
        float lfo2 = std::sin(lfo2Phase_ * 2.0f * static_cast<float>(M_PI));
        lfo1Phase_ += lfo1Rate_ / sampleRate_;
        lfo2Phase_ += lfo2Rate_ / sampleRate_;
        if (lfo1Phase_ >= 1.0f) lfo1Phase_ -= 1.0f;
        if (lfo2Phase_ >= 1.0f) lfo2Phase_ -= 1.0f;

        float sample = 0.0f;

        // Always-on masking bed: pink/brown + mid-band for speech/traffic
        sample += sampleMaskNoise() * maskNoiseGain_ * (0.92f + 0.08f * lfo2);

        switch (currentStyle_) {
            case MusicStyle::DeepAmbient: {
                if (static_cast<int>(nextRandom() * 72000) == 1) {
                    int shift = static_cast<int>(nextRandom() * 5) - 2;
                    for (int i = 0; i < activeVoices_; ++i) {
                        int intervals[] = {0, 7, 12, 16, 19, 24};
                        voices_[i].targetFrequency = midiToHz(36 + intervals[i] + shift);
                    }
                }
                for (int i = 0; i < activeVoices_; ++i) {
                    voices_[i].frequency += (voices_[i].targetFrequency - voices_[i].frequency) * 0.00008f;
                    float amp = voices_[i].amplitude * (0.88f + 0.12f * lfo1);
                    sample += oscSample(voices_[i].phase, voices_[i].waveType) * amp;
                    voices_[i].phase += (voices_[i].frequency * (1.0f + voices_[i].detune)) / sampleRate_;
                    if (voices_[i].phase >= 1.0f) voices_[i].phase -= 1.0f;
                }
                break;
            }
            case MusicStyle::SoftPiano: {
                for (int i = 0; i < activeVoices_; ++i) {
                    sample += oscSample(voices_[i].phase, 0) * voices_[i].amplitude * (0.9f + 0.1f * lfo1);
                    voices_[i].phase += voices_[i].frequency / sampleRate_;
                    if (voices_[i].phase >= 1.0f) voices_[i].phase -= 1.0f;
                }
                if (arpSamplesUntilNext_ <= 0) {
                    arpFrequency_ = scale_[static_cast<int>(nextRandom() * scale_.size()) % scale_.size()];
                    arpSamplesUntilNext_ = static_cast<int>(sampleRate_ * (1.4f + nextRandom() * 2.0f));
                    arpEnvelope_ = arpVelocity_;
                } else {
                    arpSamplesUntilNext_--;
                }
                arpEnvelope_ *= 0.9994f;
                float piano = std::sin(arpPhase_ * 2.0f * static_cast<float>(M_PI));
                arpPhase_ += arpFrequency_ / sampleRate_;
                if (arpPhase_ >= 1.0f) arpPhase_ -= 1.0f;
                sample += piano * arpEnvelope_;
                break;
            }
            case MusicStyle::RainPad: {
                for (int i = 0; i < activeVoices_; ++i) {
                    float amp = voices_[i].amplitude * (0.82f + 0.18f * lfo2);
                    sample += oscSample(voices_[i].phase, 0) * amp;
                    voices_[i].phase += voices_[i].frequency / sampleRate_;
                    if (voices_[i].phase >= 1.0f) voices_[i].phase -= 1.0f;
                }
                // High-shelf rain droplets (separate from mask bed)
                float white = nextRandom() * 2.0f - 1.0f;
                float rain = white - midLp_;
                sample += rain * padNoiseGain_ * (1.1f + 0.2f * lfo1);
                break;
            }
            case MusicStyle::ZenGarden: {
                float breath = 0.55f + 0.45f * lfo1;
                for (int i = 0; i < activeVoices_; ++i) {
                    float amp = voices_[i].amplitude * breath * (i == 0 ? 1.0f : 0.75f + 0.25f * lfo2);
                    sample += oscSample(voices_[i].phase, 0) * amp;
                    voices_[i].phase += voices_[i].frequency / sampleRate_;
                    if (voices_[i].phase >= 1.0f) voices_[i].phase -= 1.0f;
                }
                break;
            }
            case MusicStyle::LofiHaze: {
                for (int i = 0; i < activeVoices_; ++i) {
                    float amp = voices_[i].amplitude * (0.9f + 0.1f * lfo1);
                    sample += oscSample(voices_[i].phase, voices_[i].waveType) * amp;
                    voices_[i].phase += (voices_[i].frequency * (1.0f + voices_[i].detune)) / sampleRate_;
                    if (voices_[i].phase >= 1.0f) voices_[i].phase -= 1.0f;
                }
                float white = nextRandom() * 2.0f - 1.0f;
                brown_ = (brown_ + 0.02f * white) / 1.02f;
                sample += brown_ * 3.5f * padNoiseGain_;
                break;
            }
            case MusicStyle::NightForest: {
                for (int i = 0; i < activeVoices_; ++i) {
                    sample += oscSample(voices_[i].phase, 0) * voices_[i].amplitude * (0.9f + 0.1f * lfo1);
                    voices_[i].phase += voices_[i].frequency / sampleRate_;
                    if (voices_[i].phase >= 1.0f) voices_[i].phase -= 1.0f;
                }
                if (sparkleSamplesUntilNext_ <= 0) {
                    sparkleFrequency_ = midiToHz(84 + static_cast<int>(nextRandom() * 16));
                    sparkleEnvelope_ = 0.12f;
                    sparkleSamplesUntilNext_ = static_cast<int>(sampleRate_ * (12.0f + nextRandom() * 14.0f));
                } else {
                    sparkleSamplesUntilNext_--;
                }
                sparkleEnvelope_ *= 0.99985f;
                static float sparklePhase = 0.0f;
                sample += std::sin(sparklePhase * 2.0f * static_cast<float>(M_PI)) * sparkleEnvelope_;
                sparklePhase += sparkleFrequency_ / sampleRate_;
                if (sparklePhase >= 1.0f) sparklePhase -= 1.0f;
                break;
            }
        }

        // Soft saturation with higher headroom for denser mix
        sample = std::tanh(sample * 1.15f) * 0.92f;

        if (running_.load()) {
            currentGain_ = std::min(1.0f, currentGain_ + fadeStep);
        } else {
            currentGain_ = std::max(0.0f, currentGain_ - fadeStep);
        }

        float finalSample = sample * currentGain_ * targetVolume_.load() * sleepFadeGain_;

        for (int ch = 0; ch < numChannels; ++ch) {
            // Subtle stereo width on noise-ish content via tiny L/R offset
            float pan = (ch == 0) ? 0.98f : 1.02f;
            output[frame * numChannels + ch] = finalSample * pan;
        }
    }
}

void AudioEngine::render(float* output, int numFrames, int numChannels) {
    if (!running_.load() && currentGain_ <= 0.0001f) {
        std::memset(output, 0, sizeof(float) * numFrames * numChannels);
        return;
    }
    renderStyle(output, numFrames, numChannels);
}

} // namespace readingmusic
