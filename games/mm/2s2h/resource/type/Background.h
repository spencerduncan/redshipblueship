#pragma once

#include <ship/resource/Resource.h>

namespace S2H {
class Background : public Ship::Resource<uint8_t> {
  public:
    using Resource::Resource;

    Background() : Resource(std::shared_ptr<Ship::ResourceInitData>()) {
    }

    uint8_t* GetPointer();
    size_t GetPointerSize();

    std::vector<uint8_t> Data;
};
}; // namespace S2H
