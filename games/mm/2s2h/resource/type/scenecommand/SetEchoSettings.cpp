#include "SetEchoSettings.h"

namespace S2H {
EchoSettings* SetEchoSettings::GetPointer() {
    return &settings;
}

size_t SetEchoSettings::GetPointerSize() {
    return sizeof(EchoSettings);
}
} // namespace S2H
