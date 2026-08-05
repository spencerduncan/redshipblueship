/**
 * @file ShipInitBridge_SingleExe.cpp
 * @brief #539 — the CVar-change driver for MM's `S2H::ShipInit` registrar map.
 *
 * THE BUG THIS CLOSES
 * ===================
 * `2s2h/ShipInit.hpp` puts MM's registrar map in `namespace S2H` (the #375
 * COMDAT-fold fix), so MM owns a map that is disjoint from OoT's. Upstream MM
 * re-runs entries out of that map whenever a CVar changes, from its own widget
 * layer (`2s2h/BenGui/UIWidgets.cpp`) — but in single-exe that layer is drawn
 * only by the tracker windows and only ever passes tracker CVar paths, so no
 * converged enhancement key ever reaches it. The one live menu is OoT's
 * `SohMenu`, whose widgets call OoT's `ShipInit::Init(cvar)`.
 *
 * Result before this TU existed: MM's map was driven EXACTLY ONCE per process,
 * by `S2H::ShipInit::InitAll()` in `2s2h/GameExports_SingleExe.cpp`. A shared
 * key like `gEnhancements.RememberSaveLocation` re-armed OoT's registrar the
 * instant the player clicked, and left MM's latched at whatever the value had
 * been at MM's first boot. Under the one-game ruling that divergence is
 * corruption: one combo, two halves silently disagreeing about the same
 * setting.
 *
 * THE CHOKE POINT
 * ===============
 * OoT's `ShipInit::Init(path)` (`games/oot/soh/ShipInit.hpp`) — every one of
 * the menu's ~13 widget call sites funnels through it, so wiring it here is one
 * edit instead of thirteen, and a widget added tomorrow is driven for free.
 * That header calls the entry point below; the policy lives in this TU, next to
 * MM's map, rather than in OoT's header.
 *
 * WHAT IS DELIBERATELY *NOT* FORWARDED
 * ====================================
 * The map is keyed by path string, and two of those keys are pseudo-paths with
 * game-specific timing rather than CVar names:
 *
 *  - `"*"` — "every registrar". OoT drives it from boot (`OTRGlobals.cpp`) and
 *    from preset loads (`Enhancements/Presets/Presets.cpp`). Forwarding it
 *    would run MM's whole registrar set from OoT's boot, BEFORE MM's own
 *    `InitAll()` has ever run, and again on every OoT preset load. MM drives
 *    its own wildcard at MM bring-up and from `PresetManager.cpp`.
 *  - `"IS_RANDO"` — "the rando arm-state may have changed". OoT drives it when
 *    an OoT rando save loads (`Enhancements/randomizer/hook_handlers.cpp`);
 *    MM drives its own from `Rando/Rando.cpp` on MM's save-load chain, which is
 *    what the #439 arm-state locks assert. Forwarding OoT's would arm MM's
 *    rando hooks against MM's *previous* save state on an OoT-only file load.
 *
 * Both exclusions are asserted by the `mm-shipinit-driver` lock, so "simplify
 * the filter" is a red test rather than a silent behavior change.
 *
 * BLAST RADIUS
 * ============
 * Bounded by construction: the forward is keyed on the exact CVar name OoT's
 * menu just wrote, and only fires when MM's map already holds a registrar under
 * that name. A key MM does not register on is a lookup miss and nothing runs,
 * so the set of MM registrars this can reach is exactly "MM registrars keyed on
 * a CVar the unified menu writes" — i.e. the converged shared keys, which is
 * precisely the set #539 is about. The `find()` guard also keeps the forward
 * from growing MM's map: `ShipInit::Init` indexes with `operator[]`, which
 * would insert an empty vector for every OoT-only key the player ever touched.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "2s2h/ShipInit.hpp"

#include <cstring>
#include <string>

extern "C" {

/**
 * Run MM's registrars keyed on `path`, if any. Called from OoT's
 * `ShipInit::Init` — i.e. from the unified menu's widget layer — after OoT's
 * own registrars for the same path have run.
 */
void MM_ShipInit_OnCVarChanged(const char* path) {
    if (path == nullptr || path[0] == '\0') {
        return;
    }

    // Pseudo-paths: MM owns their timing. See the header comment.
    if (std::strcmp(path, "*") == 0 || std::strcmp(path, "IS_RANDO") == 0) {
        return;
    }

    auto& shipInitFuncs = S2H::ShipInit::GetAll();
    const auto it = shipInitFuncs.find(path);
    if (it == shipInitFuncs.end() || it->second.empty()) {
        // MM registers nothing on this key — the overwhelmingly common case,
        // since most menu keys are OoT-only. Returning here (rather than
        // calling Init) is what keeps operator[] from inserting an empty
        // vector per key touched.
        return;
    }

    S2H::ShipInit::Init(path);
}

/**
 * How many MM registrars are keyed on `path`. Non-inserting, unlike
 * `ShipInit::Init`'s `operator[]`. Exists so the `mm-shipinit-driver` lock can
 * assert the converged keys really do have MM registrars behind them — the
 * non-vacuity half of the fix, which is otherwise unobservable from outside
 * MM's headers.
 */
int MM_ShipInit_RegistrarCountForPath(const char* path) {
    if (path == nullptr) {
        return 0;
    }
    auto& shipInitFuncs = S2H::ShipInit::GetAll();
    const auto it = shipInitFuncs.find(path);
    return it == shipInitFuncs.end() ? 0 : static_cast<int>(it->second.size());
}

} // extern "C"

#endif // RSBS_SINGLE_EXECUTABLE
