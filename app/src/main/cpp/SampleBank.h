#pragma once

#include <cstdint>
#include <vector>

namespace readingmusic {

enum class SampleId : int {
    SoftPiano = 0,
    HarpPluck = 1,
    GlassBell = 2,
    SoftPad = 3,
    RainLoop = 4,
    VinylLoop = 5,
    WindLoop = 6,
    Count = 7
};

struct SampleBuffer {
    std::vector<float> data;
    bool loopable = false;
};

// Compact in-memory sample bank. Buffers are baked once with physical /
// additive models so the APK stays small and playback stays offline.
class SampleBank {
public:
    void build(float sampleRate);

    const SampleBuffer& get(SampleId id) const;

private:
    void bakeSoftPiano(float sr);
    void bakeHarp(float sr);
    void bakeGlass(float sr);
    void bakeSoftPad(float sr);
    void bakeRain(float sr);
    void bakeVinyl(float sr);
    void bakeWind(float sr);

    static float hashNoise(uint32_t& state);

    SampleBuffer samples_[static_cast<int>(SampleId::Count)]{};
    bool ready_ = false;
};

} // namespace readingmusic
