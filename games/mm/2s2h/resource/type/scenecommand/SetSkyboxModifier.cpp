#include "SetSkyboxModifier.h"

namespace S2H {
SkyboxModifier* SetSkyboxModifier::GetPointer() {
    return &modifier;
}

size_t SetSkyboxModifier::GetPointerSize() {
    return sizeof(SkyboxModifier);
}
} // namespace S2H
