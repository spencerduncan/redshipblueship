#include "SetCameraSettings.h"

namespace S2H {
CameraSettings* SetCameraSettings::GetPointer() {
    return &settings;
}

size_t SetCameraSettings::GetPointerSize() {
    return sizeof(CameraSettings);
}
} // namespace S2H
