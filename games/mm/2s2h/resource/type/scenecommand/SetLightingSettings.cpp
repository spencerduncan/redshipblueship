#include "SetLightingSettings.h"

namespace S2H {
EnvLightSettings* SetLightingSettings::GetPointer() {
    return settings.data();
}

size_t SetLightingSettings::GetPointerSize() {
    return settings.size() * sizeof(EnvLightSettings);
}
} // namespace S2H
