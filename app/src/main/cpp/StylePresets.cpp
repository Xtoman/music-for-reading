#include "StylePresets.h"

namespace readingmusic {

const char* styleName(MusicStyle style) {
    switch (style) {
        case MusicStyle::Fantasy: return "Fantasy";
        case MusicStyle::SciFi: return "Sci-Fi";
        case MusicStyle::Noir: return "Noir";
        case MusicStyle::Classical: return "Classical";
        case MusicStyle::Nature: return "Nature";
        case MusicStyle::Lofi: return "Lo-fi";
        case MusicStyle::Meditation: return "Meditation";
        default: return "Unknown";
    }
}

int styleCount() {
    return 7;
}

} // namespace readingmusic
