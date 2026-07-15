#include "SetAnimatedMaterialList.h"

namespace S2H {

AnimatedMaterial* SetAnimatedMaterialList::GetPointer() {
    return mat;
}

size_t SetAnimatedMaterialList::GetPointerSize() {
    return sizeof(AnimatedMaterial);
}

} // namespace S2H