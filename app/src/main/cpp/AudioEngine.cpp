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
constexpr float kTwoPi = 2.0f * static_cast<float>(M_PI);
constexpr float kRefMidi = 72.0f; // C5 — sample bake reference
}

AudioEngine::AudioEngine() {
    samples_.build(sampleRate_);
    applyStyle(MusicStyle::Fantasy);
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

std::vector<int> AudioEngine::buildScaleMidi(int root, const std::vector<int>& intervals, int octaves) const {
    std::vector<int> notes;
    for (int o = 0; o < octaves; ++o) {
        for (int iv : intervals) {
            notes.push_back(root + iv + o * 12);
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
    if (rngState_ == 0) rngState_ = 0xC0FFEE01u;
}

float AudioEngine::osc(float phase, int wave) const {
    switch (wave) {
        case 1:
            return 2.0f * std::abs(2.0f * (phase - std::floor(phase + 0.5f))) - 1.0f;
        case 2:
            return std::tanh(2.2f * (phase - std::floor(phase + 0.5f)));
        default:
            return std::sin(phase * kTwoPi);
    }
}

float AudioEngine::softMaskNoise() {
    float white = nextRandom() * 2.0f - 1.0f;
    // Paul Kellet pink
    pinkB_[0] = 0.99886f * pinkB_[0] + white * 0.0555179f;
    pinkB_[1] = 0.99332f * pinkB_[1] + white * 0.0750759f;
    pinkB_[2] = 0.96900f * pinkB_[2] + white * 0.1538520f;
    pinkB_[3] = 0.86650f * pinkB_[3] + white * 0.3104856f;
    pinkB_[4] = 0.55000f * pinkB_[4] + white * 0.5329522f;
    pinkB_[5] = -0.7616f * pinkB_[5] - white * 0.0168980f;
    float pink = pinkB_[0] + pinkB_[1] + pinkB_[2] + pinkB_[3] + pinkB_[4] + pinkB_[5] + pinkB_[6] + white * 0.5362f;
    pinkB_[6] = white * 0.115926f;
    pink *= 0.11f;
    brown_ = (brown_ + 0.02f * white) / 1.02f;
    return pink * 0.65f + brown_ * 2.2f * 0.35f;
}

void AudioEngine::fireOneShot(SampleId id, float midiNote, float gain, float pan) {
    for (auto& shot : shots_) {
        if (!shot.active) {
            shot.active = true;
            shot.sample = id;
            shot.pos = 0.0f;
            shot.rate = midiToHz(static_cast<int>(midiNote)) / midiToHz(static_cast<int>(kRefMidi));
            shot.gain = gain;
            shot.pan = pan;
            return;
        }
    }
    // Steal oldest-ish: first slot
    shots_[0].active = true;
    shots_[0].sample = id;
    shots_[0].pos = 0.0f;
    shots_[0].rate = midiToHz(static_cast<int>(midiNote)) / midiToHz(static_cast<int>(kRefMidi));
    shots_[0].gain = gain;
    shots_[0].pan = pan;
}

void AudioEngine::triggerMelodyNote() {
    if (scaleMidi_.empty()) return;

    int index = static_cast<int>(nextRandom() * scaleMidi_.size()) % static_cast<int>(scaleMidi_.size());
    if (lastMelodyIndex_ >= 0 && nextRandom() < 0.75f) {
        int step = (nextRandom() < 0.5f) ? -1 : 1;
        if (nextRandom() < 0.2f) step *= 2;
        index = std::clamp(lastMelodyIndex_ + step, 0, static_cast<int>(scaleMidi_.size()) - 1);
    }
    lastMelodyIndex_ = index;

    const float midi = static_cast<float>(scaleMidi_[index]);
    const float pan = (nextRandom() - 0.5f) * 0.7f;
    const float vel = melodyGain_ * (0.75f + 0.25f * nextRandom());
    fireOneShot(melodySample_, midi, vel, pan);

    // Occasional soft harmony interval under the melody
    if (nextRandom() < 0.28f) {
        int harm = scaleMidi_[std::clamp(index + ((nextRandom() < 0.5f) ? 2 : 4),
                                         0, static_cast<int>(scaleMidi_.size()) - 1)];
        fireOneShot(melodySample_, static_cast<float>(harm), vel * 0.35f, -pan * 0.5f);
    }
}

void AudioEngine::advanceChord() {
    if (chordRoots_.empty() || chordIntervals_.empty()) return;
    chordIndex_ = (chordIndex_ + 1) % static_cast<int>(chordRoots_.size());
    chordRootMidi_ = chordRoots_[chordIndex_];
    const int count = std::min(activePads_, static_cast<int>(chordIntervals_.size()));
    for (int i = 0; i < count; ++i) {
        pads_[i].targetFrequency = midiToHz(chordRootMidi_ + chordIntervals_[i]);
    }
}

void AudioEngine::applyStyle(MusicStyle style) {
    currentStyle_ = style;
    reseed(style);

    for (auto& p : pads_) {
        p = PadVoice{};
        p.phase = nextRandom();
    }
    for (auto& s : shots_) s = OneShot{};
    loopA_ = LoopPlayer{};
    loopB_ = LoopPlayer{};
    chordRoots_.clear();
    chordIntervals_.clear();
    scaleMidi_.clear();
    chordIndex_ = 0;
    lastMelodyIndex_ = -1;
    maskGain_ = 0.045f;
    padGainScale_ = 1.0f;

    switch (style) {
        case MusicStyle::Fantasy: {
            // Warm Dorian pads + harp melody
            chordRoots_ = {50, 53, 55, 48}; // Dm-ish roots: D F G C
            chordIntervals_ = {0, 3, 7, 10, 14};
            activePads_ = 5;
            scaleMidi_ = buildScaleMidi(62, {0, 2, 3, 5, 7, 9, 10}, 2); // D dorian
            melodySample_ = SampleId::HarpPluck;
            melodyGain_ = 0.38f;
            melodyGapMin_ = 1.4f;
            melodyGapMax_ = 3.2f;
            chordSecondsMin_ = 10.0f;
            chordSecondsMax_ = 16.0f;
            loopA_ = {SampleId::SoftPad, 0.0f, 0.12f, 0.5f};
            maskGain_ = 0.04f;
            lfo1Rate_ = 0.025f;
            lfo2Rate_ = 0.04f;
            for (int i = 0; i < activePads_; ++i) {
                pads_[i].wave = 0;
                pads_[i].amplitude = (0.14f - i * 0.018f) * padGainScale_;
                pads_[i].detune = (nextRandom() - 0.5f) * 0.003f;
            }
            break;
        }
        case MusicStyle::SciFi: {
            // Airy stacked fourths + glass bells
            chordRoots_ = {48, 53, 58, 55}; // C F Bb G
            chordIntervals_ = {0, 5, 10, 15};
            activePads_ = 4;
            scaleMidi_ = buildScaleMidi(60, {0, 2, 5, 7, 9, 12}, 2); // open / quartal feel
            melodySample_ = SampleId::GlassBell;
            melodyGain_ = 0.32f;
            melodyGapMin_ = 1.8f;
            melodyGapMax_ = 4.0f;
            chordSecondsMin_ = 12.0f;
            chordSecondsMax_ = 18.0f;
            loopA_ = {SampleId::SoftPad, 0.0f, 0.1f, 0.65f};
            maskGain_ = 0.035f;
            lfo1Rate_ = 0.04f;
            lfo2Rate_ = 0.07f;
            for (int i = 0; i < activePads_; ++i) {
                pads_[i].wave = 1;
                pads_[i].amplitude = 0.11f - i * 0.015f;
                pads_[i].detune = (nextRandom() - 0.5f) * 0.008f;
            }
            break;
        }
        case MusicStyle::Noir: {
            // Minor jazz colors, sparse piano
            chordRoots_ = {45, 50, 43, 48}; // Am Dm G C
            chordIntervals_ = {0, 3, 7, 10};
            activePads_ = 4;
            scaleMidi_ = buildScaleMidi(57, {0, 2, 3, 5, 7, 8, 10}, 2); // A minor
            melodySample_ = SampleId::SoftPiano;
            melodyGain_ = 0.4f;
            melodyGapMin_ = 1.6f;
            melodyGapMax_ = 3.8f;
            chordSecondsMin_ = 9.0f;
            chordSecondsMax_ = 14.0f;
            loopA_ = {SampleId::VinylLoop, 0.0f, 0.04f, 1.0f};
            maskGain_ = 0.05f;
            lfo1Rate_ = 0.02f;
            for (int i = 0; i < activePads_; ++i) {
                pads_[i].wave = 0;
                pads_[i].amplitude = 0.08f - i * 0.012f;
                pads_[i].detune = (nextRandom() - 0.5f) * 0.002f;
            }
            break;
        }
        case MusicStyle::Classical: {
            // Clear major progressions + soft piano
            chordRoots_ = {48, 55, 53, 48, 50, 55}; // C G F C D G
            chordIntervals_ = {0, 4, 7, 12};
            activePads_ = 4;
            scaleMidi_ = buildScaleMidi(60, {0, 2, 4, 5, 7, 9, 11}, 2); // C major
            melodySample_ = SampleId::SoftPiano;
            melodyGain_ = 0.42f;
            melodyGapMin_ = 0.9f;
            melodyGapMax_ = 2.2f;
            chordSecondsMin_ = 7.0f;
            chordSecondsMax_ = 11.0f;
            loopA_ = {SampleId::SoftPad, 0.0f, 0.08f, 0.45f};
            maskGain_ = 0.035f;
            for (int i = 0; i < activePads_; ++i) {
                pads_[i].wave = 0;
                pads_[i].amplitude = 0.1f - i * 0.015f;
            }
            break;
        }
        case MusicStyle::Nature: {
            // Soft pads + rain/wind + gentle harp
            chordRoots_ = {50, 55, 53, 48};
            chordIntervals_ = {0, 5, 7, 12};
            activePads_ = 4;
            scaleMidi_ = buildScaleMidi(62, {0, 2, 4, 7, 9}, 2); // D pentatonic
            melodySample_ = SampleId::HarpPluck;
            melodyGain_ = 0.28f;
            melodyGapMin_ = 2.0f;
            melodyGapMax_ = 4.5f;
            chordSecondsMin_ = 12.0f;
            chordSecondsMax_ = 18.0f;
            loopA_ = {SampleId::RainLoop, 0.0f, 0.11f, 1.0f};
            loopB_ = {SampleId::WindLoop, 0.0f, 0.07f, 0.85f};
            maskGain_ = 0.03f;
            for (int i = 0; i < activePads_; ++i) {
                pads_[i].wave = 0;
                pads_[i].amplitude = 0.09f - i * 0.014f;
            }
            break;
        }
        case MusicStyle::Lofi: {
            chordRoots_ = {45, 50, 41, 43}; // Am Dm F G
            chordIntervals_ = {0, 3, 7, 10};
            activePads_ = 4;
            scaleMidi_ = buildScaleMidi(57, {0, 2, 3, 5, 7, 8, 10}, 2);
            melodySample_ = SampleId::SoftPiano;
            melodyGain_ = 0.3f;
            melodyGapMin_ = 1.3f;
            melodyGapMax_ = 3.0f;
            chordSecondsMin_ = 6.0f;
            chordSecondsMax_ = 10.0f;
            loopA_ = {SampleId::VinylLoop, 0.0f, 0.08f, 1.0f};
            maskGain_ = 0.05f;
            lfo1Rate_ = 0.015f;
            for (int i = 0; i < activePads_; ++i) {
                pads_[i].wave = 1;
                pads_[i].amplitude = 0.1f - i * 0.014f;
                pads_[i].detune = (nextRandom() - 0.5f) * 0.012f;
            }
            break;
        }
        case MusicStyle::Meditation: {
            chordRoots_ = {48, 55, 53, 50};
            chordIntervals_ = {0, 7, 12};
            activePads_ = 3;
            scaleMidi_ = buildScaleMidi(60, {0, 2, 4, 7, 9}, 2);
            melodySample_ = SampleId::GlassBell;
            melodyGain_ = 0.22f;
            melodyGapMin_ = 3.5f;
            melodyGapMax_ = 7.0f;
            chordSecondsMin_ = 16.0f;
            chordSecondsMax_ = 24.0f;
            loopA_ = {SampleId::SoftPad, 0.0f, 0.14f, 0.4f};
            maskGain_ = 0.055f; // steadier bed for focus
            lfo1Rate_ = 0.06f;  // breathing
            for (int i = 0; i < activePads_; ++i) {
                pads_[i].wave = 0;
                pads_[i].amplitude = 0.13f - i * 0.03f;
            }
            break;
        }
    }

    chordRootMidi_ = chordRoots_.empty() ? 48 : chordRoots_[0];
    for (int i = 0; i < activePads_; ++i) {
        const int iv = chordIntervals_[std::min(i, static_cast<int>(chordIntervals_.size()) - 1)];
        pads_[i].frequency = midiToHz(chordRootMidi_ + iv);
        pads_[i].targetFrequency = pads_[i].frequency;
    }
    chordSamplesLeft_ = static_cast<int>(sampleRate_ * (chordSecondsMin_ + nextRandom() * 2.0f));
    melodySamplesLeft_ = static_cast<int>(sampleRate_ * (melodyGapMin_ + nextRandom()));
    triggerMelodyNote();
}

void AudioEngine::renderStyle(float* output, int numFrames, int numChannels) {
    const float fadeStep = 1.0f / (fadeSeconds_.load() * sampleRate_);

    for (int frame = 0; frame < numFrames; ++frame) {
        if (sleepFadeRequested_.load()) {
            float seconds = sleepFadeSeconds_.load();
            sleepFadeSamplesRemaining_ = static_cast<int>(seconds * sampleRate_);
            sleepFadeStep_ = 1.0f / static_cast<float>(std::max(1, sleepFadeSamplesRemaining_));
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

        float lfo1 = std::sin(lfo1Phase_ * kTwoPi);
        float lfo2 = std::sin(lfo2Phase_ * kTwoPi);
        lfo1Phase_ += lfo1Rate_ / sampleRate_;
        lfo2Phase_ += lfo2Rate_ / sampleRate_;
        if (lfo1Phase_ >= 1.0f) lfo1Phase_ -= 1.0f;
        if (lfo2Phase_ >= 1.0f) lfo2Phase_ -= 1.0f;

        if (--chordSamplesLeft_ <= 0) {
            advanceChord();
            float span = chordSecondsMin_ + nextRandom() * (chordSecondsMax_ - chordSecondsMin_);
            chordSamplesLeft_ = static_cast<int>(sampleRate_ * span);
        }

        if (--melodySamplesLeft_ <= 0) {
            // Phrase rest
            if (nextRandom() < 0.14f) {
                melodySamplesLeft_ = static_cast<int>(
                    sampleRate_ * (melodyGapMax_ * (1.2f + nextRandom())));
            } else {
                triggerMelodyNote();
                float gap = melodyGapMin_ + nextRandom() * (melodyGapMax_ - melodyGapMin_);
                melodySamplesLeft_ = static_cast<int>(sampleRate_ * gap);
            }
        }

        float left = 0.0f;
        float right = 0.0f;

        // Harmonic pad bed
        float breath = 1.0f;
        if (currentStyle_ == MusicStyle::Meditation) {
            breath = 0.6f + 0.4f * lfo1;
        } else {
            breath = 0.9f + 0.1f * lfo1;
        }
        for (int i = 0; i < activePads_; ++i) {
            auto& v = pads_[i];
            v.frequency += (v.targetFrequency - v.frequency) * 0.0001f;
            float amp = v.amplitude * breath;
            if (i > 0) amp *= (0.88f + 0.12f * lfo2);
            float s = osc(v.phase, v.wave) * amp;
            // Mild stereo spread by voice index
            float pan = (i - (activePads_ - 1) * 0.5f) * 0.18f;
            left += s * (1.0f - pan);
            right += s * (1.0f + pan);
            v.phase += (v.frequency * (1.0f + v.detune)) / sampleRate_;
            if (v.phase >= 1.0f) v.phase -= 1.0f;
        }

        // Sample one-shots (melody)
        for (auto& shot : shots_) {
            if (!shot.active) continue;
            const auto& buf = samples_.get(shot.sample);
            if (buf.data.empty()) {
                shot.active = false;
                continue;
            }
            const int i0 = static_cast<int>(shot.pos);
            if (i0 >= static_cast<int>(buf.data.size()) - 1) {
                shot.active = false;
                continue;
            }
            const float frac = shot.pos - static_cast<float>(i0);
            const float s = (buf.data[i0] * (1.0f - frac) + buf.data[i0 + 1] * frac) * shot.gain;
            left += s * (1.0f - shot.pan);
            right += s * (1.0f + shot.pan);
            shot.pos += shot.rate;
        }

        // Texture loops
        auto playLoop = [&](LoopPlayer& loop) {
            if (loop.gain <= 0.0001f) return;
            const auto& buf = samples_.get(loop.sample);
            if (buf.data.empty()) return;
            const int n = static_cast<int>(buf.data.size());
            while (loop.pos >= n) loop.pos -= static_cast<float>(n);
            const int i0 = static_cast<int>(loop.pos) % n;
            const int i1 = (i0 + 1) % n;
            const float frac = loop.pos - std::floor(loop.pos);
            float s = (buf.data[i0] * (1.0f - frac) + buf.data[i1] * frac) * loop.gain;
            s *= (0.92f + 0.08f * lfo2);
            left += s;
            right += s;
            loop.pos += loop.rate;
        };
        playLoop(loopA_);
        playLoop(loopB_);

        // Soft continuous mask (speech/traffic) — never dominant
        float mask = softMaskNoise() * maskGain_ * (0.9f + 0.1f * lfo2);
        left += mask;
        right += mask;

        left = std::tanh(left * 1.05f) * 0.9f;
        right = std::tanh(right * 1.05f) * 0.9f;

        if (running_.load()) {
            currentGain_ = std::min(1.0f, currentGain_ + fadeStep);
        } else {
            currentGain_ = std::max(0.0f, currentGain_ - fadeStep);
        }

        const float g = currentGain_ * targetVolume_.load() * sleepFadeGain_;
        if (numChannels >= 2) {
            output[frame * numChannels] = left * g;
            output[frame * numChannels + 1] = right * g;
            for (int ch = 2; ch < numChannels; ++ch) {
                output[frame * numChannels + ch] = 0.0f;
            }
        } else {
            output[frame] = 0.5f * (left + right) * g;
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
