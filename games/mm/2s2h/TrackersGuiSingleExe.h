/**
 * MM tracker registration surface for single-executable builds (#392).
 *
 * Upstream 2S2H instantiates its tracker windows in BenGui.cpp's
 * SetupGuiElements, whose only caller is the excluded BenPort.cpp — so in the
 * single exe the whole MM menu/tracker surface was link-elided. This is the
 * BenMenu-bypass registration surface from docs/unified-surface-findings.md
 * §3: it defines the BenGui::m*Window globals the tracker TUs link against and
 * registers ONLY the tracker windows on the shared Ship::Context Gui, leaving
 * BenMenu/BenMenuBar (and Rando/Menu.cpp, which references them) elided.
 *
 * Names are prefixed "MM " because SoH already owns "Check Tracker" /
 * "Item Tracker" / their settings twins on the shared Gui, and
 * Gui::AddGuiWindow rejects duplicate names (silently, from the caller's
 * perspective — SPDLOG_ERROR and return).
 *
 * Every registered window is wrapped in a per-active-game gate: gSaveContext
 * storage is unified, so an MM tracker drawing while OoT is active would read
 * OoT bytes through MM's SaveContext layout. The gate also keeps MM's
 * hardcoded ImGui window ids from colliding with OoT's tracker windows in the
 * same frame. See TrackersGuiSingleExe.cpp.
 */
#ifndef TRACKERS_GUI_SINGLE_EXE_H
#define TRACKERS_GUI_SINGLE_EXE_H

#ifdef RSBS_SINGLE_EXECUTABLE
#ifdef __cplusplus

#include <memory>

namespace Ship {
class Gui;
}

namespace S2H {
namespace TrackersGui {

// Gui registration names. SoH owns the unprefixed forms on the shared Gui —
// keep these distinct from every name games/oot/soh/SohGui/SohGui.cpp
// registers. The mm-trackers-gui CTest locks the de-collision.
inline constexpr const char* kCheckTrackerWindowName = "MM Check Tracker";
inline constexpr const char* kCheckTrackerSettingsWindowName = "MM Check Tracker Settings";
inline constexpr const char* kItemTrackerWindowName = "MM Item Tracker";
inline constexpr const char* kItemTrackerSettingsWindowName = "MM Item Tracker Settings";
inline constexpr const char* kAllTrackerWindowNames[] = {
    kCheckTrackerWindowName,
    kCheckTrackerSettingsWindowName,
    kItemTrackerWindowName,
    kItemTrackerSettingsWindowName,
};

/**
 * Instantiate the MM tracker windows (populating the BenGui::m*Window
 * globals) and register them on `gui`. Idempotent: if the check-tracker
 * window name is already registered, returns without touching anything.
 * Never call BenGui::Destroy to undo this — it calls RemoveAllGuiWindows,
 * which would drop SoH's windows off the shared Gui too.
 */
void RegisterWindows(std::shared_ptr<Ship::Gui> gui);

} // namespace TrackersGui
} // namespace S2H

extern "C" {
#endif // __cplusplus

/**
 * Per-active-game gate shared by every MM tracker window: true iff MM is the
 * active game (Context_GetCurrentGame() == GAME_MM). Split out so the
 * ROM-free lock can flip it without a Gui.
 */
bool MM_TrackersGui_ShouldDraw(void);

/**
 * Production entry point, called from MM_Rando_Init (once-only by its guard):
 * registers the tracker windows on the shared Ship::Context Gui and loads
 * MM's tracker icon textures when mm.o2r is available. Safe no-op when the
 * context has no window/Gui (ROM-free unit harness).
 */
void MM_TrackersGui_Init(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_SINGLE_EXECUTABLE

#endif // TRACKERS_GUI_SINGLE_EXE_H
