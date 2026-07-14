#include "SetTimeSettings.h"

namespace S2H {
TimeSettings* SetTimeSettings::GetPointer() {
    return &settings;
}

size_t SetTimeSettings::GetPointerSize() {
    return sizeof(TimeSettings);
}
} // namespace S2H
