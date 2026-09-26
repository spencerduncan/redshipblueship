/**
 * @file soh_ui_snapshot.cpp
 * @brief `redship --test ui-snapshot`: render Ship of Harkinian's own menu pages
 *        and every page RedShipBlueShip added, the same way, into PNGs an agent
 *        can Read and compare.
 *
 * ============================================================================
 * WHY THIS EXISTS
 * ============================================================================
 * The operator's direction (2026-09-27) is that the vanilla SoH menus stay as
 * shipped and that everything this project adds should look like it belongs
 * next to them -- judged by PIXELS, without the operator playtesting each
 * change. So the harness renders both an SoH reference page and one of ours in
 * one process, one window, one backend, one set of pinned settings, and writes:
 *
 *   ui-snapshots/pages/<slug>.png         the whole window, as drawn
 *   ui-snapshots/pages/<slug>.txt         every string the frame submitted
 *   ui-snapshots/compare/<ours>__vs__<ref>[@variant].png   reference over ours
 *   ui-snapshots/iter/<slug>.png, .diff.png  before over after, with a baseline
 *   ui-snapshots/manifest.json            what was drawn, how, and the verdicts
 *   ui-snapshots/runtime-lint.txt         the runtime copy lint (R1-R7)
 *   ui-snapshots/soh-names.txt            SoH's own row names and tooltips (R8)
 *
 * WHAT IT ASSERTS IS STRUCTURE, NEVER APPEARANCE: the drawable is the profile
 * size; each PNG decodes back to the same hash; the page is not blank; the menu
 * window and the page's own section child were active (so the selection did not
 * silently fall back to the first sidebar entry); the text log names the page;
 * the frame converged; no popup leaked; no game framebuffer was composited; the
 * player's config and imgui.ini were not touched; the runtime lint has no hit
 * outside its baseline. "Does ours look like SoH" is judged by whoever reads the
 * composites. No pixel is ever compared against a stored image, and nothing here
 * reads or writes tests/golden/ or runs a generation.
 *
 * ============================================================================
 * HOW A FRAME IS MADE (and why each step is where it is)
 * ============================================================================
 *   HandleEvents; poll IsFrameReady
 *   [input suppression, except on hover frames]
 *   gui->StartDraw()      ImGui NewFrame + the whole menu and every GuiWindow
 *   fast->StartFrame(); fast->RunGuiOnly()
 *   colour clear          RunGuiOnly clears only depth when the game does not
 *                         render to its own framebuffer, so without this the
 *                         back buffer holds the previous frame's pixels
 *   [extra ImGui]         the creation overlay, through its test seam
 *   gui->EndDraw()        ImGui renders into framebuffer 0
 *   READBACK              before the present, because after the swap the back
 *                         buffer's contents are undefined
 *   fast->EndFrame()      present
 *
 * The readback is RGBA8 on both backends and needs no libultraship change: GL
 * reads the back buffer with GL 1.1 entry points fetched through SDL, and DX11
 * copies the swap chain's buffer 0 to a staging texture through the DXGI window
 * backend's public GetSwapChain().
 *
 * ============================================================================
 * ROM-RICH AND ROM-FREE
 * ============================================================================
 * With oot.o2r mounted the production menu is complete and every SoH reference
 * page is drawn. Without it (hosted CI), SoH's own menu is never populated
 * (SetupMenuElements is skipped, and SohMenu::DrawElement refuses to draw an
 * unpopulated menu), so the harness draws a probe menu of its own holding
 * exactly the sections known to register ROM-free: the Randomizer header with
 * the Cross-Game pointer page, the tier-4 Combo section, and Dev Tools. The
 * probe is drawn inside each frame by the harness and never takes the Gui's
 * single menu slot (the SetMenuCount row pins that slot to the live shell).
 * Pages that need the full menu are recorded as skipped with the reason. soh.o2r is
 * required in both modes: the menu's fonts come only from it, and without them
 * Menu::DrawElement returns before drawing anything.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "soh/OTRGlobals.h"
#include "soh/SohGui/SohGui.hpp"
#include "soh/SohGui/SohMenu.h"
#include "soh/SohGui/CreationProgressOverlay.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <fast/Fast3dWindow.h>
#include <fast/interpreter.h>
#include <ship/Context.h>
#include <ship/window/gui/Gui.h>

// The same platform split OTRGlobals.cpp uses for its SDL include.
#ifdef __APPLE__
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

#include <algorithm>
#include <cctype>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// src/common (on soh's include path). All game-header-free.
#include "ComboMmOptionsWindow.h"
#include "ComboSpoilerWindow.h"
#include "ComboTrackerWindow.h"
#include "combo_mm_options_view.h"
#include "combo_mm_tricks_view.h"
#include "combo_settings_view.h"
#include "context.h"
#include "cvar_shared_keys.h"
#include "foreign_items.h"
#include "gen_progress_overlay.h"
#include "headless_crash.h"
#include "rsbs_version.h"
#include "ui_snapshot_image.h"

extern "C" void InitOTRForMMFirstBoot(int argc, char* argv[]);
extern "C" int OoT_InitSharedContextSubsystems(void);
extern "C" void MM_TrackersGui_Init(void);
// The build-stamp commit hash (games/oot/src/boot/build.c), for the manifest.
extern "C" const char OoT_gGitCommitHash[];

namespace SohGui {
// Defined in SohMenuRandomizer.cpp and declared in no header (the combo section
// lock declares it the same way): AddMenuRandomizer as a whole cannot run
// ROM-free, so the probe menu drives the pointer page directly.
void AddCrossGamePointerWidgets(SohMenu& menu, WidgetPath& path);
} // namespace SohGui

// The DX11 readback. ENABLE_DX11 is a PRIVATE define of libultraship, and
// gfx_dxgi.h is guarded on it, so it is defined here, around this one include,
// on the one platform that has the backend. No other libultraship header in this
// TU depends on it (only the backend headers test it).
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef ENABLE_DX11
#define ENABLE_DX11
#define RSBS_UI_SNAPSHOT_DEFINED_DX11
#endif
#include <fast/backends/gfx_window_manager_api.h>
#include <fast/backends/gfx_dxgi.h>
#ifdef RSBS_UI_SNAPSHOT_DEFINED_DX11
#undef ENABLE_DX11
#undef RSBS_UI_SNAPSHOT_DEFINED_DX11
#endif
#include <d3d11.h>
#endif

namespace {

namespace fs = std::filesystem;

// ============================================================================
// Options (environment)
// ============================================================================

struct Options {
    std::string pages = "all";
    std::string outDir = "ui-snapshots";
    std::string profile = "auto";
    std::string backend = "auto";
    int settleMin = 4;
    int settleMax = 12;
    std::string baseline;
    std::string cvars;
    std::string lintBaseline;
};

std::string EnvOr(const char* name, const std::string& fallback) {
    const char* v = std::getenv(name);
    return (v != nullptr && v[0] != '\0') ? std::string(v) : fallback;
}

Options ReadOptions(const char* pagesArg, const char* outArg) {
    Options o;
    o.pages = EnvOr("RSBS_UI_SNAPSHOT_PAGES", pagesArg != nullptr && pagesArg[0] ? pagesArg : "all");
    o.outDir = EnvOr("RSBS_UI_SNAPSHOT_OUT", outArg != nullptr && outArg[0] ? outArg : "ui-snapshots");
    o.profile = EnvOr("RSBS_UI_SNAPSHOT_PROFILE", "auto");
    o.backend = EnvOr("RSBS_UI_SNAPSHOT_BACKEND", "auto");
    const std::string settle = EnvOr("RSBS_UI_SNAPSHOT_SETTLE", "4,12");
    int a = 4;
    int b = 12;
    if (sscanf(settle.c_str(), "%d,%d", &a, &b) == 2 && a >= 2 && b >= a && b <= 120) {
        o.settleMin = a;
        o.settleMax = b;
    }
    o.baseline = EnvOr("RSBS_UI_SNAPSHOT_BASELINE", "");
    o.cvars = EnvOr("RSBS_UI_SNAPSHOT_CVARS", "");
    o.lintBaseline = EnvOr("RSBS_UI_LINT_BASELINE", "");
    return o;
}

// ============================================================================
// Small utilities
// ============================================================================

std::string JsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += (char)c;
                }
        }
    }
    return out;
}

std::string Hex64(uint64_t v) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%016" PRIx64, v);
    return buf;
}

/** A file-name-safe slug: lower-case alphanumerics, '@' kept, everything else '-'. */
std::string Slug(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        if (std::isalnum(c)) {
            out += (char)std::tolower(c);
        } else if (c == '@') {
            out += '@';
        } else if (!out.empty() && out.back() != '-') {
            out += '-';
        }
    }
    while (!out.empty() && out.back() == '-') {
        out.pop_back();
    }
    return out;
}

bool WriteTextFile(const fs::path& p, const std::string& text) {
    std::ofstream f(p, std::ios::binary);
    if (!f) {
        return false;
    }
    f << text;
    return (bool)f;
}

std::string ReadTextFile(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) {
        return std::string();
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

/** FNV-1a 64 of a file's bytes, or "absent". The isolation check's unit. */
std::string FileDigest(const fs::path& p) {
    std::error_code ec;
    if (!fs::exists(p, ec)) {
        return "absent";
    }
    const std::string bytes = ReadTextFile(p);
    return Hex64(Ui_Fnv1a64(bytes.data(), bytes.size(), UI_FNV1A64_OFFSET));
}

/** Glob with '*' only, the selector syntax RSBS_UI_SNAPSHOT_PAGES documents. */
bool GlobMatch(const char* pat, const char* s) {
    if (*pat == '\0') {
        return *s == '\0';
    }
    if (*pat == '*') {
        for (const char* t = s;; t++) {
            if (GlobMatch(pat + 1, t)) {
                return true;
            }
            if (*t == '\0') {
                return false;
            }
        }
    }
    return *pat == *s && GlobMatch(pat + 1, s + 1);
}

std::vector<std::string> Split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    out.push_back(cur);
    return out;
}

std::string Trim(const std::string& s) {
    size_t a = 0;
    size_t b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) {
        a++;
    }
    while (b > a && std::isspace((unsigned char)s[b - 1])) {
        b--;
    }
    return s.substr(a, b - a);
}

// ============================================================================
// Menu access without a production header change
// ============================================================================
//
// The harness needs the menu's registered rows (hover targets, the runtime
// lint, R8's name dump) and, ROM-free, a menu it can populate itself. Both come
// from this one subclass. `&UiSnapshotMenu::menuEntries` names a protected
// member of Ship::Menu through a class derived from it, which is the legal way
// to form that pointer-to-member; applied to the PRODUCTION menu object it reads
// the same rows the player sees, without a test-only accessor in a shipped
// header (the convention the menu locks already follow).
class UiSnapshotMenu final : public SohGui::SohMenu {
  public:
    // Its OWN visibility CVar: the production menu (which a ROM-free process
    // still constructs, unpopulated) syncs "gOpenWindows.Menu" from its own state
    // every frame, and two windows writing one key would fight over it.
    UiSnapshotMenu() : SohGui::SohMenu("gUiSnapshot.ProbeMenu", "Port Menu") {
    }

    // SohMenu::DrawElement refuses until its PRIVATE mMenuElementsInitialized is
    // set by AddMenuElements, which a ROM-free process can never run. This probe
    // is populated section by section instead, so it draws unconditionally.
    void DrawElement() override {
        Ship::Menu::DrawElement();
    }

    using EntriesMap = std::unordered_map<std::string, MainMenuEntry>;
    static EntriesMap Ship::Menu::*EntriesMember() {
        return &UiSnapshotMenu::menuEntries;
    }
    static std::vector<std::string> Ship::Menu::*OrderMember() {
        return &UiSnapshotMenu::menuOrder;
    }
};

UiSnapshotMenu::EntriesMap& MenuEntries(Ship::Menu& menu) {
    return menu.*UiSnapshotMenu::EntriesMember();
}

std::vector<std::string>& MenuOrder(Ship::Menu& menu) {
    return menu.*UiSnapshotMenu::OrderMember();
}

// ============================================================================
// Readback (RGBA8, both backends, no libultraship change)
// ============================================================================

#ifdef _WIN32
#define UISNAP_GLAPI __stdcall
#else
#define UISNAP_GLAPI
#endif

// GL 1.1 enums, spelled out so this TU includes no GL header (a GL header here
// would race GLEW/SDL_opengl include order against libultraship's).
constexpr unsigned kGlColorBufferBit = 0x4000;
constexpr unsigned kGlScissorTest = 0x0C11;
constexpr unsigned kGlBack = 0x0405;
constexpr unsigned kGlPackAlignment = 0x0D05;
constexpr unsigned kGlPackRowLength = 0x0D02;
constexpr unsigned kGlRgba = 0x1908;
constexpr unsigned kGlUnsignedByte = 0x1401;
constexpr unsigned kGlFramebufferBinding = 0x8CA6;
constexpr unsigned kGlPixelPackBufferBinding = 0x88ED;
constexpr unsigned kGlPixelPackBuffer = 0x88EB;

struct GlApi {
    void(UISNAP_GLAPI* GetIntegerv)(unsigned, int*) = nullptr;
    void(UISNAP_GLAPI* ReadBuffer)(unsigned) = nullptr;
    void(UISNAP_GLAPI* PixelStorei)(unsigned, int) = nullptr;
    void(UISNAP_GLAPI* ReadPixels)(int, int, int, int, unsigned, unsigned, void*) = nullptr;
    void(UISNAP_GLAPI* ClearColor)(float, float, float, float) = nullptr;
    void(UISNAP_GLAPI* Clear)(unsigned) = nullptr;
    void(UISNAP_GLAPI* Disable)(unsigned) = nullptr;
    void(UISNAP_GLAPI* ColorMask)(unsigned char, unsigned char, unsigned char, unsigned char) = nullptr;
    void(UISNAP_GLAPI* BindBuffer)(unsigned, unsigned) = nullptr;
    void(UISNAP_GLAPI* Finish)(void) = nullptr;

    bool Load() {
        GetIntegerv = (decltype(GetIntegerv))SDL_GL_GetProcAddress("glGetIntegerv");
        ReadBuffer = (decltype(ReadBuffer))SDL_GL_GetProcAddress("glReadBuffer");
        PixelStorei = (decltype(PixelStorei))SDL_GL_GetProcAddress("glPixelStorei");
        ReadPixels = (decltype(ReadPixels))SDL_GL_GetProcAddress("glReadPixels");
        ClearColor = (decltype(ClearColor))SDL_GL_GetProcAddress("glClearColor");
        Clear = (decltype(Clear))SDL_GL_GetProcAddress("glClear");
        Disable = (decltype(Disable))SDL_GL_GetProcAddress("glDisable");
        ColorMask = (decltype(ColorMask))SDL_GL_GetProcAddress("glColorMask");
        BindBuffer = (decltype(BindBuffer))SDL_GL_GetProcAddress("glBindBuffer");
        Finish = (decltype(Finish))SDL_GL_GetProcAddress("glFinish");
        return GetIntegerv && ReadBuffer && PixelStorei && ReadPixels && ClearColor && Clear && Disable && ColorMask &&
               BindBuffer && Finish;
    }
};

enum class Backend { GL, DX11 };

struct Renderer {
    Backend backend = Backend::GL;
    std::string readbackName;
    GlApi gl;
    std::string lastError;

    // Clear the back buffer to opaque black before ImGui renders. GL: scissor off
    // and all channels writable, or glClear is clipped by whatever state the
    // interpreter left. DX11: the swap chain buffer's RTV.
    bool ClearColour(Fast::Interpreter* interp) {
        if (backend == Backend::GL) {
            gl.Disable(kGlScissorTest);
            gl.ColorMask(1, 1, 1, 1);
            gl.ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            gl.Clear(kGlColorBufferBit);
            return true;
        }
#ifdef _WIN32
        return Dx11ClearOrRead(interp, nullptr);
#else
        (void)interp;
        return false;
#endif
    }

    bool Read(Fast::Interpreter* interp, int w, int h, UiImage* out) {
        if (backend == Backend::GL) {
            int fb = -1;
            gl.GetIntegerv(kGlFramebufferBinding, &fb);
            if (fb != 0) {
                lastError = "framebuffer binding is " + std::to_string(fb) + ", not the default framebuffer 0";
                return false;
            }
            int pbo = 0;
            gl.GetIntegerv(kGlPixelPackBufferBinding, &pbo);
            if (pbo != 0) {
                gl.BindBuffer(kGlPixelPackBuffer, 0);
            }
            gl.ReadBuffer(kGlBack);
            gl.PixelStorei(kGlPackAlignment, 1);
            gl.PixelStorei(kGlPackRowLength, 0);
            if (UiImage_Alloc(out, w, h) != 0) {
                lastError = "out of memory";
                return false;
            }
            gl.Finish();
            gl.ReadPixels(0, 0, w, h, kGlRgba, kGlUnsignedByte, out->rgba);
            UiImage_FlipRows(out);
            return true;
        }
#ifdef _WIN32
        (void)w;
        (void)h;
        return Dx11ClearOrRead(interp, out);
#else
        (void)interp;
        return false;
#endif
    }

#ifdef _WIN32
    // One routine for both DX11 operations because both need the same chain of
    // COM objects: the DXGI window backend's swap chain, its buffer 0, the device
    // and the immediate context. `out == nullptr` means clear; otherwise read.
    bool Dx11ClearOrRead(Fast::Interpreter* interp, UiImage* out) {
        auto* dxgi = dynamic_cast<Fast::GfxWindowBackendDXGI*>(interp->mWapi);
        if (dxgi == nullptr || dxgi->GetSwapChain() == nullptr) {
            lastError = "the window backend is not DXGI or has no swap chain";
            return false;
        }
        ID3D11Texture2D* back = nullptr;
        if (FAILED(dxgi->GetSwapChain()->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back)) || back == nullptr) {
            lastError = "IDXGISwapChain1::GetBuffer(0) failed";
            return false;
        }
        ID3D11Device* device = nullptr;
        back->GetDevice(&device);
        ID3D11DeviceContext* dc = nullptr;
        device->GetImmediateContext(&dc);
        bool ok = false;
        if (out == nullptr) {
            ID3D11RenderTargetView* rtv = nullptr;
            if (SUCCEEDED(device->CreateRenderTargetView(back, nullptr, &rtv)) && rtv != nullptr) {
                const float black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
                dc->ClearRenderTargetView(rtv, black);
                rtv->Release();
                ok = true;
            } else {
                lastError = "CreateRenderTargetView on the back buffer failed";
            }
        } else {
            D3D11_TEXTURE2D_DESC desc;
            back->GetDesc(&desc);
            desc.Usage = D3D11_USAGE_STAGING;
            desc.BindFlags = 0;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            desc.MiscFlags = 0;
            ID3D11Texture2D* staging = nullptr;
            if (SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &staging)) && staging != nullptr) {
                dc->CopyResource(staging, back);
                D3D11_MAPPED_SUBRESOURCE map;
                if (SUCCEEDED(dc->Map(staging, 0, D3D11_MAP_READ, 0, &map))) {
                    const bool bgra =
                        desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM || desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
                    const bool rgba =
                        desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM || desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
                    if ((bgra || rgba) && UiImage_Alloc(out, (int)desc.Width, (int)desc.Height) == 0) {
                        for (UINT y = 0; y < desc.Height; y++) {
                            const uint8_t* src = (const uint8_t*)map.pData + (size_t)y * map.RowPitch;
                            uint8_t* dst = out->rgba + (size_t)y * desc.Width * 4;
                            memcpy(dst, src, (size_t)desc.Width * 4);
                            if (bgra) {
                                for (UINT x = 0; x < desc.Width; x++) {
                                    std::swap(dst[x * 4 + 0], dst[x * 4 + 2]);
                                }
                            }
                        }
                        ok = true;
                    } else {
                        lastError = "unsupported back buffer format " + std::to_string((int)desc.Format);
                    }
                    dc->Unmap(staging, 0);
                } else {
                    lastError = "Map of the staging copy failed";
                }
                staging->Release();
            } else {
                lastError = "CreateTexture2D (staging) failed";
            }
        }
        dc->Release();
        device->Release();
        back->Release();
        return ok;
    }
#endif
};

// ============================================================================
// ImGui hooks: the text log and the hover pointer
// ============================================================================

struct HookState {
    bool installed = false;
    bool logEnabled = false;
    std::string lastLog;
    bool hoverActive = false;
    ImVec2 hoverPos = ImVec2(0, 0);
};

HookState gHooks;

void HookNewFramePre(ImGuiContext* ctx, ImGuiContextHook*) {
    if (!gHooks.hoverActive) {
        return;
    }
    // Runs before NewFrame's UpdateInputEvents, so replacing the queue here makes
    // the pointer exactly where the harness put it, whatever the real mouse did.
    // No button events: a hover must never click.
    ImGuiIO& io = ctx->IO;
    io.ClearEventsQueue();
    io.AddMousePosEvent(gHooks.hoverPos.x, gHooks.hoverPos.y);
}

void HookNewFramePost(ImGuiContext* ctx, ImGuiContextHook*) {
    if (!gHooks.logEnabled || ctx->LogEnabled) {
        return;
    }
    // Depth 0: logging must not auto-open tree nodes, or the log would change
    // what the frame draws. LogBegin also sets ItemUnclipByLog, so rows scrolled
    // out of a child's view are still logged -- the .txt covers the whole page.
    ImGui::LogToBuffer(0);
}

void HookEndFramePre(ImGuiContext* ctx, ImGuiContextHook*) {
    if (!ctx->LogEnabled || (ctx->LogFlags & ImGuiLogFlags_OutputBuffer) == 0) {
        return;
    }
    gHooks.lastLog.assign(ctx->LogBuffer.c_str(), (size_t)ctx->LogBuffer.size());
    ImGui::LogFinish();
}

void InstallHooks() {
    if (gHooks.installed) {
        return;
    }
    ImGuiContext* ctx = ImGui::GetCurrentContext();
    ImGuiContextHook pre;
    pre.Type = ImGuiContextHookType_NewFramePre;
    pre.Callback = HookNewFramePre;
    ImGui::AddContextHook(ctx, &pre);
    ImGuiContextHook post;
    post.Type = ImGuiContextHookType_NewFramePost;
    post.Callback = HookNewFramePost;
    ImGui::AddContextHook(ctx, &post);
    ImGuiContextHook end;
    end.Type = ImGuiContextHookType_EndFramePre;
    end.Callback = HookEndFramePre;
    ImGui::AddContextHook(ctx, &end);
    gHooks.installed = true;
}

/** ImGui input off for one frame, restored by RAII (the creation overlay's shape). */
struct InputSuppression {
    ImGuiIO& io;
    ImGuiConfigFlags saved;
    InputSuppression(ImGuiIO& ioRef, bool mouseToo) : io(ioRef), saved(ioRef.ConfigFlags) {
        io.ConfigFlags |= ImGuiConfigFlags_NoKeyboard;
        if (mouseToo) {
            io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
            io.ClearEventsQueue();
            io.ClearInputMouse();
        }
        io.ClearInputKeys();
    }
    ~InputSuppression() {
        io.ConfigFlags = saved;
    }
    InputSuppression(const InputSuppression&) = delete;
    InputSuppression& operator=(const InputSuppression&) = delete;
};

// ============================================================================
// The page model
// ============================================================================

enum class Kind { MENU_PAGE, WINDOW, OVERLAY, MODAL };
enum class Origin { RSBS, SOH_REFERENCE };

const char* KindName(Kind k) {
    switch (k) {
        case Kind::MENU_PAGE:
            return "MENU_PAGE";
        case Kind::WINDOW:
            return "WINDOW";
        case Kind::OVERLAY:
            return "OVERLAY";
        case Kind::MODAL:
            return "MODAL";
    }
    return "?";
}

struct PageSpec {
    std::string id;
    Origin origin = Origin::RSBS;
    Kind kind = Kind::MENU_PAGE;
    std::string header;
    std::string sidebar;
    std::string window; // WINDOW/OVERLAY/MODAL: the ImGui window name to crop
    std::string compareWith;
    bool compareDefaulted = false;
    std::vector<std::string> states; // "" = the default state
    std::vector<std::string> hovers; // hover variant names (MENU_PAGE only)
    std::vector<std::string> expectText;
    bool scroll = true;
    bool needsRom = false;
};

struct Capture {
    std::string id;
    std::string variant;
    std::string state;
    const PageSpec* spec = nullptr;
    std::string status = "pass";
    std::string reason;
    std::string pngRel;
    std::string txtRel;
    uint64_t rgbaHash = 0;
    uint64_t textHash = 0;
    int settleFrames = 0;
    bool converged = false;
    double nonBlank = 0.0;
    int distinct = 0;
    int size[2] = { 0, 0 };
    float contentRect[4] = { 0, 0, 0, 0 };
    int scrollIndex = -1;
    float scrollY = 0.0f;
    float scrollMax = 0.0f;
    std::string hover;
    std::vector<std::string> found;
    std::vector<std::string> missing;
    size_t popupsQueued = 0;
    int64_t changedPixels = -1;
    UiImage image = { 0, 0, nullptr };
    std::string text;
};

// ============================================================================
// The session: one window, one profile, one backend, every page
// ============================================================================

struct Profile {
    std::string name;
    int w = 1280;
    int h = 800;
    int posX = 100;
    int posY = 100;
};

class Session {
  public:
    explicit Session(const Options& o) : opt(o) {
    }

    int Run();

  private:
    Options opt;
    Profile profile;
    Renderer renderer;
    std::shared_ptr<Fast::Fast3dWindow> fast;
    std::shared_ptr<Ship::Gui> gui;
    std::shared_ptr<Ship::Menu> menu; // the menu the pages live in (production, or the probe)
    // ROM-free only: the probe, which the harness draws itself inside each frame.
    // It is deliberately NOT handed to Gui::SetMenu -- that slot is single, a
    // second SetMenu call silently replaces the live shell, and the SetMenuCount
    // row holds the tree to exactly the two known call sites (ADR 0004 section 3).
    std::shared_ptr<UiSnapshotMenu> probe;
    bool romFree = true;
    bool windowActivated = false;
    int colorBits[3] = { 0, 0, 0 };
    std::vector<PageSpec> pages;
    std::vector<Capture> captures;
    std::vector<std::string> failures;
    std::string sohIdsLoaded;
    std::map<std::string, std::string> isolationBefore;
    fs::path out;

    // R5: every interactive row's registered name (and where it lives), and the
    // hits collected after each page's captures.
    std::map<const WidgetInfo*, std::pair<std::string, std::string>> registeredNames;
    std::map<const WidgetInfo*, std::string> textRows;
    std::set<std::string> dynamicHits;
    void CollectDynamicLint();

    // hover target recording
    std::string hoverTarget;
    bool hoverRectValid = false;
    ImRect hoverRect;
    ImGuiWindow* hoverWindow = nullptr;

    // bring-up
    bool BringUp(std::string& why);
    bool ChooseProfile(std::string& why);
    void MirrorProductionWindows();
    void BuildRomFreeMenu();
    void ApplyCvarOverrides();

    // pages
    void BuildPageList();
    bool Selected(const PageSpec& p, const std::string& variant) const;

    // frames
    bool PumpFrame(UiImage* img, bool hover, const std::function<void()>& extra, std::string& why);
    bool Settle(Capture& c, bool hover, const std::function<void()>& extra, const std::function<void()>& beforeFrame);

    // captures
    void CaptureMenuPage(const PageSpec& p);
    void CaptureWindowPage(const PageSpec& p);
    void CaptureOverlay(const PageSpec& p);
    void CaptureModal(const PageSpec& p);
    void Navigate(const PageSpec& p);
    std::vector<ImGuiWindow*> SectionChildren(const PageSpec& p);
    bool Oracle(const PageSpec& p, Capture& c);
    void Finish(Capture& c, const PageSpec& p);
    void Record(Capture&& c);

    // state authoring
    void EnterState(const PageSpec& p, const std::string& state);
    void LeaveState(const PageSpec& p, const std::string& state);

    // outputs
    void WriteComposites();
    void WriteIterComposites();
    bool WriteManifest();
    int RuntimeLint();
    void DumpSohNames();

    void Fail(const std::string& what) {
        failures.push_back(what);
        fprintf(stderr, "[UI-SNAPSHOT] FAIL: %s\n", what.c_str());
    }
};

// ---- profile -----------------------------------------------------------------

bool Session::ChooseProfile(std::string& why) {
    // Widths are multiples of 64 and above 800 px, so every profile keeps SoH's
    // multi-column layout (Menu.cpp collapses to one column below 800). 1280 is
    // the full-bleed breakpoint. The third profile exists for a hosted Windows
    // runner whose desktop may be 1024x768 with a taskbar.
    Profile desk{ "desk-1280x800", 1280, 800, 100, 100 };
    Profile compact{ "small-960x704", 960, 704, 100, 100 };
    Profile minimal{ "min-832x600", 832, 600, 100, 100 };
    for (Profile* named : { &desk, &compact, &minimal }) {
        if (opt.profile == named->name) {
            profile = *named;
            return true;
        }
    }
    if (opt.profile != "auto") {
        why = "unknown RSBS_UI_SNAPSHOT_PROFILE '" + opt.profile + "'";
        return false;
    }
    // AUTO: the largest profile whose OUTER window fits the usable desktop. A
    // window that runs off the screen may not be rendered (or read back) in full,
    // and the page layout depends on the size, so the choice is recorded and
    // comparisons are only ever made within one run.
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        why = std::string("SDL video init failed: ") + SDL_GetError();
        return false;
    }
    SDL_Rect usable{ 0, 0, 0, 0 };
    if (SDL_GetDisplayUsableBounds(0, &usable) != 0) {
        usable = { 0, 0, 1280, 1024 };
    }
    const int decoW = 16;
    const int decoH = 40;
    auto fits = [&](Profile& p) {
        if (usable.w >= p.w + decoW + 200 && usable.h >= p.h + decoH + 200) {
            p.posX = usable.x + 100;
            p.posY = usable.y + 100;
            return true;
        }
        if (usable.w >= p.w + decoW && usable.h >= p.h + decoH) {
            p.posX = usable.x + (usable.w - p.w) / 2;
            p.posY = usable.y + decoH - 8;
            return true;
        }
        return false;
    };
    if (fits(desk)) {
        profile = desk;
    } else if (fits(compact)) {
        profile = compact;
    } else if (fits(minimal)) {
        profile = minimal;
    } else {
        // Nothing fits; take the smallest and let assert 1 (drawable == profile)
        // say so if the window manager shrank it.
        profile = minimal;
        profile.posX = usable.x;
        profile.posY = usable.y;
    }
    return true;
}

// ---- bring-up ------------------------------------------------------------------

const char* kConfigName = "rsbs-ui-snapshot.json";

bool Session::BringUp(std::string& why) {
    if (!ChooseProfile(why)) {
        return false;
    }

#if defined(_WIN32)
    const bool ci = std::getenv("CI") != nullptr;
    Backend be = (opt.backend == "dx11" || (opt.backend == "auto" && ci)) ? Backend::DX11 : Backend::GL;
#else
    if (opt.backend == "dx11") {
        why = "the dx11 backend exists only on Windows";
        return false;
    }
    Backend be = Backend::GL;
#endif
    if (opt.backend != "auto" && opt.backend != "gl" && opt.backend != "dx11") {
        why = "unknown RSBS_UI_SNAPSHOT_BACKEND '" + opt.backend + "'";
        return false;
    }
    renderer.backend = be;

    // A FRESH config every run. The harness owns its own config file (the first
    // config path a process names is the one the Context keeps), so the player's
    // shipofharkinian.json is never read or written, and every CVar starts at its
    // default -- which is what "pinned" means for the theme, the scale, the
    // background opacity and the rest. Only the values below are set.
    {
        std::string json = "{\n  \"Window\": {\n    \"Backend\": { \"Id\": ";
        json += (be == Backend::DX11) ? "0, \"Name\": \"DirectX\"" : "1, \"Name\": \"OpenGL\"";
        json += " },\n    \"Width\": " + std::to_string(profile.w) + ",\n    \"Height\": " + std::to_string(profile.h) +
                ",\n    \"PositionX\": " + std::to_string(profile.posX) +
                ",\n    \"PositionY\": " + std::to_string(profile.posY) +
                ",\n    \"Fullscreen\": { \"Enabled\": false }\n  }\n}\n";
        if (!WriteTextFile(kConfigName, json)) {
            why = std::string("could not write ") + kConfigName;
            return false;
        }
    }

    // Before the window exists: a snapshot run must not steal focus from whatever
    // the person at the workstation is doing. (DXGI activates regardless; the
    // workstation therefore defaults to GL.)
    SDL_SetHint(SDL_HINT_WINDOW_NO_ACTIVATION_WHEN_SHOWN, "1");

    auto ctx = Ship::Context::CreateUninitializedInstance(RSBS_WINDOW_TITLE " - UI snapshot", "soh", kConfigName);
    if (ctx == nullptr) {
        why = "could not create the Ship::Context";
        return false;
    }
    HeadlessCrash_ForceHeadless();
    HeadlessCrash_ClaimAndInstall();

    // soh.o2r is a HARD requirement, checked before bring-up: the menu's fonts
    // come only from a version-matched soh.o2r, and without them Menu::DrawElement
    // returns before drawing anything, so every capture would be blank.
    const std::string sohArchive = Ship::Context::LocateFileAcrossAppDirs("soh.o2r");
    if (!fs::exists(sohArchive)) {
        why = "no soh.o2r resolvable (tried '" + sohArchive +
              "'); the menu fonts live only there. Build it with the GenerateSohOtr target.";
        return false;
    }

    if (OoT_InitSharedContextSubsystems() != 0) {
        why = "the shared Context bring-up reported failure";
        return false;
    }
    // Multi-viewports default ON in Gui::Init; a floating platform window would
    // escape the capture, so it is pinned off before the Gui exists.
    CVarSetInteger(CVAR_ENABLE_MULTI_VIEWPORTS, 0);
    CVarSetInteger(CVAR_MSAA_VALUE, 1);
    CVarSetInteger(CVAR_SETTING("Menu.Popout"), 0);

    static char arg0[] = "redship";
    static char* fakeArgv[] = { arg0, nullptr };
    InitOTRForMMFirstBoot(1, fakeArgv);

    fast = std::dynamic_pointer_cast<Fast::Fast3dWindow>(ctx->GetWindow());
    if (fast == nullptr || fast->GetGui() == nullptr) {
        why = "no Fast3D window or Gui after bring-up";
        return false;
    }
    gui = fast->GetGui();
    if (OTRGlobals::Instance == nullptr || OTRGlobals::Instance->fontStandardLargest == nullptr) {
        why = "the menu fonts did not load (soh.o2r missing or not version-matched to this binary); "
              "Menu::DrawElement draws nothing without them";
        return false;
    }

    // Before the first NewFrame, which is when ImGui would load imgui.ini: the
    // run must neither read the player's window layout nor write one.
    ImGui::GetIO().IniFilename = nullptr;
    // ImGui's recoverable-error tooltip would flicker over every capture after a
    // widget misbehaves; the harness reports the error itself, so the debug log
    // is the one channel left on (ImGui requires at least one).
    ImGui::GetIO().ConfigErrorRecoveryEnableTooltip = false;
    ImGui::GetIO().ConfigErrorRecoveryEnableDebugLog = true;
    InstallHooks();

    const bool rich = OTRGlobals::Instance->HasOriginal() || OTRGlobals::Instance->HasMasterQuest();
    romFree = !rich;
    MirrorProductionWindows();
    if (romFree) {
        BuildRomFreeMenu();
    } else {
        menu = SohGui::GetSohMenu();
    }
    if (menu == nullptr) {
        why = "no menu to draw";
        return false;
    }
    ApplyCvarOverrides();

    if (be == Backend::GL) {
        if (!renderer.gl.Load()) {
            why = "could not resolve the GL 1.1 entry points through SDL_GL_GetProcAddress";
            return false;
        }
        renderer.readbackName = "rgba8-gl";
        SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &colorBits[0]);
        SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &colorBits[1]);
        SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &colorBits[2]);
    } else {
        renderer.readbackName = "rgba8-dxgi";
        colorBits[0] = colorBits[1] = colorBits[2] = 8;
    }
    return true;
}

void Session::MirrorProductionWindows() {
    // The same registrations rsbs/src/main.cpp makes after the first game starts.
    // Without them the Cross-Game Windows rows grey themselves (#535: a window
    // button whose window is not registered renders disabled), and the captures
    // would show a state no player sees.
    Combo_SpoilerWindow_Init();
    Combo_MMOptionsWindow_Init();
    MM_TrackersGui_Init();
    Combo_TrackerWindow_Init();
}

void Session::BuildRomFreeMenu() {
    // The ROM-free probe: exactly the sections proven to register without an OoT
    // archive, in production's relative order (Randomizer, Combo, Dev Tools).
    probe = std::make_shared<UiSnapshotMenu>();
    // What Gui::SetMenu would do for a menu, minus taking the slot: Init() runs
    // InitElement (the window-backend tables and SoH's disabledMap).
    probe->Init();
    probe->AddMenuEntry("Randomizer", CVAR_SETTING("Menu.RandomizerSidebarSection"));
    WidgetPath path = { "Randomizer", "Cross-Game", SECTION_COLUMN_1 };
    SohGui::AddCrossGamePointerWidgets(*probe, path);
    probe->AddMenuCombo();
    probe->AddMenuDevTools();
    probe->ApplySharedIntentMarkers();
    menu = probe;
}

void Session::ApplyCvarOverrides() {
    if (opt.cvars.empty()) {
        return;
    }
    for (const std::string& kv : Split(opt.cvars, ';')) {
        const std::string t = Trim(kv);
        const size_t eq = t.find('=');
        if (t.empty() || eq == std::string::npos) {
            continue;
        }
        const std::string key = t.substr(0, eq);
        const std::string val = t.substr(eq + 1);
        char* end = nullptr;
        const long iv = strtol(val.c_str(), &end, 10);
        if (end != nullptr && *end == '\0' && !val.empty()) {
            CVarSetInteger(key.c_str(), (int32_t)iv);
            continue;
        }
        const float fv = strtof(val.c_str(), &end);
        if (end != nullptr && *end == '\0' && !val.empty()) {
            CVarSetFloat(key.c_str(), fv);
            continue;
        }
        CVarSetString(key.c_str(), val.c_str());
    }
}

// ---- page list -----------------------------------------------------------------

void Session::BuildPageList() {
    auto menuPage = [](const std::string& header, const std::string& sidebar, Origin origin) {
        PageSpec p;
        p.id = header + "/" + sidebar;
        p.header = header;
        p.sidebar = sidebar;
        p.origin = origin;
        p.kind = Kind::MENU_PAGE;
        p.states = { "" };
        p.expectText = { sidebar };
        return p;
    };

    // SoH references. Only Dev Tools/General registers ROM-free.
    {
        PageSpec p = menuPage("Dev Tools", "General", Origin::SOH_REFERENCE);
        pages.push_back(p);
    }
    for (const auto& [h, s] : std::vector<std::pair<std::string, std::string>>{
             { "Randomizer", "General" },
             { "Randomizer", "Item Tracker" },
             { "Randomizer", "Tricks/Glitches" },
             { "Randomizer", "Logic/Access" },
             { "Enhancements", "Quality of Life" },
             { "Settings", "General" },
         }) {
        PageSpec p = menuPage(h, s, Origin::SOH_REFERENCE);
        p.needsRom = true;
        pages.push_back(p);
    }
    {
        PageSpec p;
        p.id = "modal/Clear Config";
        p.origin = Origin::SOH_REFERENCE;
        p.kind = Kind::MODAL;
        p.window = "Clear Config";
        p.states = { "" };
        p.expectText = { "Clear Config", "Cancel" };
        p.scroll = false;
        pages.push_back(p);
    }

    // Ours. Every Combo sidebar the live menu registered, in its own order, so a
    // page added later cannot escape the harness: an unlisted page compares with
    // Randomizer/General by default and says so in the manifest.
    static const std::map<std::string, std::string> kCompare = {
        { "Cross-Game Rules", "Randomizer/General" },
        { "Cross-Game Windows", "Randomizer/Item Tracker" },
        { "MM Enhancements", "Enhancements/Quality of Life" },
    };
    auto& entries = MenuEntries(*menu);
    if (entries.contains("Combo")) {
        for (const std::string& sidebar : entries.at("Combo").sidebarOrder) {
            PageSpec p = menuPage("Combo", sidebar, Origin::RSBS);
            auto it = kCompare.find(sidebar);
            if (it != kCompare.end()) {
                p.compareWith = it->second;
            } else {
                p.compareWith = "Randomizer/General";
                p.compareDefaulted = true;
            }
            if (sidebar == "Cross-Game Rules") {
                p.states = { "unpaired", "paired-legacy", "frozen", "corrupt" };
                p.hovers = { "direction", "frozen-slider" };
            } else if (sidebar == "MM Enhancements") {
                p.states = { "", "autosave" };
            }
            pages.push_back(p);
        }
    } else {
        Fail("the live menu has no \"Combo\" header, so none of this project's menu pages can be captured");
    }
    {
        PageSpec p = menuPage("Randomizer", "Cross-Game", Origin::RSBS);
        p.compareWith = "Randomizer/General";
        pages.push_back(p);
    }
    {
        PageSpec p;
        p.id = std::string("window/") + ComboGui::kComboMMOptionsWindowName;
        p.kind = Kind::WINDOW;
        p.window = ComboGui::kComboMMOptionsWindowName;
        p.states = { "unpaired", "frozen", "mm-suspended", "tricks-open" };
        p.compareWith = "Randomizer/Logic/Access";
        p.expectText = { ComboGui::kComboMMOptionsWindowName };
        p.scroll = false;
        pages.push_back(p);
    }
    {
        PageSpec p;
        p.id = std::string("window/") + ComboGui::kComboSpoilerWindowName;
        p.kind = Kind::WINDOW;
        p.window = ComboGui::kComboSpoilerWindowName;
        p.states = { "paired" };
        p.expectText = { ComboGui::kComboSpoilerWindowName };
        p.scroll = false;
        pages.push_back(p);
    }
    {
        PageSpec p;
        p.id = std::string("window/") + ComboGui::kComboTrackerWindowName;
        p.kind = Kind::WINDOW;
        p.window = ComboGui::kComboTrackerWindowName;
        p.states = { "paired", "unpaired" };
        p.expectText = { ComboGui::kComboTrackerWindowName };
        p.scroll = false;
        pages.push_back(p);
    }
    {
        PageSpec p;
        p.id = "overlay/creation-progress";
        p.kind = Kind::OVERLAY;
        p.window = "Creating your paired world";
        p.states = { "" };
        p.compareWith = "modal/Clear Config";
        p.expectText = { "Creating your paired world" };
        p.scroll = false;
        pages.push_back(p);
    }
}

bool Session::Selected(const PageSpec& p, const std::string& variant) const {
    const std::string full = variant.empty() ? p.id : p.id + "@" + variant;
    for (const std::string& raw : Split(opt.pages, ',')) {
        const std::string sel = Trim(raw);
        if (sel.empty()) {
            continue;
        }
        if (sel == "all") {
            return true;
        }
        if (sel == "refs" && p.origin == Origin::SOH_REFERENCE) {
            return true;
        }
        if (sel == "ours" && p.origin == Origin::RSBS) {
            return true;
        }
        const size_t at = sel.find('@');
        if (at == std::string::npos) {
            if (GlobMatch(sel.c_str(), p.id.c_str())) {
                return true;
            }
        } else if (GlobMatch(sel.c_str(), full.c_str()) ||
                   (GlobMatch(sel.substr(0, at).c_str(), p.id.c_str()) &&
                    GlobMatch((sel.substr(at + 1) + "*").c_str(), variant.c_str()))) {
            return true;
        }
    }
    return false;
}

// ---- frames -----------------------------------------------------------------------

bool Session::PumpFrame(UiImage* img, bool hover, const std::function<void()>& extra, std::string& why) {
    fast->HandleEvents();
    int tries = 0;
    while (!fast->IsFrameReady()) {
        if (++tries > 120) {
            why = "the window declined 120 consecutive frames (IsFrameReady)";
            return false;
        }
        SDL_Delay(5);
        fast->HandleEvents();
    }
    auto interp = fast->GetInterpreterWeak().lock();
    if (interp == nullptr) {
        why = "no Fast3D interpreter";
        return false;
    }

    // Transient game-overlay notices ("slot 0 set" and the like) fade over
    // seconds; left alone, one keeps a corner of the first captures changing and
    // they never converge. They are game HUD, not menu, so they are cleared.
    gui->GetGameOverlay()->ClearNotifications();

    gHooks.hoverActive = hover;
    gHooks.logEnabled = true;
    bool ok = true;
    bool interpreterFrame = false;
    {
        InputSuppression suppressed(ImGui::GetIO(), !hover);
        try {
            gui->StartDraw();
            fast->StartFrame();
            interpreterFrame = true;
            fast->RunGuiOnly();
            if (!renderer.ClearColour(interp.get())) {
                why = "colour clear failed: " + renderer.lastError;
                ok = false;
            }
            if (probe != nullptr) {
                // Top-level ImGui windows may be begun anywhere inside the frame,
                // so this is the same "Main Menu" window DrawMenu would submit for
                // a menu in the Gui's slot.
                probe->Update();
                probe->Draw();
            }
            if (extra) {
                extra();
            }
            if (Ship::Context::GetInstance()->GetWindow()->GetGfxFrameBuffer() != 0) {
                // DrawGame would composite a stale game framebuffer under the menu.
                why = "the interpreter produced a game framebuffer (mRendersToFb); the capture would include a "
                      "stale game image";
                ok = false;
            }
            gui->EndDraw();
            if (ok && img != nullptr) {
                UiImage_Free(img);
                if (!renderer.Read(interp.get(), (int)fast->GetWidth(), (int)fast->GetHeight(), img)) {
                    why = "readback failed: " + renderer.lastError;
                    ok = false;
                }
            }
            fast->EndFrame();
        } catch (const std::exception& e) {
            // A widget threw mid-frame (MenuDrawItem catches only
            // bad_variant_access). The ImGui frame is left open ON PURPOSE: the next
            // NewFrame resets the window and popup stacks itself, which measured
            // clean, while unwinding it here with ErrorRecoveryTryToRecoverState +
            // EndFrame left every later capture blank. Only the text log is closed,
            // so the next capture's .txt does not start with this frame's text, and
            // the interpreter's frame is presented if it was started.
            if (GImGui->LogEnabled) {
                ImGui::LogFinish();
            }
            if (interpreterFrame) {
                fast->EndFrame();
            }
            why = std::string("an exception escaped the frame: ") + e.what();
            ok = false;
        }
    }
    gHooks.hoverActive = false;
    return ok;
}

bool Session::Settle(Capture& c, bool hover, const std::function<void()>& extra,
                     const std::function<void()>& beforeFrame) {
    uint64_t prev = 0;
    bool havePrev = false;
    UiImage prevImg = { 0, 0, nullptr };
    for (int frame = 1; frame <= opt.settleMax; frame++) {
        if (beforeFrame) {
            beforeFrame();
        }
        std::string why;
        UiImage img = { 0, 0, nullptr };
        if (!PumpFrame(&img, hover, extra, why)) {
            UiImage_Free(&img);
            c.status = "fail";
            c.reason = why;
            return false;
        }
        const uint64_t h = UiImage_Fnv1a64(&img);
        UiImage_Free(&prevImg);
        prevImg = c.image;
        c.image = img;
        c.text = gHooks.lastLog;
        c.settleFrames = frame;
        if (frame >= opt.settleMin && havePrev && h == prev) {
            c.converged = true;
            c.rgbaHash = h;
            UiImage_Free(&prevImg);
            return true;
        }
        prev = h;
        havePrev = true;
    }
    c.rgbaHash = prev;
    c.converged = false;
    c.status = "fail";
    c.reason = "the frame did not converge within " + std::to_string(opt.settleMax) + " frames";
    // Name WHERE it keeps changing: the bounding box of the pixels that differ
    // between the last two frames is usually enough to identify the animating
    // widget (a caret, a timer, a fading notification).
    if (prevImg.rgba != nullptr && c.image.rgba != nullptr && prevImg.w == c.image.w && prevImg.h == c.image.h) {
        int x0 = prevImg.w, y0 = prevImg.h, x1 = -1, y1 = -1;
        for (int y = 0; y < prevImg.h; y++) {
            for (int x = 0; x < prevImg.w; x++) {
                const size_t o = ((size_t)y * (size_t)prevImg.w + (size_t)x) * 4;
                if (memcmp(prevImg.rgba + o, c.image.rgba + o, 4) != 0) {
                    x0 = std::min(x0, x);
                    y0 = std::min(y0, y);
                    x1 = std::max(x1, x);
                    y1 = std::max(y1, y);
                }
            }
        }
        if (x1 >= 0) {
            c.reason += "; the last two frames differ inside x=" + std::to_string(x0) + " y=" + std::to_string(y0) +
                        " w=" + std::to_string(x1 - x0 + 1) + " h=" + std::to_string(y1 - y0 + 1);
        }
    }
    UiImage_Free(&prevImg);
    return false;
}

// ---- navigation and the oracle ------------------------------------------------------

void Session::Navigate(const PageSpec& p) {
    auto& entries = MenuEntries(*menu);
    CVarSetString(CVAR_SETTING("Menu.ActiveHeader"), p.header.c_str());
    if (entries.contains(p.header) && entries.at(p.header).sidebarCvar != nullptr) {
        CVarSetString(entries.at(p.header).sidebarCvar, p.sidebar.c_str());
    }
    menu->Show();
}

std::vector<ImGuiWindow*> Session::SectionChildren(const PageSpec& p) {
    // ImGui names a child "<parent>/<name>_%08X". A page draws either one
    // "<sidebar> Settings" child or one "<sidebar> Settings Column <i>" per column
    // (Menu::DrawElement), and a child that was not submitted this frame is not
    // Active -- which is what catches the silent fall-back to the first sidebar
    // entry when a selection names a page that does not exist.
    std::vector<ImGuiWindow*> out;
    ImGuiContext& g = *GImGui;
    const std::string one = "/" + p.sidebar + " Settings_";
    const std::string col = "/" + p.sidebar + " Settings Column ";
    for (ImGuiWindow* w : g.Windows) {
        if (w == nullptr || !w->Active || (w->Flags & ImGuiWindowFlags_ChildWindow) == 0) {
            continue;
        }
        const std::string name = w->Name;
        if (name.rfind("Main Menu/", 0) != 0) {
            continue;
        }
        if (name.find(one) != std::string::npos || name.find(col) != std::string::npos) {
            out.push_back(w);
        }
    }
    std::sort(out.begin(), out.end(), [](ImGuiWindow* a, ImGuiWindow* b) { return a->Pos.x < b->Pos.x; });
    return out;
}

bool Session::Oracle(const PageSpec& p, Capture& c) {
    ImGuiWindow* target = nullptr;
    std::vector<ImGuiWindow*> kids;
    if (p.kind == Kind::MENU_PAGE) {
        ImGuiWindow* main = ImGui::FindWindowByName("Main Menu");
        if (main == nullptr || !main->Active) {
            c.status = "fail";
            c.reason = "the \"Main Menu\" window was not drawn";
            return false;
        }
        kids = SectionChildren(p);
        if (kids.empty()) {
            c.status = "fail";
            c.reason = "no active \"" + p.sidebar +
                       " Settings\" child: the page is not reachable (the selection fell "
                       "back to another sidebar entry, or the page does not exist)";
            return false;
        }
        float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
        for (ImGuiWindow* w : kids) {
            x0 = std::min(x0, w->Pos.x);
            y0 = std::min(y0, w->Pos.y);
            x1 = std::max(x1, w->Pos.x + w->Size.x);
            y1 = std::max(y1, w->Pos.y + w->Size.y);
        }
        c.contentRect[0] = x0;
        c.contentRect[1] = y0;
        c.contentRect[2] = x1 - x0;
        c.contentRect[3] = y1 - y0;
    } else {
        target = ImGui::FindWindowByName(p.window.c_str());
        if (target == nullptr || !target->Active) {
            c.status = "fail";
            c.reason = "the \"" + p.window + "\" window was not drawn";
            return false;
        }
        c.contentRect[0] = target->Pos.x;
        c.contentRect[1] = target->Pos.y;
        c.contentRect[2] = target->Size.x;
        c.contentRect[3] = target->Size.y;
    }
    return true;
}

// ---- state authoring -------------------------------------------------------------------

void AuthorPairing() {
    // The same identity fields the combo locks author (soh_combo_settings_rows_test,
    // test_combo_mm_options_window): a live pairing with nonzero seed, settings
    // hash and MM profile digest.
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0xC0FFEE55u;
    gComboCtx.sharedRandoSettingsHash = 0x5EED0055u;
    gComboCtx.mmProfileDigest = 0x4D4D0055u;
}

GameId gSavedGame = GAME_NONE;

void Session::EnterState(const PageSpec& p, const std::string& state) {
    gSavedGame = Context_GetCurrentGame();
    ComboContext_Init();
    if (p.id == "Combo/Cross-Game Rules") {
        if (state == "paired-legacy") {
            AuthorPairing();
        } else if (state == "frozen" || state == "corrupt") {
            AuthorPairing();
            ComboSettingsRecord rec;
            Combo_ComboSettingsDefaults(&rec);
            Combo_FreezeComboSettings(&rec);
            if (state == "corrupt") {
                // A frozen record with no live pairing: the state the status row
                // names as corrupt (SohMenuCombo.cpp's four-way status).
                gComboCtx.sourceIsRando = false;
                gComboCtx.sharedRandoSettingsHash = 0;
            }
        }
    } else if (p.id == "Combo/MM Enhancements") {
        if (state == "autosave") {
            CVarSetInteger("gEnhancements.Autosave", 1);
        }
    } else if (p.kind == Kind::WINDOW && p.window == ComboGui::kComboMMOptionsWindowName) {
        Context_SetCurrentGame(state == "mm-suspended" ? GAME_OOT : GAME_MM);
        if (state == "frozen") {
            AuthorPairing();
            gComboCtx.mmProfileDigest = 0x4D4D0001u;
        }
    } else if (p.kind == Kind::WINDOW) {
        if (state == "paired") {
            AuthorPairing();
        }
    }
}

void Session::LeaveState(const PageSpec& p, const std::string& state) {
    if (p.id == "Combo/MM Enhancements" && state == "autosave") {
        CVarClear("gEnhancements.Autosave");
    }
    ComboContext_Init();
    Context_SetCurrentGame(gSavedGame);
}

// ---- recording ---------------------------------------------------------------------------

/**
 * The rows of @p page whose current value their own combo map does not hold.
 * UIWidgets::Combobox previews with `comboMap.at(value)`, which throws, and
 * MenuDrawItem catches only bad_variant_access -- so this is the first suspect
 * when a page's frame throws "invalid map<K, T> key", and naming the row turns
 * an opaque exception into a finding.
 */
std::string ComboMapAudit(Ship::Menu& m, const PageSpec& p) {
    auto& entries = MenuEntries(m);
    if (!entries.contains(p.header) || !entries.at(p.header).sidebars.contains(p.sidebar)) {
        return std::string();
    }
    std::string out;
    for (auto& column : entries.at(p.header).sidebars.at(p.sidebar).columnWidgets) {
        for (WidgetInfo& row : column) {
            if (row.type != WIDGET_CVAR_COMBOBOX || row.cVar == nullptr || row.options == nullptr) {
                continue;
            }
            auto opts = std::static_pointer_cast<UIWidgets::ComboboxOptions>(row.options);
            const int32_t v = CVarGetInteger(row.cVar, (int32_t)opts->defaultIndex);
            if (!opts->comboMap.contains(v)) {
                out += (out.empty() ? "" : ", ") + std::string("\"") + row.name + "\" (" + row.cVar + " = " +
                       std::to_string(v) + ", not in its combo map)";
            }
        }
    }
    return out;
}

void Session::Finish(Capture& c, const PageSpec& p) {
    c.spec = &p;
    if (c.status == "fail" && c.reason.find("exception") != std::string::npos && p.kind == Kind::MENU_PAGE) {
        const std::string audit = ComboMapAudit(*menu, p);
        if (!audit.empty()) {
            c.reason += "; suspect rows: " + audit;
        }
    }
    c.size[0] = c.image.w;
    c.size[1] = c.image.h;
    c.popupsQueued = SohGui::PopupsQueued();
    if (c.status != "pass") {
        return;
    }
    // Assert 1: the drawable is the profile size.
    if (c.image.w != profile.w || c.image.h != profile.h) {
        c.status = "fail";
        c.reason = "drawable is " + std::to_string(c.image.w) + "x" + std::to_string(c.image.h) + ", profile is " +
                   std::to_string(profile.w) + "x" + std::to_string(profile.h);
        return;
    }
    // Assert 3: not blank.
    uint32_t modal = 0;
    UiImage_Stats(&c.image, 4096, &modal, &c.nonBlank, &c.distinct);
    if (c.nonBlank < 0.005 || c.distinct < 16) {
        c.status = "fail";
        c.reason = "the capture is blank (" + std::to_string(c.nonBlank * 100.0) + "% non-modal pixels, " +
                   std::to_string(c.distinct) + " colours)";
        return;
    }
    // Assert 6: the text names the page.
    c.textHash = Ui_Fnv1a64(c.text.data(), c.text.size(), UI_FNV1A64_OFFSET);
    for (const std::string& want : p.expectText) {
        if (c.text.find(want) != std::string::npos) {
            c.found.push_back(want);
        } else {
            c.missing.push_back(want);
        }
    }
    if (!c.missing.empty()) {
        c.status = "fail";
        c.reason = "the frame's text does not contain \"" + c.missing.front() + "\"";
        return;
    }
    // Assert 8: nothing queued behind the page (MODAL pages are torn down first).
    if (p.kind != Kind::MODAL && c.popupsQueued != 0) {
        c.status = "fail";
        c.reason = std::to_string(c.popupsQueued) + " popup(s) queued on a non-modal page";
    }
}

void Session::Record(Capture&& c) {
    // Every capture is a distinct state of the rows; lint the names as drawn NOW.
    CollectDynamicLint();
    const std::string stem = Slug(c.id) + (c.variant.empty() ? "" : "@" + Slug(c.variant));
    c.pngRel = "pages/" + stem + ".png";
    c.txtRel = "pages/" + stem + ".txt";
    if (c.image.rgba != nullptr) {
        const fs::path png = out / c.pngRel;
        if (UiImage_WritePng(&c.image, png.string().c_str()) != 0) {
            c.status = "fail";
            c.reason = "could not write " + png.string();
        } else {
            // Assert 2: the PNG decodes back to the same picture.
            UiImage back = { 0, 0, nullptr };
            if (UiImage_ReadPng(&back, png.string().c_str()) != 0 || UiImage_Fnv1a64(&back) != c.rgbaHash) {
                if (c.status == "pass") {
                    c.status = "fail";
                    c.reason = "the written PNG does not decode back to the captured pixels";
                }
            }
            UiImage_Free(&back);
        }
        WriteTextFile(out / c.txtRel, c.text);
    }
    if (c.status == "fail") {
        Fail(c.id + (c.variant.empty() ? "" : "@" + c.variant) + ": " + c.reason);
    }
    printf("[UI-SNAPSHOT] %-4s %s%s%s%s\n", c.status.c_str(), c.id.c_str(), c.variant.empty() ? "" : "@",
           c.variant.c_str(), c.reason.empty() ? "" : (" -- " + c.reason).c_str());
    captures.push_back(std::move(c));
}

std::string VariantName(const std::string& state, const std::string& extra) {
    if (state.empty()) {
        return extra;
    }
    return extra.empty() ? state : state + "@" + extra;
}

// ---- menu pages --------------------------------------------------------------------------

WidgetInfo* FindRow(Ship::Menu& m, const std::string& header, const std::string& sidebar,
                    const std::function<bool(const WidgetInfo&)>& pred) {
    auto& entries = MenuEntries(m);
    if (!entries.contains(header) || !entries.at(header).sidebars.contains(sidebar)) {
        return nullptr;
    }
    for (auto& column : entries.at(header).sidebars.at(sidebar).columnWidgets) {
        for (WidgetInfo& row : column) {
            if (pred(row)) {
                return &row;
            }
        }
    }
    return nullptr;
}

/** How many extra views of one page the scroll variants take at most. */
constexpr int kMaxScrollSteps = 8;

void Session::CaptureMenuPage(const PageSpec& p) {
    auto& entries = MenuEntries(*menu);
    const bool present = entries.contains(p.header) && entries.at(p.header).sidebars.contains(p.sidebar);
    for (const std::string& state : p.states) {
        const std::string base = VariantName(state, "");
        if (!Selected(p, base) && !Selected(p, VariantName(state, "scroll0"))) {
            continue;
        }
        if (!present) {
            Capture c;
            c.id = p.id;
            c.variant = base;
            c.state = state;
            c.spec = &p;
            if (p.needsRom && romFree) {
                c.status = "skip";
                c.reason = "needs oot.o2r";
            } else {
                c.status = "fail";
                c.reason = "the page is not registered in the live menu";
            }
            Record(std::move(c));
            continue;
        }
        EnterState(p, state);
        Navigate(p);
        // Scroll every column back to the top before the first capture: a page
        // captured earlier in this process may have left its children scrolled.
        std::vector<ImGuiWindow*> kids;
        int scrollIndex = 0;
        for (;;) {
            Capture c;
            c.id = p.id;
            c.state = state;
            c.scrollIndex = scrollIndex;
            c.variant = VariantName(state, p.scroll ? "scroll" + std::to_string(scrollIndex) : "");
            auto before = [&]() {
                if (scrollIndex == 0) {
                    for (ImGuiWindow* w : SectionChildren(p)) {
                        ImGui::SetScrollY(w, 0.0f);
                    }
                }
            };
            if (Settle(c, false, nullptr, before)) {
                Oracle(p, c);
            }
            kids = SectionChildren(p);
            float maxScroll = 0.0f;
            float y = 0.0f;
            for (ImGuiWindow* w : kids) {
                maxScroll = std::max(maxScroll, w->ScrollMax.y);
                y = std::max(y, w->Scroll.y);
            }
            c.scrollY = y;
            c.scrollMax = maxScroll;
            Finish(c, p);
            const bool unfinished = p.scroll && c.status == "pass" && y + 0.5f < maxScroll;
            const bool more = unfinished && scrollIndex < kMaxScrollSteps;
            if (unfinished && !more) {
                // Recorded, not failed: a very long page (Tricks/Glitches) keeps
                // its first views, and the manifest says the rest were not taken.
                c.reason = "scroll sequence truncated at " + std::to_string(kMaxScrollSteps + 1) + " views";
            }
            Record(std::move(c));
            if (!more) {
                break;
            }
            // Advance every column that still has content below: one view minus a
            // 48 px overlap, so a row on the seam appears in both captures.
            for (ImGuiWindow* w : kids) {
                const float step = std::max(64.0f, w->InnerRect.GetHeight() - 48.0f);
                ImGui::SetScrollY(w, std::min(w->ScrollMax.y, w->Scroll.y + step));
            }
            scrollIndex++;
        }

        // Hover variants: the tooltip is part of the page's look.
        for (const std::string& hv : p.hovers) {
            if ((hv == "frozen-slider") != (state == "frozen")) {
                continue;
            }
            const std::string variant = VariantName(state, "hover-" + hv);
            if (!Selected(p, variant)) {
                continue;
            }
            ComboSettingId targetId = (hv == "direction") ? COMBO_SETTING_DIRECTION : COMBO_SETTING_POOL_SIZE_OOT;
            const std::string label = Combo_ComboSettingLabel(targetId);
            WidgetInfo* row = FindRow(*menu, p.header, p.sidebar,
                                      [&](const WidgetInfo& w) { return w.name.find(label) != std::string::npos; });
            Capture c;
            c.id = p.id;
            c.state = state;
            c.variant = variant;
            c.hover = label;
            if (row == nullptr) {
                c.status = "fail";
                c.reason = "no row named \"" + label + "\" to hover";
                c.spec = &p;
                Record(std::move(c));
                continue;
            }
            // Wrap the row's postFunc (MenuDrawItem runs it right after the
            // widget) to learn where the widget is; restore it afterwards.
            WidgetFunc savedPost = row->postFunc;
            hoverRectValid = false;
            row->postFunc = [this, savedPost](WidgetInfo& info) {
                hoverRect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
                hoverWindow = GImGui->CurrentWindow;
                hoverRectValid = true;
                if (savedPost) {
                    savedPost(info);
                }
            };
            // Bring it into view, then hover its centre until the frame settles.
            std::string why;
            for (int i = 0; i < 4; i++) {
                hoverRectValid = false;
                if (!PumpFrame(nullptr, false, nullptr, why)) {
                    break;
                }
                if (hoverRectValid && hoverWindow != nullptr) {
                    const ImRect clip = hoverWindow->InnerClipRect;
                    if (hoverRect.Min.y >= clip.Min.y && hoverRect.Max.y <= clip.Max.y) {
                        break;
                    }
                    ImGui::SetScrollY(hoverWindow, std::max(0.0f, hoverWindow->Scroll.y + hoverRect.Min.y - clip.Min.y -
                                                                      clip.GetHeight() / 3.0f));
                }
            }
            if (!hoverRectValid) {
                c.status = "fail";
                c.reason = "the hover target was never drawn";
            } else {
                gHooks.hoverPos = hoverRect.GetCenter();
                if (Settle(c, true, nullptr, nullptr)) {
                    Oracle(p, c);
                }
            }
            row->postFunc = savedPost;
            Finish(c, p);
            Record(std::move(c));
            // Put the pointer away and re-settle so the next capture has no hover.
            PumpFrame(nullptr, false, nullptr, why);
        }
        LeaveState(p, state);
    }
}

// ---- panes, overlay, modal ------------------------------------------------------------------

const char* PaneCvar(const std::string& window) {
    if (window == ComboGui::kComboMMOptionsWindowName) {
        return ComboGui::kComboMMOptionsVisibilityCVar;
    }
    if (window == ComboGui::kComboSpoilerWindowName) {
        return ComboGui::kComboSpoilerVisibilityCVar;
    }
    if (window == ComboGui::kComboTrackerWindowName) {
        return ComboGui::kComboTrackerVisibilityCVar;
    }
    return nullptr;
}

void Session::CaptureWindowPage(const PageSpec& p) {
    const char* cvar = PaneCvar(p.window);
    for (const std::string& state : p.states) {
        const std::string variant = VariantName(state, "");
        if (!Selected(p, variant)) {
            continue;
        }
        Capture c;
        c.id = p.id;
        c.state = state;
        c.variant = variant;
        if (cvar == nullptr || gui->GetGuiWindow(p.window) == nullptr) {
            c.status = "fail";
            c.reason = "the pane \"" + p.window + "\" is not registered";
            c.spec = &p;
            Record(std::move(c));
            continue;
        }
        EnterState(p, state);
        menu->Hide();
        // These panes read their visibility CVar LIVE in Draw() (they override it),
        // so setting the CVar is what opens them; Show() would do nothing.
        CVarSetInteger(cvar, 1);
        std::function<void()> before;
        if (state == "tricks-open") {
            before = [&p]() {
                ImGuiWindow* w = ImGui::FindWindowByName(p.window.c_str());
                if (w != nullptr) {
                    w->StateStorage.SetInt(w->GetID("Tricks"), 1);
                }
            };
        }
        if (Settle(c, false, nullptr, before)) {
            Oracle(p, c);
        }
        Finish(c, p);
        Record(std::move(c));
        if (state == "tricks-open") {
            ImGuiWindow* w = ImGui::FindWindowByName(p.window.c_str());
            if (w != nullptr) {
                w->StateStorage.SetInt(w->GetID("Tricks"), 0);
            }
        }
        CVarSetInteger(cvar, 0);
        LeaveState(p, state);
    }
}

void Session::CaptureOverlay(const PageSpec& p) {
    if (!Selected(p, "")) {
        return;
    }
    // A fixed view, so the capture is the same picture every run: half way, the
    // first of three attempts, 12.3 s of a 30 s budget.
    ComboGenOverlayView view;
    memset(&view, 0, sizeof(view));
    view.state = (uint8_t)RSBS_GENOVERLAY_SHOWN;
    view.phase = (uint8_t)RSBS_GENPHASE_MM_FILL;
    view.attempt = 1;
    view.maxAttempts = 3;
    view.fraction = 0.5f;
    view.elapsedMs = 12300;
    view.budgetMs = 30000;
    snprintf(view.caption, sizeof(view.caption), "Building the Majora's Mask world (attempt 1 of 3)");
    menu->Hide();
    Capture c;
    c.id = p.id;
    auto extra = [&view]() { OoT_CreationProgressOverlay_TestDrawContents(&view); };
    if (Settle(c, false, extra, nullptr)) {
        Oracle(p, c);
    }
    Finish(c, p);
    Record(std::move(c));
}

void Session::CaptureModal(const PageSpec& p) {
    if (!Selected(p, "")) {
        return;
    }
    menu->Hide();
    // SoH's own strings (SohMenuSettings.cpp, the "Clear Devices" button), with
    // null callbacks: nothing is cleared, and the popup is dismissed below.
    SohGui::RegisterPopup("Clear Config",
                          "This will completely erase the controls config, including registered devices.\nContinue?",
                          "Clear", "Cancel", nullptr, nullptr);
    Capture c;
    c.id = p.id;
    // A modal's background dim fades in over about a sixth of a second of REAL
    // time (NewFrame advances DimBgRatio by DeltaTime * 6), so how many frames it
    // takes depends on the host's frame rate and the capture would race it.
    // Start every frame fully dimmed instead: the picture is the settled one,
    // deterministically.
    auto fullDim = []() { GImGui->DimBgRatio = 1.0f; };
    if (Settle(c, false, nullptr, fullDim)) {
        Oracle(p, c);
    }
    Finish(c, p);
    // Teardown, then assert nothing is left queued for the pages after this one.
    SohGui::DismissPopup(p.window);
    std::string why;
    PumpFrame(nullptr, false, nullptr, why);
    PumpFrame(nullptr, false, nullptr, why);
    if (SohGui::PopupsQueued() != 0 && c.status == "pass") {
        c.status = "fail";
        c.reason = "the modal did not dismiss (PopupsQueued " + std::to_string(SohGui::PopupsQueued()) + ")";
    }
    Record(std::move(c));
}

// ---- composites ---------------------------------------------------------------------------

const Capture* FindCapture(const std::vector<Capture>& all, const std::string& id, const std::string& variant) {
    for (const Capture& c : all) {
        if (c.id == id && c.variant == variant && c.image.rgba != nullptr) {
            return &c;
        }
    }
    return nullptr;
}

const Capture* FirstCapture(const std::vector<Capture>& all, const std::string& id) {
    for (const Capture& c : all) {
        if (c.id == id && c.image.rgba != nullptr && c.status == "pass") {
            return &c;
        }
    }
    return nullptr;
}

bool CropContent(const Capture& c, UiImage* outImg) {
    const int pad = 4;
    return UiImage_Crop(&c.image, (int)c.contentRect[0] - pad, (int)c.contentRect[1] - pad,
                        (int)c.contentRect[2] + 2 * pad, (int)c.contentRect[3] + 2 * pad, outImg) == 0;
}

void Session::WriteComposites() {
    for (const Capture& c : captures) {
        if (c.spec == nullptr || c.spec->compareWith.empty() || c.image.rgba == nullptr || c.status != "pass") {
            continue;
        }
        // Scroll captures compare against the reference's matching scroll
        // position where it has one, else its first capture.
        const Capture* ref = nullptr;
        if (c.scrollIndex >= 0) {
            ref = FindCapture(captures, c.spec->compareWith, "scroll" + std::to_string(c.scrollIndex));
        }
        if (ref == nullptr) {
            ref = FirstCapture(captures, c.spec->compareWith);
        }
        if (ref == nullptr && c.spec->kind == Kind::MENU_PAGE) {
            // ROM-free (hosted CI), the named reference is a skipped page. Dev
            // Tools/General is the one SoH page that registers without oot.o2r, so
            // every menu page still gets a same-run SoH neighbour to be read
            // against; the composite's file name says which reference it used.
            ref = FirstCapture(captures, "Dev Tools/General");
        }
        if (ref == nullptr || ref == &c) {
            continue;
        }
        UiImage a = { 0, 0, nullptr };
        UiImage b = { 0, 0, nullptr };
        UiImage stacked = { 0, 0, nullptr };
        UiImage fitted = { 0, 0, nullptr };
        if (CropContent(*ref, &a) && CropContent(c, &b) && UiImage_StackVertical(&a, &b, 8, &stacked) == 0 &&
            UiImage_FitLongSide(&stacked, 1568, &fitted) == 0) {
            const std::string name = "compare/" + Slug(c.id) + "__vs__" + Slug(ref->id) +
                                     (c.variant.empty() ? "" : "@" + Slug(c.variant)) + ".png";
            UiImage_WritePng(&fitted, (out / name).string().c_str());
        }
        UiImage_Free(&a);
        UiImage_Free(&b);
        UiImage_Free(&stacked);
        UiImage_Free(&fitted);
    }
}

void Session::WriteIterComposites() {
    if (opt.baseline.empty()) {
        return;
    }
    const fs::path base(opt.baseline);
    for (Capture& c : captures) {
        if (c.image.rgba == nullptr) {
            continue;
        }
        UiImage before = { 0, 0, nullptr };
        if (UiImage_ReadPng(&before, (base / c.pngRel).string().c_str()) != 0) {
            continue;
        }
        UiImage stacked = { 0, 0, nullptr };
        UiImage fitted = { 0, 0, nullptr };
        UiImage diff = { 0, 0, nullptr };
        const std::string stem = c.pngRel.substr(std::string("pages/").size());
        if (UiImage_StackVertical(&before, &c.image, 8, &stacked) == 0 &&
            UiImage_FitLongSide(&stacked, 1568, &fitted) == 0) {
            UiImage_WritePng(&fitted, (out / ("iter/" + stem)).string().c_str());
        }
        uint64_t changed = 0;
        if (UiImage_AbsDiffX4(&before, &c.image, &diff, &changed) == 0) {
            c.changedPixels = (int64_t)changed;
            std::string diffName = stem.substr(0, stem.size() - 4) + ".diff.png";
            UiImage_WritePng(&diff, (out / ("iter/" + diffName)).string().c_str());
        }
        UiImage_Free(&before);
        UiImage_Free(&stacked);
        UiImage_Free(&fitted);
        UiImage_Free(&diff);
    }
}

// ---- manifest -------------------------------------------------------------------------------

bool Session::WriteManifest() {
    std::string j = "{\"schema\":1,\n \"run\":{";
    j += "\"binary\":\"" + JsonEscape(std::string(RSBS_VERSION_STRING) + " " + OoT_gGitCommitHash) + "\",";
    j += "\"backend\":\"" + JsonEscape(renderer.backend == Backend::GL ? "OpenGL" : "DirectX 11") + "\",";
    j += "\"readback\":\"" + renderer.readbackName + "\",";
#if defined(_WIN32)
    j += "\"platform\":\"win32\",";
#elif defined(__APPLE__)
    j += "\"platform\":\"darwin\",";
#else
    j += "\"platform\":\"linux\",";
#endif
    j += "\"profile\":\"" + profile.name + "\",";
    j += "\"requested\":[" + std::to_string(profile.w) + "," + std::to_string(profile.h) + "],";
    j += "\"drawable\":[" + std::to_string(fast ? fast->GetWidth() : 0) + "," +
         std::to_string(fast ? fast->GetHeight() : 0) + "],";
    j += "\"windowPos\":[" + std::to_string(profile.posX) + "," + std::to_string(profile.posY) + "],";
    j += "\"colorBits\":[" + std::to_string(colorBits[0]) + "," + std::to_string(colorBits[1]) + "," +
         std::to_string(colorBits[2]) + "],";
    char scale[32];
    snprintf(scale, sizeof(scale), "%.3f", (double)ImGui::GetIO().FontGlobalScale);
    j += "\"fontGlobalScale\":" + std::string(scale) + ",";
    j += "\"imguiScaleIndex\":" + std::to_string(CVarGetInteger(CVAR_SETTING("ImGuiScale"), 1)) + ",";
    j += "\"theme\":" + std::to_string(CVarGetInteger(CVAR_SETTING("Menu.Theme"), UIWidgets::Colors::LightBlue)) + ",";
    j += std::string("\"romFree\":") + (romFree ? "true" : "false") + ",";
    j += "\"archives\":{\"soh\":true,\"oot\":" +
         std::string(OTRGlobals::Instance && OTRGlobals::Instance->HasOriginal() ? "true" : "false") +
         ",\"mm\":" + std::string(fs::exists(Ship::Context::LocateFileAcrossAppDirs("mm.o2r")) ? "true" : "false") +
         "},";
    j += std::string("\"windowActivated\":") + (windowActivated ? "true" : "false") + ",";
    j += "\"settle\":[" + std::to_string(opt.settleMin) + "," + std::to_string(opt.settleMax) + "],";
    j += "\"pages\":\"" + JsonEscape(opt.pages) + "\",";
    j += "\"baseline\":\"" + JsonEscape(opt.baseline) + "\"},\n \"pages\":[\n";
    for (size_t i = 0; i < captures.size(); i++) {
        const Capture& c = captures[i];
        const PageSpec* p = c.spec;
        char rect[128];
        snprintf(rect, sizeof(rect), "[%.0f,%.0f,%.0f,%.0f]", c.contentRect[0], c.contentRect[1], c.contentRect[2],
                 c.contentRect[3]);
        char ratio[32];
        snprintf(ratio, sizeof(ratio), "%.4f", c.nonBlank);
        j += "  {\"id\":\"" + JsonEscape(c.id) + "\",\"variant\":\"" + JsonEscape(c.variant) + "\"";
        if (p != nullptr) {
            j += std::string(",\"origin\":\"") + (p->origin == Origin::RSBS ? "RSBS" : "SOH_REFERENCE") + "\"";
            j += std::string(",\"kind\":\"") + KindName(p->kind) + "\"";
            j += ",\"header\":\"" + JsonEscape(p->header) + "\",\"sidebar\":\"" + JsonEscape(p->sidebar) + "\"";
            j += ",\"compareWith\":\"" + JsonEscape(p->compareWith) + "\"";
            if (p->compareDefaulted) {
                j += ",\"compareDefaulted\":true";
            }
        }
        j += ",\"state\":\"" + JsonEscape(c.state) + "\"";
        if (c.scrollIndex >= 0) {
            char s[96];
            snprintf(s, sizeof(s), ",\"scroll\":{\"index\":%d,\"y\":%.0f,\"max\":%.0f}", c.scrollIndex, c.scrollY,
                     c.scrollMax);
            j += s;
        } else {
            j += ",\"scroll\":null";
        }
        j += c.hover.empty() ? ",\"hover\":null" : ",\"hover\":\"" + JsonEscape(c.hover) + "\"";
        j += ",\"settleFrames\":" + std::to_string(c.settleFrames);
        j += std::string(",\"converged\":") + (c.converged ? "true" : "false");
        j += ",\"size\":[" + std::to_string(c.size[0]) + "," + std::to_string(c.size[1]) + "]";
        j += ",\"rgbaFnv1a64\":\"" + Hex64(c.rgbaHash) + "\",\"textFnv1a64\":\"" + Hex64(c.textHash) + "\"";
        j += ",\"nonBlankRatio\":" + std::string(ratio) + ",\"distinctColors\":" + std::to_string(c.distinct);
        j += ",\"contentRect\":" + std::string(rect);
        j += ",\"expectText\":{\"found\":[";
        for (size_t k = 0; k < c.found.size(); k++) {
            j += (k ? ",\"" : "\"") + JsonEscape(c.found[k]) + "\"";
        }
        j += "],\"missing\":[";
        for (size_t k = 0; k < c.missing.size(); k++) {
            j += (k ? ",\"" : "\"") + JsonEscape(c.missing[k]) + "\"";
        }
        j += "]}";
        j += ",\"popupsQueued\":" + std::to_string(c.popupsQueued);
        if (c.changedPixels >= 0) {
            j += ",\"changedPixels\":" + std::to_string(c.changedPixels);
        }
        j += ",\"png\":\"" + JsonEscape(c.pngRel) + "\",\"txt\":\"" + JsonEscape(c.txtRel) + "\"";
        j += ",\"status\":\"" + c.status + "\",\"reason\":\"" + JsonEscape(c.reason) + "\"}";
        j += (i + 1 < captures.size()) ? ",\n" : "\n";
    }
    j += " ]}\n";
    return WriteTextFile(out / "manifest.json", j);
}

// ---- runtime lint (R1-R7) and R8's dump ------------------------------------------------------

bool IsInteractive(WidgetType t) {
    switch (t) {
        case WIDGET_CHECKBOX:
        case WIDGET_COMBOBOX:
        case WIDGET_SLIDER_INT:
        case WIDGET_SLIDER_FLOAT:
        case WIDGET_CVAR_CHECKBOX:
        case WIDGET_CVAR_COMBOBOX:
        case WIDGET_CVAR_SLIDER_INT:
        case WIDGET_CVAR_SLIDER_FLOAT:
        case WIDGET_BUTTON:
        case WIDGET_INPUT:
        case WIDGET_CVAR_INPUT:
        case WIDGET_CVAR_COLOR_PICKER:
        case WIDGET_COLOR_PICKER:
        case WIDGET_WINDOW_BUTTON:
            return true;
        default:
            return false;
    }
}

bool HasRef(const std::string& s) {
    // An issue number (#NNN), an ADR reference, or a section sign.
    for (size_t i = 0; i + 3 < s.size(); i++) {
        if (s[i] == '#' && std::isdigit((unsigned char)s[i + 1]) && std::isdigit((unsigned char)s[i + 2]) &&
            std::isdigit((unsigned char)s[i + 3])) {
            return true;
        }
    }
    if (s.find("ADR ") != std::string::npos || s.find("ADR0") != std::string::npos) {
        return true;
    }
    return s.find("\xC2\xA7") != std::string::npos;
}

/** The visible label: no "##id" suffix, no printf tail, no marker. */
std::string VisibleLabel(const std::string& name) {
    std::string s = name;
    const size_t hid = s.find("##");
    if (hid != std::string::npos) {
        s = s.substr(0, hid);
    }
    const std::string marker = std::string(SohGui::SohMenu::SharedIntentMarker()) + " ";
    if (s.rfind(marker, 0) == 0) {
        s = s.substr(marker.size());
    }
    const size_t pct = s.find('%');
    if (pct != std::string::npos) {
        s = s.substr(0, pct);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == ':')) {
        s.pop_back();
    }
    return s;
}

bool IsTitleCase(const std::string& label) {
    static const std::set<std::string> kSmall = { "a",    "an", "and", "as", "at",  "by", "for", "from", "in",
                                                  "into", "of", "on",  "or", "the", "to", "vs",  "with" };
    std::istringstream in(label);
    std::string word;
    bool first = true;
    while (in >> word) {
        std::string bare;
        for (char ch : word) {
            if (std::isalpha((unsigned char)ch)) {
                bare += ch;
            }
        }
        if (bare.empty()) {
            first = false;
            continue;
        }
        std::string lower = bare;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) { return std::tolower(ch); });
        if (!first && kSmall.contains(lower)) {
            first = false;
            continue;
        }
        const size_t alpha = word.find_first_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
        if (alpha != std::string::npos && std::islower((unsigned char)word[alpha])) {
            return false;
        }
        first = false;
    }
    return true;
}

int CountSentences(const std::string& s) {
    int n = 0;
    for (size_t i = 0; i < s.size(); i++) {
        if ((s[i] == '.' || s[i] == '!' || s[i] == '?') && (i + 1 == s.size() || s[i + 1] == ' ')) {
            n++;
        }
    }
    return std::max(n, s.empty() ? 0 : 1);
}

int Session::RuntimeLint() {
    std::set<std::string> hits;
    auto hit = [&](const char* rule, const std::string& where, const std::string& what) {
        std::string w = what;
        std::replace(w.begin(), w.end(), '\n', ' ');
        if (w.size() > 90) {
            w = w.substr(0, 90);
        }
        hits.insert(std::string(rule) + " | " + where + " | " + w);
    };
    auto tooltipOf = [](const WidgetInfo& row) -> std::string {
        return (row.options != nullptr && row.options->tooltip != nullptr) ? std::string(row.options->tooltip)
                                                                           : std::string();
    };

    auto lintPage = [&](const std::string& header, const std::string& sidebar, const SidebarEntry& page) {
        const std::string where = header + "/" + sidebar;
        std::set<std::string> names;
        for (const auto& column : page.columnWidgets) {
            for (const WidgetInfo& row : column) {
                const std::string tip = tooltipOf(row);
                const bool interactive = IsInteractive(row.type);
                if (interactive && tip.empty()) {
                    hit("R1", where, row.name);
                }
                if (!tip.empty()) {
                    const char last = tip.back();
                    if (last != '.' && last != '!' && last != '?') {
                        hit("R2", where, row.name + " :: tooltip does not end a sentence");
                    }
                    if (tip.size() > 600) {
                        hit("R2", where, row.name + " :: tooltip over 600 chars");
                    } else if (tip.size() > 200) {
                        hit("R2w", where, row.name + " :: tooltip over 200 chars");
                    }
                }
                if (HasRef(row.name) || HasRef(tip) ||
                    (row.options != nullptr && row.options->disabledTooltip != nullptr &&
                     HasRef(row.options->disabledTooltip))) {
                    hit("R3", where, row.name);
                }
                if (interactive) {
                    const std::string label = VisibleLabel(row.name);
                    if (!IsTitleCase(label)) {
                        hit("R4", where, row.name + " :: not Title Case");
                    }
                    if (label.size() > 61) {
                        hit("R4", where, row.name + " :: label over 61 chars");
                    } else if (label.size() > 40) {
                        hit("R4w", where, row.name + " :: label over 40 chars");
                    }
                }
                // R6 for TEXT rows is collected after every capture instead (see
                // CollectDynamicLint): a status row's name is rewritten per state.
                if (row.type != WIDGET_SEPARATOR && row.type != WIDGET_TEXT && !names.insert(row.name).second) {
                    hit("R7", where, row.name + " :: duplicate name");
                }
            }
        }
    };

    auto& entries = MenuEntries(*menu);
    if (entries.contains("Combo")) {
        for (const auto& [sidebar, page] : entries.at("Combo").sidebars) {
            lintPage("Combo", sidebar, page);
        }
    }
    if (entries.contains("Randomizer") && entries.at("Randomizer").sidebars.contains("Cross-Game")) {
        lintPage("Randomizer", "Cross-Game", entries.at("Randomizer").sidebars.at("Cross-Game"));
    }
    // R5: an interactive row's name must not have been changed by its PreFunc in
    // any state the captures drove. The registered names were recorded before the
    // first frame; the rows now carry whatever the last draw wrote.
    for (const std::string& dyn : dynamicHits) {
        hits.insert(dyn);
    }
    // Strings that reach the player from src/common tables.
    for (std::size_t i = 0; i < RSBS::kHostedMmEnhancementCount; i++) {
        const RSBS::HostedMmEnhancement& e = RSBS::kHostedMmEnhancements[i];
        const std::string where = "cvar_shared_keys.h/kHostedMmEnhancements";
        if (HasRef(e.label ? e.label : "") || HasRef(e.tooltip ? e.tooltip : "") || HasRef(e.reason ? e.reason : "")) {
            hit("R3", where, e.label ? e.label : "(null)");
        }
    }
    auto refString = [&](const char* where, const char* text) {
        if (text != nullptr && HasRef(text)) {
            hit("R3", where, text);
        }
    };
    for (int i = 0; i < Combo_MMOptionCount(); i++) {
        const ComboMMOptionDesc* d = Combo_MMOptionAt(i);
        if (d != nullptr) {
            refString("MM options pane", d->label);
            refString("MM options pane", d->tooltip);
            refString("MM options pane", d->disabledReason);
        }
    }
    for (int i = 0; i < Combo_MMTrickCount(); i++) {
        const ComboMMTrickDesc* d = Combo_MMTrickAt(i);
        if (d != nullptr) {
            refString("MM options pane/Tricks", d->label);
            refString("MM options pane/Tricks", d->tooltip);
            refString("MM options pane/Tricks", d->disabledReason);
        }
    }

    std::string body = "# Runtime UI lint (R1-R7) over this project's rows. One hit per line: rule | where | row.\n"
                       "# R2w/R4w are warnings and are reported, never baselined or failed.\n";
    for (const std::string& h : hits) {
        body += h + "\n";
    }
    WriteTextFile(out / "runtime-lint.txt", body);

    // Baseline comparison: only on a complete run (a partial run misses R5 hits
    // from states it did not drive), and only when the row names a baseline.
    if (opt.lintBaseline.empty() || opt.pages != "all") {
        printf("[UI-SNAPSHOT] runtime lint: %zu hit(s), report only (%s)\n", hits.size(),
               opt.lintBaseline.empty() ? "no RSBS_UI_LINT_BASELINE" : "partial run");
        return 0;
    }
    std::set<std::string> baseline;
    std::istringstream in(ReadTextFile(opt.lintBaseline));
    std::string line;
    while (std::getline(in, line)) {
        line = Trim(line);
        if (!line.empty() && line[0] != '#') {
            baseline.insert(line);
        }
    }
    int bad = 0;
    for (const std::string& h : hits) {
        if (h.rfind("R2w", 0) == 0 || h.rfind("R4w", 0) == 0) {
            continue;
        }
        if (!baseline.contains(h)) {
            Fail("runtime lint: new hit not in the baseline: " + h);
            bad++;
        }
    }
    for (const std::string& b : baseline) {
        if (!hits.contains(b)) {
            Fail("runtime lint: baseline entry no longer hit (shrink " + opt.lintBaseline + "): " + b);
            bad++;
        }
    }
    printf("[UI-SNAPSHOT] runtime lint: %zu hit(s), %d outside the baseline\n", hits.size(), bad);
    return bad;
}

void Session::DumpSohNames() {
    if (romFree) {
        return;
    }
    // R8: SoH's own sections, names and tooltips, in registration order. A lane
    // that changes this file (beyond the ruled "[Both Games]" marker, which is
    // already in the baseline it compares with) changed an original SoH row.
    std::string body;
    auto& entries = MenuEntries(*menu);
    for (const std::string& header : MenuOrder(*menu)) {
        if (header == "Combo" || !entries.contains(header)) {
            continue;
        }
        const MainMenuEntry& e = entries.at(header);
        for (const std::string& sidebar : e.sidebarOrder) {
            if (header == "Randomizer" && sidebar == "Cross-Game") {
                continue;
            }
            if (!e.sidebars.contains(sidebar)) {
                continue;
            }
            for (const auto& column : e.sidebars.at(sidebar).columnWidgets) {
                for (const WidgetInfo& row : column) {
                    std::string tip = (row.options != nullptr && row.options->tooltip != nullptr)
                                          ? std::string(row.options->tooltip)
                                          : std::string();
                    std::replace(tip.begin(), tip.end(), '\n', ' ');
                    body += header + "/" + sidebar + " | " + row.name + " | " + tip + "\n";
                }
            }
        }
    }
    WriteTextFile(out / "soh-names.txt", body);
    if (!opt.baseline.empty()) {
        const std::string before = ReadTextFile(fs::path(opt.baseline) / "soh-names.txt");
        if (!before.empty() && before != body) {
            Fail("R8: SoH's own row names or tooltips differ from the baseline run's soh-names.txt");
        }
    }
}

void Session::CollectDynamicLint() {
    // R5: a PreFunc may rewrite a TEXT row's name (an SoH idiom for status lines)
    // but never an interactive row's -- a label that carries state changes under
    // the player's pointer, and it is the menu search key.
    for (const auto& [row, where] : registeredNames) {
        if (row->name != where.second) {
            std::string now = row->name;
            if (now.size() > 60) {
                now = now.substr(0, 60);
            }
            dynamicHits.insert("R5 | " + where.first + " | " + where.second + " -> " + now);
        }
    }
    // R6: a note is one or two sentences, at most 200 characters, in EVERY state
    // it was drawn in. Keyed by the note's first 60 characters so a fingerprint
    // or a count later in the text does not churn the baseline.
    for (const auto& [row, where] : textRows) {
        if (CountSentences(row->name) > 2 || row->name.size() > 200) {
            std::string head = row->name.substr(0, std::min<size_t>(60, row->name.size()));
            std::replace(head.begin(), head.end(), '\n', ' ');
            dynamicHits.insert("R6 | " + where + " | " + head);
        }
    }
}

// ---- the run ----------------------------------------------------------------------------------

int Session::Run() {
    out = fs::path(opt.outDir);
    std::error_code ec;
    fs::create_directories(out / "pages", ec);
    fs::create_directories(out / "compare", ec);
    if (!opt.baseline.empty()) {
        fs::create_directories(out / "iter", ec);
    }
    if (!fs::is_directory(out / "pages", ec)) {
        fprintf(stderr, "[UI-SNAPSHOT] FAIL: cannot create %s\n", (out / "pages").string().c_str());
        return 1;
    }

    // Assert 10's "before": the player's files in the directory the binary resolves
    // its config and ImGui layout from.
    for (const char* f : { "shipofharkinian.json", "imgui.ini" }) {
        isolationBefore[f] = FileDigest(Ship::Context::GetPathRelativeToAppDirectory(f));
    }

    std::string why;
    if (!BringUp(why)) {
        fprintf(stderr, "[UI-SNAPSHOT] FAIL: bring-up: %s\n", why.c_str());
        return 1;
    }
    printf("[UI-SNAPSHOT] profile %s (%dx%d at %d,%d), backend %s, readback %s, %s\n", profile.name.c_str(), profile.w,
           profile.h, profile.posX, profile.posY, renderer.backend == Backend::GL ? "OpenGL" : "DirectX 11",
           renderer.readbackName.c_str(),
           romFree ? "ROM-free (probe menu: Randomizer pointer page, Combo, Dev Tools)" : "ROM-rich (production menu)");
    if (renderer.backend == Backend::GL && (colorBits[0] < 8 || colorBits[1] < 8 || colorBits[2] < 8)) {
        fprintf(stderr,
                "[UI-SNAPSHOT] FAIL: the GL visual has %d/%d/%d colour bits; an 8-bit-per-channel "
                "display is required (xvfb-run -s \"-screen 0 1920x1080x24\")\n",
                colorBits[0], colorBits[1], colorBits[2]);
        return 1;
    }

    BuildPageList();
    // R5 needs every interactive row's registered name before anything draws it.
    for (auto& [header, entry] : MenuEntries(*menu)) {
        if (header != "Combo" && header != "Randomizer") {
            continue;
        }
        for (auto& [sidebar, page] : entry.sidebars) {
            for (auto& column : page.columnWidgets) {
                for (WidgetInfo& row : column) {
                    if (IsInteractive(row.type)) {
                        registeredNames[&row] = { header + "/" + sidebar, row.name };
                    } else if (row.type == WIDGET_TEXT) {
                        textRows[&row] = header + "/" + sidebar;
                    }
                }
            }
        }
    }

    // Warm up: the first frames build fonts and dock nodes.
    menu->Show();
    Navigate(pages.front());
    for (int i = 0; i < 3; i++) {
        PumpFrame(nullptr, false, nullptr, why);
    }
    windowActivated = (SDL_GetKeyboardFocus() != nullptr);

    for (const PageSpec& p : pages) {
        // One page throwing (a std::map::at on a missing key inside a draw, say)
        // must fail THAT page by name, not take the run down with an unhandled
        // exception and no manifest.
        try {
            switch (p.kind) {
                case Kind::MENU_PAGE:
                    CaptureMenuPage(p);
                    break;
                case Kind::WINDOW:
                    CaptureWindowPage(p);
                    break;
                case Kind::OVERLAY:
                    CaptureOverlay(p);
                    break;
                case Kind::MODAL:
                    CaptureModal(p);
                    break;
            }
        } catch (const std::exception& e) { Fail(p.id + ": an exception escaped the capture: " + e.what()); }
        menu->Show();
    }

    WriteComposites();
    WriteIterComposites();
    RuntimeLint();
    DumpSohNames();

    // Assert 10's "after".
    for (const auto& [f, digest] : isolationBefore) {
        const std::string now = FileDigest(Ship::Context::GetPathRelativeToAppDirectory(f));
        if (now != digest) {
            Fail(std::string("isolation: ") + f + " changed during the run (" + digest + " -> " + now + ")");
        }
    }

    int captured = 0;
    int skipped = 0;
    for (const Capture& c : captures) {
        if (c.status == "pass") {
            captured++;
        } else if (c.status == "skip") {
            skipped++;
        }
    }
    if (captured == 0) {
        Fail("no page was captured: the page selector '" + opt.pages + "' matched nothing, or every capture failed");
    }
    if (!WriteManifest()) {
        Fail("could not write the manifest");
    }
    for (Capture& c : captures) {
        UiImage_Free(&c.image);
    }
    printf("[UI-SNAPSHOT] %d captured, %d skipped, %zu failure(s); output in %s\n", captured, skipped, failures.size(),
           fs::absolute(out).string().c_str());
    return failures.empty() ? 0 : 1;
}

} // namespace

extern "C" int OoT_UiSnapshot_Run(const char* pages, const char* outDir) {
    // Unbuffered, so a crash still leaves every per-capture line in the log.
    setvbuf(stdout, nullptr, _IONBF, 0);
    Options o = ReadOptions(pages, outDir);
    Session s(o);
    return s.Run();
}

#endif // RSBS_SINGLE_EXECUTABLE
