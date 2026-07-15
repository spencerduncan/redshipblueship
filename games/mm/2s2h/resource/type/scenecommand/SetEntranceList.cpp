#include "SetEntranceList.h"

namespace S2H {
EntranceEntry* SetEntranceList::GetPointer() {
    return entrances.data();
}

size_t SetEntranceList::GetPointerSize() {
    return entrances.size() * sizeof(EntranceEntry);
}
} // namespace S2H
