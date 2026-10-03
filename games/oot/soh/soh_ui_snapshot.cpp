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
 *   ui-snapshots/runtime-lint.txt         the runtime copy lint (R1-R7, R9)
 *   ui-snapshots/label-fit.txt            R9's measurements: every row that overruns its column
 *   ui-snapshots/soh-names.txt            SoH's own row names and tooltips (R8)
 *
 * WHAT IT ASSERTS IS STRUCTURE, NEVER APPEARANCE: the drawable is the profile
 * size; each PNG decodes back to the same hash; the page is not blank; the menu
 * window and the page's own section child were active (so the selection did not
 * silently fall back to the first sidebar entry); the text log holds a string
 * only the page's BODY draws (the sidebar labels are drawn on every page, so a
 * sidebar name proves nothing) and that string is absent from a sibling page's
 * capture; every authored state and every hover shows its own distinguishing
 * text (for a hover, every authored line of the row's tooltip), and that text is
 * ABSENT from the contrast capture (the unauthored state,
 * or the same state without the hover), so reverting the authoring or the pointer
 * injection turns the row red; a pane's state text was inside a captured view
 * (panes are scrolled like menu pages and their .txt holds only what was on
 * screen); the frame converged; no popup leaked; no game framebuffer was
 * composited; no ImGui frame was left open; the player's config and imgui.ini
 * were not touched; the runtime lint has no hit outside its baseline. "Does ours
 * look like SoH" is judged by whoever reads the composites. No pixel is ever
 * compared against a stored image, and nothing here reads or writes tests/golden/
 * or runs a generation.
 *
 * RSBS_UI_SNAPSHOT_SABOTAGE (comma list) breaks one mechanism on purpose so its
 * assert can be SEEN going red: no-state (EnterState authors nothing), no-hover
 * (no pointer injection), no-scroll (panes keep their first view), throw (the
 * first project page's first row throws mid-draw), leave-open (the exception
 * path leaves the ImGui frame open, the pre-fix behaviour), keep-imgui-ini (the
 * player's imgui.ini path is left armed and ImGui's shutdown save is run),
 * player-config (the harness names the player's shipofharkinian.json as its
 * config), hover-first-line (a hovered row draws only its tooltip's first
 * line), no-menu-under (the creation overlay's over-menu variant leaves the
 * menu hidden), no-dim (the creation overlay's dim is drawn transparent),
 * activate (the no-activation hint is overridden to "0" before the window
 * exists, so the window is shown the way a player's is), no-label-fit (R9's
 * measuring frame records no row, as if the page were not the one drawn). A
 * sabotaged run is expected to fail; docs/ui-style-guide.md section 12 lists
 * what each one must turn red.
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
#include "soh/SohGui/UIWidgets.hpp"
#include "soh/SohGui/CreationProgressOverlay.h"
#include "soh/Notification/Notification.h"

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
#include <cmath>
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
#include <stdexcept>
#include <string>
#include <vector>

// src/common (on soh's include path). All game-header-free.
#include "ComboSpoilerWindow.h"
#include "ComboTrackerWindow.h"
#include "combo_logic.h"           // RSBS_COMBO_HALF_* (the goal-warning toast page)
#include "pairing_refusal_toast.h" // the cross-game refusal toast pages
#include "combo_mm_options_page.h" // Combo > MM Randomizer / MM Tricks: page names, row suffix, reset title
#include "combo_mm_options_view.h"
#include "combo_mm_tricks_view.h"
#include "combo_settings_view.h"
#include "combo_tracker_view.h"
#include "combo_ui.h"
#include "context.h"
#include "crossing_store.h" // the crossings the tracker and spoiler states author (#755)
#include "cvar_shared_keys.h"
#include "game.h"                  // MM_SAVE_CONTEXT_SIZE (the authored MM shadow)
#include "mm_mod_set.h"            // #706: the MM Mods page's model, authored for its three states
#include "combo_save_files_view.h" // lane W5: the Save Files page's model, authored for its two states
#include "foreign_items.h"
#include "gen_progress_overlay.h"
#include "headless_crash.h"
#include "notification_bridge.h"
#include "rsbs_version.h"
#include "save.h" // RsbsSave_EmitLoadToast: the paired-file load's toasts (#781)
#include "ui_snapshot_image.h"

// The unified Item Tracker overlay (#458 U2), its states' item adapters and
// shared pool.
#include "ComboItemTrackerWindow.h"
#include "combo_item_view.h"
#include "shared_resources.h"

extern "C" void InitOTRForMMFirstBoot(int argc, char* argv[]);
extern "C" int OoT_InitSharedContextSubsystems(void);
extern "C" void MM_TrackersGui_Init(void);
// The build-stamp commit hash (games/oot/src/boot/build.c), for the manifest.
extern "C" const char OoT_gGitCommitHash[];
// The creation seam's two toasts (games/oot/soh/Enhancements/randomizer/ForeignItemsSingleExe.cpp),
// drawn from their production emitters so a toast page shows what a player gets.
extern "C" void OoT_Creation_EmitShortfallToast(int placed, int requested);
extern "C" void OoT_Creation_EmitGoalWarningToast(uint32_t unprovedHalves);
extern "C" void OoT_Creation_ReportFailureAtFileSelect(int slot, int reason);
// The item adapters' authoring seams (#458 U1): a started save written through
// each game's own layout, variant 0 the "shadow" world and 1 the "live" one.
extern "C" int OoT_ItemAdapter_TestAuthorSave(void* buf, size_t size, int variant);
extern "C" int MM_ItemAdapter_TestAuthorSave(void* buf, size_t size, int variant);

namespace SohGui {
// Defined in SohMenuRandomizer.cpp and declared in no header (the combo section
// lock declares it the same way): AddMenuRandomizer as a whole cannot run
// ROM-free, so the probe menu drives the pointer page directly.
void AddCrossGamePointerWidgets(SohMenu& menu, WidgetPath& path);
// Combo > Majora's Mask's body over a manifest table, and its group note's
// sentence (SohMenuComboMmEnhancements.cpp, declared in no header, #747). The
// MM note probe builds a page from a synthetic non-live table through it.
void AddMmEnhancementRows(SohMenu& menu, WidgetPath& path, const RSBS::HostedMmEnhancement* rows, std::size_t count);
const char* MmEnhancementsGroupNoteText();
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
    std::string sabotage;

    bool Sabotaged(const char* what) const {
        for (size_t at = 0; at < sabotage.size();) {
            size_t comma = sabotage.find(',', at);
            if (comma == std::string::npos) {
                comma = sabotage.size();
            }
            if (sabotage.compare(at, comma - at, what) == 0 && std::strlen(what) == comma - at) {
                return true;
            }
            at = comma + 1;
        }
        return false;
    }
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
    o.sabotage = EnvOr("RSBS_UI_SNAPSHOT_SABOTAGE", "");
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
    // False while a PANE is captured: its .txt then holds only the items that
    // were inside a clip rect this frame, i.e. what the PNG shows, so a state's
    // text can be asserted to be ON SCREEN and not merely submitted.
    bool unclipLog = true;
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
    if (!gHooks.unclipLog) {
        // LogBegin just set it; ItemAdd then returns false for a clipped item
        // (Text, TreeNode, Checkbox, ...), which is what keeps it out of the log.
        ctx->ItemUnclipByLog = false;
    }
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

enum class Kind { MENU_PAGE, WINDOW, OVERLAY, MODAL, TOAST };
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
        case Kind::TOAST:
            return "TOAST";
    }
    return "?";
}

// The MM Mods page's "listed" state (#706): two enabled archives, one in a
// subfolder, and one disabled, so both columns and every arrow are drawn.
const char* const kMmModsListedEnabled[] = { "10-hd-textures.o2r", "packs/20-retro-hud.o2r" };
const char* const kMmModsListedDisabled = "30-alt-link.o2r";

// The Save Files page's "listed" state (lane W5): a ready file, a healthy file
// this session's load refused for its cross-game rules (its name still shows,
// and its status repeats the load toast's words, recorded as the load records
// them), and a file refused for its CRC with its original set aside (no toast:
// the page's own words), so every column and both refusal shapes draw. Authored
// through the model's seam, so the harness never reads a Save folder.
static rsbs::SlotMeta SaveFilesListedMeta(int slot) {
    rsbs::SlotMeta m{};
    m.state = RSBS_SLOT_ABSENT;
    m.lastGame = GAME_NONE;
    if (slot == 0 || slot == 1) {
        m.exists = true;
        m.valid = true;
        m.state = RSBS_SLOT_VALID;
        m.lastGame = slot == 0 ? GAME_MM : GAME_OOT;
        std::snprintf(m.ootName, sizeof(m.ootName), "%s", slot == 0 ? "Link" : "Zelda");
        std::snprintf(m.mmName, sizeof(m.mmName), "%s", slot == 0 ? "Link" : "Zelda");
        m.ootStarted = true;
        m.mmStarted = slot == 0;
        if (slot == 1) {
            m.state = RSBS_SLOT_REFUSED;
            m.refuseReason = RSBS_REFUSE_IDENTITY;
            std::snprintf(m.refuseWords, sizeof(m.refuseWords), "%s",
                          RsbsSave_LoadToastRefusalMessage(RSBS_LOAD_TOAST_REFUSED_RULES));
        }
    } else {
        m.exists = true;
        m.state = RSBS_SLOT_REFUSED;
        m.refuseReason = RSBS_REFUSE_CRC;
        m.hasQuarantine = true;
    }
    return m;
}

struct PageSpec {
    std::string id;
    Origin origin = Origin::RSBS;
    Kind kind = Kind::MENU_PAGE;
    std::string header;
    std::string sidebar;
    std::string window; // WINDOW/OVERLAY/MODAL: the ImGui window name to crop (TOAST: a name prefix)
    std::string compareWith;
    // Per state: a different reference for that state's captures (unused by the
    // shipped pages since the MM options pane became pages; kept for a page
    // whose states read against different SoH pages).
    std::map<std::string, std::string> stateCompareWith;
    // Hover captures: the page whose hover capture they are read against, when
    // the state's reference has none (SoH's Tricks page draws its trick names
    // as plain text with no item id, so no hover of its own can be injected).
    std::string hoverCompareWith;
    bool compareDefaulted = false;
    std::vector<std::string> states; // "" = the default state
    std::vector<std::string> hovers; // hover variant names (MENU_PAGE only)
    // Hover name -> the exact name of the row it points at, for a hover whose
    // target is not one of Cross-Game Rules' settings. Captured in the page's
    // first state only (and, when hoverStates is set, only if listed there),
    // unless hoverRowState names the one state it is captured in.
    std::map<std::string, std::string> hoverRows;
    std::map<std::string, std::string> hoverRowState;
    // Named hovers on a DISABLED row: their tooltip must be SoH's disabled shape
    // (DisabledShapeVerdict), as the row-state probe's are.
    std::set<std::string> disabledHovers;
    std::vector<std::string> expectText;
    // MENU_PAGE: a string only this page's body draws (assert 6); empty when no
    // registered row yields one, which fails the page.
    std::string bodyText;
    // Per state: strings that must appear in that state's captures (union over
    // its scroll views), and the state whose captures must NOT contain them --
    // the capture the row would produce if EnterState stopped authoring.
    std::map<std::string, std::vector<std::string>> stateText;
    std::map<std::string, std::string> stateContrast;
    bool scroll = true;
    bool needsRom = false;
    // When non-empty, hover variants are taken in these states only. A race
    // lockout rebuilds the tooltip from `activeDisables` plus "- Race Lockout
    // Active": it REPLACES a directly written disabled tooltip (ours) and only
    // appends to an SoH disabledMap one, so our row's hover there shows none of
    // its own text and proves nothing about it.
    std::vector<std::string> hoverStates;
    // The row-state probe: a harness-only Combo sidebar installed for this page's
    // captures and removed afterwards (see InstallRowStateProbe).
    bool rowStateProbe = false;
    // The MM note probe (#747): the same, built by Combo > Majora's Mask's own
    // builder over a synthetic non-live manifest (see InstallMmNoteProbe).
    bool mmNoteProbe = false;
    // Hover variants on a row drawn through the combo_ui seam, found by the
    // label it passes to the seam's rect recorder (such a row has no WidgetInfo
    // whose postFunc could be wrapped: a pane's rows, or a trick row inside
    // Combo > MM Tricks' custom list), each captured in one named state. WINDOW
    // and MENU_PAGE alike.
    struct PaneHover {
        std::string name;
        std::string state;
        std::string label;
        bool disabled = false; // the row is disabled: its tooltip must be SoH's disabled shape
    };
    std::vector<PaneHover> paneHovers;
    // Combo > MM Tricks' row census (CheckTrickCensus): in every state, the
    // trick list draws each of MM's tricks exactly once, in the column its
    // value puts it in, with the tooltip its state gives it.
    bool trickCensus = false;
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
    std::string hoverText;               // the first line's prefix (the manifest's hoverText)
    std::vector<std::string> hoverLines; // every authored line's prefix; a hover must show them all
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

// The row-state probe's sidebar and rows (InstallRowStateProbe).
constexpr const char* kRowStatesSidebar = "Row States";
constexpr const char* kProbeLiveRow = "Live Setting";
constexpr const char* kProbeSuspendedRow = "Suspended Setting";
constexpr const char* kProbeGatedRow = "Gated Setting";
constexpr const char* kProbeDecidedRow = "Decided Setting";
bool gProbeLive = false;
bool gProbeSuspended = false;
bool gProbeGated = false;
bool gProbeDecided = false;

// The MM note probe's sidebar and synthetic manifest (InstallMmNoteProbe, #747).
// Combo > Majora's Mask's own builder over rows that are NOT all Live, because
// every shipped row is, so its disabled rows and group notes never reach a pixel
// on the shipped page. The shipped page's shape, with a non-live row in EACH
// group so both note placements are drawn: the heading group holds a Live
// checkbox and a Dormant one gated on it (the note sits right under the
// heading); then a pointer row opens a second group holding a Dormant checkbox
// and a Partial slider gated on the pointer's key (the note sits under the
// pointer's sentence, directly above those rows). Every non-live row is gated,
// so each note depends on authored state: hidden with its rows in "", drawn over
// them once "heading-on" / "gate-on" writes its key -- which also makes the
// harness's own `no-state` sabotage turn both notes' checks red. Static storage:
// the page's PreFuncs keep pointers into the table.
constexpr const char* kMmNoteSidebar = "MM Row States";
constexpr const char* kMmNoteLiveRow = "Live Enhancement";
constexpr const char* kMmNoteLiveKey = "gRsbsUiSnapshot.MmNote.Live";
constexpr const char* kMmNoteAbsentRow = "Absent Enhancement";
constexpr const char* kMmNoteDormantRow = "Dormant Enhancement";
constexpr const char* kMmNotePartialLabel = "Partial Setting";
constexpr const char* kMmNoteParentKey = "gRsbsUiSnapshot.MmNote.Parent";
constexpr RSBS::HostedMmEnhancement kMmNoteManifest[] = {
    { kMmNoteLiveKey, kMmNoteLiveRow, "Toggles a setting that applies now. Majora's Mask only.", "harness", nullptr, "",
      RSBS::MmEnhancementHosting::OwnRow, RSBS::MmEnhancementLiveness::Live, "" },
    { "gRsbsUiSnapshot.MmNote.Absent", kMmNoteAbsentRow,
      "Toggles a setting whose provider is not in this build. Majora's Mask only.", "harness", nullptr, "",
      RSBS::MmEnhancementHosting::OwnRow, RSBS::MmEnhancementLiveness::Dormant, "Provider Not in This Build",
      RSBS::MmEnhancementWidget::Checkbox, 0, 0, 0, nullptr, kMmNoteLiveKey, 747 },
    { kMmNoteParentKey, "Parent Setting", "Parent Setting is on another page and applies to both games.", "harness",
      nullptr, "", RSBS::MmEnhancementHosting::HostedElsewhere, RSBS::MmEnhancementLiveness::Live, "" },
    { "gRsbsUiSnapshot.MmNote.Dormant", kMmNoteDormantRow,
      "Toggles a setting whose provider is not in this build. Majora's Mask only.", "harness", nullptr, "",
      RSBS::MmEnhancementHosting::OwnRow, RSBS::MmEnhancementLiveness::Dormant, "Provider Not in This Build",
      RSBS::MmEnhancementWidget::Checkbox, 0, 0, 0, nullptr, kMmNoteParentKey, 747 },
    { "gRsbsUiSnapshot.MmNote.Partial", "Partial Setting: %d minutes",
      "Sets a value whose draw is not wired yet. Majora's Mask only.", "harness", nullptr, "",
      RSBS::MmEnhancementHosting::OwnRow, RSBS::MmEnhancementLiveness::Partial, "Draw Not Wired in Majora's Mask",
      RSBS::MmEnhancementWidget::SliderInt, 1, 60, 5, "%d minutes", kMmNoteParentKey, 747 },
};
static_assert(RSBS::HostedMmEnhancementRowsAreHonest(kMmNoteManifest,
                                                     sizeof(kMmNoteManifest) / sizeof(kMmNoteManifest[0])),
              "the MM note probe's synthetic manifest must itself pass the manifest's honesty rule");

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
    // SDL_HINT_WINDOW_NO_ACTIVATION_WHEN_SHOWN as the window was created.
    std::string noActivationHint;
    // compare/ and iter/ images that could not be written. Counted into the
    // summary line and the manifest: a missing composite is what an agent
    // iterating from the output directory would otherwise never notice.
    int compositeWriteFailures = 0;
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
    // R9: every row of one of this project's pages fits its column at the run's
    // profile (MeasureLabelFit). The report lists every overrun the run measured,
    // SoH's reference pages included (report only, never a failure: SoH's pages
    // are the reference, and some of their rows overrun a narrow column).
    std::vector<std::string> labelFitReport;
    int labelFitMeasured = 0;
    void MeasureLabelFit(const PageSpec& p, const std::string& state);

    // The row-state probe (ADR 0004 section 6's four presentations on synthetic
    // rows): installed only while its own page is captured.
    std::vector<const WidgetInfo*> probeRows;
    void InstallRowStateProbe();
    void RemoveRowStateProbe();
    // The MM note probe (#747), on the same pattern and the same probeRows.
    void InstallMmNoteProbe();
    void RemoveMmNoteProbe();

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
    void CapturePaneHovers(const PageSpec& p, const std::string& state);
    void CheckTrickCensus(const PageSpec& p, const std::string& state);
    void CaptureOverlay(const PageSpec& p);
    void CaptureModal(const PageSpec& p);
    void CaptureModalVariant(const PageSpec& p, const std::string& state);
    void CaptureToast(const PageSpec& p);
    void Navigate(const PageSpec& p);
    std::vector<ImGuiWindow*> SectionChildren(const PageSpec& p);
    bool Oracle(const PageSpec& p, Capture& c);
    bool HoverVerdict(Capture& c, const std::string& label, const std::string& authoredTip);
    static void DisabledShapeVerdict(Capture& c, const std::string& label, const std::string& authoredTip);
    void Finish(Capture& c, const PageSpec& p);
    void Record(Capture&& c);

    // state authoring
    void EnterState(const PageSpec& p, const std::string& state);
    void LeaveState(const PageSpec& p, const std::string& state);
    void CheckStateText();
    void CheckBodyText();
    void CloseAbandonedFrame();
    bool sabotageThrowArmed = false;

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
/** The Context's short name; also the appName every app-directory lookup here
 *  passes, so none of them dereferences Ship::Context::GetInstance() (which the
 *  NON_PORTABLE branch of GetAppDirectoryPath does when appName is empty, and
 *  which does not exist yet for the isolation check's "before" digests). */
const char* kShortName = "soh";

/** Where the Context will read @p name from: GetAppDirectoryPath honours
 *  SHIP_HOME (Linux, macOS) and the platform pref path (NON_PORTABLE), and the
 *  Context resolves its config file through exactly this call (Context.cpp,
 *  InitConfiguration), so the harness writes its pins where they are read. */
std::string AppPath(const std::string& name) {
    return Ship::Context::GetPathRelativeToAppDirectory(name, kShortName);
}

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

    // RSBS_UI_SNAPSHOT_SABOTAGE=player-config names the PLAYER's config instead:
    // the regression assert 10 exists for (the fresh config below overwrites it).
    const char* configName = opt.Sabotaged("player-config") ? "shipofharkinian.json" : kConfigName;
    // A FRESH config every run. The harness owns its own config file (the first
    // config path a process names is the one the Context keeps), so the player's
    // shipofharkinian.json is never read or written, and every CVar starts at its
    // default -- which is what "pinned" means for the theme, the scale, the
    // background opacity and the rest. Only the values below are set. It is
    // written to the SAME resolved path the Context reads (AppPath): relative to
    // the working directory it would miss whenever SHIP_HOME or the pref path
    // moves the app directory, and the run would silently use a stale config
    // that CheckSaveCvars had persisted there.
    {
        std::string json = "{\n  \"Window\": {\n    \"Backend\": { \"Id\": ";
        json += (be == Backend::DX11) ? "0, \"Name\": \"DirectX\"" : "1, \"Name\": \"OpenGL\"";
        json += " },\n    \"Width\": " + std::to_string(profile.w) + ",\n    \"Height\": " + std::to_string(profile.h) +
                ",\n    \"PositionX\": " + std::to_string(profile.posX) +
                ",\n    \"PositionY\": " + std::to_string(profile.posY) +
                ",\n    \"Fullscreen\": { \"Enabled\": false }\n  }\n}\n";
        const std::string configPath = AppPath(configName);
        std::error_code mk;
        fs::create_directories(fs::path(configPath).parent_path(), mk);
        if (!WriteTextFile(configPath, json)) {
            why = "could not write " + configPath;
            return false;
        }
    }

    // Before the window exists: a snapshot run must not steal focus from whatever
    // the person at the workstation is doing. (DXGI activates regardless; the
    // workstation therefore defaults to GL.)
    // rsbs/src/main.cpp has already armed the same hint for every --test process
    // (at OVERRIDE priority, lane F1) before this runs; this call is kept so the
    // harness states its own requirement where the window is made.
    SDL_SetHint(SDL_HINT_WINDOW_NO_ACTIVATION_WHEN_SHOWN, "1");
    if (opt.Sabotaged("activate")) {
        // The regression the windowActivated failure exists to catch: the window
        // is shown the way a player's is, and activates.
        SDL_SetHintWithPriority(SDL_HINT_WINDOW_NO_ACTIVATION_WHEN_SHOWN, "0", SDL_HINT_OVERRIDE);
    }
    {
        const char* hint = SDL_GetHint(SDL_HINT_WINDOW_NO_ACTIVATION_WHEN_SHOWN);
        noActivationHint = hint != nullptr ? hint : "";
    }

    auto ctx = Ship::Context::CreateUninitializedInstance(RSBS_WINDOW_TITLE " - UI snapshot", kShortName, configName);
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
    // ImGui writes IniFilename in two places: the dirty-timer save after a window
    // is moved, resized or collapsed (IniSavingRate, 5 s), and ImGui::Shutdown.
    // Neither fires in a run as it stands (nothing moves a window, and the
    // process exits without destroying the context), so the null is what keeps a
    // later change from arming either. RSBS_UI_SNAPSHOT_SABOTAGE=keep-imgui-ini
    // leaves Gui::Init's path armed and runs the shutdown's save before the
    // isolation check, which must then go red.
    if (!opt.Sabotaged("keep-imgui-ini")) {
        ImGui::GetIO().IniFilename = nullptr;
    }
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
    // Without them the Combo > Windows rows grey themselves (#535: a window
    // button whose window is not registered renders disabled), and the captures
    // would show a state no player sees.
    Combo_SpoilerWindow_Init();
    Combo_MMOptionsPages_Init();
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

// Defined with the runtime lint below.
std::string VisibleLabel(const std::string& name);
bool IsInteractive(WidgetType t);

/** At most @p max characters of @p s, cut back to a word boundary. A tooltip is
 *  hard-wrapped at about 60 characters (UIWidgets::WrappedText), so a prefix of
 *  40 always lies on its first logged line. An authored line break ends the
 *  prefix too: SoH's disabled shape ("This setting is disabled because: " then a
 *  blank line and "- Reason", Menu.cpp) would otherwise put a newline inside it,
 *  and the text log holds each line separately. */
std::string WordPrefix(const std::string& in, size_t max) {
    const std::string s = in.substr(0, in.find('\n'));
    size_t cut = s.size();
    if (s.size() > max) {
        cut = s.rfind(' ', max);
        if (cut == std::string::npos || cut < max / 2) {
            cut = max;
        }
    }
    std::string out = s.substr(0, cut);
    while (!out.empty() && (out.back() == ' ' || out.back() == ',' || out.back() == ':')) {
        out.pop_back();
    }
    return out;
}

/** WordPrefix of every authored line of @p tip that has any text. A tooltip in
 *  SoH's disabled shape is "This setting is disabled because: ", a blank line,
 *  then "- Reason": its first line is shared by every disabled SoH row, so the
 *  reason line is the one that proves this row's tooltip was drawn. */
std::vector<std::string> TooltipLinePrefixes(const std::string& tip) {
    std::vector<std::string> out;
    size_t at = 0;
    while (at <= tip.size()) {
        size_t nl = tip.find('\n', at);
        if (nl == std::string::npos) {
            nl = tip.size();
        }
        const std::string prefix = WordPrefix(tip.substr(at, nl - at), 40);
        if (prefix.find_first_not_of(" \t") != std::string::npos) {
            out.push_back(prefix);
        }
        at = nl + 1;
    }
    return out;
}

/**
 * Cross-game crossings for the spoiler's and the tracker's populated states,
 * through the crossing store (Combo_Crossings_Replace), the single bag's only
 * record of them (#755), with items looked up by describer name, the way the
 * ROM-free locks do (test_named_items.h). Needs AuthorPairing first: the panes
 * list nothing for an unpaired world. Two OoT items on real MM checks, one MM
 * item on an OoT check the synthetic OoT world below names; the MM checks are
 * named by MM's tracker adapter, which MirrorProductionWindows registers.
 */
constexpr const char* kSnapshotOoTItemA = "Lens of Truth";
// The SoH pane our tracker and spoiler panes are read against (compareWith).
constexpr const char* kSohPaneReference = "window/Check Tracker Settings";
constexpr const char* kSohItemTrackerReference = "window/Item Tracker";
constexpr const char* kSnapshotOoTItemB = "Megaton Hammer";
// MM checks hosting the OoT items; the first is collected in the authored MM save.
constexpr uint16_t kSnapshotOoTToMMCheckA = 0x0401;
constexpr uint16_t kSnapshotOoTToMMCheckB = 0x0402;
constexpr const char* kSnapshotOoTItemC = "Progressive Slingshot";
constexpr uint16_t kSnapshotOoTToMMCheckC = 0x0403;
// The OoT check hosting the MM item: the synthetic OoT world's collected
// "KF Kokiri Sword Chest" (kSnapshotOoTChecks[0] below).
constexpr uint16_t kSnapshotMMToOoTCheckId = 0x0101;
// A string only the MM-items-in-OoT crossing list draws (its note).
constexpr const char* kSnapshotMMToOoTNote = "1 Majora's Mask item was placed in Ocarina of Time checks.";

extern "C" int OoT_ComboLogic_TestEnsureItemTableTransient(void);

bool SnapshotNamedItem(uint8_t origin, const char* name, SharedItem* out) {
    if (Combo_GetForeignItemByNameFor(origin, name, out)) {
        return true;
    }
    // ROM-free (hosted CI): OoT's item table is not built at OTR bring-up there.
    if (origin == (uint8_t)GAME_OOT && OoT_ComboLogic_TestEnsureItemTableTransient() == 0) {
        return Combo_GetForeignItemByNameFor(origin, name, out);
    }
    return false;
}

void AuthorCrossings() {
    SharedItem item;
    std::vector<ComboCrossing> mmHosted;
    std::vector<ComboCrossing> ootHosted;
    if (SnapshotNamedItem((uint8_t)GAME_OOT, kSnapshotOoTItemA, &item)) {
        mmHosted.push_back({ kSnapshotOoTToMMCheckA, 0x0001, item });
    }
    if (SnapshotNamedItem((uint8_t)GAME_OOT, kSnapshotOoTItemB, &item)) {
        mmHosted.push_back({ kSnapshotOoTToMMCheckB, 0x0001, item });
    }
    // The name #815 was found on: one word nearly as wide as the 480-px pane's
    // Item column, which a balanced wrap used to break inside itself.
    if (SnapshotNamedItem((uint8_t)GAME_OOT, kSnapshotOoTItemC, &item)) {
        mmHosted.push_back({ kSnapshotOoTToMMCheckC, 0x0001, item });
    }
    // MM's describer reads static data, so this lookup needs no ROM-free
    // fallback; if it ever fails, the row is missing and the states'
    // kSnapshotMMToOoTNote text fails the capture instead of passing silently.
    if (SnapshotNamedItem((uint8_t)GAME_MM, "Lens of Truth", &item)) {
        ootHosted.push_back({ kSnapshotMMToOoTCheckId, 0x0001, item });
    }
    Combo_Crossings_Clear();
    Combo_Crossings_Replace(ootHosted.empty() ? nullptr : ootHosted.data(), (int)ootHosted.size(),
                            mmHosted.empty() ? nullptr : mmHosted.data(), (int)mmHosted.size());
}

/**
 * The MM save the crossing states read found state from: a randomized MM world
 * in the shadow, at the offsets MM's tracker adapter registered, with the first
 * MM host collected and the second not. The resident shadow is kept aside and
 * put back in LeaveState, so no later page sees it.
 */
std::vector<uint8_t> gSnapshotMMShadowBackup;

void AuthorMMShadow(bool withItems = false) {
    const ComboMMTrackerDesc* desc = Combo_Tracker_GetMMDesc();
    // The shadow storage exists only once the frozen-state manager is
    // initialized (idempotent); before that GetMMSaveContext answers NULL.
    Context_InitFrozenStates();
    const void* resident = Context_GetMMSaveContext();
    if (desc == nullptr || resident == nullptr) {
        return;
    }
    gSnapshotMMShadowBackup.assign((const uint8_t*)resident, (const uint8_t*)resident + MM_SAVE_CONTEXT_SIZE);
    std::vector<uint8_t> blob((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    if (withItems) {
        // The Item Tracker overlay's states (#458 U2): MM's item adapter's own
        // "shadow" world (a bow with 25 arrows, beans, masks, a song, remains),
        // written through MM's layout; the tracker fields below go on top.
        MM_ItemAdapter_TestAuthorSave(blob.data(), blob.size(), 0);
    }
    // A paired file's MM final seed is a hash of the paired seed and the options
    // (Rando::Foreign::MixPairedFinalSeedForAttempt), never the paired seed itself
    // (#816), so the authored half carries a different number, as a real one does.
    const uint32_t seed = gComboCtx.sharedRandoSeed ^ 0x8DDF1DEEu;
    std::memcpy(blob.data() + desc->newfOffset, desc->newf, desc->newfLen);
    std::memcpy(blob.data() + desc->saveTypeOffset, &desc->saveTypeRando, sizeof(uint32_t));
    std::memcpy(blob.data() + desc->finalSeedOffset, &seed, sizeof(uint32_t));
    for (const uint16_t check : { kSnapshotOoTToMMCheckA, kSnapshotOoTToMMCheckB, kSnapshotOoTToMMCheckC }) {
        uint8_t* row = blob.data() + desc->checkTableOffset + (size_t)check * desc->checkStride;
        row[desc->shuffledOffset] = 1;
        row[desc->obtainedOffset] = (check == kSnapshotOoTToMMCheckA) ? 1 : 0;
    }
    Context_UpdateShadowCopy(GAME_MM, blob.data(), blob.size());
}

void RestoreMMShadow() {
    if (!gSnapshotMMShadowBackup.empty()) {
        Context_UpdateShadowCopy(GAME_MM, gSnapshotMMShadowBackup.data(), gSnapshotMMShadowBackup.size());
        gSnapshotMMShadowBackup.clear();
    }
}

/**
 * The unified Item Tracker overlay's states (#458 U2). "live": Ocarina of Time
 * played live (its item adapter wrapped with a live source holding the adapter's
 * "live" world: the Hookshot and the Song of Time), Majora's Mask from the
 * snapshot AuthorMMShadow writes, and a shared pool as a departing game leaves
 * it. "snapshot": the mirror image, Majora's Mask live (its "live" world: the
 * Hookshot, the Zora Mask, the Song of Soaring, the Kokiri Sword) and Ocarina of
 * Time from its snapshot (the adapter's "shadow" world: the Longshot, a bow with
 * 35 arrows, a medallion, tokens). "no-data": both snapshots empty, no live
 * source, an empty pool. Every adapter, both shadows and the pool are put back
 * in LeaveState.
 */
std::vector<uint8_t> gSnapshotOoTShadowBackup;
std::vector<uint8_t> gSnapshotItemOoTLive;
std::vector<uint8_t> gSnapshotItemMMLive;
const ComboItemOps* gSnapshotItemOoTOps = nullptr;
const ComboItemOps* gSnapshotItemMMOps = nullptr;
ComboItemOps gSnapshotItemOoTWrapped;
ComboItemOps gSnapshotItemMMWrapped;

const void* SnapshotItemOoTLive(void) {
    return gSnapshotItemOoTLive.empty() ? nullptr : gSnapshotItemOoTLive.data();
}
const void* SnapshotItemMMLive(void) {
    return gSnapshotItemMMLive.empty() ? nullptr : gSnapshotItemMMLive.data();
}

void AuthorItemTrackerState(const std::string& state) {
    Context_InitFrozenStates();
    gSnapshotItemOoTOps = Combo_Item_GetOps((uint8_t)GAME_OOT);
    gSnapshotItemMMOps = Combo_Item_GetOps((uint8_t)GAME_MM);
    if (const void* resident = Context_GetOoTSaveContext()) {
        gSnapshotOoTShadowBackup.assign((const uint8_t*)resident, (const uint8_t*)resident + OOT_SAVE_CONTEXT_SIZE);
    }
    std::vector<uint8_t> ootShadow((size_t)OOT_SAVE_CONTEXT_SIZE, 0);
    if (state == "no-data") {
        if (const void* resident = Context_GetMMSaveContext()) {
            gSnapshotMMShadowBackup.assign((const uint8_t*)resident, (const uint8_t*)resident + MM_SAVE_CONTEXT_SIZE);
        }
        std::vector<uint8_t> mmZeros((size_t)MM_SAVE_CONTEXT_SIZE, 0);
        Context_UpdateShadowCopy(GAME_MM, mmZeros.data(), mmZeros.size());
        Context_UpdateShadowCopy(GAME_OOT, ootShadow.data(), ootShadow.size());
        Combo_ResetSharedResourceWatermarks();
        Context_SetCurrentGame(GAME_OOT);
        CVarSetInteger(RSBS_CVAR_COMBO_ITEMS_WINDOW_TYPE, ComboGui::COMBO_ITEM_TRACKER_WINDOW);
        return;
    }
    AuthorMMShadow(true);
    OoT_ItemAdapter_TestAuthorSave(ootShadow.data(), ootShadow.size(), 0);
    Context_UpdateShadowCopy(GAME_OOT, ootShadow.data(), ootShadow.size());
    // The pool a departing game leaves: 250 rupees, a tier-2 quiver holding 33
    // arrows, five hearts and two pieces.
    Combo_ResetSharedResourceWatermarks();
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, 250);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_QUIVER_TIER, 2);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_ARROW_COUNT, 33);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_HEALTH_QUARTERS, Combo_MakeHealthQuarters(0x50, 2));
    const bool ootLive = state == "live";
    std::vector<uint8_t>& live = ootLive ? gSnapshotItemOoTLive : gSnapshotItemMMLive;
    live.assign(ootLive ? (size_t)OOT_SAVE_CONTEXT_SIZE : (size_t)MM_SAVE_CONTEXT_SIZE, 0);
    if (ootLive && gSnapshotItemOoTOps != nullptr) {
        OoT_ItemAdapter_TestAuthorSave(live.data(), live.size(), 1);
        gSnapshotItemOoTWrapped = *gSnapshotItemOoTOps;
        gSnapshotItemOoTWrapped.liveSave = SnapshotItemOoTLive;
        Combo_Item_RegisterOps((uint8_t)GAME_OOT, &gSnapshotItemOoTWrapped);
    } else if (!ootLive && gSnapshotItemMMOps != nullptr) {
        MM_ItemAdapter_TestAuthorSave(live.data(), live.size(), 1);
        gSnapshotItemMMWrapped = *gSnapshotItemMMOps;
        gSnapshotItemMMWrapped.liveSave = SnapshotItemMMLive;
        Combo_Item_RegisterOps((uint8_t)GAME_MM, &gSnapshotItemMMWrapped);
    }
    Context_SetCurrentGame(ootLive ? GAME_OOT : GAME_MM);
}

void RestoreItemTrackerState() {
    Combo_Item_RegisterOps((uint8_t)GAME_OOT, gSnapshotItemOoTOps);
    Combo_Item_RegisterOps((uint8_t)GAME_MM, gSnapshotItemMMOps);
    gSnapshotItemOoTLive.clear();
    gSnapshotItemMMLive.clear();
    if (!gSnapshotOoTShadowBackup.empty()) {
        Context_UpdateShadowCopy(GAME_OOT, gSnapshotOoTShadowBackup.data(), gSnapshotOoTShadowBackup.size());
        gSnapshotOoTShadowBackup.clear();
    }
    RestoreMMShadow();
    Combo_ResetSharedResourceWatermarks();
    CVarClear(RSBS_CVAR_COMBO_ITEMS_WINDOW_TYPE);
}

/**
 * A synthetic OoT tracker adapter for the tracker's "progress" state and the
 * spoiler's "crossings" state: a resident randomized world with one collected,
 * one skipped and two open checks, so the Checks list draws every status glyph
 * and the MM-items-in-OoT crossing reads its found state. Installed through the
 * production registrar (Combo_Tracker_RegisterOoT) and replaced by the real
 * adapter in LeaveState.
 */
struct SnapshotCheck {
    uint16_t id;
    const char* name;
    const char* shortName; // location_list.cpp's short name, which SoH prints under the area header
    uint8_t status;        // ComboTrackerCheckStatus
    uint16_t areaKey;
    const char* areaName;
    const char* item; // the item the row reveals, or NULL
    uint8_t itemGame;
};
// Rows as OoT's adapter fills them (#458 U4): two areas, Kokiri Forest's first
// check collected and saved (a crossing host, so it names the crossed MM item as
// SoH's check tracker does), one skipped, two open.
constexpr SnapshotCheck kSnapshotOoTChecks[] = {
    { 0x0101, "KF Kokiri Sword Chest", "Kokiri Sword Chest", COMBO_TRACKER_CHECK_SAVED, 0, "Kokiri Forest",
      "Lens of Truth", (uint8_t)GAME_MM },
    { 0x0102, "KF Mido Top Left Chest", "Mido Top Left Chest", COMBO_TRACKER_CHECK_SKIPPED, 0, "Kokiri Forest", nullptr,
      (uint8_t)GAME_NONE },
    { 0x0103, "KF Mido Top Right Chest", "Mido Top Right Chest", COMBO_TRACKER_CHECK_UNCHECKED, 0, "Kokiri Forest",
      nullptr, (uint8_t)GAME_NONE },
    { 0x0104, "Deku Tree Map Chest", "Map Chest", COMBO_TRACKER_CHECK_UNCHECKED, 20, "Deku Tree", nullptr,
      (uint8_t)GAME_NONE },
};
constexpr int kSnapshotOoTCheckCount = (int)(sizeof(kSnapshotOoTChecks) / sizeof(kSnapshotOoTChecks[0]));

bool SnapshotOoTSummary(ComboTrackerGameSummary* out) {
    out->hasWorld = true;
    out->seed = 0xC0FFEE55u;
    out->totalChecks = kSnapshotOoTCheckCount;
    out->shuffled = kSnapshotOoTCheckCount;
    out->obtained = 1;
    out->skipped = 1;
    return true;
}
int SnapshotOoTCheckCount(void) {
    return kSnapshotOoTCheckCount;
}
bool SnapshotOoTCheckAt(int index, ComboTrackerCheckRow* out) {
    if (index < 0 || index >= kSnapshotOoTCheckCount) {
        return false;
    }
    const SnapshotCheck& c = kSnapshotOoTChecks[index];
    out->checkId = c.id;
    out->name = c.name;
    out->shortName = c.shortName;
    out->shuffled = true;
    out->status = c.status;
    out->obtained = c.status == COMBO_TRACKER_CHECK_SAVED || c.status == COMBO_TRACKER_CHECK_COLLECTED;
    out->skipped = c.status == COMBO_TRACKER_CHECK_SKIPPED;
    out->areaKey = c.areaKey;
    out->areaName = c.areaName;
    out->placedItemName = c.item;
    out->placedItemGame = c.itemGame;
    return true;
}
const char* SnapshotOoTCheckName(uint16_t checkId) {
    for (const SnapshotCheck& c : kSnapshotOoTChecks) {
        if (c.id == checkId) {
            return c.name;
        }
    }
    return nullptr;
}
// OoT is the active game in "progress", with its world loaded: its panel is the
// LIVE one and takes skip writes, so it draws the skip buttons (#458 U5). The
// synthetic world is fixed, so a write changes nothing; the harness never clicks.
bool SnapshotOoTSkipWritable(void) {
    return true;
}
bool SnapshotOoTSetSkipped(uint16_t, bool) {
    return false;
}
const ComboOoTTrackerOps kSnapshotOoTOps = { SnapshotOoTSummary,   SnapshotOoTCheckCount,   SnapshotOoTCheckAt,
                                             SnapshotOoTCheckName, SnapshotOoTSkipWritable, SnapshotOoTSetSkipped };
// The skip buttons that synthetic world draws: one per check of the seed not yet
// found (the skipped one and the two open ones), none on the saved one.
constexpr int kSnapshotOoTSkipButtons = 3;

/** The id of a game panel's "Checks" tree node, as ComboTrackerWindow.cpp forms it
 *  inside the pane's Begin: PushID((int)game), then TreeNode("Checks"). */
ImGuiID TrackerChecksId(ImGuiWindow* w, int game) {
    return ImHashStr("Checks", 0, ImHashData(&game, sizeof(game), w->ID));
}

/**
 * The first trick row Combo > MM Tricks draws (DrawMmTrickList,
 * SohMenuComboMmRandomizer.cpp): the lowest area id that has a row (it walks
 * areas in id order), and that area's first row in table order. @p areaOut
 * receives the area id, -1 if none. With @p want set, only rows it accepts
 * count (the first RESERVED row, say).
 */
const ComboMMTrickDesc* FirstTrickArea(int* areaOut, bool (*want)(const ComboMMTrickDesc*) = nullptr) {
    *areaOut = -1;
    const ComboMMTrickDesc* best = nullptr;
    for (int i = 0; i < Combo_MMTrickCount(); i++) {
        const ComboMMTrickDesc* d = Combo_MMTrickAt(i);
        if (d != nullptr && (want == nullptr || want(d)) && (best == nullptr || d->area < best->area)) {
            best = d;
        }
    }
    if (best != nullptr) {
        *areaOut = (int)best->area;
    }
    return best;
}

bool IsReservedTrick(const ComboMMTrickDesc* d) {
    return d->reserved;
}

/**
 * The tricks Combo > MM Tricks' states turn on, so both of the list's columns
 * draw rows and the trick census counts both: the last two settable rows in
 * table order (the hovers' targets are the first area's, so they stay put).
 */
std::vector<const ComboMMTrickDesc*> CensusOnTricks() {
    std::vector<const ComboMMTrickDesc*> on;
    for (int i = Combo_MMTrickCount() - 1; i >= 0 && on.size() < 2; i--) {
        const ComboMMTrickDesc* d = Combo_MMTrickAt(i);
        if (d != nullptr && d->bound && !d->reserved) {
            on.push_back(d);
        }
    }
    return on;
}

/** The row with the longest name (then the most chips), the worst case for wrapping. */
const ComboMMTrickDesc* LongestTrick() {
    const ComboMMTrickDesc* best = nullptr;
    for (int i = 0; i < Combo_MMTrickCount(); i++) {
        const ComboMMTrickDesc* d = Combo_MMTrickAt(i);
        if (d == nullptr || d->label == nullptr) {
            continue;
        }
        if (best == nullptr || strlen(d->label) > strlen(best->label) ||
            (strlen(d->label) == strlen(best->label) && d->chipCount > best->chipCount)) {
            best = d;
        }
    }
    return best;
}

/** The first row of @p area, in table order, that @p want accepts; NULL if none. */
const ComboMMTrickDesc* FirstTrickIn(int area, bool (*want)(const ComboMMTrickDesc*)) {
    for (int i = 0; i < Combo_MMTrickCount(); i++) {
        const ComboMMTrickDesc* d = Combo_MMTrickAt(i);
        if (d != nullptr && (int)d->area == area && want(d)) {
            return d;
        }
    }
    return nullptr;
}

/**
 * Assert 6's string for a menu page: something only the page BODY draws.
 *
 * The sidebar loop (Menu.cpp) draws every sidebar label of the active header,
 * and the header bar every header, on every frame, so a sidebar name is in the
 * log whichever page is selected. This takes the first registered row whose
 * label is not a substring of that chrome, preferring a section header (stable
 * across states), then an interactive row, then a note. A row with a PreFunc is
 * skipped: a PreFunc may rename it (the status line) or hide it (a gated row)
 * per state. CheckBodyText then proves the choice discriminates, against a
 * sibling page's capture.
 */
std::string PickBodyText(Ship::Menu& m, const std::string& header, const std::string& sidebar) {
    auto& entries = MenuEntries(m);
    if (!entries.contains(header) || !entries.at(header).sidebars.contains(sidebar)) {
        return std::string();
    }
    std::vector<std::string> chrome;
    for (const auto& [h, e] : entries) {
        chrome.push_back(h);
    }
    for (const std::string& s : entries.at(header).sidebarOrder) {
        chrome.push_back(s);
    }
    auto usable = [&](const std::string& label) {
        if (label.size() < 4) {
            return false;
        }
        for (const std::string& c : chrome) {
            if (c.find(label) != std::string::npos) {
                return false;
            }
        }
        return true;
    };
    const SidebarEntry& page = entries.at(header).sidebars.at(sidebar);
    for (int pass = 0; pass < 3; pass++) {
        for (const auto& column : page.columnWidgets) {
            for (const WidgetInfo& row : column) {
                if (row.preFunc) {
                    continue;
                }
                const bool want = pass == 0   ? row.type == WIDGET_SEPARATOR_TEXT
                                  : pass == 1 ? IsInteractive(row.type)
                                              : row.type == WIDGET_TEXT;
                if (!want) {
                    continue;
                }
                const std::string label = WordPrefix(VisibleLabel(row.name), 40);
                if (usable(label)) {
                    return label;
                }
            }
        }
    }
    return std::string();
}

void Session::BuildPageList() {
    auto menuPage = [this](const std::string& header, const std::string& sidebar, Origin origin) {
        PageSpec p;
        p.id = header + "/" + sidebar;
        p.header = header;
        p.sidebar = sidebar;
        p.origin = origin;
        p.kind = Kind::MENU_PAGE;
        p.states = { "" };
        p.bodyText = PickBodyText(*menu, header, sidebar);
        // A page whose only row is a WIDGET_CUSTOM draws its own widgets, so no
        // registered name is drawn; its string is named here instead, and
        // CheckBodyText holds it to the same sibling contrast.
        static const std::map<std::string, std::string> kCustomBody = {
            { "Randomizer/Tricks/Glitches", "Enable Visible" }, // DrawTricksMenu's button row
        };
        if (p.bodyText.empty() && kCustomBody.contains(p.id)) {
            p.bodyText = kCustomBody.at(p.id);
        }
        if (!p.bodyText.empty()) {
            p.expectText = { p.bodyText };
        }
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
        // SoH's own disabled row, the reference for every unavailable or locked
        // row of ours (docs/ui-style-guide.md section 8): with Match Refresh Rate
        // on, SohMenuSettings.cpp's Current FPS PreFunc pushes
        // DISABLE_FOR_MATCH_REFRESH_RATE_ON and MenuDrawItem builds the
        // "This setting is disabled because:" tooltip from disabledMap.
        PageSpec p = menuPage("Settings", "Graphics", Origin::SOH_REFERENCE);
        p.needsRom = true;
        p.states = { "match-refresh-rate" };
        p.hovers = { "current-fps" };
        p.hoverRows = { { "current-fps", "Current FPS" } };
        pages.push_back(p);
    }
    {
        PageSpec p;
        p.id = "modal/Clear Config";
        p.origin = Origin::SOH_REFERENCE;
        p.kind = Kind::MODAL;
        p.window = "Clear Config";
        // over-menu: the same modal over the same open menu page the creation
        // overlay's over-menu variant sits on (Combo > Cross-Game Rules), so the
        // two can be read side by side: the dim, and what shows through the box.
        p.states = { "", "over-menu" };
        p.expectText = { "Clear Config", "Cancel" };
        p.scroll = false;
        pages.push_back(p);
    }
    {
        // SoH's one progress dialog: the ROM-extraction modal (OTRGlobals.cpp,
        // RunExtract), drawn by PushExtractionFrameStyle + DrawExtractionReference
        // below at a fixed half-way state. The creation overlay is a progress
        // dialog too, so this is its reference; "Clear Config" stays the reference
        // for confirms. The copy is held to RunExtract's style statements by the
        // static lint (check-ui-parity-lint.py, "C1"), so it cannot drift silently.
        PageSpec p;
        p.id = "modal/ROM Extraction";
        p.origin = Origin::SOH_REFERENCE;
        p.kind = Kind::MODAL;
        p.window = "ROM Extraction";
        p.states = { "" };
        p.expectText = { "ROM Extraction", "Extracting" };
        p.scroll = false;
        pages.push_back(p);
    }
    {
        // SoH's own toast shape (Enhancements/QoL/Autosave.cpp): one short message,
        // Notification::Options' default colours and the player's configured
        // duration. Muted here only so the harness plays no sound.
        PageSpec p;
        p.id = "toast/Game Autosaved";
        p.origin = Origin::SOH_REFERENCE;
        p.kind = Kind::TOAST;
        p.window = "notification#";
        p.states = { "" };
        p.expectText = { "Game autosaved" };
        p.scroll = false;
        pages.push_back(p);
    }
    {
        // SoH's own pane, the reference for the Combo Tracker and Cross-Game
        // Spoiler panes (docs/ui-style-guide.md section 10): its title bar and
        // close button, and SoH's table shape (CellPadding 8x8, BordersH|V,
        // TableSetupColumn + TableHeadersRow) at pane scale. The Check Tracker
        // itself draws only "Waiting for file load..." without a save.
        PageSpec p;
        p.id = kSohPaneReference;
        p.origin = Origin::SOH_REFERENCE;
        p.kind = Kind::WINDOW;
        p.window = "Check Tracker Settings";
        p.states = { "" };
        p.expectText = { "Check Tracker Settings", "General settings" };
        p.needsRom = true;
        pages.push_back(p);
    }
    {
        // SoH's own Item Tracker overlay, the reference for the unified Item
        // Tracker overlay (#458 U2): floating, as SoH ships it, with no save
        // loaded, so every icon is drawn faded. It draws icons and no text, so
        // there is no text to expect; the window being drawn and not blank is
        // the oracle. OoT is made the active game for it (its window is
        // OoT-gated, #797).
        PageSpec p;
        p.id = kSohItemTrackerReference;
        p.origin = Origin::SOH_REFERENCE;
        p.kind = Kind::WINDOW;
        p.window = "Item Tracker";
        p.states = { "" };
        p.needsRom = true;
        pages.push_back(p);
    }

    // Ours. Every Combo sidebar the live menu registered, in its own order, so a
    // page added later cannot escape the harness: an unlisted page compares with
    // Randomizer/General by default and says so in the manifest.
    static const std::map<std::string, std::string> kCompare = {
        { "Cross-Game Rules", "Randomizer/General" },
        { "Windows", "Randomizer/Item Tracker" },
        { "Majora's Mask", "Enhancements/Quality of Life" },
        // OoT's own mod menu (Settings/Mod Menu, which embeds ModMenuWindow)
        // cannot be the reference: with the harness's fresh config and no mods
        // folder, its GetEnabledModsFromCVar yields one empty name, UpdateModFiles
        // leaves it in place when the folder is absent, and DrawMods's
        // filePaths.at("") throws. Tricks/Glitches is SoH's other two-column
        // Disabled/Enabled table page, the layout this page shares.
        { "MM Mods", "Randomizer/Tricks/Glitches" },
        // MM's randomizer options and tricks, pages since 2026-09-27 (they were
        // a pop-out pane): read against the SoH pages whose shape they take.
        { COMBO_MM_OPTIONS_PAGE_NAME, "Randomizer/General" },
        { COMBO_MM_TRICKS_PAGE_NAME, "Randomizer/Tricks/Glitches" },
        // Each save file's cross-game state (lane W5): SoH's captured bordered
        // table page (DrawLocationsMenu's table, whose own page is not captured).
        { "Save Files", "Randomizer/Tricks/Glitches" },
    };
    auto& entries = MenuEntries(*menu);
    if (entries.contains("Combo")) {
        // Every page named in kCompare must be registered. The loop below captures
        // what IS registered, so without this a contributed page whose registrar
        // was elided or renamed would vanish from the run and the row stay green
        // (#706's review).
        for (const auto& [named, reference] : kCompare) {
            const auto& order = entries.at("Combo").sidebarOrder;
            if (std::find(order.begin(), order.end(), named) == order.end()) {
                Fail("Combo/" + named + " has a reference (" + reference +
                     ") but no registered Combo page carries that name: its registrar was elided or renamed, so "
                     "its captures would silently drop out of this run");
            }
        }
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
                // (empty-oot-classes drew the empty-set note until #834 retired
                // the item-class rows.)
                p.states = { "unpaired", "paired-legacy", "frozen", "corrupt" };
                // The goal row (ADR 0010 D1) is hovered too: its tooltip carries
                // one "Value: effect" line per goal, all of which the oracle reads.
                // frozen-goal: the goal row's disabled tooltip once the world is decided.
                // (frozen-slider hovered a pool-size slider until #801 retired both.)
                p.hovers = { "direction", "goal", "frozen-goal" };
                // The status line's four sentences (ComboRuleStatusPreFunc in
                // SohMenuCombo.cpp). Copied, deliberately: a rewording there
                // turns this row red and the lane updates the words here.
                p.stateText["unpaired"] = { "apply to the next paired world",
                                            "Loading a paired file restores its own rules" };
                p.stateText["paired-legacy"] = { "Your paired world predates these rules",
                                                 "Keep them at the defaults until you have crossed into it once" };
                p.stateText["frozen"] = { "Already decided when this world was created" };
                p.stateText["corrupt"] = { "Session state is corrupt" };
                p.stateContrast = { { "unpaired", "paired-legacy" },
                                    { "paired-legacy", "unpaired" },
                                    { "frozen", "unpaired" },
                                    { "corrupt", "unpaired" } };
            } else if (sidebar == "Majora's Mask") {
                p.states = { "", "autosave" };
                // The row gated on gEnhancements.Autosave (the table's
                // shownWhileKey), which hides while Autosave is off.
                for (std::size_t i = 0; i < RSBS::kHostedMmEnhancementCount; i++) {
                    const RSBS::HostedMmEnhancement& e = RSBS::kHostedMmEnhancements[i];
                    if (e.shownWhileKey != nullptr && std::strcmp(e.shownWhileKey, "gEnhancements.Autosave") == 0 &&
                        e.label != nullptr) {
                        p.stateText["autosave"] = { VisibleLabel(e.label) };
                    }
                }
                if (p.stateText["autosave"].empty()) {
                    Fail("Combo/Majora's Mask: no hosted row is gated on gEnhancements.Autosave, so the autosave "
                         "state has nothing to show");
                }
                p.stateContrast = { { "autosave", "" } };
                // The first row's tooltip: the page's tooltip voice, with its
                // trailing "Majora's Mask only." caveat (guide R-TT6).
                if (RSBS::kHostedMmEnhancementCount > 0 && RSBS::kHostedMmEnhancements[0].label != nullptr) {
                    p.hovers = { "first-row" };
                    p.hoverRows["first-row"] = RSBS::kHostedMmEnhancements[0].label;
                }
            } else if (sidebar == "MM Mods") {
                // "": an empty model, so the empty-folder note. "listed": a
                // synthetic list with one disabled mod, so both columns and every
                // arrow draw. "unfinished": the same list from a walk that ended
                // early, which is read-only (mm_mod_set.h rule 4): its note, and
                // every arrow disabled. All three are authored through the model's
                // test seam, so the harness's own mods folder cannot change them.
                p.states = { "", "listed", "unfinished" };
                p.stateText[""] = { "No Majora's Mask mods found" };
                p.stateText["listed"] = { kMmModsListedEnabled[0], kMmModsListedDisabled,
                                          "Changes apply when Majora's Mask starts" };
                // Only the note: the list itself is the same as "listed" (the contrast),
                // so a file name here could not prove this state was authored.
                p.stateText["unfinished"] = { "did not finish" };
                p.stateContrast = { { "", "listed" }, { "listed", "" }, { "unfinished", "listed" } };
                p.hovers = { "rescan" };
                p.hoverRows["rescan"] = "Rescan Mods Folder";
            } else if (sidebar == "Save Files") {
                // "": no cross-game record at any slot, so its note and three
                // record-less rows. "listed": a ready file, a load refused for its
                // rules (the toast's words) and a CRC-refused one with a backup
                // (SaveFilesListedMeta), so its note and both refusal shapes draw.
                // "backup": a record set aside by a refusal in an earlier session
                // (after a restart only its evidence remains), so the backup note
                // and row. All through the model's seam
                // (Combo_SaveFiles_SetMetaForTest).
                p.states = { "", "listed", "backup" };
                p.stateText[""] = { "No cross-game record yet" };
                p.stateText["listed"] = { "A refused file does not open",
                                          std::string("Not paired: ") + Combo_SaveFiles_RefuseText(RSBS_REFUSE_CRC),
                                          std::string("Not paired: ") +
                                              RsbsSave_LoadToastRefusalMessage(RSBS_LOAD_TOAST_REFUSED_RULES) };
                p.stateText["backup"] = { "was kept as a backup", "No cross-game record (backup kept)" };
                p.stateContrast = { { "", "listed" }, { "listed", "" }, { "backup", "" } };
            } else if (sidebar == "Windows") {
                // An MM tracker toggle: SoH's "Toggles the <Window>." plus the
                // sentence that explains its blank window under Ocarina of Time.
                p.hovers = { "mm-item-tracker" };
                p.hoverRows["mm-item-tracker"] = "Toggle MM Item Tracker";
            } else if (sidebar == COMBO_MM_OPTIONS_PAGE_NAME) {
                // The model's state note in each state (combo_mm_options_page.c),
                // and the suspended note, which only a non-MM running game shows.
                p.states = { "unpaired", "frozen", "mm-suspended" };
                p.stateText["unpaired"] = { "Loading a paired file restores its own options" };
                p.stateText["frozen"] = { "Already decided when this world was created" };
                p.stateText["mm-suspended"] = { "Majora's Mask is suspended" };
                p.stateContrast = { { "unpaired", "frozen" },
                                    { "frozen", "unpaired" },
                                    { "mm-suspended", "unpaired" } };
                // Hovers, by row name (the page's own derivation: the "##MMRando"
                // id suffix on a checkbox or combobox, "Name: %d" on a slider):
                // the first row's description unpaired, the same row frozen
                // (SoH's disabled shape, "Already Decided"), and the first row a
                // capability blocks (SoH's disabled shape, the table's reason).
                auto rowName = [](const ComboMMOptionDesc* d) {
                    if (d->widget == COMBO_MM_WIDGET_SLIDER) {
                        return std::string(d->label) + ": %d";
                    }
                    if (d->widget == COMBO_MM_WIDGET_TIME) {
                        return std::string(d->label);
                    }
                    return std::string(d->label) + COMBO_MM_OPTIONS_ROW_ID_SUFFIX;
                };
                if (Combo_MMOptionCount() > 0 && Combo_MMOptionAt(0) != nullptr) {
                    p.hovers = { "first-row", "frozen-row" };
                    p.hoverRows["first-row"] = rowName(Combo_MMOptionAt(0));
                    p.hoverRows["frozen-row"] = rowName(Combo_MMOptionAt(0));
                    p.hoverRowState["first-row"] = "unpaired";
                    p.hoverRowState["frozen-row"] = "frozen";
                    p.disabledHovers.insert("frozen-row");
                }
                for (int i = 0; i < Combo_MMOptionCount(); i++) {
                    const ComboMMOptionDesc* d = Combo_MMOptionAt(i);
                    if (d != nullptr &&
                        (d->liveness == COMBO_MM_LIVENESS_PARTIAL || d->liveness == COMBO_MM_LIVENESS_DORMANT)) {
                        p.hovers.push_back("blocked-row");
                        p.hoverRows["blocked-row"] = rowName(d);
                        p.hoverRowState["blocked-row"] = "unpaired";
                        p.disabledHovers.insert("blocked-row");
                        break;
                    }
                }
            } else if (sidebar == COMBO_MM_TRICKS_PAGE_NAME) {
                // Unpaired: the model's headline; frozen: the freeze sentence.
                p.states = { "unpaired", "frozen" };
                p.stateText["unpaired"] = { "tricks are supported by the randomizer logic",
                                            "Loading a paired file restores its own tricks" };
                p.stateText["frozen"] = { "Already decided when this world was created" };
                p.stateContrast = { { "unpaired", "frozen" }, { "frozen", "unpaired" } };
                // Every trick row draws its name through the combo_ui seam, whose
                // rect recorder is how a hover finds it (the list is one custom
                // row, so no WidgetInfo per trick). A live trick (its
                // description), an unbound and a reserved one (SoH's disabled
                // shape with the table's reason), and a live one again frozen.
                p.hoverCompareWith = "Settings/Graphics";
                int area = -1;
                const ComboMMTrickDesc* first = FirstTrickArea(&area);
                if (first != nullptr) {
                    const ComboMMTrickDesc* live =
                        FirstTrickIn(area, [](const ComboMMTrickDesc* d) { return d->bound && !d->reserved; });
                    const ComboMMTrickDesc* unbound =
                        FirstTrickIn(area, [](const ComboMMTrickDesc* d) { return !d->bound && !d->reserved; });
                    if (live == nullptr) {
                        int liveArea = -1;
                        live = FirstTrickArea(&liveArea,
                                              [](const ComboMMTrickDesc* d) { return d->bound && !d->reserved; });
                    }
                    if (unbound == nullptr) {
                        int unboundArea = -1;
                        unbound = FirstTrickArea(&unboundArea,
                                                 [](const ComboMMTrickDesc* d) { return !d->bound && !d->reserved; });
                    }
                    if (live != nullptr) {
                        p.paneHovers.push_back({ "trick-live", "unpaired", live->label, false });
                        p.paneHovers.push_back({ "trick-frozen", "frozen", live->label, true });
                    }
                    if (unbound != nullptr) {
                        p.paneHovers.push_back({ "trick-unbound", "unpaired", unbound->label, true });
                    }
                }
                int reservedArea = -1;
                const ComboMMTrickDesc* reserved = FirstTrickArea(&reservedArea, IsReservedTrick);
                if (reserved != nullptr) {
                    p.paneHovers.push_back({ "trick-reserved", "unpaired", reserved->label, true });
                }
                // The hovers above read four rows; the census reads every row.
                p.trickCensus = true;
            }
            pages.push_back(p);
        }
    } else {
        Fail("the live menu has no \"Combo\" header, so none of this project's menu pages can be captured");
    }
    if (entries.contains("Combo")) {
        // ADR 0004 section 6's four presentations on synthetic rows (see
        // InstallRowStateProbe), compared with SoH's own disabled row.
        PageSpec p;
        p.id = std::string("Combo/") + kRowStatesSidebar;
        p.header = "Combo";
        p.sidebar = kRowStatesSidebar;
        p.origin = Origin::RSBS;
        p.kind = Kind::MENU_PAGE;
        p.rowStateProbe = true;
        p.compareWith = "Settings/Graphics";
        p.states = { "", "race-lockout" };
        p.hovers = { "capability", "frozen" };
        p.hoverRows = { { "capability", kProbeGatedRow }, { "frozen", kProbeDecidedRow } };
        p.hoverStates = { "" };
        {
            const std::vector<std::string> notes = {
                SohGui::SohMenu::PresentationNoteText(SohGui::SOH_MENU_PRESENT_INACTIVE_GAME),
                SohGui::SohMenu::PresentationNoteText(SohGui::SOH_MENU_PRESENT_CAPABILITY),
                SohGui::SohMenu::PresentationNoteText(SohGui::SOH_MENU_PRESENT_FROZEN),
            };
            p.stateText[""] = notes;
            p.stateText["race-lockout"] = notes;
        }
        p.bodyText = "Suspended Game";
        p.expectText = { p.bodyText };
        pages.push_back(p);
    }
    if (entries.contains("Combo")) {
        // #747: Combo > Majora's Mask's group note and disabled rows, drawn by
        // the page's own builder over a synthetic non-live manifest (see
        // InstallMmNoteProbe), compared with the shipped page's reference.
        PageSpec p;
        p.id = std::string("Combo/") + kMmNoteSidebar;
        p.header = "Combo";
        p.sidebar = kMmNoteSidebar;
        p.origin = Origin::RSBS;
        p.kind = Kind::MENU_PAGE;
        p.mmNoteProbe = true;
        p.compareWith = "Enhancements/Quality of Life";
        // "": both gates are off, so every non-live row and both notes are
        // hidden and the page shows only its Live row and the pointer.
        // "heading-on": the Live row's key is on, so the heading group's
        // Dormant row draws disabled under the heading group's note.
        // "gate-on": the pointer's key is on, so the second group's two rows
        // draw disabled under that group's note, below the pointer sentence.
        // No race-lockout state: every row this builder registers is
        // .RaceDisable(false) and Menu.cpp's TEXT draw never reads `disabled`,
        // so a lockout changes nothing on this page by construction, and a
        // capture of it could only repeat "gate-on".
        p.states = { "", "heading-on", "gate-on" };
        const std::string note = SohGui::MmEnhancementsGroupNoteText();
        p.stateText["heading-on"] = { note, kMmNoteAbsentRow };
        p.stateText["gate-on"] = { note, kMmNoteDormantRow, kMmNotePartialLabel };
        p.stateContrast = { { "heading-on", "" }, { "gate-on", "" } };
        p.hovers = { "dormant" };
        p.hoverRows = { { "dormant", kMmNoteDormantRow } };
        p.hoverRowState = { { "dormant", "gate-on" } };
        p.disabledHovers = { "dormant" };
        p.bodyText = kMmNoteLiveRow;
        p.expectText = { p.bodyText };
        pages.push_back(p);
    }
    {
        PageSpec p = menuPage("Randomizer", "Cross-Game", Origin::RSBS);
        p.compareWith = "Randomizer/General";
        pages.push_back(p);
    }
    {
        PageSpec p;
        p.id = std::string("window/") + ComboGui::kComboSpoilerWindowName;
        p.kind = Kind::WINDOW;
        p.window = ComboGui::kComboSpoilerWindowName;
        // "crossings" authors crossings both ways (AuthorCrossings), an MM save and
        // an OoT world to read their found state from, so both tables are drawn:
        // "paired" alone holds none and only ever showed the empty notes.
        p.states = { "paired", "crossings", "unpaired" };
        p.compareWith = kSohPaneReference;
        p.expectText = { ComboGui::kComboSpoilerWindowName };
        p.stateText["crossings"] = { kSnapshotOoTItemA, kSnapshotMMToOoTNote };
        p.stateText["unpaired"] = { "No paired world" };
        p.stateContrast = { { "crossings", "paired" }, { "unpaired", "paired" } };
        pages.push_back(p);
    }
    {
        PageSpec p;
        p.id = std::string("window/") + ComboGui::kComboTrackerWindowName;
        p.kind = Kind::WINDOW;
        p.window = ComboGui::kComboTrackerWindowName;
        // "progress" installs a synthetic OoT adapter with a resident world and
        // opens its Checks list, authors an MM save, and authors crossings both
        // ways, so the check glyphs and both crossing tables are drawn with their
        // found state: without a loaded save, "paired" only ever showed the
        // no-data notes.
        p.states = { "paired", "unpaired", "progress" };
        p.compareWith = kSohPaneReference;
        p.expectText = { ComboGui::kComboTrackerWindowName };
        p.stateText["progress"] = { kSnapshotOoTChecks[0].name, kSnapshotOoTItemA, kSnapshotMMToOoTNote };
        p.stateText["unpaired"] = { "No paired world" };
        p.stateContrast = { { "progress", "paired" }, { "unpaired", "paired" } };
        pages.push_back(p);
    }
    {
        // The unified Item Tracker overlay (#458 U2), read against SoH's own
        // Item Tracker overlay (window/Item Tracker). "live": OoT live, MM from
        // its snapshot; "snapshot": the mirror image; both floating, as SoH's
        // default is (no title bar). "no-data": nothing to read, drawn as the
        // window type (title bar), the overlay's other chrome
        // (AuthorItemTrackerState). The floating overlay has no scrollbar and
        // takes no input, so a player sees its first view and nothing else: it
        // is captured unscrolled, and FitOracle fails it if anything is clipped
        // at its edge or the window runs past the game window's. Every state's
        // one view must hold the three section headers (expectText) and each
        // section's freshness note and a row from the last section (stateText).
        PageSpec p;
        p.id = std::string("window/") + ComboGui::kComboItemTrackerWindowName;
        p.kind = Kind::WINDOW;
        p.window = ComboGui::kComboItemTrackerWindowName;
        p.states = { "no-data", "live", "snapshot" };
        p.compareWith = kSohItemTrackerReference;
        p.scroll = false;
        p.expectText = { "Ocarina of Time", "Majora's Mask", "Shared" };
        // Row text as the overlay prints it: MM's snapshot bow and OoT's live
        // song in "live"; OoT's snapshot bow and MM's live mask in "snapshot";
        // the shared pool's arrows (the last section) in both.
        p.stateText["live"] = { "Updated live.", "As of file creation.", "As of the last switch or save.",
                                "Bow 25/40",     "Song of Time",         "Arrows 33/40" };
        p.stateText["snapshot"] = {
            "Updated live.",   "As of the last game switch or save.", "As of the last switch or save.",
            "Fairy Bow 35/40", "Gold Skulltula Tokens 17/100",        "Arrows 33/40"
        };
        p.stateText["no-data"] = { "No data.", ComboGui::kComboItemTrackerWindowName };
        p.stateContrast = { { "live", "no-data" }, { "snapshot", "no-data" }, { "no-data", "live" } };
        pages.push_back(p);
    }
    // Ours: the Cross-Game Rules Reset confirm, opened through the row's own
    // Callback (so the capture is the popup a player gets) and compared with
    // SoH's "Clear Config" modal.
    if (entries.contains("Combo") && entries.at("Combo").sidebars.contains("Cross-Game Rules")) {
        PageSpec p;
        p.id = "modal/Reset Combo Rules";
        p.origin = Origin::RSBS;
        p.kind = Kind::MODAL;
        p.header = "Combo";
        p.sidebar = "Cross-Game Rules";
        p.window = "Reset Combo Rules";
        p.states = { "" };
        p.compareWith = "modal/Clear Config";
        p.expectText = { "Reset Combo Rules", "Cancel" };
        p.scroll = false;
        pages.push_back(p);
    }
    // Ours: Combo > MM Randomizer's Reset confirm, opened through the row's own
    // Callback (so the capture is the popup a player gets) and compared with
    // SoH's "Clear Config" modal. It was the MM options pane's confirm until the
    // options became pages.
    if (entries.contains("Combo") && entries.at("Combo").sidebars.contains(COMBO_MM_OPTIONS_PAGE_NAME)) {
        PageSpec p;
        p.id = std::string("modal/") + COMBO_MM_OPTIONS_RESET_TITLE;
        p.origin = Origin::RSBS;
        p.kind = Kind::MODAL;
        p.header = "Combo";
        p.sidebar = COMBO_MM_OPTIONS_PAGE_NAME;
        p.window = COMBO_MM_OPTIONS_RESET_TITLE;
        p.states = { "" };
        p.compareWith = "modal/Clear Config";
        p.expectText = { COMBO_MM_OPTIONS_RESET_TITLE, "Cancel" };
        p.scroll = false;
        pages.push_back(p);
    }
    {
        PageSpec p;
        p.id = "overlay/creation-progress";
        p.kind = Kind::OVERLAY;
        p.window = "Creating Your Paired World";
        // over-menu: the menu left open under the overlay, as a pumped frame draws
        // it (Gui::StartDraw -> DrawMenu). A modal dims the menu too, and this
        // variant's oracle (DimOracle) requires the overlay's dim to: the menu
        // drawn, the dim window between it and the box, and every pixel outside
        // the box equal to the undimmed menu blended with ModalWindowDimBg.
        p.states = { "", "over-menu" };
        p.compareWith = "modal/ROM Extraction";
        p.expectText = { "Creating Your Paired World" };
        p.scroll = false;
        pages.push_back(p);
    }
    for (const auto& [id, text] : std::vector<std::pair<std::string, std::string>>{
             { "toast/creation-shortfall", "Fewer cross-game items:" },
             { "toast/creation-failure", "Not created:" },
             { "toast/creation-goal-warning", "Not proven:" },
             // The cross-game refusals (src/common/pairing_refusal_toast.h), each
             // through its production emitter; the rules and spoiler pages draw
             // their longest shapes.
             { "toast/pairing-refused-mm-options", "Not saved:" },
             { "toast/pairing-refused-rules", "Not saved:" },
             { "toast/pairing-refused-missing-half", "Not saved:" },
             { "toast/pairing-refused-spoiler", "Not saved:" },
             { "toast/paired-spoiler-not-loaded", "Spoiler not loaded:" },
             // The paired-file load's toasts (#781), each through the load's
             // own emitter with the longest input a real load can pass it.
             { "toast/load-rules-restored", "Restored from file:" },
             { "toast/load-mm-restored-one", "Restored for Majora's Mask:" },
             { "toast/load-mm-restored-many", "Restored for Majora's Mask:" },
             { "toast/load-mm-not-restored", "Not restored:" },
             { "toast/load-refused-rules", "Not paired:" },
             { "toast/load-refused-other-build", "Not paired:" },
             { "toast/load-refused-damaged", "Not paired:" },
             // The file select's refusal (#836), with the longest words it
             // can show ("This file has no Majora's Mask world").
             { "toast/load-refused-file-select", "Not paired:" },
         }) {
        PageSpec p;
        p.id = id;
        p.kind = Kind::TOAST;
        p.window = "notification#";
        p.states = { "" };
        p.compareWith = "toast/Game Autosaved";
        p.expectText = { text };
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

    // NewFrame IM_ASSERTs that the previous frame was ended ("Forgot to call
    // Render() or EndFrame()"), and IM_ASSERT is assert(): a Debug build (the
    // project's default CMAKE_BUILD_TYPE) aborts the whole run right there, with
    // no manifest. A Release build compiles the assert out and would carry on, so
    // the condition is checked HERE, in every build type, and fails the run by
    // name instead of depending on which build caught it.
    {
        ImGuiContext& g = *GImGui;
        if (g.FrameCount != 0 && g.FrameCountEnded != g.FrameCount) {
            Fail("an ImGui frame was left open by the previous pump (FrameCount " + std::to_string(g.FrameCount) +
                 ", FrameCountEnded " + std::to_string(g.FrameCountEnded) +
                 "); ImGui::NewFrame would assert here in a Debug build");
            CloseAbandonedFrame();
        }
    }

    gHooks.hoverActive = hover && !opt.Sabotaged("no-hover");
    gHooks.logEnabled = true;
    bool ok = true;
    bool interpreterFrame = false;
    {
        InputSuppression suppressed(ImGui::GetIO(), !gHooks.hoverActive);
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
            // bad_variant_access). The text log is closed first, so the next
            // capture's .txt does not start with this frame's text; then the
            // ImGui frame is CLOSED, because the next NewFrame asserts that it was
            // (see the guard at the top of this function); then the interpreter's
            // frame is presented if it was started. RSBS_UI_SNAPSHOT_SABOTAGE=
            // leave-open skips the close, which is the behaviour this replaced,
            // and the guard then fails the run.
            if (GImGui->LogEnabled) {
                ImGui::LogFinish();
            }
            if (!opt.Sabotaged("leave-open")) {
                CloseAbandonedFrame();
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

void Session::CloseAbandonedFrame() {
    ImGuiContext& g = *GImGui;
    if (!g.WithinFrameScope || g.FrameCountEnded == g.FrameCount) {
        return;
    }
    // Unwind whatever Begin/Push the throw skipped, then end the frame. The
    // recovery reports each unbalanced call through IM_ASSERT_USER_ERROR, which
    // asserts when ConfigErrorRecoveryEnableAssert is set (the default), so a
    // Debug build would abort in the recovery itself: off for the unwind only,
    // since the page is already failed by name.
    ImGuiIO& io = g.IO;
    const bool savedAssert = io.ConfigErrorRecoveryEnableAssert;
    io.ConfigErrorRecoveryEnableAssert = false;
    ImGui::ErrorRecoveryTryToRecoverState(&g.StackSizesInNewFrame);
    ImGui::EndFrame();
    io.ConfigErrorRecoveryEnableAssert = savedAssert;
    if ((io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0) {
        // NewFrame also asserts UpdatePlatformWindows ran after EndFrame when
        // viewports are on (the harness pins them off; this is belt and braces).
        ImGui::UpdatePlatformWindows();
    }
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
    Combo_Crossings_Clear();
    if (opt.Sabotaged("no-state")) {
        return;
    }
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
    } else if (p.id == "Combo/Majora's Mask") {
        if (state == "autosave") {
            CVarSetInteger("gEnhancements.Autosave", 1);
        }
    } else if (p.id == "Combo/MM Mods") {
        // Every state loads its model through the test seam, which persists
        // nothing and reads no disk, so the page never scans the harness's own
        // mods root (it scans lazily only when the model is empty).
        Combo_MMModSet_Reset();
        if (state == "listed" || state == "unfinished") {
            CVarSetString(RSBS_CVAR_MM_DISABLED_MODS, kMmModsListedDisabled);
            const char* keys[] = { kMmModsListedEnabled[0], kMmModsListedEnabled[1], kMmModsListedDisabled };
            Combo_MMModSet_LoadForTest(keys, 3, state == "listed");
        } else {
            Combo_MMModSet_LoadForTest(nullptr, 0, true);
        }
    } else if (p.id == "Combo/Save Files") {
        // Every state reads the seam's table, never the disk.
        if (state == "listed") {
            rsbs::SlotMeta metas[RSBS_SAVE_MAX_SLOTS];
            for (int i = 0; i < RSBS_SAVE_MAX_SLOTS; i++) {
                metas[i] = SaveFilesListedMeta(i);
            }
            Combo_SaveFiles_SetMetaForTest(metas, RSBS_SAVE_MAX_SLOTS);
        } else if (state == "backup") {
            // Slot 1's record was quarantined for its CRC in an earlier session:
            // no record, no session refusal, only the evidence's file name.
            rsbs::SlotMeta metas[2] = {};
            metas[1].state = RSBS_SLOT_ABSENT;
            metas[1].hasQuarantine = true;
            metas[1].quarantineReason = RSBS_REFUSE_CRC;
            Combo_SaveFiles_SetMetaForTest(metas, 2);
        } else {
            static const rsbs::SlotMeta kNoFile{};
            Combo_SaveFiles_SetMetaForTest(&kNoFile, 0);
        }
    } else if (p.id == "Settings/Graphics") {
        if (state == "match-refresh-rate") {
            CVarSetInteger(CVAR_SETTING("MatchRefreshRate"), 1);
        }
    } else if (p.rowStateProbe || p.mmNoteProbe) {
        if (p.mmNoteProbe && state == "heading-on") {
            CVarSetInteger(kMmNoteLiveKey, 1);
        }
        if (p.mmNoteProbe && state == "gate-on") {
            CVarSetInteger(kMmNoteParentKey, 1);
        }
        if (state == "race-lockout") {
            // MenuDrawItem's race lockout (Menu.cpp) disables every RaceDisable
            // row and REPLACES its disabled tooltip: the case the gray notes are
            // for.
            CVarSetInteger(CVAR_SETTING("DisableChanges"), 1);
        }
    } else if (p.id == std::string("Combo/") + COMBO_MM_OPTIONS_PAGE_NAME ||
               p.id == std::string("Combo/") + COMBO_MM_TRICKS_PAGE_NAME) {
        // Majora's Mask running unless the state is the suspended one, so the
        // suspended note shows in exactly one state; frozen is a creation stamp.
        Context_SetCurrentGame(state == "mm-suspended" ? GAME_OOT : GAME_MM);
        if (p.trickCensus) {
            // Two tricks on before any freeze (the writer refuses once frozen),
            // so the Enabled Tricks column has rows for the census to count.
            for (const ComboMMTrickDesc* d : CensusOnTricks()) {
                Combo_MMTrickSetValue(d, true);
            }
        }
        if (state == "frozen") {
            AuthorPairing();
            gComboCtx.mmProfileDigest = 0x4D4D0001u;
        }
    } else if (p.kind == Kind::WINDOW) {
        if (p.window == ComboGui::kComboItemTrackerWindowName) {
            AuthorItemTrackerState(state);
        } else if (p.id == kSohItemTrackerReference) {
            Context_SetCurrentGame(GAME_OOT); // SoH's tracker draws only while OoT is the active game (#797)
        }
        if (state == "paired") {
            AuthorPairing();
        } else if (state == "crossings" || state == "progress") {
            AuthorPairing();
            AuthorCrossings();
            AuthorMMShadow();
            Context_SetCurrentGame(GAME_OOT);
            Combo_Tracker_RegisterOoT(&kSnapshotOoTOps);
        }
    }
}

void Session::LeaveState(const PageSpec& p, const std::string& state) {
    if (p.id == "Combo/Save Files") {
        Combo_SaveFiles_SetMetaForTest(nullptr, 0);
    }
    if (p.id == "Combo/MM Mods") {
        CVarClear(RSBS_CVAR_MM_ENABLED_MODS);
        CVarClear(RSBS_CVAR_MM_DISABLED_MODS);
        Combo_MMModSet_Reset();
    }
    if (p.id == "Combo/Majora's Mask" && state == "autosave") {
        CVarClear("gEnhancements.Autosave");
    }
    if (p.id == "Settings/Graphics" && state == "match-refresh-rate") {
        CVarClear(CVAR_SETTING("MatchRefreshRate"));
    }
    if (p.rowStateProbe && state == "race-lockout") {
        CVarClear(CVAR_SETTING("DisableChanges"));
    }
    if (p.mmNoteProbe && state == "heading-on") {
        CVarClear(kMmNoteLiveKey);
    }
    if (p.mmNoteProbe && state == "gate-on") {
        CVarClear(kMmNoteParentKey);
    }
    if (p.kind == Kind::WINDOW && (state == "progress" || state == "crossings")) {
        OoT_TrackerAdapter_Register();
        RestoreMMShadow();
    }
    if (p.kind == Kind::WINDOW && p.window == ComboGui::kComboItemTrackerWindowName) {
        RestoreItemTrackerState();
    }
    ComboContext_Init();
    Combo_Crossings_Clear();
    if (p.trickCensus) {
        // After ComboContext_Init: the writers refuse while the stamp stands.
        for (const ComboMMTrickDesc* d : CensusOnTricks()) {
            Combo_MMTrickClear(d);
        }
    }
    Context_SetCurrentGame(gSavedGame);
}

// ---- the row-state probe ------------------------------------------------------------------

/**
 * ADR 0004 section 6's four presentations, on synthetic rows. No production row
 * takes a gated path today (every MM Enhancements row is Live), so without this
 * page the presentation code would never reach a pixel. It is installed into the
 * live menu's Combo header only while its own captures run and removed right
 * after, so no other page's sidebar and no SoH reference ever shows it. Each row
 * goes through the production API a real caller uses: the capability row through
 * SohMenu::CapabilityGate on a real built-in capability (COMBO_PAIRED, absent in
 * every state this page authors, because EnterState resets gComboCtx), the other
 * two gated rows through SohMenu::ApplyPresentation. The rows join R5/R6 while
 * installed, so a presentation that rewrites a row's name is a runtime-lint hit.
 * No group title repeats a reason fragment ("Already Decided"): SeparatorText
 * logs as "--- <title>", which would hold the "- <Reason>" tooltip line and
 * defeat the hover contrast.
 */
void Session::InstallRowStateProbe() {
    auto soh = std::dynamic_pointer_cast<SohGui::SohMenu>(menu);
    auto& entries = MenuEntries(*menu);
    if (soh == nullptr || !entries.contains("Combo") || entries.at("Combo").sidebars.contains(kRowStatesSidebar)) {
        return;
    }
    soh->AddSidebarEntry("Combo", kRowStatesSidebar, 2);
    WidgetPath path = { "Combo", kRowStatesSidebar, SECTION_COLUMN_1 };
    soh->AddWidget(path, "Applies Now", WIDGET_SEPARATOR_TEXT);
    soh->AddWidget(path, kProbeLiveRow, WIDGET_CHECKBOX)
        .ValuePointer(&gProbeLive)
        .Options(UIWidgets::CheckboxOptions().Tooltip("Toggles a setting that applies now."));
    soh->AddWidget(path, "Suspended Game", WIDGET_SEPARATOR_TEXT);
    soh->AddWidget(path, "Suspended Game Note", WIDGET_TEXT)
        .RaceDisable(false)
        .HideInSearch(true)
        .PreFunc([](WidgetInfo& note) {
            SohGui::SohMenu::ApplyPresentationNote(note, SohGui::SOH_MENU_PRESENT_INACTIVE_GAME);
        })
        .Options(UIWidgets::TextOptions().Color(UIWidgets::Colors::Gray));
    soh->AddWidget(path, kProbeSuspendedRow, WIDGET_CHECKBOX)
        .ValuePointer(&gProbeSuspended)
        .PreFunc([](WidgetInfo& info) {
            SohGui::SohMenu::ApplyPresentation(info, info.name, SohGui::SOH_MENU_PRESENT_INACTIVE_GAME,
                                               "Majora's Mask is suspended");
        })
        .Options(UIWidgets::CheckboxOptions().Tooltip("Toggles a setting that takes effect in Majora's Mask."));
    path.column = SECTION_COLUMN_2;
    soh->AddWidget(path, "Gated by Capability", WIDGET_SEPARATOR_TEXT);
    soh->AddWidget(path, "Gated by Capability Note", WIDGET_TEXT)
        .RaceDisable(false)
        .HideInSearch(true)
        .PreFunc(SohGui::SohMenu::CapabilityNote(SohGui::SOH_MENU_CAP_COMBO_PAIRED))
        .Options(UIWidgets::TextOptions().Color(UIWidgets::Colors::Gray));
    soh->AddWidget(path, kProbeGatedRow, WIDGET_CHECKBOX)
        .ValuePointer(&gProbeGated)
        .PreFunc(SohGui::SohMenu::CapabilityGate(SohGui::SOH_MENU_CAP_COMBO_PAIRED))
        .Options(UIWidgets::CheckboxOptions().Tooltip("Toggles a setting that needs a paired world."));
    soh->AddWidget(path, "Frozen at Creation", WIDGET_SEPARATOR_TEXT);
    soh->AddWidget(path, "Frozen at Creation Note", WIDGET_TEXT)
        .RaceDisable(false)
        .HideInSearch(true)
        .PreFunc(
            [](WidgetInfo& note) { SohGui::SohMenu::ApplyPresentationNote(note, SohGui::SOH_MENU_PRESENT_FROZEN); })
        .Options(UIWidgets::TextOptions().Color(UIWidgets::Colors::Gray));
    soh->AddWidget(path, kProbeDecidedRow, WIDGET_CHECKBOX)
        .ValuePointer(&gProbeDecided)
        .PreFunc([](WidgetInfo& info) {
            SohGui::SohMenu::ApplyPresentation(info, info.name, SohGui::SOH_MENU_PRESENT_FROZEN, nullptr);
        })
        .Options(UIWidgets::CheckboxOptions().Tooltip("Toggles a setting that was fixed when the world was created."));

    // Resolved after every AddWidget (a push_back can reallocate a column).
    const std::string where = std::string("Combo/") + kRowStatesSidebar;
    for (auto& column : entries.at("Combo").sidebars.at(kRowStatesSidebar).columnWidgets) {
        for (WidgetInfo& row : column) {
            probeRows.push_back(&row);
            if (IsInteractive(row.type)) {
                registeredNames[&row] = { where, row.name };
            } else if (row.type == WIDGET_TEXT) {
                textRows[&row] = where;
            }
        }
    }
}

/**
 * #747: Combo > Majora's Mask's group note, on a harness-only page built by the
 * shipped page's own builder (SohGui::AddMmEnhancementRows) over a synthetic
 * manifest that holds two Dormant rows and a Partial one, a non-live row in each
 * of its two groups. Every shipped row is Live, so
 * without this page neither the page's disabled rows nor its gray notes would
 * ever reach a pixel. Installed and removed around its own captures, like the
 * row-state probe, and its rows join R5/R6 the same way.
 */
void Session::InstallMmNoteProbe() {
    auto soh = std::dynamic_pointer_cast<SohGui::SohMenu>(menu);
    auto& entries = MenuEntries(*menu);
    if (soh == nullptr || !entries.contains("Combo") || entries.at("Combo").sidebars.contains(kMmNoteSidebar)) {
        return;
    }
    // Two columns, the shipped page's count.
    soh->AddSidebarEntry("Combo", kMmNoteSidebar, 2);
    WidgetPath path = { "Combo", kMmNoteSidebar, SECTION_COLUMN_1 };
    SohGui::AddMmEnhancementRows(*soh, path, kMmNoteManifest, sizeof(kMmNoteManifest) / sizeof(kMmNoteManifest[0]));

    const std::string where = std::string("Combo/") + kMmNoteSidebar;
    for (auto& column : entries.at("Combo").sidebars.at(kMmNoteSidebar).columnWidgets) {
        for (WidgetInfo& row : column) {
            probeRows.push_back(&row);
            if (IsInteractive(row.type)) {
                registeredNames[&row] = { where, row.name };
            } else if (row.type == WIDGET_TEXT) {
                textRows[&row] = where;
            }
        }
    }
}

void Session::RemoveMmNoteProbe() {
    for (const WidgetInfo* row : probeRows) {
        registeredNames.erase(row);
        textRows.erase(row);
    }
    probeRows.clear();
    auto& entries = MenuEntries(*menu);
    if (!entries.contains("Combo")) {
        return;
    }
    MainMenuEntry& combo = entries.at("Combo");
    combo.sidebars.erase(kMmNoteSidebar);
    combo.sidebarOrder.erase(std::remove(combo.sidebarOrder.begin(), combo.sidebarOrder.end(), kMmNoteSidebar),
                             combo.sidebarOrder.end());
}

void Session::RemoveRowStateProbe() {
    // The R5/R6 hits these rows produced are already in dynamicHits; the rows
    // themselves are about to be destroyed.
    for (const WidgetInfo* row : probeRows) {
        registeredNames.erase(row);
        textRows.erase(row);
    }
    probeRows.clear();
    auto& entries = MenuEntries(*menu);
    if (!entries.contains("Combo")) {
        return;
    }
    MainMenuEntry& combo = entries.at("Combo");
    combo.sidebars.erase(kRowStatesSidebar);
    combo.sidebarOrder.erase(std::remove(combo.sidebarOrder.begin(), combo.sidebarOrder.end(), kRowStatesSidebar),
                             combo.sidebarOrder.end());
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

/** The capture cropped to its content rectangle plus a 4 px margin (defined with the composites). */
bool CropContent(const Capture& c, UiImage* outImg);

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
    // Assert 3: not blank. A toast is a small window in a corner by design, so its
    // share of the whole frame says nothing about it (SoH's own "Game autosaved"
    // covers 0.3% of a 1280x800 frame); it is measured inside its own rectangle,
    // which CaptureToast's oracle has already required to be exactly one window.
    uint32_t modal = 0;
    UiImage toastCrop = { 0, 0, nullptr };
    const UiImage* measured = &c.image;
    if (p.kind == Kind::TOAST && CropContent(c, &toastCrop)) {
        measured = &toastCrop;
    }
    UiImage_Stats(measured, 4096, &modal, &c.nonBlank, &c.distinct);
    UiImage_Free(&toastCrop);
    if (c.nonBlank < 0.005 || c.distinct < 16) {
        c.status = "fail";
        c.reason = "the capture is blank (" + std::to_string(c.nonBlank * 100.0) + "% non-modal pixels, " +
                   std::to_string(c.distinct) + " colours)";
        return;
    }
    // Assert 6: the text holds a string only this page's body draws (menu
    // pages; see PickBodyText), or the pane's own title (panes, overlay, modal).
    c.textHash = Ui_Fnv1a64(c.text.data(), c.text.size(), UI_FNV1A64_OFFSET);
    if (p.kind == Kind::MENU_PAGE && p.bodyText.empty()) {
        c.status = "fail";
        c.reason = "no registered row of this page yields a string only its body draws (every candidate has a "
                   "PreFunc or is a substring of a header or sidebar label), so reachability by text is unprovable";
        return;
    }
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
    if (p.rowStateProbe) {
        InstallRowStateProbe();
    }
    if (p.mmNoteProbe) {
        InstallMmNoteProbe();
    }
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
        // RSBS_UI_SNAPSHOT_SABOTAGE=throw: the first project page's first row
        // throws once, mid-draw, inside the menu window and its section child --
        // the shape of the Settings > General std::map::at this harness met.
        WidgetInfo* throwRow = nullptr;
        WidgetFunc throwSaved;
        if (opt.Sabotaged("throw") && !sabotageThrowArmed && p.origin == Origin::RSBS) {
            throwRow = FindRow(*menu, p.header, p.sidebar, [](const WidgetInfo&) { return true; });
            if (throwRow != nullptr) {
                sabotageThrowArmed = true;
                throwSaved = throwRow->preFunc;
                auto fired = std::make_shared<bool>(false);
                throwRow->preFunc = [throwSaved, fired](WidgetInfo& info) {
                    if (!*fired) {
                        *fired = true;
                        throw std::runtime_error("RSBS_UI_SNAPSHOT_SABOTAGE=throw");
                    }
                    if (throwSaved) {
                        throwSaved(info);
                    }
                };
            }
        }
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
            const bool measure = scrollIndex == 0;
            Record(std::move(c));
            if (measure && captures.back().status == "pass") {
                // R9, once per state, on the settled first view: the layout is
                // horizontal, so the scroll position does not change the answer.
                MeasureLabelFit(p, state);
            }
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
        if (throwRow != nullptr) {
            throwRow->preFunc = throwSaved;
        }

        // Hover variants: the tooltip is part of the page's look.
        for (const std::string& hv : p.hovers) {
            const auto namedRow = p.hoverRows.find(hv);
            const bool byName = namedRow != p.hoverRows.end();
            const auto namedState = p.hoverRowState.find(hv);
            const std::string& hoverState = namedState != p.hoverRowState.end() ? namedState->second : p.states.front();
            if (byName ? (state != hoverState) : ((hv.rfind("frozen-", 0) == 0) != (state == "frozen"))) {
                continue;
            }
            if (!p.hoverStates.empty() &&
                std::find(p.hoverStates.begin(), p.hoverStates.end(), state) == p.hoverStates.end()) {
                continue;
            }
            const std::string variant = VariantName(state, "hover-" + hv);
            if (!Selected(p, variant)) {
                continue;
            }
            std::string label;
            if (byName) {
                label = namedRow->second;
            } else {
                ComboSettingId targetId = (hv == "direction") ? COMBO_SETTING_DIRECTION : COMBO_SETTING_GOAL;
                label = Combo_ComboSettingLabel(targetId);
            }
            WidgetInfo* row = FindRow(*menu, p.header, p.sidebar, [&](const WidgetInfo& w) {
                return byName ? w.name == label : w.name.find(label) != std::string::npos;
            });
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
            // Wrap the row's preFunc to record the tooltip it AUTHORED this frame:
            // the one the oracle then looks for, every line of it. Recorded before
            // anything later in MenuDrawItem (a race lockout replaces a disabled
            // tooltip with its own) could change it, so a replaced tooltip is red.
            // RSBS_UI_SNAPSHOT_SABOTAGE=hover-first-line then cuts what the row
            // DRAWS to the first line, which must turn the hover red.
            WidgetFunc savedPre = row->preFunc;
            const char* savedTooltip = row->options != nullptr ? row->options->tooltip : nullptr;
            const bool firstLineOnly = opt.Sabotaged("hover-first-line");
            std::string authoredTip;
            std::string drawnTip;
            Ship::Menu* hoverMenu = menu.get();
            row->preFunc = [savedPre, savedTooltip, firstLineOnly, hoverMenu, &authoredTip,
                            &drawnTip](WidgetInfo& info) {
                if (info.options != nullptr) {
                    // Undo last frame's sabotage first: a row whose PreFunc does not
                    // rewrite its tooltip would otherwise read the cut one back as
                    // authored, and the sabotage would prove nothing.
                    info.options->tooltip = savedTooltip;
                }
                if (savedPre) {
                    savedPre(info);
                }
                if (info.options == nullptr) {
                    return;
                }
                const bool useDisabled = info.options->disabled && info.options->disabledTooltip != nullptr &&
                                         info.options->disabledTooltip[0] != '\0';
                const char* tip = useDisabled ? info.options->disabledTooltip : info.options->tooltip;
                authoredTip = tip != nullptr ? tip : "";
                if (!info.activeDisables.empty()) {
                    // SoH's disabledMap shape: MenuDrawItem builds this tooltip
                    // right after the PreFunc returns (Menu.cpp), in exactly
                    // this form.
                    authoredTip = "This setting is disabled because: \n";
                    for (auto option : info.activeDisables) {
                        authoredTip += std::string("\n- ") + hoverMenu->GetDisabledMap().at(option).reason;
                    }
                }
                if (firstLineOnly) {
                    drawnTip = authoredTip.substr(0, authoredTip.find('\n'));
                    if (useDisabled) {
                        info.options->disabledTooltip = drawnTip.c_str();
                    } else {
                        info.options->tooltip = drawnTip.c_str();
                    }
                }
            };
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
            row->preFunc = savedPre;
            if (row->options != nullptr) {
                row->options->tooltip = savedTooltip;
            }
            Finish(c, p);
            // The row-state probe's hovers are the four presentation states'
            // disabled rows: each must show SoH's disabled shape (a), and no
            // tracker number (ADR 0004's 2026-09-27 amendment). So must a named
            // hover the page lists as disabled (Combo > MM Randomizer's frozen and
            // capability-blocked rows).
            if (HoverVerdict(c, label, authoredTip) && (p.rowStateProbe || p.disabledHovers.contains(hv))) {
                DisabledShapeVerdict(c, label, authoredTip);
            }
            Record(std::move(c));
            // Put the pointer away and re-settle so the next capture has no hover.
            PumpFrame(nullptr, false, nullptr, why);
        }

        // Seam hovers on a menu page: a row drawn inside a custom widget through
        // the combo_ui seam (Combo > MM Tricks' trick rows), found by the label
        // it hands the seam's rect recorder, scrolled into view in whichever
        // window drew it (the tricks table's own child), then hovered.
        CapturePaneHovers(p, state);
        CheckTrickCensus(p, state);
        LeaveState(p, state);
    }
    if (p.rowStateProbe) {
        RemoveRowStateProbe();
    }
    if (p.mmNoteProbe) {
        RemoveMmNoteProbe();
    }
}

/**
 * A hover capture's verdict, shared by menu rows and pane rows: every authored
 * line of the tooltip the row set up (a disabled row shows its disabled tooltip
 * instead), each as a prefix that sits on that line's first wrapped line, must
 * be in the capture's text. All of them, not the first: SoH's disabled shape
 * opens every disabled row's tooltip with the same "This setting is disabled
 * because:", so only the reason line shows that THIS row's tooltip was drawn.
 * Each line must also be ABSENT from the same state's captures without the
 * pointer, or its presence proves nothing about a tooltip. Returns false when the
 * capture had already failed (nothing was judged).
 */
bool Session::HoverVerdict(Capture& c, const std::string& label, const std::string& authoredTip) {
    c.hoverLines = TooltipLinePrefixes(authoredTip);
    c.hoverText = c.hoverLines.empty() ? std::string() : c.hoverLines.front();
    if (c.status != "pass") {
        return false;
    }
    if (c.hoverLines.empty()) {
        c.status = "fail";
        c.reason = "the hovered row \"" + label + "\" has no tooltip to show";
    }
    for (const std::string& line : c.hoverLines) {
        if (c.status != "pass") {
            break;
        }
        if (c.text.find(line) == std::string::npos) {
            c.status = "fail";
            c.reason = "the hover capture does not show its row's tooltip: \"" + line + "\" is not in its text";
            break;
        }
        for (const Capture& other : captures) {
            if (other.id == c.id && other.state == c.state && other.hover.empty() &&
                other.text.find(line) != std::string::npos) {
                c.status = "fail";
                c.reason = "\"" + line + "\" is also in " + other.variant +
                           ", captured without the pointer, so it does not prove a tooltip";
                break;
            }
        }
    }
    for (const std::string& line : c.hoverLines) {
        if (c.text.find(line) != std::string::npos) {
            c.found.push_back(line);
        } else {
            c.missing.push_back(line);
        }
    }
    return true;
}

/**
 * A hovered DISABLED row's tooltip must be SoH's disabled shape (a) ("This
 * setting is disabled because:" then "- <Reason>") and carry no tracker number
 * (ADR 0004's 2026-09-27 amendment). Checked on the AUTHORED tooltip, which
 * HoverVerdict has just proved is drawn; a no-op on a capture that already failed.
 */
void Session::DisabledShapeVerdict(Capture& c, const std::string& label, const std::string& authoredTip) {
    if (c.status != "pass") {
        return;
    }
    const std::string head = "This setting is disabled because:";
    bool number = false;
    for (std::size_t k = 0; k + 1 < authoredTip.size(); k++) {
        if (authoredTip[k] == '#' && authoredTip[k + 1] >= '0' && authoredTip[k + 1] <= '9') {
            number = true;
        }
    }
    if (authoredTip.compare(0, head.size(), head) != 0) {
        c.status = "fail";
        c.reason = "the hovered row \"" + label + "\" does not show SoH's disabled tooltip (\"" + head +
                   "\" then \"- <Reason>\"): \"" + authoredTip + "\"";
    } else if (number) {
        c.status = "fail";
        c.reason = "the hovered row \"" + label + "\" prints a tracker number in its tooltip: \"" + authoredTip +
                   "\"; the issue belongs in the capability's record, not in the pixels";
    }
}

// ---- panes, overlay, modal ------------------------------------------------------------------

/** The combo_ui rect recorder's sink for one seam hover: the row whose label matches. */
struct PaneHoverProbe {
    std::string label;
    bool found = false;
    ImRect rect;
    std::string tooltip;
    // The window the row was drawn in: the pane itself, or the child a menu
    // page's custom list scrolls in (Combo > MM Tricks' table columns).
    ImGuiWindow* window = nullptr;
};

void PaneHoverRecord(void* user, const char* label, const char* tooltip, float minX, float minY, float maxX,
                     float maxY) {
    PaneHoverProbe* probe = static_cast<PaneHoverProbe*>(user);
    if (label == nullptr || probe->label != label) {
        return;
    }
    probe->found = true;
    probe->rect = ImRect(minX, minY, maxX, maxY);
    probe->tooltip = tooltip != nullptr ? tooltip : "";
    probe->window = GImGui->CurrentWindow;
}

/**
 * The combo_ui rect recorder's sink for Combo > MM Tricks' row census: every
 * trick name the list drew in one frame, the column child that drew it
 * (DrawMmTrickList's "ChildMmTricksDisabled" / "ChildMmTricksEnabled"), and the
 * tooltip a hover would show. A report from any other window is not a trick row
 * and is left out.
 */
struct TrickCensusRow {
    std::string label;
    std::string tooltip;
    bool enabledColumn = false;
};

/**
 * The combo_ui rect recorder's sink for the Combo Tracker's search boxes (#458
 * U4): how many times one frame reported the "##checkSearch" field and its
 * eraser, with a non-empty rect. Each game panel's open Checks list draws one
 * of each, so a seam widget that stops reporting (and so cannot be found or
 * hovered by the harness) turns the progress capture red.
 */
struct SearchRectCensus {
    int fields = 0;
    int erasers = 0;
    int skipButtons = 0; // "##skip<id>" (#458 U5): drawn on the LIVE panel only
};

void SearchRectRecord(void* user, const char* label, const char*, float minX, float minY, float maxX, float maxY) {
    SearchRectCensus* census = static_cast<SearchRectCensus*>(user);
    if (label == nullptr || maxX <= minX || maxY <= minY) {
        return;
    }
    if (std::strcmp(label, "##checkSearch") == 0) {
        census->fields++;
    } else if (std::strcmp(label, "##checkSearch##eraser") == 0) {
        census->erasers++;
    } else if (std::strncmp(label, "##skip", 6) == 0) {
        census->skipButtons++;
    }
}

void TrickCensusRecord(void* user, const char* label, const char* tooltip, float, float, float, float) {
    const ImGuiWindow* w = GImGui->CurrentWindow;
    const char* name = w != nullptr && w->Name != nullptr ? w->Name : "";
    const bool enabled = std::strstr(name, "ChildMmTricksEnabled") != nullptr;
    if (!enabled && std::strstr(name, "ChildMmTricksDisabled") == nullptr) {
        return;
    }
    static_cast<std::vector<TrickCensusRow>*>(user)->push_back(
        TrickCensusRow{ label != nullptr ? label : "", tooltip != nullptr ? tooltip : "", enabled });
}

const char* PaneCvar(const std::string& window) {
    if (window == ComboGui::kComboSpoilerWindowName) {
        return ComboGui::kComboSpoilerVisibilityCVar;
    }
    if (window == ComboGui::kComboTrackerWindowName) {
        return ComboGui::kComboTrackerVisibilityCVar;
    }
    if (window == ComboGui::kComboItemTrackerWindowName) {
        return ComboGui::kComboItemTrackerVisibilityCVar;
    }
    return nullptr;
}

/**
 * The seam hovers of @p p in @p state: each row is found by the label it hands
 * the combo_ui seam, whose rect recorder reports where it drew, in which window,
 * and the tooltip a hover shows; it is scrolled into view in that window and
 * hovered the way a menu row is. Shared by panes and by menu pages whose rows
 * live inside a custom widget (Combo > MM Tricks).
 */
void Session::CapturePaneHovers(const PageSpec& p, const std::string& state) {
    for (const PageSpec::PaneHover& ph : p.paneHovers) {
        if (ph.state != state) {
            continue;
        }
        const std::string variant = VariantName(state, "hover-" + ph.name);
        if (!Selected(p, variant)) {
            continue;
        }
        Capture c;
        c.id = p.id;
        c.state = state;
        c.variant = variant;
        c.hover = ph.label;
        PaneHoverProbe probe;
        probe.label = ph.label;
        ComboUi_SetRectRecorder(PaneHoverRecord, &probe);
        std::string why;
        // Rows are brought into view on both axes: at a narrow profile the
        // tricks table's child clips a long row's name off its right edge (the
        // child scrolls horizontally, as SoH's does), and a pointer there would
        // hover nothing. The horizontal scroll is put back after the capture.
        ImGuiWindow* scroller = nullptr;
        float scrollXBefore = -1.0f;
        ImRect clip;
        for (int i = 0; i < 6; i++) {
            probe.found = false;
            probe.window = nullptr;
            if (!PumpFrame(nullptr, false, nullptr, why)) {
                break;
            }
            scroller = probe.window != nullptr ? probe.window : ImGui::FindWindowByName(p.window.c_str());
            if (!probe.found || scroller == nullptr) {
                continue;
            }
            clip = scroller->InnerClipRect;
            const bool yIn = probe.rect.Min.y >= clip.Min.y && probe.rect.Max.y <= clip.Max.y;
            const bool xIn = probe.rect.Min.x >= clip.Min.x &&
                             probe.rect.Min.x + std::min(probe.rect.GetWidth(), 48.0f) <= clip.Max.x;
            if (yIn && xIn) {
                break;
            }
            if (!yIn) {
                ImGui::SetScrollY(scroller, std::max(0.0f, scroller->Scroll.y + probe.rect.Min.y - clip.Min.y -
                                                               clip.GetHeight() / 3.0f));
            }
            if (!xIn && scroller->ScrollMax.x > 0.0f) {
                if (scrollXBefore < 0.0f) {
                    scrollXBefore = scroller->Scroll.x;
                }
                ImGui::SetScrollX(scroller, std::max(0.0f, scroller->Scroll.x + probe.rect.Min.x - clip.Min.x - 8.0f));
            }
        }
        if (!probe.found) {
            c.status = "fail";
            c.reason = "no row labelled \"" + ph.label + "\" was drawn through the combo_ui seam";
        } else {
            // The middle of the part of the row its window shows.
            ImRect shown = probe.rect;
            if (scroller != nullptr) {
                shown.ClipWith(clip);
            }
            gHooks.hoverPos =
                shown.GetWidth() > 0.0f && shown.GetHeight() > 0.0f ? shown.GetCenter() : probe.rect.GetCenter();
            if (Settle(c, true, nullptr, nullptr)) {
                Oracle(p, c);
            }
        }
        if (scroller != nullptr && scrollXBefore >= 0.0f) {
            ImGui::SetScrollX(scroller, scrollXBefore);
        }
        ComboUi_SetRectRecorder(nullptr, nullptr);
        Finish(c, p);
        if (HoverVerdict(c, ph.label, probe.tooltip) && ph.disabled) {
            DisabledShapeVerdict(c, ph.label, probe.tooltip);
        }
        Record(std::move(c));
        // Put the pointer away and re-settle so the next capture has no hover.
        PumpFrame(nullptr, false, nullptr, why);
    }
}

/**
 * Combo > MM Tricks' row census, in @p state: one frame of the page drawn with
 * the rect recorder listening, then every one of MM's trick descriptors must
 * have been drawn exactly once, in the column its value puts it in (Enabled
 * when on), with the tooltip its row state gives it (its description while
 * live; SoH's disabled shape around the model's reason while blocked or
 * frozen), and nothing drawn that is not in the table. Every tag of the filter
 * bar starts shown and every area starts open, so the list the page opens on is
 * the whole table. The hovers read four rows; this is what turns a draw change
 * that drops, doubles or misfiles a trick (a visibility or area-walk bug) red.
 */
void Session::CheckTrickCensus(const PageSpec& p, const std::string& state) {
    if (!p.trickCensus) {
        return;
    }
    const std::string where = p.id + " (" + (state.empty() ? std::string("default") : state) + ")";
    std::vector<TrickCensusRow> drawn;
    ComboUi_SetRectRecorder(TrickCensusRecord, &drawn);
    std::string why;
    // Two frames, the census being the last one's alone: the first may still
    // carry a hover or scroll the previous capture left.
    for (int i = 0; i < 2; i++) {
        drawn.clear();
        if (!PumpFrame(nullptr, false, nullptr, why)) {
            break;
        }
    }
    ComboUi_SetRectRecorder(nullptr, nullptr);
    const int count = Combo_MMTrickCount();
    if (count <= 0) {
        Fail(where + ": the trick census found no MM trick table to count");
        return;
    }
    // Per label, how often each column should and did draw it (a label shared by
    // two descriptors counts twice; the table's labels are unique today).
    std::map<std::string, std::pair<int, int>> want; // label -> (Disabled column, Enabled column)
    std::map<std::string, std::pair<int, int>> got;
    std::map<std::string, const ComboMMTrickDesc*> byLabel;
    for (int i = 0; i < count; i++) {
        const ComboMMTrickDesc* d = Combo_MMTrickAt(i);
        if (d == nullptr || d->label == nullptr) {
            Fail(where + ": trick descriptor " + std::to_string(i) + " has no label to find it by");
            continue;
        }
        if (Combo_MMTrickGetValue(d)) {
            want[d->label].second++;
        } else {
            want[d->label].first++;
        }
        byLabel.emplace(d->label, d);
    }
    int problems = 0;
    int enabledRows = 0;
    auto report = [&](const std::string& what) {
        if (problems++ < 12) {
            Fail(where + ": " + what);
        }
    };
    for (const TrickCensusRow& row : drawn) {
        if (row.enabledColumn) {
            got[row.label].second++;
            enabledRows++;
        } else {
            got[row.label].first++;
        }
        const auto known = byLabel.find(row.label);
        if (known == byLabel.end()) {
            report("the trick list drew \"" + row.label + "\", which is not in MM's trick table");
            continue;
        }
        const ComboMMTrickDesc* d = known->second;
        const char* reason = "";
        const ComboMMRowState rowState = Combo_MMOptionsPage_TrickState(d, &reason);
        const std::string expected = rowState == COMBO_MM_ROW_LIVE
                                         ? std::string(d->tooltip != nullptr ? d->tooltip : "")
                                         : std::string(SohGui::SohMenu::DisabledTooltip(reason));
        if (row.tooltip != expected) {
            report("the trick \"" + row.label + "\" shows the tooltip \"" + row.tooltip + "\"; its row state (" +
                   std::to_string((int)rowState) + ") gives \"" + expected + "\"");
        }
    }
    for (const auto& [label, expect] : want) {
        const auto g = got.find(label);
        const std::pair<int, int> seen = g != got.end() ? g->second : std::make_pair(0, 0);
        if (seen != expect) {
            report("the trick \"" + label + "\" was drawn " + std::to_string(seen.first) + "x in Disabled Tricks and " +
                   std::to_string(seen.second) + "x in Enabled Tricks; its value puts it " +
                   std::to_string(expect.first) + "x / " + std::to_string(expect.second) + "x");
        }
    }
    if (problems > 12) {
        Fail(where + ": the trick census found " + std::to_string(problems) + " problems in all (12 shown)");
    }
    if (enabledRows == 0 || (size_t)enabledRows == drawn.size()) {
        Fail(where + ": the trick census drew rows in only one column; the state must put rows in both, or one "
                     "column's walk goes uncounted");
    }
    printf("[UI-SNAPSHOT] trick census %s: %d descriptors, %zu rows drawn (%d Enabled Tricks, %zu Disabled Tricks), "
           "%d problems\n",
           where.c_str(), count, drawn.size(), enabledRows, drawn.size() - (size_t)enabledRows, problems);
}

/**
 * The no-scroll pane's oracle: every pixel of its contents is in its one view.
 * Fails the capture when the window clips its own contents (ScrollMax > 0 in
 * either direction: a size constraint held it smaller than what it drew) or
 * when the window runs past the game window's edge.
 */
void FitOracle(const ImGuiWindow* w, Capture& c) {
    char why[256];
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float right = viewport->WorkPos.x + viewport->WorkSize.x;
    const float bottom = viewport->WorkPos.y + viewport->WorkSize.y;
    if (w->ScrollMax.y > 0.5f || w->ScrollMax.x > 0.5f) {
        snprintf(why, sizeof(why),
                 "the pane does not scroll, and %.0f px of its contents below (%.0f px to the right of) its edge "
                 "cannot be seen",
                 w->ScrollMax.y, w->ScrollMax.x);
    } else if (w->Pos.x + w->Size.x > right + 0.5f || w->Pos.y + w->Size.y > bottom + 0.5f) {
        snprintf(why, sizeof(why), "the pane (%.0f,%.0f %.0fx%.0f) runs past the game window's edge (%.0fx%.0f)",
                 w->Pos.x, w->Pos.y, w->Size.x, w->Size.y, right, bottom);
    } else {
        return;
    }
    c.status = "fail";
    c.reason = why;
}

void Session::CaptureWindowPage(const PageSpec& p) {
    // Ours read their visibility CVar LIVE in Draw() (they override it), so
    // setting the CVar is what opens them; Show() would do nothing. An SoH pane
    // is the other way round: Ship::GuiWindow latches the CVar in its ctor and
    // Draw() tests IsVisible(), so it is opened with Show() and closed with Hide().
    const bool sohPane = p.origin == Origin::SOH_REFERENCE;
    const char* cvar = sohPane ? nullptr : PaneCvar(p.window);
    auto guiWindow = gui->GetGuiWindow(p.window);
    for (const std::string& state : p.states) {
        const std::string base = VariantName(state, "");
        if (!Selected(p, base) && !Selected(p, VariantName(state, "scroll0"))) {
            continue;
        }
        if ((!sohPane && cvar == nullptr) || guiWindow == nullptr) {
            Capture c;
            c.id = p.id;
            c.state = state;
            c.variant = base;
            if (p.needsRom && romFree) {
                c.status = "skip";
                c.reason = "needs oot.o2r";
            } else {
                c.status = "fail";
                c.reason = "the pane \"" + p.window + "\" is not registered";
            }
            c.spec = &p;
            Record(std::move(c));
            continue;
        }
        EnterState(p, state);
        menu->Hide();
        if (sohPane) {
            guiWindow->Show();
        } else {
            CVarSetInteger(cvar, 1);
        }
        const bool checksOpen = (state == "progress" && p.window == ComboGui::kComboTrackerWindowName);
        // The pane's .txt is VISIBLE-ONLY (see HookState::unclipLog), so a state's
        // text is asserted against what some captured view actually showed.
        gHooks.unclipLog = false;
        // The pane scrolls as a whole (it has no section children), and it is
        // stepped the way a menu page's columns are, one view minus a 48 px
        // overlap per capture, so a long pane is captured to its end instead of
        // to its first fold.
        int scrollIndex = 0;
        for (;;) {
            Capture c;
            c.id = p.id;
            c.state = state;
            c.scrollIndex = scrollIndex;
            c.variant = VariantName(state, "scroll" + std::to_string(scrollIndex));
            auto before = [&]() {
                if (checksOpen) {
                    ImGuiWindow* tw = ImGui::FindWindowByName(p.window.c_str());
                    if (tw != nullptr) {
                        // Both games' Checks lists: OoT's synthetic world and
                        // MM's authored save, whose collected crossing host names
                        // the crossed OoT item (#458 U4).
                        tw->StateStorage.SetInt(TrackerChecksId(tw, (int)GAME_OOT), 1);
                        tw->StateStorage.SetInt(TrackerChecksId(tw, (int)GAME_MM), 1);
                    }
                }
                if (scrollIndex == 0) {
                    ImGuiWindow* w = ImGui::FindWindowByName(p.window.c_str());
                    if (w != nullptr) {
                        ImGui::SetScrollY(w, 0.0f);
                    }
                }
            };
            if (Settle(c, false, nullptr, before)) {
                Oracle(p, c);
            }
            ImGuiWindow* w = ImGui::FindWindowByName(p.window.c_str());
            if (w != nullptr) {
                c.scrollY = w->Scroll.y;
                c.scrollMax = w->ScrollMax.y;
            }
            Finish(c, p);
            if (!p.scroll && w != nullptr && c.status == "pass") {
                // A pane that does not scroll for its player (p.scroll false: a
                // floating overlay has no scrollbar and takes no input) is seen
                // in its first view only, so that view must hold all of it:
                // nothing clipped at the window's edge, and the window inside
                // the game window.
                FitOracle(w, c);
            }
            const bool unfinished = p.scroll && w != nullptr && c.status == "pass" && c.scrollY + 0.5f < c.scrollMax;
            const bool more = unfinished && scrollIndex < kMaxScrollSteps && !opt.Sabotaged("no-scroll");
            if (unfinished && !more) {
                c.reason = opt.Sabotaged("no-scroll")
                               ? "scroll sequence stopped at the first view (RSBS_UI_SNAPSHOT_SABOTAGE=no-scroll)"
                               : "scroll sequence truncated at " + std::to_string(kMaxScrollSteps + 1) + " views";
            }
            Record(std::move(c));
            if (!more) {
                break;
            }
            const float step = std::max(64.0f, w->InnerRect.GetHeight() - 48.0f);
            ImGui::SetScrollY(w, std::min(w->ScrollMax.y, w->Scroll.y + step));
            scrollIndex++;
        }

        if (checksOpen) {
            // Every seam widget reports its rectangle (docs/ui-style-guide.md
            // section 10), the search box and its eraser included: one frame with
            // both Checks lists open must report one of each per game panel.
            SearchRectCensus census;
            ComboUi_SetRectRecorder(SearchRectRecord, &census);
            std::string why;
            PumpFrame(nullptr, false, nullptr, why);
            ComboUi_SetRectRecorder(nullptr, nullptr);
            printf("[UI-SNAPSHOT] search census %s (%s): %d search fields, %d eraser buttons reported\n", p.id.c_str(),
                   state.c_str(), census.fields, census.erasers);
            if (census.fields != 2 || census.erasers != 2) {
                Fail(p.id + " (" + state + "): the Checks lists' search boxes reported " +
                     std::to_string(census.fields) + " fields and " + std::to_string(census.erasers) +
                     " eraser buttons to the combo_ui rect recorder; each game panel draws one of each");
            }
            // The skip toggle (#458 U5): OoT's LIVE panel draws one button per
            // check of the seed not yet found; MM's panel, the frozen shadow in
            // this state, draws none although its authored save has open checks.
            printf("[UI-SNAPSHOT] skip census %s (%s): %d skip buttons reported (want %d, all OoT's live panel)\n",
                   p.id.c_str(), state.c_str(), census.skipButtons, kSnapshotOoTSkipButtons);
            if (census.skipButtons != kSnapshotOoTSkipButtons) {
                Fail(p.id + " (" + state + "): the Checks lists reported " + std::to_string(census.skipButtons) +
                     " skip buttons; OoT's live panel draws " + std::to_string(kSnapshotOoTSkipButtons) +
                     " and MM's snapshot panel none");
            }
        }

        CapturePaneHovers(p, state);
        gHooks.unclipLog = true;
        if (sohPane) {
            guiWindow->Hide();
        } else {
            CVarSetInteger(cvar, 0);
        }
        LeaveState(p, state);
    }
}

/** The menu page every over-menu variant sits on: one of ours, drawn on a ROM-free run too. */
PageSpec OverMenuPage() {
    PageSpec under;
    under.header = "Combo";
    under.sidebar = "Cross-Game Rules";
    return under;
}

// The creation overlay's dim window (CreationProgressOverlay.cpp, kDimWindowName).
// Named here rather than exported: a rename turns the over-menu oracle red by
// name, which is the point of it.
constexpr const char* kOverlayDimWindow = "##CreationProgressDim";

/**
 * THE OVER-MENU ORACLE: is the menu under this box dimmed the way a modal dims it?
 *
 * @p bare is the same menu page settled with nothing over it; @p c is the capture
 * with the dimming surface over it, its contentRect already the box (Oracle).
 * Three requirements, each failing the capture by name:
 *   1. the menu was drawn ("Main Menu" active in the dimmed frame), and the bare
 *      frame is not nearly uniform outside the box (so there was a menu to dim);
 *   2. when @p dimWindow is named (the overlay draws its own dim), that window was
 *      drawn and sits above the menu and below the box in the display order;
 *   3. every pixel outside the box equals the bare pixel blended with the style's
 *      ImGuiCol_ModalWindowDimBg, as the GPU blends it, within 2 per channel.
 * A real BeginPopupModal passes the same check with @p dimWindow null (ImGui draws
 * that dim itself), which is what makes "dims the way a modal dims" a measured
 * claim rather than a reading of the picture.
 */
void DimOracle(const UiImage& bare, Capture& c, const char* dimWindow, const char* boxWindow) {
    if (c.status != "pass") {
        return;
    }
    ImGuiContext& g = *GImGui;
    auto fail = [&c](const std::string& why) {
        c.status = "fail";
        c.reason = why;
    };
    ImGuiWindow* menuWindow = ImGui::FindWindowByName("Main Menu");
    if (menuWindow == nullptr || !menuWindow->Active) {
        fail("the menu under the dim was not drawn (\"Main Menu\" not active)");
        return;
    }
    auto orderOf = [&g](ImGuiWindow* w) {
        for (int i = 0; i < g.Windows.Size; i++) {
            if (g.Windows[i] == w) {
                return i;
            }
        }
        return -1;
    };
    if (dimWindow != nullptr) {
        ImGuiWindow* dim = ImGui::FindWindowByName(dimWindow);
        ImGuiWindow* box = ImGui::FindWindowByName(boxWindow);
        if (dim == nullptr || !dim->Active) {
            fail(std::string("the dim window \"") + dimWindow + "\" was not drawn");
            return;
        }
        const int m = orderOf(menuWindow);
        const int d = orderOf(dim);
        const int b = box == nullptr ? -1 : orderOf(box);
        if (!(m < d && d < b)) {
            fail("display order is menu " + std::to_string(m) + ", dim " + std::to_string(d) + ", box " +
                 std::to_string(b) + " (back to front); the dim must sit above the menu and below the box");
            return;
        }
    }
    const UiImage& img = c.image;
    if (bare.rgba == nullptr || img.rgba == nullptr || bare.w != img.w || bare.h != img.h) {
        fail("no undimmed capture of the same size to compare the dim with");
        return;
    }
    // The dim as the frame drew it: the style colour with the style alpha, packed
    // to 8 bits per channel (GetColorU32), then blended src-alpha over the menu.
    ImVec4 dimColour = ImGui::GetStyle().Colors[ImGuiCol_ModalWindowDimBg];
    dimColour.w *= ImGui::GetStyle().Alpha;
    const ImU32 packed = ImGui::ColorConvertFloat4ToU32(dimColour);
    const float src[3] = { (float)((packed >> IM_COL32_R_SHIFT) & 0xFF), (float)((packed >> IM_COL32_G_SHIFT) & 0xFF),
                           (float)((packed >> IM_COL32_B_SHIFT) & 0xFF) };
    const float alpha = (float)((packed >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
    // The box, grown by 2 px for its border's antialiased edge.
    const int bx0 = (int)std::floor(c.contentRect[0]) - 2;
    const int by0 = (int)std::floor(c.contentRect[1]) - 2;
    const int bx1 = (int)std::ceil(c.contentRect[0] + c.contentRect[2]) + 2;
    const int by1 = (int)std::ceil(c.contentRect[1] + c.contentRect[3]) + 2;
    int64_t checked = 0;
    int64_t off = 0;
    int firstX = -1;
    int firstY = -1;
    int got[3] = { 0, 0, 0 };
    int want[3] = { 0, 0, 0 };
    std::set<uint32_t> bareColours;
    for (int y = 0; y < img.h; y++) {
        for (int x = 0; x < img.w; x++) {
            if (x >= bx0 && x < bx1 && y >= by0 && y < by1) {
                continue;
            }
            const size_t o = ((size_t)y * (size_t)img.w + (size_t)x) * 4;
            checked++;
            if (bareColours.size() < 64) {
                bareColours.insert((uint32_t)bare.rgba[o] | ((uint32_t)bare.rgba[o + 1] << 8) |
                                   ((uint32_t)bare.rgba[o + 2] << 16));
            }
            bool ok = true;
            int expected[3];
            for (int ch = 0; ch < 3; ch++) {
                expected[ch] = (int)std::lround(src[ch] * alpha + (float)bare.rgba[o + ch] * (1.0f - alpha));
                if (std::abs((int)img.rgba[o + ch] - expected[ch]) > 2) {
                    ok = false;
                }
            }
            if (!ok) {
                if (off == 0) {
                    firstX = x;
                    firstY = y;
                    for (int ch = 0; ch < 3; ch++) {
                        got[ch] = img.rgba[o + ch];
                        want[ch] = expected[ch];
                    }
                }
                off++;
            }
        }
    }
    if (bareColours.size() < 16) {
        fail("outside the box the undimmed frame has only " + std::to_string(bareColours.size()) +
             " colours: there is no menu under the dim to check");
        return;
    }
    if (off != 0) {
        auto rgb = [](const int v[3]) {
            return "(" + std::to_string(v[0]) + "," + std::to_string(v[1]) + "," + std::to_string(v[2]) + ")";
        };
        fail(std::to_string(off) + " of " + std::to_string(checked) +
             " pixels outside the box are not the menu dimmed by ModalWindowDimBg (first at " + std::to_string(firstX) +
             "," + std::to_string(firstY) + ": " + rgb(got) + ", expected " + rgb(want) + ")");
    }
}

void Session::CaptureOverlay(const PageSpec& p) {
    for (const std::string& state : p.states) {
        const std::string variant = VariantName(state, "");
        if (!Selected(p, variant)) {
            continue;
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
        const bool overMenu = state == "over-menu";
        Capture c;
        c.id = p.id;
        c.variant = variant;
        c.state = state;
        // RSBS_UI_SNAPSHOT_SABOTAGE=no-menu-under leaves the menu hidden under the
        // over-menu variant; =no-dim draws the overlay's dim fully transparent.
        // Each must turn DimOracle red.
        if (overMenu && !opt.Sabotaged("no-menu-under")) {
            Navigate(OverMenuPage());
        } else {
            menu->Hide();
        }
        Capture bare;
        if (overMenu && !Settle(bare, false, nullptr, nullptr)) {
            c.status = "fail";
            c.reason = "the menu alone did not settle: " + bare.reason;
        }
        const bool noDim = opt.Sabotaged("no-dim");
        auto extra = [&view, noDim]() {
            if (noDim) {
                ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            }
            OoT_CreationProgressOverlay_TestDrawContents(&view);
            if (noDim) {
                ImGui::PopStyleColor();
            }
        };
        if (c.status == "pass" && Settle(c, false, extra, nullptr)) {
            if (Oracle(p, c) && overMenu) {
                DimOracle(bare.image, c, kOverlayDimWindow, p.window.c_str());
            }
        }
        UiImage_Free(&bare.image);
        Finish(c, p);
        Record(std::move(c));
    }
}

/**
 * SoH's ROM-extraction progress modal, reproduced for the reference page
 * "modal/ROM Extraction". RunExtract (OTRGlobals.cpp, the ImGui-driven
 * extraction flow ported from upstream SoH) pushes two style colours around its
 * WHOLE frame -- the themed active title bar and an opaque DarkGray modal dim,
 * both still in force when ImGui::Render draws the dim -- and five more around
 * the modal itself. PushExtractionFrameStyle/PopExtractionFrameStyle are the
 * first two, called outside the harness's frames for the same reason;
 * DrawExtractionReference is the modal, with a fixed state instead of a live
 * extraction (half way, one archive file named). It is a copy because the
 * original is inline in a boot loop and cannot be called. The static lint's C1
 * check (.github/scripts/check-ui-parity-lint.py) compares every style push,
 * pop, the modal's flags and the bar's size in these three functions with
 * RunExtract's, statement for statement, so the copy cannot drift silently.
 */
void PushExtractionFrameStyle() {
    UIWidgets::Colors themeColor =
        static_cast<UIWidgets::Colors>(CVarGetInteger(CVAR_SETTING("Menu.Theme"), UIWidgets::Colors::LightBlue));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, UIWidgets::ColorValues.at(themeColor));
    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, UIWidgets::ColorValues.at(UIWidgets::Colors::DarkGray));
}

/**
 * RunExtract calls OpenPopup and BeginPopupModal with ImGui's implicit fallback
 * window current; the harness's frame has none at the point extra ImGui is
 * submitted, and both calls hash the popup's name through the current window, so
 * the copy opens an empty, input-less host window first. The modal is still its
 * own top-level window; only the popup's id is seeded differently.
 *
 * @p mode: 0 opens (or keeps) the modal, 1 submits CloseCurrentPopup (the
 * teardown frame), 2 only reports through @p open whether it is still open.
 */
void DrawExtractionReference(int mode, bool* open) {
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(1.0f, 1.0f), ImGuiCond_Always);
    ImGui::Begin("##ExtractionReferenceHost", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    if (open != nullptr) {
        *open = ImGui::IsPopupOpen("ROM Extraction");
    }
    if (mode == 2) {
        ImGui::End();
        return;
    }
    const bool close = mode == 1;
    if (!close && !ImGui::IsPopupOpen("ROM Extraction")) {
        ImGui::OpenPopup("ROM Extraction");
    }
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 8.0f));
    auto color = UIWidgets::ColorValues.at(THEME_COLOR);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(color.x, color.y, color.z, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(color.x, color.y, color.z, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.3f));
    if (ImGui::BeginPopupModal("ROM Extraction", NULL,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::Text("Extracting %s...%s", "gameplay_keep", "");
        ImGui::ProgressBar(0.5f, ImVec2(600.0f, 50.0f), "50%");
        if (close) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
    ImGui::End();
}

void PopExtractionFrameStyle() {
    ImGui::PopStyleColor(2);
}

void Session::CaptureModal(const PageSpec& p) {
    for (const std::string& state : p.states) {
        const std::string variant = VariantName(state, "");
        if (Selected(p, variant)) {
            CaptureModalVariant(p, state);
        }
    }
}

void Session::CaptureModalVariant(const PageSpec& p, const std::string& state) {
    Capture c;
    c.id = p.id;
    c.variant = VariantName(state, "");
    c.state = state;
    // A modal's background dim fades in over about a sixth of a second of REAL
    // time (NewFrame advances DimBgRatio by DeltaTime * 6), so how many frames it
    // takes depends on the host's frame rate and the capture would race it.
    // Start every frame fully dimmed instead: the picture is the settled one,
    // deterministically.
    auto fullDim = []() { GImGui->DimBgRatio = 1.0f; };
    // over-menu: the menu page the creation overlay's over-menu variant sits on,
    // settled alone first so DimOracle has the undimmed picture.
    const bool overMenu = state == "over-menu";
    Capture bare;
    if (overMenu) {
        Navigate(OverMenuPage());
        if (!Settle(bare, false, nullptr, fullDim)) {
            c.status = "fail";
            c.reason = "the menu alone did not settle: " + bare.reason;
            UiImage_Free(&bare.image);
            Finish(c, p);
            Record(std::move(c));
            return;
        }
    } else {
        menu->Hide();
    }
    // The extraction reference is drawn inside the frame, not queued.
    const bool extraction = p.origin == Origin::SOH_REFERENCE && p.window == "ROM Extraction";
    int extractionMode = 0;
    bool extractionOpen = false;
    std::function<void()> extra;
    if (extraction) {
        extra = [&extractionMode, &extractionOpen]() { DrawExtractionReference(extractionMode, &extractionOpen); };
    } else if (p.origin == Origin::SOH_REFERENCE) {
        // SoH's own strings (SohMenuSettings.cpp, the "Clear Devices" button), with
        // null callbacks: nothing is cleared, and the popup is dismissed below.
        SohGui::RegisterPopup(
            "Clear Config", "This will completely erase the controls config, including registered devices.\nContinue?",
            "Clear", "Cancel", nullptr, nullptr);
    } else {
        // Ours: fire the button row's own Callback, which queues its confirm.
        // DismissPopup below never runs the popup's buttons, so nothing is reset.
        WidgetInfo* row = FindRow(*menu, p.header, p.sidebar, [](const WidgetInfo& w) {
            return w.type == WIDGET_BUTTON && w.name.find("Reset") != std::string::npos;
        });
        if (row == nullptr || row->callback == nullptr) {
            c.status = "fail";
            c.reason = "no Reset button row with a Callback on " + p.header + "/" + p.sidebar;
            c.spec = &p;
            UiImage_Free(&bare.image);
            Record(std::move(c));
            return;
        }
        row->callback(*row);
        if (SohGui::PopupsQueued() == 0) {
            c.status = "fail";
            c.reason = "the Reset button queued no confirm popup";
            c.spec = &p;
            UiImage_Free(&bare.image);
            Record(std::move(c));
            return;
        }
    }
    // RunExtract's frame-wide pushes, outside the frames as RunExtract has them.
    if (extraction) {
        PushExtractionFrameStyle();
    }
    if (Settle(c, false, extra, fullDim)) {
        if (Oracle(p, c) && overMenu) {
            DimOracle(bare.image, c, nullptr, p.window.c_str());
        }
    }
    UiImage_Free(&bare.image);
    Finish(c, p);
    // Teardown, then assert nothing is left queued for the pages after this one.
    std::string why;
    if (extraction) {
        extractionMode = 1;
        PumpFrame(nullptr, false, extra, why);
        extractionMode = 2;
        PumpFrame(nullptr, false, extra, why);
        PopExtractionFrameStyle();
        if (extractionOpen && c.status == "pass") {
            c.status = "fail";
            c.reason = "the extraction reference modal did not close";
        }
    } else {
        SohGui::DismissPopup(p.window);
        PumpFrame(nullptr, false, nullptr, why);
        PumpFrame(nullptr, false, nullptr, why);
    }
    if (SohGui::PopupsQueued() != 0 && c.status == "pass") {
        c.status = "fail";
        c.reason = "the modal did not dismiss (PopupsQueued " + std::to_string(SohGui::PopupsQueued()) + ")";
    }
    Record(std::move(c));
}

/**
 * The paired-file load's toasts (#781) through RsbsSave_EmitLoadToast, the one
 * emitter the load and the MM arrival call, each with the longest input a real
 * load can hand it: every Cross-Game Rule restored at once (their labels in the
 * page's order), the settable MM trick with the longest label alone (the cut
 * case), and every settable MM trick at once (the "+N" case).
 */
static void EmitLoadToastPage(const std::string& id) {
    if (id == "toast/load-rules-restored") {
        std::string rules;
        for (ComboSettingId rule : { COMBO_SETTING_GOAL, COMBO_SETTING_DIRECTION, COMBO_SETTING_SHARED_OCARINA }) {
            rules += (rules.empty() ? "" : ", ") + std::string(Combo_ComboSettingLabel(rule));
        }
        RsbsSave_EmitLoadToast(RSBS_LOAD_TOAST_RULES_RESTORED, rules.c_str(), 0);
    } else if (id == "toast/load-mm-restored-one" || id == "toast/load-mm-restored-many") {
        const bool many = id == "toast/load-mm-restored-many";
        std::string longest;
        std::string all;
        int count = 0;
        for (int i = 0; i < Combo_MMTrickCount(); i++) {
            const ComboMMTrickDesc* d = Combo_MMTrickAt(i);
            if (d == nullptr || !d->bound || d->reserved || d->label == nullptr) {
                continue;
            }
            count++;
            all += (all.empty() ? "" : ", ") + std::string(d->label);
            if (std::strlen(d->label) > longest.size()) {
                longest = d->label;
            }
        }
        if (count == 0) {
            // ROM-free with no MM trick table: an option label stands in.
            longest = all = "Starting Hearts";
            count = 1;
        }
        if (many) {
            RsbsSave_EmitLoadToast(RSBS_LOAD_TOAST_MM_RESTORED, all.c_str(), count);
        } else {
            RsbsSave_EmitLoadToast(RSBS_LOAD_TOAST_MM_RESTORED, longest.c_str(), 1);
        }
    } else if (id == "toast/load-mm-not-restored") {
        RsbsSave_EmitLoadToast(RSBS_LOAD_TOAST_MM_NOT_RESTORED, nullptr, 0);
    } else if (id == "toast/load-refused-rules") {
        RsbsSave_EmitLoadToast(RSBS_LOAD_TOAST_REFUSED_RULES, nullptr, 0);
    } else if (id == "toast/load-refused-other-build") {
        RsbsSave_EmitLoadToast(RSBS_LOAD_TOAST_REFUSED_OTHER_BUILD, nullptr, 0);
    } else if (id == "toast/load-refused-damaged") {
        RsbsSave_EmitLoadToast(RSBS_LOAD_TOAST_REFUSED_DAMAGED, nullptr, 0);
    } else if (id == "toast/load-refused-file-select") {
        RsbsSave_EmitFileSelectRefusalToast("This file has no Majora's Mask world");
    }
}

void Session::CaptureToast(const PageSpec& p) {
    if (!Selected(p, "")) {
        return;
    }
    menu->Hide();
    Capture c;
    c.id = p.id;
    c.spec = &p;
    // The toasts draw from SoH's Notification::Window, which SohGui registers with
    // the rest of its windows (SetupGuiElements; ROM-rich only). ROM-free, the
    // harness registers one of its own under the same name, with no CVar (a
    // GuiWindow with a CVar name writes it into the config), so hosted CI draws
    // the same toasts.
    if (gui->GetGuiWindow("Notifications Window") == nullptr) {
        auto window = std::make_shared<Notification::Window>("", true, "Notifications Window");
        gui->AddGuiWindow(window);
    }
    OoT_Notification_ClearForTest();
    if (p.id == "toast/Game Autosaved") {
        Notification::Emit({ .message = "Game autosaved", .mute = true });
    } else if (p.id == "toast/creation-shortfall") {
        // Two of four: the numbers a small under-supplied seed reports.
        OoT_Creation_EmitShortfallToast(2, 4);
    } else if (p.id == "toast/creation-failure") {
        OoT_Creation_ReportFailureAtFileSelect(0, 0);
    } else if (p.id == "toast/creation-goal-warning") {
        // ADR 0010 section 1.2's creation warning, as a "Ganon" world raises it:
        // MM's half carries no proof. The longest of its three copies.
        OoT_Creation_EmitGoalWarningToast(RSBS_COMBO_HALF_MM);
    } else if (p.id == "toast/pairing-refused-mm-options") {
        MM_Rando_EmitPairingRefusalToast(RSBS_PAIRING_REFUSAL_MM_OPTIONS, nullptr);
    } else if (p.id == "toast/pairing-refused-rules") {
        // A goal changed on a triforce-hunt file: two fields, both named (the
        // shape mm-combo-settings-gate leg 4 asserts).
        char fields[192];
        Combo_ComboSettingsDivergenceDescribe(RSBS_COMBO_DIVERGE_GOAL | RSBS_COMBO_DIVERGE_TRIFORCE, fields,
                                              sizeof(fields));
        MM_Rando_EmitPairingRefusalToast(RSBS_PAIRING_REFUSAL_RULES, fields);
    } else if (p.id == "toast/pairing-refused-missing-half") {
        MM_Rando_EmitPairingRefusalToast(RSBS_PAIRING_REFUSAL_MISSING_HALF, nullptr);
    } else if (p.id == "toast/pairing-refused-spoiler") {
        // The longest of the spoiler routes' messages.
        MM_Rando_EmitPairingRefusalToast(RSBS_PAIRING_REFUSAL_SPOILER, RSBS_SPOILER_REFUSAL_OTHER_MM_OPTIONS);
    } else if (p.id == "toast/paired-spoiler-not-loaded") {
        // SeedContext.cpp's refusal emitter, muted as every harness toast is.
        OoT_EmitPairedSpoilerRefusalToast(/*mute=*/1);
    } else if (p.id.rfind("toast/load-", 0) == 0) {
        EmitLoadToastPage(p.id);
    }
    if (Settle(c, false, nullptr, nullptr)) {
        // The window is named "notification#<id>" and the id is the overlay's
        // own counter, so the oracle finds it by prefix: every active toast
        // window, whose union is the content rectangle.
        float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
        int found = 0;
        for (ImGuiWindow* w : GImGui->Windows) {
            if (w == nullptr || !w->Active || std::string(w->Name).rfind(p.window, 0) != 0) {
                continue;
            }
            found++;
            x0 = std::min(x0, w->Pos.x);
            y0 = std::min(y0, w->Pos.y);
            x1 = std::max(x1, w->Pos.x + w->Size.x);
            y1 = std::max(y1, w->Pos.y + w->Size.y);
        }
        if (found != 1) {
            c.status = "fail";
            c.reason = std::to_string(found) + " toast window(s) drawn; the page emits exactly one";
        } else {
            c.contentRect[0] = x0;
            c.contentRect[1] = y0;
            c.contentRect[2] = x1 - x0;
            c.contentRect[3] = y1 - y0;
            // A toast wider than the window is drawn off its left edge: SoH's
            // overlay draws every field on ONE line and never wraps.
            if (x0 < 0.0f || x1 > (float)profile.w) {
                c.status = "fail";
                c.reason = "the toast runs off the window (x " + std::to_string((int)x0) + " to " +
                           std::to_string((int)x1) + " of " + std::to_string(profile.w) + ")";
            }
        }
    }
    Finish(c, p);
    // Teardown: nothing may linger into the pages after this one.
    OoT_Notification_ClearForTest();
    std::string why;
    PumpFrame(nullptr, false, nullptr, why);
    Record(std::move(c));
}

// ---- state and body text ---------------------------------------------------------------------

/** Every capture of @p id in @p state without a hover, text concatenated. */
std::string StateText(const std::vector<Capture>& all, const std::string& id, const std::string& state, bool* any) {
    std::string out;
    *any = false;
    for (const Capture& c : all) {
        if (c.id == id && c.state == state && c.hover.empty() && c.image.rgba != nullptr) {
            out += c.text;
            out += "\n";
            *any = true;
        }
    }
    return out;
}

/** The first capture of @p id in @p state without a hover. */
Capture* FirstStateCapture(std::vector<Capture>& all, const std::string& id, const std::string& state) {
    for (Capture& c : all) {
        if (c.id == id && c.state == state && c.hover.empty()) {
            return &c;
        }
    }
    return nullptr;
}

void Session::CheckStateText() {
    // Each authored state must SHOW its own text (in the union of its views; for
    // a pane those views are visible-only), and that text must be ABSENT from
    // the contrast state's captures. The contrast is what the state would look
    // like if EnterState authored nothing, so the pair is the red half: revert
    // the authoring and the "present" check fails; pick a string every state
    // draws and the "absent" check fails.
    for (const PageSpec& p : pages) {
        for (const auto& [state, wants] : p.stateText) {
            bool any = false;
            StateText(captures, p.id, state, &any);
            Capture* first = FirstStateCapture(captures, p.id, state);
            if (!any || first == nullptr || first->status != "pass") {
                continue; // not captured this run (a page selector), or already failed
            }
            std::string why;
            for (const std::string& want : wants) {
                // Credited to the view that showed it, so the manifest says WHERE.
                Capture* shownIn = nullptr;
                for (Capture& c : captures) {
                    if (c.id == p.id && c.state == state && c.hover.empty() && c.text.find(want) != std::string::npos) {
                        shownIn = &c;
                        break;
                    }
                }
                if (shownIn == nullptr) {
                    first->missing.push_back(want);
                    if (why.empty()) {
                        why = "state \"" + state + "\" does not show \"" + want + "\" in any captured view";
                    }
                } else {
                    shownIn->found.push_back(want);
                }
            }
            const auto contrast = p.stateContrast.find(state);
            if (why.empty() && contrast != p.stateContrast.end()) {
                bool haveContrast = false;
                const std::string other = StateText(captures, p.id, contrast->second, &haveContrast);
                for (const std::string& want : wants) {
                    if (haveContrast && other.find(want) != std::string::npos) {
                        why = "\"" + want + "\" is also shown in state \"" + contrast->second +
                              "\", so it does not prove state \"" + state + "\" was authored";
                        break;
                    }
                }
            }
            if (!why.empty()) {
                first->status = "fail";
                first->reason = why;
                Fail(p.id + "@" + first->variant + ": " + why);
            }
        }
    }
}

void Session::CheckBodyText() {
    // Assert 6's discrimination: a page's body string must be absent from a
    // sibling page's capture (same header, so the same sidebar and header
    // chrome), or it names the chrome and not the page.
    for (const PageSpec& p : pages) {
        if (p.kind != Kind::MENU_PAGE || p.bodyText.empty()) {
            continue;
        }
        Capture* mine = nullptr;
        for (Capture& c : captures) {
            if (c.id == p.id && c.status == "pass" && c.image.rgba != nullptr) {
                mine = &c;
                break;
            }
        }
        if (mine == nullptr) {
            continue;
        }
        for (const Capture& other : captures) {
            if (other.spec == nullptr || other.spec->kind != Kind::MENU_PAGE || other.spec->header != p.header ||
                other.id == p.id || other.image.rgba == nullptr) {
                continue;
            }
            if (other.text.find(p.bodyText) != std::string::npos) {
                mine->status = "fail";
                mine->reason = "its body string \"" + p.bodyText + "\" is also in " + other.id +
                               (other.variant.empty() ? "" : "@" + other.variant) +
                               ", so it does not identify the page";
                Fail(p.id + ": " + mine->reason);
            }
            break; // one sibling is the contrast
        }
    }
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
        if (c.spec == nullptr || c.image.rgba == nullptr || c.status != "pass") {
            continue;
        }
        const auto perState = c.spec->stateCompareWith.find(c.state);
        const std::string& compareWith =
            (!c.hover.empty() && !c.spec->hoverCompareWith.empty())
                ? c.spec->hoverCompareWith
                : (perState != c.spec->stateCompareWith.end() ? perState->second : c.spec->compareWith);
        if (compareWith.empty()) {
            continue;
        }
        // Scroll captures compare against the reference's matching scroll
        // position where it has one, else its first capture.
        const Capture* ref = nullptr;
        if (!c.hover.empty()) {
            for (const Capture& r : captures) {
                if (r.id == compareWith && !r.hover.empty() && r.image.rgba != nullptr && r.status == "pass") {
                    ref = &r;
                    break;
                }
            }
        }
        if (ref == nullptr && c.scrollIndex >= 0) {
            ref = FindCapture(captures, compareWith, "scroll" + std::to_string(c.scrollIndex));
        }
        if (ref == nullptr) {
            ref = FirstCapture(captures, compareWith);
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
            const std::string path = (out / name).string();
            if (UiImage_WritePng(&fitted, path.c_str()) != 0) {
                // Not a failed capture, but never silent: counted into the summary
                // line and the manifest. (UiImage_WritePng retries a Windows path
                // past MAX_PATH through the extended-length namespace, so this is
                // a real write failure, not a long output directory.)
                compositeWriteFailures++;
                printf("[UI-SNAPSHOT] warning: could not write %s (%zu characters)\n", path.c_str(), path.size());
            }
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
            const std::string path = (out / ("iter/" + stem)).string();
            if (UiImage_WritePng(&fitted, path.c_str()) != 0) {
                compositeWriteFailures++;
                printf("[UI-SNAPSHOT] warning: could not write %s (%zu characters)\n", path.c_str(), path.size());
            }
        }
        uint64_t changed = 0;
        if (UiImage_AbsDiffX4(&before, &c.image, &diff, &changed) == 0) {
            c.changedPixels = (int64_t)changed;
            std::string diffName = stem.substr(0, stem.size() - 4) + ".diff.png";
            if (UiImage_WritePng(&diff, (out / ("iter/" + diffName)).string().c_str()) != 0) {
                compositeWriteFailures++;
            }
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
    j += "\"noActivationHint\":\"" + JsonEscape(noActivationHint) + "\",";
    j += "\"settle\":[" + std::to_string(opt.settleMin) + "," + std::to_string(opt.settleMax) + "],";
    j += "\"pages\":\"" + JsonEscape(opt.pages) + "\",";
    j += "\"sabotage\":\"" + JsonEscape(opt.sabotage) + "\",";
    j += "\"compositeWriteFailures\":" + std::to_string(compositeWriteFailures) + ",";
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
            const auto perState = p->stateCompareWith.find(c.state);
            j += ",\"compareWith\":\"" +
                 JsonEscape(perState != p->stateCompareWith.end() ? perState->second : p->compareWith) + "\"";
            if (p->kind == Kind::MENU_PAGE) {
                j += ",\"bodyText\":\"" + JsonEscape(p->bodyText) + "\"";
            }
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
        if (!c.hoverText.empty()) {
            j += ",\"hoverText\":\"" + JsonEscape(c.hoverText) + "\"";
            j += ",\"hoverLines\":[";
            for (size_t k = 0; k < c.hoverLines.size(); k++) {
                j += (k == 0 ? "\"" : ",\"") + JsonEscape(c.hoverLines[k]) + "\"";
            }
            j += "]";
        }
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

// ---- runtime lint (R1-R7, R9) and R8's dump --------------------------------------------------

bool IsInteractive(WidgetType t);

/**
 * R9: does every row of @p p fit its column at this run's profile? One frame of
 * the page is drawn with every measurable row's postFunc wrapped (MenuDrawItem
 * runs it right after the widget, the hover captures' mechanism), and each row
 * reports the rectangle it drew and the clip rectangle of the column child it
 * drew into. A row fits when its right edge is inside that clip rectangle:
 *   - a checkbox's item rectangle ends where its label ends (UIWidgets::Checkbox
 *     sizes total_bb to the label), and a slider's, combobox's or button's
 *     group ends where its widest part ends (the label above, or the box). A
 *     slider's label carries its value, so it is also measured at both ends of
 *     its range: a row that fits at 5 minutes and not at 60 does not fit;
 *   - a SEPARATOR_TEXT's rectangle always spans the column (ImGui's
 *     SeparatorTextEx), and ImGui ELLIPSIZES a title that does not fit, so its
 *     need is computed: the title's width plus the separator padding on both
 *     sides, against the rectangle's own right edge as well as the clip;
 *   - a TEXT row wraps at the column (TextWrapped), so it overruns only when one
 *     word is wider than the column.
 * Custom rows (a trick table, a mod list) are left out: their last item is
 * whatever their function drew last, not the row. The trick tables scroll
 * sideways as SoH's Tricks/Glitches table does; the mod list's table has no
 * horizontal scroll and clips a long name at its column, as SoH's Mod Menu does
 * (mod_menu.cpp, the same table flags).
 *
 * R9 cannot pass by measuring nothing: a measuring frame that fails is a run
 * failure on any page, and a page of this project's with measurable rows that
 * reports none of them (the frame drew another page, or no row reached its
 * postFunc) is a run failure too, by name.
 *
 * A row of this project's pages that does not fit is an R9 hit, which the
 * runtime lint fails like any other hit outside the baseline. SoH's reference
 * pages are measured the same way and only REPORTED (label-fit.txt): rule 0
 * keeps them as SoH shipped them, and they do overrun a narrow column.
 */
void Session::MeasureLabelFit(const PageSpec& p, const std::string& state) {
    auto& entries = MenuEntries(*menu);
    if (!entries.contains(p.header) || !entries.at(p.header).sidebars.contains(p.sidebar)) {
        return;
    }
    struct Drawn {
        std::string name;
        float needX = 0.0f;
        float limitX = 0.0f;
        float columnW = 0.0f;
    };
    auto drawn = std::make_shared<std::vector<Drawn>>();
    std::vector<std::pair<WidgetInfo*, WidgetFunc>> saved;
    for (auto& column : entries.at(p.header).sidebars.at(p.sidebar).columnWidgets) {
        for (WidgetInfo& row : column) {
            if (!IsInteractive(row.type) && row.type != WIDGET_SEPARATOR_TEXT && row.type != WIDGET_TEXT) {
                continue;
            }
            saved.emplace_back(&row, row.postFunc);
            WidgetFunc prev = row.postFunc;
            row.postFunc = [drawn, prev](WidgetInfo& info) {
                const ImGuiWindow* w = GImGui->CurrentWindow;
                const ImRect item(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
                Drawn d;
                d.name = info.name;
                d.needX = item.Max.x;
                d.limitX = w->InnerClipRect.Max.x;
                d.columnW = w->InnerClipRect.GetWidth();
                if (info.type == WIDGET_SEPARATOR_TEXT) {
                    const float pad = GImGui->Style.SeparatorTextPadding.x;
                    d.needX = item.Min.x + pad * 2.0f + ImGui::CalcTextSize(info.name.c_str(), nullptr, true).x;
                    d.limitX = std::min(d.limitX, item.Max.x);
                } else if ((info.type == WIDGET_SLIDER_INT || info.type == WIDGET_CVAR_SLIDER_INT) &&
                           info.options != nullptr) {
                    // A slider's label is printf'd with its value (UIWidgets::SliderInt:
                    // ImGui::Text(label, *value)), so the value drawn now is not the
                    // widest: the label must fit at either end of its range too.
                    const auto opts = std::static_pointer_cast<UIWidgets::IntSliderOptions>(info.options);
                    if (opts->labelPosition == UIWidgets::LabelPositions::Above &&
                        opts->alignment == UIWidgets::ComponentAlignments::Left) {
                        for (const int32_t v : { opts->min, opts->max }) {
                            char text[256];
                            ImFormatString(text, sizeof(text), info.name.c_str(), v);
                            d.needX = std::max(d.needX, item.Min.x + ImGui::CalcTextSize(text).x);
                        }
                    }
                }
                drawn->push_back(d);
                if (prev) {
                    prev(info);
                }
            };
        }
    }
    std::string why;
    const bool pumped = PumpFrame(nullptr, false, nullptr, why);
    for (auto& [row, post] : saved) {
        row->postFunc = post;
    }

    const bool ours = p.origin == Origin::RSBS;
    const std::string where = p.header + "/" + p.sidebar;
    const std::string at = where + (state.empty() ? "" : "@" + state);
    if (ours && opt.Sabotaged("no-label-fit")) {
        drawn->clear();
    }
    if (!pumped) {
        Fail("R9: the measuring frame of " + at + " failed: " + why);
        return;
    }
    if (ours && !saved.empty() && drawn->empty()) {
        Fail("R9: measured no row of " + at + " (" + std::to_string(saved.size()) +
             " measurable row(s) registered, none drawn)");
        return;
    }
    for (const Drawn& d : *drawn) {
        labelFitMeasured++;
        const float over = d.needX - d.limitX;
        if (over <= 0.5f) {
            continue;
        }
        std::string name = d.name;
        std::replace(name.begin(), name.end(), '\n', ' ');
        char detail[96];
        snprintf(detail, sizeof(detail), "over by %.0f px in a %.0f px column", over, d.columnW);
        labelFitReport.push_back(std::string(ours ? "ours | " : "soh  | ") + where +
                                 (state.empty() ? "" : "@" + state) + " | " + name + " | " + detail);
        if (ours) {
            dynamicHits.insert("R9 | " + where + " | " + name + " :: does not fit its column at " + profile.name);
        }
    }
}

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
            refString("Combo/MM Randomizer table", d->label);
            refString("Combo/MM Randomizer table", d->tooltip);
            refString("Combo/MM Randomizer table", d->disabledReason);
        }
    }
    for (int i = 0; i < Combo_MMTrickCount(); i++) {
        const ComboMMTrickDesc* d = Combo_MMTrickAt(i);
        if (d != nullptr) {
            refString("Combo/MM Tricks table", d->label);
            refString("Combo/MM Tricks table", d->tooltip);
            refString("Combo/MM Tricks table", d->disabledReason);
        }
    }

    std::string body = "# Runtime UI lint (R1-R7, R9) over this project's rows. One hit per line: rule | where | row.\n"
                       "# R2w/R4w are warnings and are reported, never baselined or failed.\n";
    for (const std::string& h : hits) {
        body += h + "\n";
    }
    WriteTextFile(out / "runtime-lint.txt", body);

    // R9's measurements, SoH's reference pages included (report only for those).
    std::string fit = "# Label fit at " + profile.name + " (R9): every row that overruns its column. " +
                      std::to_string(labelFitMeasured) +
                      " row draw(s) measured.\n"
                      "# ours rows are R9 hits (runtime-lint.txt); soh rows are the reference, reported only.\n";
    for (const std::string& line : labelFitReport) {
        fit += line + "\n";
    }
    WriteTextFile(out / "label-fit.txt", fit);

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

/**
 * R8's one volatile pair: SoH's Settings > General "About" column names the
 * binary's own build (SohMenuSettings.cpp: "Branch: <branch>", "Commit: <hash>"),
 * so any two differently committed binaries differ there although no SoH wording
 * changed. Both sides of the comparison mask those two values; every other row,
 * the About column's other rows included, is compared byte for byte.
 */
static std::string MaskBuildStamp(const std::string& names) {
    std::istringstream in(names);
    std::string line;
    std::string out;
    while (std::getline(in, line)) {
        const size_t a = line.find(" | ");
        if (a != std::string::npos && line.rfind("Settings/General | ", 0) == 0) {
            const size_t b = line.find(" | ", a + 3);
            const std::string row = line.substr(a + 3, b == std::string::npos ? std::string::npos : b - a - 3);
            if (row.rfind("Commit: ", 0) == 0 || row.rfind("Branch: ", 0) == 0) {
                line = line.substr(0, a + 3) + row.substr(0, 8) + "<build stamp>" +
                       (b == std::string::npos ? std::string() : line.substr(b));
            }
        }
        out += line + "\n";
    }
    return out;
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
    body = MaskBuildStamp(body);
    WriteTextFile(out / "soh-names.txt", body);
    if (!opt.baseline.empty()) {
        // Masked again on read: a baseline written before the mask existed
        // carries its build's literal commit and branch.
        const std::string before = MaskBuildStamp(ReadTextFile(fs::path(opt.baseline) / "soh-names.txt"));
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
        isolationBefore[f] = FileDigest(AppPath(f));
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
#if defined(_WIN32)
    // A snapshot run must not take the keyboard from the person at the
    // workstation, and on Windows with GL it provably need not: SDL shows the
    // window with SW_SHOWNA under the hint both main.cpp and BringUp arm. So there
    // an activated window is a failure, not a report (lane F1). It stays a report
    // on DirectX 11, whose HWND libultraship shows with ShowWindow(SW_SHOW)
    // (gfx_dxgi.cpp) without consulting SDL's hint, and off Windows, where an
    // Xvfb display has no window manager and gives a new window X input focus
    // regardless (the Linux CI manifests read windowActivated=true with the hint
    // set).
    if (windowActivated && renderer.backend == Backend::GL) {
        Fail(
            "focus: the harness window took keyboard focus on Windows/OpenGL (SDL_HINT_WINDOW_NO_ACTIVATION_WHEN_SHOWN "
            "was '" +
            noActivationHint +
            "' when it was created); a test window must never take the caret from the person at "
            "the workstation");
    }
#endif

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
                case Kind::TOAST:
                    CaptureToast(p);
                    break;
            }
        } catch (const std::exception& e) { Fail(p.id + ": an exception escaped the capture: " + e.what()); }
        menu->Show();
    }

    CheckStateText();
    CheckBodyText();
    WriteComposites();
    WriteIterComposites();
    RuntimeLint();
    DumpSohNames();

    if (opt.Sabotaged("keep-imgui-ini") && ImGui::GetIO().IniFilename != nullptr) {
        // What ImGui::Shutdown does when the context is destroyed with a path set.
        ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
    }
    // Assert 10's "after".
    for (const auto& [f, digest] : isolationBefore) {
        const std::string now = FileDigest(AppPath(f));
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
    printf("[UI-SNAPSHOT] %d captured, %d skipped, %zu failure(s), %d composite(s) not written; output in %s\n",
           captured, skipped, failures.size(), compositeWriteFailures, fs::absolute(out).string().c_str());
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
