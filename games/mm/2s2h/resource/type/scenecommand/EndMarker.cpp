#include "EndMarker.h"

namespace S2H {
Marker* EndMarker::GetPointer() {
    return &endMarker;
}

size_t EndMarker::GetPointerSize() {
    return sizeof(Marker);
}
} // namespace S2H
