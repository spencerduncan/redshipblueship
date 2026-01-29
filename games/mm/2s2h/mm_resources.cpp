/**
 * MM-specific Resource Manager Functions
 *
 * These functions are only in MM, not in OoT. They're extracted here
 * so BenPort.cpp can be excluded in single-exe builds (too many collisions).
 */

#include <libultraship/libultraship.h>
#include <fast/lus_gbi.h>
#include "resource/type/Array.h"
#include "z64animation.h"
#include "z64keyframe.h"

// Helper to get resource by name
static std::shared_ptr<Ship::IResource> MM_GetResourceByName(const char* path) {
    return Ship::Context::GetInstance()->GetResourceManager()->LoadResource(path);
}

// Helper to get raw data from resource
static void* MM_ResourceGetDataByName(const char* path) {
    auto res = MM_GetResourceByName(path);
    if (res == nullptr) {
        return nullptr;
    }
    return res->GetRawPointer();
}

extern "C" char* ResourceMgr_LoadVtxArrayByName(const char* path) {
    auto res = std::static_pointer_cast<SOH::Array>(MM_GetResourceByName(path));
    if (res == nullptr) {
        return nullptr;
    }
    return (char*)res->Vertices.data();
}

extern "C" size_t ResourceMgr_GetVtxArraySizeByName(const char* path) {
    auto res = std::static_pointer_cast<SOH::Array>(MM_GetResourceByName(path));
    if (res == nullptr) {
        return 0;
    }
    return res->Vertices.size();
}

extern "C" u8* ResourceMgr_LoadArrayByNameAsU8(const char* path, u8* buffer) {
    auto res = std::static_pointer_cast<SOH::Array>(MM_GetResourceByName(path));
    if (res == nullptr) {
        return nullptr;
    }

    if (buffer == nullptr) {
        buffer = (u8*)malloc(sizeof(u8) * res->Scalars.size());
    }

    for (size_t i = 0; i < res->Scalars.size(); i++) {
        buffer[i] = res->Scalars[i].u8;
    }

    return buffer;
}

extern "C" Mtx* ResourceMgr_LoadMtxByName(const char* path) {
    return (Mtx*)MM_ResourceGetDataByName(path);
}

extern "C" KeyFrameSkeleton* ResourceMgr_LoadKeyFrameSkelByName(const char* path) {
    return (KeyFrameSkeleton*)MM_ResourceGetDataByName(path);
}

extern "C" KeyFrameAnimation* ResourceMgr_LoadKeyFrameAnimByName(const char* path) {
    return (KeyFrameAnimation*)MM_ResourceGetDataByName(path);
}
