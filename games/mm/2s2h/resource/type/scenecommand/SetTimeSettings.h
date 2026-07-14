#pragma once

#include <cstdint>
#include "SceneCommand.h"

namespace S2H {
typedef struct {
    uint8_t hour;
    uint8_t minute;
    uint8_t timeIncrement;
} TimeSettings;

class SetTimeSettings : public SceneCommand<TimeSettings> {
  public:
    using SceneCommand::SceneCommand;

    TimeSettings* GetPointer();
    size_t GetPointerSize();

    TimeSettings settings;
};
}; // namespace S2H
