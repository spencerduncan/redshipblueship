#pragma once

#include <cstdint>
#include "SceneCommand.h"

namespace S2H {
typedef struct {
    int8_t cameraMovement;
    int32_t worldMapArea;
} CameraSettings;

class SetCameraSettings : public SceneCommand<CameraSettings> {
  public:
    using SceneCommand::SceneCommand;

    CameraSettings* GetPointer();
    size_t GetPointerSize();

    CameraSettings settings;
};
}; // namespace S2H
