#pragma once

#include <cstdint>
#include <ship/resource/Resource.h>
#include "SceneCommand.h"

namespace S2H {
typedef struct {
    int8_t echo;
} EchoSettings;

class SetEchoSettings : public SceneCommand<EchoSettings> {
  public:
    using SceneCommand::SceneCommand;

    EchoSettings* GetPointer();
    size_t GetPointerSize();

    EchoSettings settings;
};
}; // namespace S2H
