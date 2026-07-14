#include "Path.h"

namespace S2H {
PathDataMM* PathMM::GetPointer() {
    return pathData.data();
}
size_t PathMM::GetPointerSize() {
    return pathData.size() * sizeof(PathDataMM);
}

} // namespace S2H
