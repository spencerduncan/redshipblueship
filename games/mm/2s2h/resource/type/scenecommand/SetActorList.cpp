#include "SetActorList.h"

namespace S2H {
ActorEntry* SetActorList::GetPointer() {
    return actorList.data();
}

size_t SetActorList::GetPointerSize() {
    return actorList.size() * sizeof(ActorEntry);
}
} // namespace S2H
