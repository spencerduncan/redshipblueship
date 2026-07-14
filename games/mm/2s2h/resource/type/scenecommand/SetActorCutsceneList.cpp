#include "SetActorCutsceneList.h"

namespace S2H {
CutsceneEntry* SetActorCutsceneList::GetPointer() {
    return entries.data();
}

size_t SetActorCutsceneList::GetPointerSize() {
    return entries.size() * sizeof(CutsceneEntry);
}

} // namespace S2H