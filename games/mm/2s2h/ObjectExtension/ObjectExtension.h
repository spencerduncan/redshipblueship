#pragma once

#ifdef RSBS_SINGLE_EXECUTABLE
// Single-exe symbol split (Lane C0, #392): OoT ships an identically-named
// `class ObjectExtension` (games/oot/soh/ObjectExtension/ObjectExtension.h,
// in the always-linked soh split), so MM's out-of-line members
// (GetInstance/RegisterId/Free) would cross-bind to OoT's bodies — MM actors
// registered into OoT's extension store, the #395/#382 hazard class. MM's
// class moves into namespace S2H (the ShipInit.hpp precedent: only MM is
// renamed, unqualified MM callers compile unchanged via the using-decl), and
// the extern "C" surface gets the MM_ prefix through this header, which every
// caller (z_actor.c included) reaches it by.
#define ObjectExtension_Free MM_ObjectExtension_Free
#endif

#ifdef __cplusplus

#include <any>
#include <cassert>
#include <limits>
#include <stdint.h>
#include <unordered_map>

#ifdef RSBS_SINGLE_EXECUTABLE
namespace S2H {
#endif

/*
 * This class can attach additional data to pointers. It can only attach a single instance of each type of data.
 * Use the ObjectExtension::Register class to register a type to be used as an object extension.
 * An example usage is:
 *
 * struct MyData {
 *     s32 data = -1;
 * };
 * static ObjectExtension::Register<MyData> MyDataRegister;
 *
 * Then you can get with
 * ObjectExtension::GetInstance().Get<MyData>(ptr);
 * and set with
 * ObjectExtension::GetInstance().Set<MyData>(ptr, MyData{});
 * (or with the returned pointer from Get()).
 */
class ObjectExtension {
  public:
    using Id = uint32_t;

    static constexpr Id InvalidId = std::numeric_limits<Id>::max();

    // Registers type T to be used as an object extension
    template <typename T> class Register {
      public:
        Register() {
            Id = ObjectExtension::GetInstance().RegisterId();
        }

        static ObjectExtension::Id Id;
    };

    // Gets the singleton ObjectExtension instance
    static ObjectExtension& GetInstance();

    // Gets the data of type T associated with an object, or nullptr if no such data has been attached
    template <typename T> T* Get(const void* object) {
        assert(ObjectExtension::Register<T>::Id != InvalidId);
        if (object == nullptr) {
            return nullptr;
        }

        auto it = Data.find(std::make_pair(object, ObjectExtension::Register<T>::Id));
        if (it == Data.end()) {
            return nullptr;
        }

        return std::any_cast<T>(&(it->second));
    }

    // Sets the data of type T for an object. Data will be copied.
    template <typename T> void Set(const void* object, const T&& data) {
        assert(ObjectExtension::Register<T>::Id != InvalidId);
        if (object != nullptr) {
            Data[std::make_pair(object, ObjectExtension::Register<T>::Id)] = data;
        }
    }

    // Returns true if an object has data of type T associated with it
    template <typename T> bool Has(const void* object) {
        assert(ObjectExtension::Register<T>::Id != InvalidId);
        if (object == nullptr) {
            return false;
        }

        return Data.contains(std::make_pair(object, ObjectExtension::Register<T>::Id));
    }

    // Removes data of type T from an object
    template <typename T> void Remove(const void* object) {
        assert(ObjectExtension::Register<T>::Id != InvalidId);

        Data.erase(std::make_pair(object, ObjectExtension::Register<T>::Id));
    }

    // Removes all data from an object
    void Free(const void* object);

  private:
    ObjectExtension() = default;

    // Returns the next free object extension Id
    Id RegisterId();

    ObjectExtension::Id NextId = 0;

    struct KeyHash {
        std::size_t operator()(const std::pair<const void*, ObjectExtension::Id>& key) const {
            return std::hash<const void*>{}(key.first) ^ (std::hash<ObjectExtension::Id>{}(key.second) << 1);
        }
    };

    // Collection of all object extension data.
    std::unordered_map<std::pair<const void*, ObjectExtension::Id>, std::any, KeyHash> Data;
};

// Static template globals
template <typename T> ObjectExtension::Id ObjectExtension::Register<T>::Id = ObjectExtension::InvalidId;

#ifdef RSBS_SINGLE_EXECUTABLE
} // namespace S2H

// Let unqualified upstream MM callers resolve to the S2H version unchanged.
using S2H::ObjectExtension;
#endif

extern "C" {
#endif // __cplusplus

void ObjectExtension_Free(const void* object);
#ifdef __cplusplus
}
#endif