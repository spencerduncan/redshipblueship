#include "SetSpecialObjects.h"

namespace S2H {
SpecialObjects* SetSpecialObjects::GetPointer() {
    return &specialObjects;
}

size_t SetSpecialObjects::GetPointerSize() {
    return sizeof(SpecialObjects);
}
} // namespace S2H
