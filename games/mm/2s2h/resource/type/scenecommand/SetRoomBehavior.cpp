#include "SetRoomBehavior.h"

namespace S2H {
RoomBehaviorMM* SetRoomBehaviorMM::GetPointer() {
    return &roomBehavior;
}
size_t SetRoomBehaviorMM::GetPointerSize() {
    return sizeof(RoomBehaviorMM);
}
} // namespace S2H
