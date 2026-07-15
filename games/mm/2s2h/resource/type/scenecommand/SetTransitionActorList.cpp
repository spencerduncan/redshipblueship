#include "SetTransitionActorList.h"

namespace S2H {
TransitionActorEntry* SetTransitionActorList::GetPointer() {
    return transitionActorList.data();
}

size_t SetTransitionActorList::GetPointerSize() {
    return transitionActorList.size() * sizeof(TransitionActorEntry);
}
} // namespace S2H
