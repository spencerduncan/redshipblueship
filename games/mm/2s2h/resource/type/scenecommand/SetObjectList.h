#pragma once

#include <cstdint>
#include <vector>
#include "SceneCommand.h"

namespace S2H {
class SetObjectList : public SceneCommand<int16_t> {
  public:
    using SceneCommand::SceneCommand;

    int16_t* GetPointer();
    size_t GetPointerSize();

    uint32_t numObjects;
    std::vector<int16_t> objects;
};
}; // namespace S2H
