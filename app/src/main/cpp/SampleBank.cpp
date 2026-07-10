#include "SampleBank.h"

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace readingmusic {

namespace {
constexpr float kTwoPi = 2.0f * static_cast<float>(M_PI);
}

float SampleBank::hashNoise(uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    return static_cast<float>(state) / static_cast<float>(UINT32_MAX) * 2.0f - 1.0f;
}

void SampleBank::build(float sampleRate) {
    if (ready_) return;
    bakeSoftPiano(sampleRate);
    bakeHarp(sampleRate);
    bakeGlass(sampleRate);
    bakeSoftPad(sampleRate);
    bakeRain(sampleRate);
    bakeVinyl(sampleRate);
    bakeWind(sampleRate);
    ready_ = true;
}

const SampleBuffer& SampleBank::get(SampleId id) const {
    return samples_[static_cast<int>(id)];
}

void SampleBank::bakeSoftPiano(float sr) {
    // ~1.8s soft piano-like one-shot at C5 reference (523 Hz)
    const int n = static_cast<int>(sr * 1.8f);
    auto& buf = samples_[static_cast<int>(SampleId::SoftPiano)];
    buf.data.resize(n);
    buf.loopable = false;
    const float f0 = 523.25f;
    for (int i = 0; i < n; ++i) {
        const float t = static_cast<float>(i) / sr;
        const float env = std::exp(-2.8f * t) * (1.0f - std::exp(-90.0f * t));
        float s = 0.0f;
        // Inharmonic-ish partials for a piano-like body
        s += 1.00f * std::sin(kTwoPi * f0 * t) * std::exp(-2.6f * t);
        s += 0.45f * std::sin(kTwoPi * f0 * 2.003f * t) * std::exp(-3.4f * t);
        s += 0.22f * std::sin(kTwoPi * f0 * 3.01f * t) * std::exp(-4.2f * t);
        s += 0.10f * std::sin(kTwoPi * f0 * 4.02f * t) * std::exp(-5.5f * t);
        s += 0.05f * std::sin(kTwoPi * f0 * 5.04f * t) * std::exp(-7.0f * t);
        // Soft hammer noise at attack
        uint32_t st = 0x51AFu + static_cast<uint32_t>(i);
        s += 0.04f * hashNoise(st) * std::exp(-55.0f * t);
        buf.data[i] = s * env * 0.55f;
    }
}

void SampleBank::bakeHarp(float sr) {
    // Karplus–Strong plucked string ~1.4s at C5
    const int n = static_cast<int>(sr * 1.4f);
    auto& buf = samples_[static_cast<int>(SampleId::HarpPluck)];
    buf.data.resize(n);
    buf.loopable = false;

    const float freq = 523.25f;
    int delay = std::max(2, static_cast<int>(sr / freq));
    std::vector<float> line(delay);
    uint32_t rng = 0xA24Bu;
    for (int i = 0; i < delay; ++i) {
        line[i] = hashNoise(rng);
    }
    int idx = 0;
    float prev = 0.0f;
    for (int i = 0; i < n; ++i) {
        float filtered = 0.5f * (line[idx] + prev);
        prev = line[idx];
        // Light stretch / brightness
        filtered = 0.996f * filtered + 0.004f * hashNoise(rng) * std::exp(-8.0f * i / sr);
        line[idx] = filtered;
        idx = (idx + 1) % delay;
        const float env = std::exp(-1.6f * i / sr);
        buf.data[i] = filtered * env * 0.7f;
    }
}

void SampleBank::bakeGlass(float sr) {
    // Soft FM glass / crystal bell ~1.6s
    const int n = static_cast<int>(sr * 1.6f);
    auto& buf = samples_[static_cast<int>(SampleId::GlassBell)];
    buf.data.resize(n);
    buf.loopable = false;
    const float car = 640.0f;
    const float mod = 980.0f;
    for (int i = 0; i < n; ++i) {
        const float t = static_cast<float>(i) / sr;
        const float env = std::exp(-2.2f * t) * (1.0f - std::exp(-40.0f * t));
        const float index = 2.4f * std::exp(-3.0f * t);
        float s = std::sin(kTwoPi * car * t + index * std::sin(kTwoPi * mod * t));
        s += 0.25f * std::sin(kTwoPi * car * 1.5f * t) * std::exp(-3.5f * t);
        buf.data[i] = s * env * 0.45f;
    }
}

void SampleBank::bakeSoftPad(float sr) {
    // Seamless-ish soft pad grain (~2s), used as sustained texture
    const int n = static_cast<int>(sr * 2.0f);
    auto& buf = samples_[static_cast<int>(SampleId::SoftPad)];
    buf.data.resize(n);
    buf.loopable = true;
    for (int i = 0; i < n; ++i) {
        const float t = static_cast<float>(i) / sr;
        float s = 0.0f;
        s += 0.55f * std::sin(kTwoPi * 110.0f * t);
        s += 0.35f * std::sin(kTwoPi * 164.8f * t + 0.3f);
        s += 0.25f * std::sin(kTwoPi * 220.0f * t + 0.7f);
        s += 0.15f * std::sin(kTwoPi * 277.2f * t + 1.1f);
        // Slow AM for movement
        s *= 0.85f + 0.15f * std::sin(kTwoPi * 0.35f * t);
        // Fade edges for looping
        float edge = 1.0f;
        const float fade = 0.08f * sr;
        if (i < fade) edge = i / fade;
        if (i > n - fade) edge = (n - i) / fade;
        buf.data[i] = s * edge * 0.25f;
    }
}

void SampleBank::bakeRain(float sr) {
    const int n = static_cast<int>(sr * 2.0f);
    auto& buf = samples_[static_cast<int>(SampleId::RainLoop)];
    buf.data.resize(n);
    buf.loopable = true;
    uint32_t rng = 0xA14Cu;
    float lp = 0.0f;
    float hp = 0.0f;
    for (int i = 0; i < n; ++i) {
        float white = hashNoise(rng);
        lp += 0.12f * (white - lp);
        hp += 0.02f * (lp - hp);
        float drop = white - hp;
        // Occasional louder droplets
        if ((rng & 0x3FFu) == 0) {
            drop += hashNoise(rng) * 0.8f;
        }
        float edge = 1.0f;
        const float fade = 0.05f * sr;
        if (i < fade) edge = i / fade;
        if (i > n - fade) edge = (n - i) / fade;
        buf.data[i] = drop * 0.18f * edge;
    }
}

void SampleBank::bakeVinyl(float sr) {
    const int n = static_cast<int>(sr * 1.2f);
    auto& buf = samples_[static_cast<int>(SampleId::VinylLoop)];
    buf.data.resize(n);
    buf.loopable = true;
    uint32_t rng = 0xB1C1u;
    float brown = 0.0f;
    for (int i = 0; i < n; ++i) {
        float white = hashNoise(rng);
        brown = (brown + 0.02f * white) / 1.02f;
        float crackle = 0.0f;
        if ((rng & 0x1FFu) < 3) {
            crackle = hashNoise(rng) * 0.6f;
        }
        float edge = 1.0f;
        const float fade = 0.04f * sr;
        if (i < fade) edge = i / fade;
        if (i > n - fade) edge = (n - i) / fade;
        buf.data[i] = (brown * 2.2f * 0.35f + crackle * 0.25f + white * 0.02f) * edge;
    }
}

void SampleBank::bakeWind(float sr) {
    const int n = static_cast<int>(sr * 2.5f);
    auto& buf = samples_[static_cast<int>(SampleId::WindLoop)];
    buf.data.resize(n);
    buf.loopable = true;
    uint32_t rng = 0xC1DEu;
    float lp = 0.0f;
    for (int i = 0; i < n; ++i) {
        float white = hashNoise(rng);
        lp += 0.04f * (white - lp);
        const float t = static_cast<float>(i) / sr;
        float mod = 0.7f + 0.3f * std::sin(kTwoPi * 0.15f * t);
        float edge = 1.0f;
        const float fade = 0.1f * sr;
        if (i < fade) edge = i / fade;
        if (i > n - fade) edge = (n - i) / fade;
        buf.data[i] = lp * 3.0f * mod * 0.22f * edge;
    }
}

} // namespace readingmusic
