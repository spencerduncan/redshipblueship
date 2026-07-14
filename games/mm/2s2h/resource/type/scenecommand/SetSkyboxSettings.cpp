#include "SetSkyboxSettings.h"

namespace S2H {
SkyboxSettings* SetSkyboxSettings::GetPointer() {
    return &settings;
}

size_t SetSkyboxSettings::GetPointerSize() {
    return sizeof(SetSkyboxSettings);
}
} // namespace S2H
