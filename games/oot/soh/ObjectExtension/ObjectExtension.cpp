#include "ObjectExtension.h"

ObjectExtension& ObjectExtension::GetInstance() {
    static ObjectExtension instance;
    return instance;
}

ObjectExtension::Id ObjectExtension::RegisterId() {
    return NextId++;
}

void ObjectExtension::Free(const void* object) {
    if (object == nullptr) {
        return;
    }

    std::erase_if(Data, [&object](const auto& iter) {
        auto const& [key, value] = iter;
        return key.first == object;
    });
}

#ifdef RSBS_SINGLE_EXECUTABLE
size_t ObjectExtension::ClearAll() {
    const size_t cleared = Data.size();
    Data.clear();
    return cleared;
}
#endif

extern "C" void ObjectExtension_Free(const void* object) {
    ObjectExtension::GetInstance().Free(object);
}
