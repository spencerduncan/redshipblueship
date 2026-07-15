#pragma once

#include <vector>
#include <string>
#include <ship/resource/Resource.h>

namespace S2H {

class PlayerAnimation : public Ship::Resource<int16_t> {
  public:
    using Resource::Resource;

    PlayerAnimation() : Resource(std::shared_ptr<Ship::ResourceInitData>()) {
    }

    int16_t* GetPointer();
    size_t GetPointerSize();

    std::vector<int16_t> limbRotData;
};
} // namespace S2H
