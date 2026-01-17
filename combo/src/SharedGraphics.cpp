#include "combo/SharedGraphics.h"
#include <atomic>
#include <mutex>

namespace {
    // Static storage for shared graphics
    // Using atomic for the window ID and mutex for the context pointer
    // to ensure thread-safe access across DLL boundaries
    std::atomic<uint32_t> g_sharedWindowID{0};
    void* g_sharedGLContext = nullptr;
    std::mutex g_sharedGraphicsMutex;
}

extern "C" {

void Combo_SetSharedGraphics(uint32_t sdlWindowID, void* glContext) {
    std::lock_guard<std::mutex> lock(g_sharedGraphicsMutex);
    g_sharedWindowID.store(sdlWindowID, std::memory_order_release);
    g_sharedGLContext = glContext;
}

bool Combo_GetSharedGraphics(uint32_t* sdlWindowID, void** glContext) {
    std::lock_guard<std::mutex> lock(g_sharedGraphicsMutex);

    uint32_t windowID = g_sharedWindowID.load(std::memory_order_acquire);
    void* ctx = g_sharedGLContext;

    if (windowID == 0 || ctx == nullptr) {
        return false;
    }

    if (sdlWindowID != nullptr) {
        *sdlWindowID = windowID;
    }
    if (glContext != nullptr) {
        *glContext = ctx;
    }

    return true;
}

bool Combo_HasSharedGraphics(void) {
    std::lock_guard<std::mutex> lock(g_sharedGraphicsMutex);
    return g_sharedWindowID.load(std::memory_order_acquire) != 0
           && g_sharedGLContext != nullptr;
}

void Combo_ClearSharedGraphics(void) {
    std::lock_guard<std::mutex> lock(g_sharedGraphicsMutex);
    g_sharedWindowID.store(0, std::memory_order_release);
    g_sharedGLContext = nullptr;
}

} // extern "C"
