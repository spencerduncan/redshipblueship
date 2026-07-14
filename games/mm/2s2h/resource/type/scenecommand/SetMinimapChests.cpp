#include "SetMinimapChests.h"

namespace S2H {

MinimapChestData* SetMinimapChests::GetPointer() {
    return chests.data();
}

size_t SetMinimapChests::GetPointerSize() {
    return sizeof(MinimapChestData);
}

} // namespace S2H