#pragma once

#include <cstdint>
#include "SceneCommand.h"

namespace S2H {
typedef struct {
    int8_t windWest;
    int8_t windVertical;
    int8_t windSouth;
    uint8_t windSpeed;
} WindSettings;

class SetWindSettings : public SceneCommand<WindSettings> {
  public:
    using SceneCommand::SceneCommand;

    WindSettings* GetPointer();
    size_t GetPointerSize();

    WindSettings settings;
};
}; // namespace S2H
