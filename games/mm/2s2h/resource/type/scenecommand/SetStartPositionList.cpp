#include "SetStartPositionList.h"

namespace S2H {
ActorEntry* SetStartPositionList::GetPointer() {
    return startPositions.data();
}

size_t SetStartPositionList::GetPointerSize() {
    return startPositions.size() * sizeof(ActorEntry);
}
} // namespace S2H
