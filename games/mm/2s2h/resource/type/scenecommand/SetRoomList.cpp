#include "SetRoomList.h"

namespace S2H {
RomFile* SetRoomList::GetPointer() {
    return rooms.data();
}

size_t SetRoomList::GetPointerSize() {
    return rooms.size() * sizeof(RomFile);
}
} // namespace S2H
