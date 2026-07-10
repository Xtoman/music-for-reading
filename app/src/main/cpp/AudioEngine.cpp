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
constexpr float kRefMidi = 72.0f;
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

int AudioEngine::nearestScaleIndex(int midi) const {
    if (scaleMidi_.empty()) return 0;
    int best = 0;
    int bestDist = 999;
    for (int i = 0; i < static_cast<int>(scaleMidi_.size()); ++i) {
        int d = std::abs(scaleMidi_[i] - midi);
        if (d < bestDist) {
            bestDist = d;
            best = i;
        }
    }
    return best;
}

void AudioEngine::fireOneShot(SampleId id, float midiNote, float gain, float pan) {
    for (auto& shot : shots_) {
        if (!shot.active) {
            shot.active = true;
            shot.sample = id;
            shot.pos = 0.0f;
            shot.rate = midiToHz(static_cast<int>(midiNote)) / midiToHz(static_cast<int>(kRefMidi));
            shot.gain = 0.0f; // soft attack — avoids percussive chord-like hits
            shot.targetGain = gain;
            shot.pan = pan;
            return;
        }
    }
    shots_[0].active = true;
    shots_[0].sample = id;
    shots_[0].pos = 0.0f;
    shots_[0].rate = midiToHz(static_cast<int>(midiNote)) / midiToHz(static_cast<int>(kRefMidi));
    shots_[0].gain = 0.0f;
    shots_[0].targetGain = gain;
    shots_[0].pan = pan;
}

void AudioEngine::startMelodyPhrase() {
    phraseNotesRemaining_ = phraseLenMin_ + static_cast<int>(nextRandom() * (phraseLenMax_ - phraseLenMin_ + 1));
    phraseDirection_ = (nextRandom() < 0.5f) ? -1.0f : 1.0f;

    // Start near a chord tone so the line feels grounded in the harmony
    if (!chordIntervals_.empty() && !scaleMidi_.empty()) {
        int chordTone = chordRootMidi_ + chordIntervals_[static_cast<int>(nextRandom() * chordIntervals_.size()) % static_cast<int>(chordIntervals_.size())];
        // Prefer upper octave for melody
        if (chordTone < 60) chordTone += 12;
        lastMelodyIndex_ = nearestScaleIndex(chordTone);
    } else if (lastMelodyIndex_ < 0 && !scaleMidi_.empty()) {
        lastMelodyIndex_ = static_cast<int>(scaleMidi_.size() / 3);
    }

    triggerNextPhraseNote();
}

void AudioEngine::triggerNextPhraseNote() {
    if (scaleMidi_.empty() || phraseNotesRemaining_ <= 0) return;

    int index = lastMelodyIndex_;
    if (index < 0) {
        index = static_cast<int>(nextRandom() * scaleMidi_.size()) % static_cast<int>(scaleMidi_.size());
    } else {
        // Mostly stepwise motion — this is what makes a melody, not chords
        int step = 1;
        if (nextRandom() < 0.18f) step = 2;
        if (nextRandom() < 0.12f) step = 0; // neighbor tone repeat / hold feel
        if (nextRandom() < 0.22f) phraseDirection_ = -phraseDirection_;
        index = std::clamp(index + static_cast<int>(phraseDirection_) * step,
                           0, static_cast<int>(scaleMidi_.size()) - 1);
        // Bounce at edges
        if (index == 0 || index == static_cast<int>(scaleMidi_.size()) - 1) {
            phraseDirection_ = -phraseDirection_;
        }
    }
    lastMelodyIndex_ = index;

    const float midi = static_cast<float>(scaleMidi_[index]);
    // Narrow pan — melody stays centered as one line
    const float pan = (nextRandom() - 0.5f) * 0.25f;
    // Gentle contour: slightly louder in the middle of the phrase
    const float phrasePos = 1.0f - std::abs(phraseNotesRemaining_ - phraseLenMax_ * 0.5f) / (phraseLenMax_ * 0.5f + 0.01f);
    const float vel = melodyGain_ * (0.72f + 0.2f * phrasePos + 0.08f * nextRandom());
    fireOneShot(melodySample_, midi, vel, pan);

    phraseNotesRemaining_--;
    const float gap = noteGapMin_ + nextRandom() * (noteGapMax_ - noteGapMin_);
    phraseNoteSamplesLeft_ = static_cast<int>(sampleRate_ * gap);
}

void AudioEngine::morphHarmony() {
    // Change only ONE pad voice at a time (voice leading) — no block chord jumps
    if (chordRoots_.empty() || chordIntervals_.empty() || activePads_ <= 0) return;

    if (nextVoiceToMorph_ == 0) {
        // Occasionally advance the harmonic center when we wrap voices
        if (nextRandom() < 0.55f) {
            chordIndex_ = (chordIndex_ + 1) % static_cast<int>(chordRoots_.size());
            chordRootMidi_ = chordRoots_[chordIndex_];
        }
    }

    const int voice = nextVoiceToMorph_ % activePads_;
    const int iv = chordIntervals_[voice % static_cast<int>(chordIntervals_.size())];
    // Small optional octave displacement for smoothness
    int midi = chordRootMidi_ + iv;
    if (voice == 0 && midi > 52) midi -= 12; // keep bass low
    pads_[voice].targetFrequency = midiToHz(midi);

    nextVoiceToMorph_ = (nextVoiceToMorph_ + 1) % activePads_;
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
    nextVoiceToMorph_ = 0;
    phraseNotesRemaining_ = 0;
    phraseNoteSamplesLeft_ = 0;
    padGlide_ = 0.000035f;
    padLevel_ = 0.65f;
    maskGain_ = 0.045f;

    switch (style) {
        case MusicStyle::Fantasy: {
            chordRoots_ = {50, 53, 55, 48};
            chordIntervals_ = {0, 3, 7, 10, 14};
            activePads_ = 5;
            scaleMidi_ = buildScaleMidi(62, {0, 2, 3, 5, 7, 9, 10}, 2);
            melodySample_ = SampleId::HarpPluck;
            melodyGain_ = 0.3f;
            noteGapMin_ = 0.42f;
            noteGapMax_ = 0.75f;
            phraseRestMin_ = 3.0f;
            phraseRestMax_ = 6.0f;
            phraseLenMin_ = 5;
            phraseLenMax_ = 8;
            loopA_ = {SampleId::SoftPad, 0.0f, 0.1f, 0.5f};
            maskGain_ = 0.04f;
            padLevel_ = 0.6f;
            lfo1Rate_ = 0.02f;
            lfo2Rate_ = 0.035f;
            for (int i = 0; i < activePads_; ++i) {
                pads_[i].wave = 0;
                pads_[i].amplitude = (0.11f - i * 0.014f) * padLevel_;
                pads_[i].detune = (nextRandom() - 0.5f) * 0.0025f;
            }
            break;
        }
        case MusicStyle::SciFi: {
            chordRoots_ = {48, 53, 58, 55};
            chordIntervals_ = {0, 5, 10, 15};
            activePads_ = 4;
            scaleMidi_ = buildScaleMidi(60, {0, 2, 5, 7, 9, 12}, 2);
            melodySample_ = SampleId::GlassBell;
            melodyGain_ = 0.26f;
            noteGapMin_ = 0.5f;
            noteGapMax_ = 0.9f;
            phraseRestMin_ = 4.0f;
            phraseRestMax_ = 7.5f;
            phraseLenMin_ = 4;
            phraseLenMax_ = 7;
            loopA_ = {SampleId::SoftPad, 0.0f, 0.09f, 0.65f};
            maskGain_ = 0.035f;
            padLevel_ = 0.55f;
            padGlide_ = 0.000025f;
            lfo1Rate_ = 0.035f;
            lfo2Rate_ = 0.06f;
            for (int i = 0; i < activePads_; ++i) {
                pads_[i].wave = 1;
                pads_[i].amplitude = (0.09f - i * 0.012f) * padLevel_;
                pads_[i].detune = (nextRandom() - 0.5f) * 0.006f;
            }
            break;
        }
        case MusicStyle::Noir: {
            chordRoots_ = {45, 50, 43, 48};
            chordIntervals_ = {0, 3, 7, 10};
            activePads_ = 4;
            scaleMidi_ = buildScaleMidi(57, {0, 2, 3, 5, 7, 8, 10}, 2);
            melodySample_ = SampleId::SoftPiano;
            melodyGain_ = 0.32f;
            noteGapMin_ = 0.48f;
            noteGapMax_ = 0.85f;
            phraseRestMin_ = 4.5f;
            phraseRestMax_ = 8.0f;
            phraseLenMin_ = 4;
            phraseLenMax_ = 6;
            loopA_ = {SampleId::VinylLoop, 0.0f, 0.035f, 1.0f};
            maskGain_ = 0.05f;
            padLevel_ = 0.5f;
            for (int i = 0; i < activePads_; ++i) {
                pads_[i].wave = 0;
                pads_[i].amplitude = (0.07f - i * 0.01f) * padLevel_;
            }
            break;
        }
        case MusicStyle::Classical: {
            chordRoots_ = {48, 55, 53, 48, 50, 55};
            chordIntervals_ = {0, 4, 7, 12};
            activePads_ = 4;
            scaleMidi_ = buildScaleMidi(60, {0, 2, 4, 5, 7, 9, 11}, 2);
            melodySample_ = SampleId::SoftPiano;
            melodyGain_ = 0.34f;
            noteGapMin_ = 0.35f;
            noteGapMax_ = 0.65f;
            phraseRestMin_ = 2.8f;
            phraseRestMax_ = 5.5f;
            phraseLenMin_ = 5;
            phraseLenMax_ = 9;
            loopA_ = {SampleId::SoftPad, 0.0f, 0.07f, 0.45f};
            maskGain_ = 0.035f;
            padLevel_ = 0.55f;
            for (int i = 0; i < activePads_; ++i) {
                pads_[i].wave = 0;
                pads_[i].amplitude = (0.085f - i * 0.012f) * padLevel_;
            }
            break;
        }
        case MusicStyle::Nature: {
            chordRoots_ = {50, 55, 53, 48};
            chordIntervals_ = {0, 5, 7, 12};
            activePads_ = 4;
            scaleMidi_ = buildScaleMidi(62, {0, 2, 4, 7, 9}, 2);
            melodySample_ = SampleId::HarpPluck;
            melodyGain_ = 0.24f;
            noteGapMin_ = 0.55f;
            noteGapMax_ = 0.95f;
            phraseRestMin_ = 4.0f;
            phraseRestMax_ = 8.0f;
            phraseLenMin_ = 4;
            phraseLenMax_ = 7;
            loopA_ = {SampleId::RainLoop, 0.0f, 0.1f, 1.0f};
            loopB_ = {SampleId::WindLoop, 0.0f, 0.06f, 0.85f};
            maskGain_ = 0.03f;
            padLevel_ = 0.5f;
            for (int i = 0; i < activePads_; ++i) {
                pads_[i].wave = 0;
                pads_[i].amplitude = (0.075f - i * 0.012f) * padLevel_;
            }
            break;
        }
        case MusicStyle::Lofi: {
            chordRoots_ = {45, 50, 41, 43};
            chordIntervals_ = {0, 3, 7, 10};
            activePads_ = 4;
            scaleMidi_ = buildScaleMidi(57, {0, 2, 3, 5, 7, 8, 10}, 2);
            melodySample_ = SampleId::SoftPiano;
            melodyGain_ = 0.26f;
            noteGapMin_ = 0.45f;
            noteGapMax_ = 0.8f;
            phraseRestMin_ = 3.2f;
            phraseRestMax_ = 6.5f;
            phraseLenMin_ = 4;
            phraseLenMax_ = 7;
            loopA_ = {SampleId::VinylLoop, 0.0f, 0.07f, 1.0f};
            maskGain_ = 0.05f;
            padLevel_ = 0.55f;
            padGlide_ = 0.00003f;
            lfo1Rate_ = 0.015f;
            for (int i = 0; i < activePads_; ++i) {
                pads_[i].wave = 1;
                pads_[i].amplitude = (0.085f - i * 0.012f) * padLevel_;
                pads_[i].detune = (nextRandom() - 0.5f) * 0.01f;
            }
            break;
        }
        case MusicStyle::Meditation: {
            chordRoots_ = {48, 55, 53, 50};
            chordIntervals_ = {0, 7, 12};
            activePads_ = 3;
            scaleMidi_ = buildScaleMidi(60, {0, 2, 4, 7, 9}, 2);
            melodySample_ = SampleId::GlassBell;
            melodyGain_ = 0.18f;
            noteGapMin_ = 0.7f;
            noteGapMax_ = 1.2f;
            phraseRestMin_ = 6.0f;
            phraseRestMax_ = 11.0f;
            phraseLenMin_ = 3;
            phraseLenMax_ = 5;
            loopA_ = {SampleId::SoftPad, 0.0f, 0.12f, 0.4f};
            maskGain_ = 0.055f;
            padLevel_ = 0.7f;
            padGlide_ = 0.00002f;
            lfo1Rate_ = 0.055f;
            for (int i = 0; i < activePads_; ++i) {
                pads_[i].wave = 0;
                pads_[i].amplitude = (0.11f - i * 0.025f) * padLevel_;
            }
            break;
        }
    }

    chordRootMidi_ = chordRoots_.empty() ? 48 : chordRoots_[0];
    for (int i = 0; i < activePads_; ++i) {
        const int iv = chordIntervals_[std::min(i, static_cast<int>(chordIntervals_.size()) - 1)];
        int midi = chordRootMidi_ + iv;
        if (i == 0 && midi > 52) midi -= 12;
        pads_[i].frequency = midiToHz(midi);
        pads_[i].targetFrequency = pads_[i].frequency;
    }

    // Start with a short rest, then a phrase — avoids an abrupt opening chord hit
    harmonySamplesLeft_ = static_cast<int>(sampleRate_ * (4.0f + nextRandom() * 3.0f));
    phraseRestSamplesLeft_ = static_cast<int>(sampleRate_ * (1.5f + nextRandom() * 1.5f));
    phraseNoteSamplesLeft_ = 0;
    phraseNotesRemaining_ = 0;
}

void AudioEngine::renderStyle(float* output, int numFrames, int numChannels) {
    const float fadeStep = 1.0f / (fadeSeconds_.load() * sampleRate_);
    const float attackCoeff = 1.0f - std::exp(-1.0f / (0.012f * sampleRate_)); // ~12ms soft attack

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

        // Slow single-voice harmony morph — never a full chord slam
        if (--harmonySamplesLeft_ <= 0) {
            morphHarmony();
            harmonySamplesLeft_ = static_cast<int>(sampleRate_ * (3.5f + nextRandom() * 4.0f));
        }

        // Phrase engine: connected notes, then a rest
        if (phraseNotesRemaining_ > 0) {
            if (--phraseNoteSamplesLeft_ <= 0) {
                triggerNextPhraseNote();
            }
        } else if (--phraseRestSamplesLeft_ <= 0) {
            startMelodyPhrase();
            // Schedule rest after this phrase finishes (approximate)
            const float rest = phraseRestMin_ + nextRandom() * (phraseRestMax_ - phraseRestMin_);
            phraseRestSamplesLeft_ = static_cast<int>(sampleRate_ * rest);
        }

        float left = 0.0f;
        float right = 0.0f;

        float breath = (currentStyle_ == MusicStyle::Meditation)
            ? (0.65f + 0.35f * lfo1)
            : (0.92f + 0.08f * lfo1);

        for (int i = 0; i < activePads_; ++i) {
            auto& v = pads_[i];
            v.frequency += (v.targetFrequency - v.frequency) * padGlide_;
            float amp = v.amplitude * breath;
            if (i > 0) amp *= (0.9f + 0.1f * lfo2);
            float s = osc(v.phase, v.wave) * amp;
            float pan = (i - (activePads_ - 1) * 0.5f) * 0.12f;
            left += s * (1.0f - pan);
            right += s * (1.0f + pan);
            v.phase += (v.frequency * (1.0f + v.detune)) / sampleRate_;
            if (v.phase >= 1.0f) v.phase -= 1.0f;
        }

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
            shot.gain += (shot.targetGain - shot.gain) * attackCoeff;
            const float frac = shot.pos - static_cast<float>(i0);
            const float s = (buf.data[i0] * (1.0f - frac) + buf.data[i0 + 1] * frac) * shot.gain;
            left += s * (1.0f - shot.pan);
            right += s * (1.0f + shot.pan);
            shot.pos += shot.rate;
        }

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

        float mask = softMaskNoise() * maskGain_ * (0.9f + 0.1f * lfo2);
        left += mask;
        right += mask;

        left = std::tanh(left * 1.02f) * 0.9f;
        right = std::tanh(right * 1.02f) * 0.9f;

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
