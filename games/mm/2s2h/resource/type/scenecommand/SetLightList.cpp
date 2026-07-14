#include "SetLightList.h"

namespace S2H {
LightInfo* SetLightList::GetPointer() {
    return lightList.data();
}

size_t SetLightList::GetPointerSize() {
    return lightList.size() * sizeof(LightInfo);
}
} // namespace S2H
