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

// Visibility CVars, kept as MM's upstream "gWindows.*" names (OoT's windows
// use "gOpenWindows.*", so there is no store collision to rename around).
// These are the SAME strings the ctor calls in RegisterWindows pass; naming
// them here is what keeps the ctor CVar, the CVar->visibility sync, and
// ItemTracker.cpp's live Draw-time read from drifting apart. CheckTracker.cpp
// spells its own copy as CVAR_NAME_SHOW_CHECK_TRACKER (CheckTracker.cpp:54);
// that one is vendored and stays where it is.
inline constexpr const char* kCheckTrackerVisibilityCVar = "gWindows.CheckTracker";
inline constexpr const char* kCheckTrackerSettingsVisibilityCVar = "gWindows.CheckTrackerSettings";
inline constexpr const char* kItemTrackerVisibilityCVar = "gWindows.ItemTracker";
inline constexpr const char* kItemTrackerSettingsVisibilityCVar = "gWindows.ItemTrackerSettings";

/**
 * Instantiate the MM tracker windows (populating the BenGui::m*Window
 * globals) and register them on `gui`. Idempotent: if the check-tracker
 * window name is already registered, returns without touching anything.
 * Never call BenGui::Destroy to undo this — it calls RemoveAllGuiWindows,
 * which would drop SoH's windows off the shared Gui too.
 */
void RegisterWindows(std::shared_ptr<Ship::Gui> gui);

/**
 * Reconcile each registered MM tracker window's visibility with the live value
 * of its "gWindows.*" CVar (#489 cause 1).
 *
 * Ship::GuiWindow reads its visibility CVar exactly once, in the constructor
 * (libultraship GuiWindow.cpp:13-15), and SyncVisibilityConsoleVariable only
 * ever writes visibility -> CVar, never the reverse. Upstream 2S2H reopened
 * windows through BenMenu's WIDGET_WINDOW_BUTTON rows, and BenMenu is
 * link-elided here — so without this call three of the four MM tracker windows
 * can never be opened after MM's first boot, whatever the CVar says.
 *
 * Safe with no real Ship::Window: it only ever drives a window's visibility TO
 * the value its own CVar already holds, and GuiWindow::SetVisibility assigns
 * mIsVisible BEFORE computing `shouldSave = storedCVar != IsVisible()`. That
 * comparison is therefore always false here, so the
 * Context::GetWindow()->GetGui() deref at GuiWindow.cpp:61 — a null deref in
 * the ROM-free harness — is never reached.
 */
void SyncVisibilityFromCVars(void);

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
 * Per-window visibility predicate (#489 cause 1): true iff the MM tracker
 * window registered under `windowName` exists AND currently considers itself
 * visible. An unknown name is false.
 *
 * This reports the window's OWN latched visibility, deliberately not a CVar
 * read — that is what makes it a real check on SyncVisibilityFromCVars rather
 * than a tautology over the CVar store. Pure: no ImGui, no save, no gSaveContext.
 */
bool MM_TrackersGui_ShouldShow(const char* windowName);

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
