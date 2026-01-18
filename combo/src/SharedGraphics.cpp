#include "combo/SharedGraphics.h"
#include <atomic>
#include <mutex>
#include <spdlog/spdlog.h>

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
    SPDLOG_INFO("[SharedGraphics] SetSharedGraphics called: windowID={}, ctx={}",
                sdlWindowID, glContext);
    std::lock_guard<std::mutex> lock(g_sharedGraphicsMutex);
    g_sharedWindowID.store(sdlWindowID, std::memory_order_release);
    g_sharedGLContext = glContext;
    SPDLOG_INFO("[SharedGraphics] Stored shared graphics: windowID={}", sdlWindowID);
}

bool Combo_GetSharedGraphics(uint32_t* sdlWindowID, void** glContext) {
    SPDLOG_INFO("[SharedGraphics] GetSharedGraphics called");
    std::lock_guard<std::mutex> lock(g_sharedGraphicsMutex);

    uint32_t windowID = g_sharedWindowID.load(std::memory_order_acquire);
    void* ctx = g_sharedGLContext;
    SPDLOG_INFO("[SharedGraphics] Current state: windowID={}, ctx={}", windowID, ctx);

    if (windowID == 0 || ctx == nullptr) {
        SPDLOG_WARN("[SharedGraphics] No shared graphics available (windowID={}, ctx={})",
                    windowID, ctx);
        return false;
    }

    if (sdlWindowID != nullptr) {
        *sdlWindowID = windowID;
    }
    if (glContext != nullptr) {
        *glContext = ctx;
    }

    SPDLOG_INFO("[SharedGraphics] Returning shared graphics: windowID={}", windowID);
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
