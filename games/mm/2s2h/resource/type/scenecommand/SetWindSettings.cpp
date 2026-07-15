#include "SetWindSettings.h"

namespace S2H {
WindSettings* SetWindSettings::GetPointer() {
    return &settings;
}

size_t SetWindSettings::GetPointerSize() {
    return sizeof(WindSettings);
}
} // namespace S2H
