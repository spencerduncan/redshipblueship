#include "SetMinimapList.h"

namespace S2H {

MinimapListData* SetMinimapList::GetPointer() {
    return &list;
}

size_t SetMinimapList::GetPointerSize() {
    return sizeof(list);
}

} // namespace S2H