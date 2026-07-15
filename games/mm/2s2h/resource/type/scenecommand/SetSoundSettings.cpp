#include "SetSoundSettings.h"

namespace S2H {
SoundSettings* SetSoundSettings::GetPointer() {
    return &settings;
}

size_t SetSoundSettings::GetPointerSize() {
    return sizeof(SoundSettings);
}
} // namespace S2H
