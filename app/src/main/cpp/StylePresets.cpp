#include "StylePresets.h"

namespace readingmusic {

const char* styleName(MusicStyle style) {
    switch (style) {
        case MusicStyle::DeepAmbient: return "Deep Ambient";
        case MusicStyle::SoftPiano: return "Soft Piano";
        case MusicStyle::RainPad: return "Rain & Pad";
        case MusicStyle::ZenGarden: return "Zen Garden";
        case MusicStyle::LofiHaze: return "Lo-fi Haze";
        case MusicStyle::NightForest: return "Night Forest";
        default: return "Unknown";
    }
}

int styleCount() {
    return 6;
}

} // namespace readingmusic
