#include "SetPathways.h"

namespace S2H {
PathDataMM** SetPathwaysMM::GetPointer() {
    return paths.data();
}
size_t SetPathwaysMM::GetPointerSize() {
    return paths.size() * sizeof(PathDataMM*);
}
} // namespace S2H
