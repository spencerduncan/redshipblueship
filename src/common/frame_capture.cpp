/**
 * @file frame_capture.cpp
 * @brief See frame_capture.h (#843).
 */

#include "frame_capture.h"
#include "ui_snapshot_image.h"

#include <SDL2/SDL.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <ship/Context.h>
#include <ship/window/Window.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace {

struct CaptureConfig {
    bool parsed = false;
    bool active = false;
    bool verify = false;
    std::vector<int> frames; // RSBS_CAPTURE_FRAMES=<n>[,<n>...]
    int every = 0;           // RSBS_CAPTURE_FRAMES=every:<k>
    std::string outDir;
};

CaptureConfig sConfig;
int sGameFrames[3] = { 0, 0, 0 };       // indexed by GameId
std::vector<std::string> sPending;      // names armed for the next drawn frame
std::vector<std::string> sInFlight;     // names the armed draw callback will write
ImGuiContext* sHookedContext = nullptr; // the context the RenderPre hook is on
bool sBackendRefused = false;

const char* NonEmptyEnv(const char* name) {
    const char* v = std::getenv(name);
    return (v != nullptr && v[0] != '\0') ? v : nullptr;
}

const CaptureConfig& Config() {
    if (sConfig.parsed) {
        return sConfig;
    }
    sConfig.parsed = true;
    const char* frames = NonEmptyEnv("RSBS_CAPTURE_FRAMES");
    const char* out = NonEmptyEnv("RSBS_CAPTURE_OUT");
    const char* shotDir = NonEmptyEnv("RSBS_GP_SHOT_DIR");
    const char* verify = NonEmptyEnv("RSBS_CAPTURE_VERIFY");
    sConfig.active = frames != nullptr || out != nullptr || shotDir != nullptr;
    sConfig.verify = verify != nullptr && std::strcmp(verify, "0") != 0;
    sConfig.outDir = out != nullptr ? out : (shotDir != nullptr ? shotDir : "frame-capture");
    if (frames != nullptr) {
        if (std::strncmp(frames, "every:", 6) == 0) {
            sConfig.every = std::atoi(frames + 6);
        } else {
            const char* p = frames;
            while (*p != '\0') {
                char* end = nullptr;
                const long n = std::strtol(p, &end, 10);
                if (end == p) {
                    p++; // skip a separator or stray character
                    continue;
                }
                if (n > 0) {
                    sConfig.frames.push_back((int)n);
                }
                p = end;
            }
        }
    }
    if (sConfig.active) {
        std::string list;
        for (int n : sConfig.frames) {
            list += (list.empty() ? "" : ",") + std::to_string(n);
        }
        std::fprintf(stderr, "[FRAME-CAPTURE] armed: frames=%s every=%d out=%s verify=%d\n",
                     list.empty() ? "-" : list.c_str(), sConfig.every, sConfig.outDir.c_str(), sConfig.verify ? 1 : 0);
        std::fflush(stderr);
    }
    return sConfig;
}

const char* GameTag(GameId game) {
    return game == GAME_OOT ? "oot" : (game == GAME_MM ? "mm" : "none");
}

std::string FramePath(GameId game, int n) {
    return Config().outDir + "/" + GameTag(game) + "-frame-" + std::to_string(n) + ".png";
}

// ----------------------------------------------------------------------------
// Readback: GL 1.1 entry points through SDL, as soh_ui_snapshot.cpp does, so
// this TU includes no GL header.
// ----------------------------------------------------------------------------
#ifdef _WIN32
#define FRAMECAP_GLAPI __stdcall
#else
#define FRAMECAP_GLAPI
#endif

constexpr unsigned kGlViewport = 0x0BA2;
constexpr unsigned kGlBack = 0x0405;
constexpr unsigned kGlReadBuffer = 0x0C02;
constexpr unsigned kGlPackAlignment = 0x0D05;
constexpr unsigned kGlPackRowLength = 0x0D02;
constexpr unsigned kGlRgba = 0x1908;
constexpr unsigned kGlUnsignedByte = 0x1401;
constexpr unsigned kGlFramebufferBinding = 0x8CA6;
constexpr unsigned kGlPixelPackBufferBinding = 0x88ED;
constexpr unsigned kGlPixelPackBuffer = 0x88EB;

struct GlApi {
    void(FRAMECAP_GLAPI* GetIntegerv)(unsigned, int*) = nullptr;
    void(FRAMECAP_GLAPI* ReadBuffer)(unsigned) = nullptr;
    void(FRAMECAP_GLAPI* PixelStorei)(unsigned, int) = nullptr;
    void(FRAMECAP_GLAPI* ReadPixels)(int, int, int, int, unsigned, unsigned, void*) = nullptr;
    void(FRAMECAP_GLAPI* BindBuffer)(unsigned, unsigned) = nullptr;
    bool loaded = false;

    bool Load() {
        if (!loaded) {
            GetIntegerv = (decltype(GetIntegerv))SDL_GL_GetProcAddress("glGetIntegerv");
            ReadBuffer = (decltype(ReadBuffer))SDL_GL_GetProcAddress("glReadBuffer");
            PixelStorei = (decltype(PixelStorei))SDL_GL_GetProcAddress("glPixelStorei");
            ReadPixels = (decltype(ReadPixels))SDL_GL_GetProcAddress("glReadPixels");
            BindBuffer = (decltype(BindBuffer))SDL_GL_GetProcAddress("glBindBuffer");
            loaded = true;
        }
        return GetIntegerv && ReadBuffer && PixelStorei && ReadPixels && BindBuffer;
    }
};

GlApi sGl;

void WriteInFlight(const UiImage* img, const char* why) {
    std::error_code ec;
    std::filesystem::create_directories(Config().outDir, ec);
    for (const std::string& name : sInFlight) {
        const std::string path = Config().outDir + "/" + name + ".png";
        if (img == nullptr) {
            std::fprintf(stderr, "[FRAME-CAPTURE] %s: NOT captured (%s)\n", path.c_str(), why);
            continue;
        }
        const int rc = UiImage_WritePng(img, path.c_str());
        std::fprintf(stderr, "[FRAME-CAPTURE] %s: %dx%d %s\n", path.c_str(), img->w, img->h,
                     rc == 0 ? "written" : "write FAILED");
    }
    std::fflush(stderr);
    sInFlight.clear();
}

// The draw callback, run by the OpenGL ImGui backend as the LAST command of the
// main viewport: the game frame and every ImGui window are in the back buffer.
void ReadbackCallback(const ImDrawList*, const ImDrawCmd*) {
    if (sInFlight.empty()) {
        return;
    }
    if (!sGl.Load()) {
        WriteInFlight(nullptr, "GL entry points unavailable");
        return;
    }
    int fb = -1;
    sGl.GetIntegerv(kGlFramebufferBinding, &fb);
    if (fb != 0) {
        WriteInFlight(nullptr, "the bound framebuffer is not the window's");
        return;
    }
    int viewport[4] = { 0, 0, 0, 0 };
    sGl.GetIntegerv(kGlViewport, viewport);
    const int w = viewport[2];
    const int h = viewport[3];
    UiImage img = {};
    if (w <= 0 || h <= 0 || UiImage_Alloc(&img, w, h) != 0) {
        WriteInFlight(nullptr, "bad viewport size");
        return;
    }
    int pbo = 0;
    int readBuffer = 0;
    int packAlign = 4;
    int packRowLength = 0;
    sGl.GetIntegerv(kGlPixelPackBufferBinding, &pbo);
    sGl.GetIntegerv(kGlReadBuffer, &readBuffer);
    sGl.GetIntegerv(kGlPackAlignment, &packAlign);
    sGl.GetIntegerv(kGlPackRowLength, &packRowLength);
    if (pbo != 0) {
        sGl.BindBuffer(kGlPixelPackBuffer, 0);
    }
    sGl.ReadBuffer(kGlBack);
    sGl.PixelStorei(kGlPackAlignment, 1);
    sGl.PixelStorei(kGlPackRowLength, 0);
    sGl.ReadPixels(viewport[0], viewport[1], w, h, kGlRgba, kGlUnsignedByte, img.rgba);
    // Put back what the renderer had.
    sGl.PixelStorei(kGlPackAlignment, packAlign);
    sGl.PixelStorei(kGlPackRowLength, packRowLength);
    sGl.ReadBuffer((unsigned)readBuffer);
    if (pbo != 0) {
        sGl.BindBuffer(kGlPixelPackBuffer, (unsigned)pbo);
    }
    UiImage_FlipRows(&img);
    for (size_t i = 0; i < (size_t)w * (size_t)h; i++) {
        img.rgba[i * 4 + 3] = 255; // the window's alpha channel is not part of the picture
    }
    WriteInFlight(&img, nullptr);
    UiImage_Free(&img);
}

bool BackendIsOpenGl() {
    auto ctx = Ship::Context::GetInstance();
    auto window = ctx != nullptr ? ctx->GetWindow() : nullptr;
    return window != nullptr && window->GetWindowBackend() == Ship::WindowBackend::FAST3D_SDL_OPENGL;
}

void OnRenderPre(ImGuiContext*, ImGuiContextHook*) {
    if (sPending.empty()) {
        return;
    }
    if (!BackendIsOpenGl()) {
        if (!sBackendRefused) {
            std::fprintf(stderr, "[FRAME-CAPTURE] the window backend is not OpenGL: nothing is captured\n");
            std::fflush(stderr);
            sBackendRefused = true;
        }
        sPending.clear();
        return;
    }
    for (std::string& name : sPending) {
        sInFlight.push_back(std::move(name));
    }
    sPending.clear();
    ImGui::GetForegroundDrawList(ImGui::GetMainViewport())->AddCallback(ReadbackCallback, nullptr);
}

void Arm(std::string name) {
    ImGuiContext* ctx = ImGui::GetCurrentContext();
    if (ctx == nullptr) {
        std::fprintf(stderr, "[FRAME-CAPTURE] %s: no ImGui context yet, not captured\n", name.c_str());
        std::fflush(stderr);
        return;
    }
    if (ctx != sHookedContext) {
        ImGuiContextHook hook;
        hook.Type = ImGuiContextHookType_RenderPre;
        hook.Callback = OnRenderPre;
        ImGui::AddContextHook(ctx, &hook);
        sHookedContext = ctx;
    }
    sPending.push_back(std::move(name));
}

bool ImageIsUniform(const UiImage& img, double* nonModal, int* distinct) {
    uint32_t modal = 0;
    *nonModal = 0.0;
    *distinct = 0;
    if (UiImage_Stats(&img, 64, &modal, nonModal, distinct) != 0) {
        return true;
    }
    return *nonModal < 0.02 || *distinct < 8;
}

} // namespace

extern "C" {

bool FrameCapture_IsActive(void) {
    return Config().active;
}

bool FrameCapture_VerifyRequested(void) {
    return Config().active && Config().verify;
}

void FrameCapture_OnGameFrame(GameId game) {
    if (!Config().active || (game != GAME_OOT && game != GAME_MM)) {
        return;
    }
    const int n = ++sGameFrames[game];
    bool want = Config().every > 0 && n % Config().every == 0;
    for (int f : Config().frames) {
        want = want || f == n;
    }
    if (want) {
        Arm(std::string(GameTag(game)) + "-frame-" + std::to_string(n));
    }
}

void FrameCapture_OnOoTFrame(void) {
    FrameCapture_OnGameFrame(GAME_OOT);
}

void FrameCapture_OnMMFrame(void) {
    FrameCapture_OnGameFrame(GAME_MM);
}

void IntegrationTest_CaptureFrame(const char* name) {
    if (!Config().active || name == nullptr || name[0] == '\0') {
        return;
    }
    Arm(name);
}

void FrameCapture_ResetForVerify(void) {
    if (!FrameCapture_VerifyRequested()) {
        return;
    }
    const int lastEvery = Config().every;
    for (GameId game : { GAME_OOT, GAME_MM }) {
        std::vector<int> frames = Config().frames;
        if (lastEvery > 0) {
            frames.push_back(lastEvery);
        }
        for (int n : frames) {
            std::error_code ec;
            if (std::filesystem::remove(FramePath(game, n), ec)) {
                std::fprintf(stderr, "[FRAME-CAPTURE] removed a previous run's %s\n", FramePath(game, n).c_str());
            }
        }
    }
    std::fflush(stderr);
}

bool FrameCapture_Verify(char* msg, size_t cap) {
    std::vector<int> frames = Config().frames;
    if (Config().every > 0) {
        frames.push_back(Config().every);
    }
    if (frames.empty()) {
        std::snprintf(msg, cap, "RSBS_CAPTURE_VERIFY needs RSBS_CAPTURE_FRAMES");
        return false;
    }
    std::string summary;
    for (GameId game : { GAME_OOT, GAME_MM }) {
        int nonUniform = 0;
        for (int n : frames) {
            const std::string path = FramePath(game, n);
            UiImage img = {};
            if (UiImage_ReadPng(&img, path.c_str()) != 0) {
                std::snprintf(msg, cap, "no %s frame %d: %s was not written", GameTag(game), n, path.c_str());
                return false;
            }
            double ratio = 0.0;
            int distinct = 0;
            const bool uniform = ImageIsUniform(img, &ratio, &distinct);
            char line[256];
            std::snprintf(line, sizeof(line), "%s%s %dx%d nonModal=%.3f distinct=%d%s", summary.empty() ? "" : "; ",
                          path.c_str(), img.w, img.h, ratio, distinct, uniform ? " UNIFORM" : "");
            summary += line;
            nonUniform += uniform ? 0 : 1;
            UiImage_Free(&img);
        }
        if (nonUniform == 0) {
            std::snprintf(msg, cap, "every %s capture is uniform: %s", GameTag(game), summary.c_str());
            return false;
        }
    }
    std::snprintf(msg, cap, "%s", summary.c_str());
    return true;
}

} // extern "C"
