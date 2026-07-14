#include "SetMesh.h"

namespace S2H {
MeshHeader* SetMesh::GetPointer() {
    return &meshHeader;
}

size_t SetMesh::GetPointerSize() {
    return sizeof(MeshHeader);
}
} // namespace S2H
