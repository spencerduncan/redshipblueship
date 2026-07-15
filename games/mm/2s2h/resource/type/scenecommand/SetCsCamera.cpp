#include "SetCsCamera.h"

namespace S2H {
SetCsCamera::~SetCsCamera() {
    for (auto c : csCamera) {
        if (c.actorCsCamFuncData != nullptr) {
            delete[] c.actorCsCamFuncData;
        }
    }
}
ActorCsCamInfoData* SetCsCamera::GetPointer() {
    return csCamera.data();
}

size_t SetCsCamera::GetPointerSize() {
    return sizeof(ActorCsCamInfoData);
}
} // namespace S2H
