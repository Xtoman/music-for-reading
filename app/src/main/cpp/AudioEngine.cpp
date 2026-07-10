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
        case 1: // triangle
            return 2.0f * std::abs(2.0f * (phase - std::floor(phase + 0.5f))) - 1.0f;
        case 2: // soft saw (band-limited-ish via tanh)
            return std::tanh(2.0f * (phase - std::floor(phase + 0.5f)));
        default: // sine
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
    rngState_ = static_cast<uint32_t>(nanos ^ (nanos >> 32))
        ^ (0xA5A5A5A5u + static_cast<uint32_t>(style) * 0x9E3779B9u);
    if (rngState_ == 0) {
        rngState_ = 0xC0FFEE01u;
    }
}

float AudioEngine::sampleTextureNoise() {
    float white = nextRandom() * 2.0f - 1.0f;
    float pink = pinkNoise(white, pinkB_);
    brown_ = (brown_ + 0.02f * white) / 1.02f;
    return pink * 0.7f + brown_ * 2.0f;
}

void AudioEngine::triggerMelodyNote() {
    if (scale_.empty()) return;

    int index = static_cast<int>(nextRandom() * scale_.size()) % static_cast<int>(scale_.size());
    // Prefer stepwise motion for a more melodic feel
    if (lastMelodyIndex_ >= 0 && nextRandom() < 0.7f) {
        int step = (nextRandom() < 0.5f) ? -1 : 1;
        if (nextRandom() < 0.25f) step *= 2;
        index = std::clamp(lastMelodyIndex_ + step, 0, static_cast<int>(scale_.size()) - 1);
    }
    lastMelodyIndex_ = index;
    melodyFrequency_ = scale_[index];
    melodyEnvelope_ = melodyVelocity_ * (0.75f + 0.25f * nextRandom());
}

void AudioEngine::advanceChord() {
    if (chordRoots_.empty() || chordIntervals_.empty()) return;

    chordIndex_ = (chordIndex_ + 1) % static_cast<int>(chordRoots_.size());
    chordRootMidi_ = chordRoots_[chordIndex_];

    const int count = std::min(activeVoices_, static_cast<int>(chordIntervals_.size()));
    for (int i = 0; i < count; ++i) {
        voices_[i].targetFrequency = midiToHz(chordRootMidi_ + chordIntervals_[i]);
    }
}

void AudioEngine::applyStyle(MusicStyle style) {
    currentStyle_ = style;
    reseed(style);

    for (auto& voice : voices_) {
        voice.phase = nextRandom();
        voice.amplitude = 0.0f;
        voice.detune = 0.0f;
        voice.waveType = 0;
        voice.frequency = 220.0f;
        voice.targetFrequency = 220.0f;
    }

    rainGain_ = 0.0f;
    textureGain_ = 0.025f;
    lfo1Rate_ = 0.03f;
    lfo2Rate_ = 0.05f;
    melodyEnvelope_ = 0.0f;
    melodyPhase_ = nextRandom();
    lastMelodyIndex_ = -1;
    chordIndex_ = 0;
    chordRoots_.clear();
    chordIntervals_.clear();
    scale_.clear();

    switch (style) {
        case MusicStyle::DeepAmbient: {
            // Warm major/add9 pads with slow chord motion + soft high melody
            chordRoots_ = {48, 53, 55, 50}; // C, F, G, D
            chordIntervals_ = {0, 4, 7, 11};
            activeVoices_ = 4;
            chordRootMidi_ = chordRoots_[0];
            for (int i = 0; i < 4; ++i) {
                voices_[i].frequency = midiToHz(chordRootMidi_ + chordIntervals_[i]);
                voices_[i].targetFrequency = voices_[i].frequency;
                voices_[i].waveType = 0;
                voices_[i].amplitude = 0.16f - i * 0.02f;
                voices_[i].detune = (nextRandom() - 0.5f) * 0.003f;
            }
            scale_ = buildScale(60, {0, 2, 4, 7, 9}, 2);
            melodyVelocity_ = 0.18f;
            melodyDecay_ = 0.99972f;
            melodyWave_ = 0;
            melodySamplesUntilNext_ = static_cast<int>(sampleRate_ * (3.0f + nextRandom() * 3.0f));
            chordSamplesUntilNext_ = static_cast<int>(sampleRate_ * (14.0f + nextRandom() * 8.0f));
            textureGain_ = 0.02f;
            lfo1Rate_ = 0.02f;
            lfo2Rate_ = 0.035f;
            break;
        }
        case MusicStyle::SoftPiano: {
            // Quiet pad + clear piano-like melody
            chordRoots_ = {48, 45, 41, 43}; // C, Am, F, G
            chordIntervals_ = {0, 7, 12};
            activeVoices_ = 3;
            chordRootMidi_ = chordRoots_[0];
            for (int i = 0; i < 3; ++i) {
                voices_[i].frequency = midiToHz(chordRootMidi_ + chordIntervals_[i]);
                voices_[i].targetFrequency = voices_[i].frequency;
                voices_[i].waveType = 0;
                voices_[i].amplitude = 0.07f - i * 0.012f;
            }
            scale_ = buildScale(60, {0, 2, 4, 5, 7, 9, 11}, 2);
            melodyVelocity_ = 0.42f;
            melodyDecay_ = 0.99935f;
            melodyWave_ = 0;
            melodySamplesUntilNext_ = static_cast<int>(sampleRate_ * (0.7f + nextRandom() * 0.8f));
            chordSamplesUntilNext_ = static_cast<int>(sampleRate_ * (10.0f + nextRandom() * 6.0f));
            textureGain_ = 0.015f;
            break;
        }
        case MusicStyle::RainPad: {
            // Lush pad + gentle pentatonic phrases + light rain
            chordRoots_ = {50, 53, 55, 48}; // D, F, G, C
            chordIntervals_ = {0, 3, 7, 10};
            activeVoices_ = 4;
            chordRootMidi_ = chordRoots_[0];
            for (int i = 0; i < 4; ++i) {
                voices_[i].frequency = midiToHz(chordRootMidi_ + chordIntervals_[i]);
                voices_[i].targetFrequency = voices_[i].frequency;
                voices_[i].waveType = 0;
                voices_[i].amplitude = 0.14f - i * 0.018f;
                voices_[i].detune = (nextRandom() - 0.5f) * 0.004f;
            }
            scale_ = buildScale(62, {0, 2, 4, 7, 9}, 2);
            melodyVelocity_ = 0.22f;
            melodyDecay_ = 0.9996f;
            melodyWave_ = 1;
            melodySamplesUntilNext_ = static_cast<int>(sampleRate_ * (2.0f + nextRandom() * 2.5f));
            chordSamplesUntilNext_ = static_cast<int>(sampleRate_ * (12.0f + nextRandom() * 6.0f));
            rainGain_ = 0.045f;
            textureGain_ = 0.01f;
            lfo1Rate_ = 0.018f;
            break;
        }
        case MusicStyle::ZenGarden: {
            // Sparse pentatonic melody over breathing fifths
            chordRoots_ = {55, 50, 53, 48}; // G, D, F, C
            chordIntervals_ = {0, 7, 12};
            activeVoices_ = 3;
            chordRootMidi_ = chordRoots_[0];
            for (int i = 0; i < 3; ++i) {
                voices_[i].frequency = midiToHz(chordRootMidi_ + chordIntervals_[i]);
                voices_[i].targetFrequency = voices_[i].frequency;
                voices_[i].waveType = 0;
                voices_[i].amplitude = 0.12f - i * 0.025f;
            }
            scale_ = buildScale(67, {0, 2, 4, 7, 9}, 2); // G pentatonic
            melodyVelocity_ = 0.28f;
            melodyDecay_ = 0.99955f;
            melodyWave_ = 0;
            melodySamplesUntilNext_ = static_cast<int>(sampleRate_ * (2.5f + nextRandom() * 3.5f));
            chordSamplesUntilNext_ = static_cast<int>(sampleRate_ * (16.0f + nextRandom() * 8.0f));
            textureGain_ = 0.02f;
            lfo1Rate_ = 0.08f; // ~12.5s breath
            lfo2Rate_ = 0.05f;
            break;
        }
        case MusicStyle::LofiHaze: {
            // Soft seventh chords + lazy melody
            chordRoots_ = {45, 50, 41, 43}; // Am, Dm, F, G
            chordIntervals_ = {0, 3, 7, 10};
            activeVoices_ = 4;
            chordRootMidi_ = chordRoots_[0];
            for (int i = 0; i < 4; ++i) {
                voices_[i].frequency = midiToHz(chordRootMidi_ + chordIntervals_[i]);
                voices_[i].targetFrequency = voices_[i].frequency;
                voices_[i].waveType = 1;
                voices_[i].amplitude = 0.11f - i * 0.015f;
                voices_[i].detune = (nextRandom() - 0.5f) * 0.01f;
            }
            scale_ = buildScale(57, {0, 2, 3, 5, 7, 8, 10}, 2); // A natural minor
            melodyVelocity_ = 0.26f;
            melodyDecay_ = 0.99945f;
            melodyWave_ = 1;
            melodySamplesUntilNext_ = static_cast<int>(sampleRate_ * (1.4f + nextRandom() * 1.8f));
            chordSamplesUntilNext_ = static_cast<int>(sampleRate_ * (8.0f + nextRandom() * 5.0f));
            textureGain_ = 0.035f; // light vinyl warmth
            lfo1Rate_ = 0.015f;
            break;
        }
        case MusicStyle::NightForest: {
            // Low drone + frequent soft melodic sparkles
            chordRoots_ = {43, 46, 41, 48}; // G, Bb, F, C
            chordIntervals_ = {0, 7, 12};
            activeVoices_ = 3;
            chordRootMidi_ = chordRoots_[0];
            for (int i = 0; i < 3; ++i) {
                voices_[i].frequency = midiToHz(chordRootMidi_ + chordIntervals_[i]);
                voices_[i].targetFrequency = voices_[i].frequency;
                voices_[i].waveType = 0;
                voices_[i].amplitude = 0.15f - i * 0.03f;
            }
            scale_ = buildScale(67, {0, 3, 5, 7, 10}, 2); // G minor pentatonic-ish
            melodyVelocity_ = 0.2f;
            melodyDecay_ = 0.9997f;
            melodyWave_ = 0;
            melodySamplesUntilNext_ = static_cast<int>(sampleRate_ * (1.8f + nextRandom() * 2.5f));
            chordSamplesUntilNext_ = static_cast<int>(sampleRate_ * (18.0f + nextRandom() * 10.0f));
            textureGain_ = 0.03f;
            lfo1Rate_ = 0.012f;
            break;
        }
    }

    triggerMelodyNote();
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

        if (chordSamplesUntilNext_ <= 0) {
            advanceChord();
            float span = 10.0f;
            switch (currentStyle_) {
                case MusicStyle::SoftPiano: span = 8.0f; break;
                case MusicStyle::LofiHaze: span = 6.0f; break;
                case MusicStyle::ZenGarden: span = 14.0f; break;
                case MusicStyle::NightForest: span = 16.0f; break;
                default: span = 10.0f; break;
            }
            chordSamplesUntilNext_ = static_cast<int>(sampleRate_ * (span + nextRandom() * span * 0.6f));
        } else {
            chordSamplesUntilNext_--;
        }

        if (melodySamplesUntilNext_ <= 0) {
            triggerMelodyNote();
            float gap = 1.5f;
            switch (currentStyle_) {
                case MusicStyle::SoftPiano: gap = 0.55f + nextRandom() * 0.9f; break;
                case MusicStyle::LofiHaze: gap = 1.1f + nextRandom() * 1.6f; break;
                case MusicStyle::ZenGarden: gap = 2.2f + nextRandom() * 3.0f; break;
                case MusicStyle::DeepAmbient: gap = 2.8f + nextRandom() * 3.5f; break;
                case MusicStyle::RainPad: gap = 1.8f + nextRandom() * 2.2f; break;
                case MusicStyle::NightForest: gap = 1.5f + nextRandom() * 2.4f; break;
            }
            // Occasional longer rest for phrasing
            if (nextRandom() < 0.12f) {
                gap *= 1.8f;
            }
            melodySamplesUntilNext_ = static_cast<int>(sampleRate_ * gap);
        } else {
            melodySamplesUntilNext_--;
        }

        float sample = 0.0f;

        // Soft harmonic pad
        float padMod = 0.9f + 0.1f * lfo1;
        if (currentStyle_ == MusicStyle::ZenGarden) {
            padMod = 0.55f + 0.45f * lfo1; // breathing
        }
        for (int i = 0; i < activeVoices_; ++i) {
            voices_[i].frequency += (voices_[i].targetFrequency - voices_[i].frequency) * 0.00012f;
            float amp = voices_[i].amplitude * padMod;
            if (i > 0) {
                amp *= (0.85f + 0.15f * lfo2);
            }
            sample += oscSample(voices_[i].phase, voices_[i].waveType) * amp;
            voices_[i].phase += (voices_[i].frequency * (1.0f + voices_[i].detune)) / sampleRate_;
            if (voices_[i].phase >= 1.0f) voices_[i].phase -= 1.0f;
        }

        // Melodic voice (with a soft 2nd harmonic for piano-ish body)
        melodyEnvelope_ *= melodyDecay_;
        float mel = oscSample(melodyPhase_, melodyWave_);
        if (currentStyle_ == MusicStyle::SoftPiano) {
            mel = mel * 0.78f + oscSample(std::fmod(melodyPhase_ * 2.0f, 1.0f), 0) * 0.22f;
        }
        sample += mel * melodyEnvelope_;
        melodyPhase_ += melodyFrequency_ / sampleRate_;
        if (melodyPhase_ >= 1.0f) melodyPhase_ -= 1.0f;

        // Style-specific light texture
        if (textureGain_ > 0.0f) {
            sample += sampleTextureNoise() * textureGain_ * (0.9f + 0.1f * lfo2);
        }
        if (rainGain_ > 0.0f) {
            float white = nextRandom() * 2.0f - 1.0f;
            midLp_ += 0.18f * (white - midLp_);
            float rain = white - midLp_;
            sample += rain * rainGain_ * (1.0f + 0.15f * lfo1);
        }

        sample = std::tanh(sample * 1.05f) * 0.9f;

        if (running_.load()) {
            currentGain_ = std::min(1.0f, currentGain_ + fadeStep);
        } else {
            currentGain_ = std::max(0.0f, currentGain_ - fadeStep);
        }

        float finalSample = sample * currentGain_ * targetVolume_.load() * sleepFadeGain_;

        for (int ch = 0; ch < numChannels; ++ch) {
            float pan = (ch == 0) ? 0.985f : 1.015f;
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
