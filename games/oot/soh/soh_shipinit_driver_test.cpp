/**
 * @file soh_shipinit_driver_test.cpp
 * @brief ROM-free, display-free lock for #539: a CVar change on the unified
 *        menu path re-arms MM's registrars, not only OoT's.
 *
 * CTest label "redship", row `MMShipInitDriver` in CMake/SingleExecutable.cmake,
 * dispatch `mm-shipinit-driver` in src/common/test_runner.cpp.
 *
 * WHAT WAS BROKEN
 * ===============
 * ADR 0003 converged `gEnhancements.RememberSaveLocation` (and Autosave) into
 * single shared keys, but MM's registrar map is a separate map — `S2H::ShipInit`
 * (2s2h/ShipInit.hpp, the #375 COMDAT-fold split). The one live menu is OoT's
 * `SohMenu`, whose widgets call OoT's `ShipInit::Init(cvar)`. Nothing in the
 * single-exe link ever called `S2H::ShipInit::Init()` with an enhancement key,
 * so MM's map was driven exactly once per process by `InitAll()` at MM's first
 * boot. One click re-armed OoT and left MM latched — one combo, two halves
 * disagreeing about the same setting, which the one-game ruling calls
 * corruption.
 *
 * WHY THIS FILE IS ON THE OoT SIDE
 * ================================
 * It must call the REAL production entry point the menu widgets call —
 * `ShipInit::Init(path)`, which is an inline member of OoT's header, so it
 * cannot be reached across a C boundary and cannot be re-derived without
 * testing a copy instead of the code. The probes it observes must live in MM's
 * map, which needs MM's header; and no TU can hold both headers, because MM's
 * ends with `using S2H::ShipInit;` at global scope. Hence the split: probes in
 * games/mm/2s2h/mm_shipinit_driver_test.cpp, driving here.
 *
 * NON-VACUITY
 * ===========
 * Every leg asserts its counter is zero before the drive and non-zero after
 * (or stays zero, for the exclusion legs), so a probe that never registered
 * cannot pass. Leg 1 is the reality check that keeps the whole lock honest: it
 * asserts MM really does keep registrars under a CONVERGED key, so the bug
 * being fixed is a bug about real registrars and not about a synthetic one.
 *
 * VERIFIED RED BEFORE GREEN: with the `MM_ShipInit_OnCVarChanged` call removed
 * from ShipInit.hpp's `Init` (the whole fix), leg 2 fails FAIL(2). Legs 3 and 4
 * pass in that state — they lock the deliberate scoping, and their
 * counterfactual is deleting the pseudo-path filter in the bridge, which turns
 * them red.
 *
 * LEG 6 (#614): RegisterAutosave was reachable only from MM_Rando_Init's
 * once-only bring-up, so it sat OUTSIDE MM's `S2H::ShipInit` map entirely and
 * leg 2's driver had nothing to re-arm for it — the exact gap #539's own body
 * flagged as deliberately out of scope. Unlike legs 1-5, this leg drives the
 * REAL production `RegisterAutosave` through the REAL production CVar
 * (`gEnhancements.Autosave`), because a synthetic probe cannot show that the
 * registrar itself was missing from the map; only observing the registrar's
 * OWN side effect can. `OnGameStateDrawFinish` has exactly one possible
 * registrant tree-wide — RegisterAutosave's CVar-gated branch
 * (SavingEnhancements.cpp; see mm_registrar_coverage_test.cpp's ATTRIBUTION
 * note) — so its settled count is an exact tell for whether RegisterAutosave
 * ran and what it saw. This needs a live Ship::Context (RegisterAutosave
 * reads the CVar through CVarGetInteger), which is why
 * `Test_MMShipInitDriver` (src/common/test_runner.cpp) now stands one up
 * before calling in, unlike before #614.
 *
 * ARM-STATE FINDING, verified here rather than assumed: RegisterAutosave
 * already unregisters both its hooks unconditionally before conditionally
 * re-registering them (SavingEnhancements.cpp), the same shape as COND_HOOK —
 * so re-arming is idempotent by construction. Leg 6 proves it: it drives
 * RegisterAutosave twice with the CVar ON and asserts the settled count stays
 * 1 both times, not 2. No new policy was invented for this; the existing
 * registrar was already written the idempotent way, it just was not wired
 * into the map that makes idempotence matter.
 *
 * VERIFIED RED BEFORE GREEN (leg 6): with the `RegisterShipInitFunc`
 * registration removed from SavingEnhancements.cpp (RegisterAutosave's own
 * #614 fix), leg 6 fails FAIL(6) — MM has no registrar under the converged
 * Autosave key at all, so re-arming it is a no-op by construction.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "ShipInit.hpp"
#include <libultraship/bridge/consolevariablebridge.h>

#include <cstdio>
#include <cstring>

// MM-side probe support (games/mm/2s2h/mm_shipinit_driver_test.cpp) and the
// production bridge's non-inserting map query
// (games/mm/2s2h/ShipInitBridge_SingleExe.cpp).
extern "C" {
void MMShipInitTest_ResetProbes(void);
int MMShipInitTest_CVarProbeRuns(void);
int MMShipInitTest_RandoProbeRuns(void);
const char* MMShipInitTest_ProbePath(void);
void MMShipInitTest_DumpCVarKeys(void);
int MM_ShipInit_RegistrarCountForPath(const char* path);
void MM_ShipInit_OnCVarChanged(const char* path);
// #614: real-registrar settled-count probe (games/mm/2s2h/mm_shipinit_driver_test.cpp).
int MMShipInitTest_AutosaveDrawFinishSettledCount(void);
}

namespace {

// The converged key #539 names first. Spelled here rather than included from
// MM's SavingEnhancements.cpp (which this TU must not see) — the point of the
// leg is that BOTH games agree on this literal, so a divergence in either
// game's spelling is exactly what should turn it red.
constexpr const char* kConvergedKey = "gEnhancements.RememberSaveLocation";

// #614: the Autosave leg's converged key, spelled here for the same reason —
// this TU must not see MM's SavingEnhancements.cpp, so a spelling drift
// between the two games shows up as a red leg 6 rather than a silent pass.
constexpr const char* kAutosaveKey = "gEnhancements.Autosave";

int Fail(int code, const char* msg) {
    printf("[TEST] FAIL(%d): %s\n", code, msg);
    return code;
}

} // namespace

extern "C" int OoT_ShipInitMMDriver_RunHeadless(void) {
    // ---- leg 1: the converged key really has MM registrars behind it -------
    // If this is zero, either MM's SavingEnhancements TU was elided from the
    // link (the #516 class) or the two games' spellings of the converged key
    // drifted. Either way the driver would be re-arming nothing and every
    // other leg here would be theatre.
    const int convergedRegistrars = MM_ShipInit_RegistrarCountForPath(kConvergedKey);
    if (convergedRegistrars <= 0) {
        printf("[TEST] MM registrar count for '%s' is %d\n", kConvergedKey, convergedRegistrars);
        MMShipInitTest_DumpCVarKeys();
        return Fail(1, "no MM registrar is keyed on the converged enhancement key — either MM's TU is elided or the "
                       "two games' spellings drifted, and re-arming MM would be a no-op (#539)");
    }

    const char* probePath = MMShipInitTest_ProbePath();

    // ---- leg 2: THE FIX. OoT's Init reaches MM's map. ---------------------
    // This is the exact call `UIWidgets::CVarCheckbox` and friends make after
    // writing the CVar (games/oot/soh/SohGui/UIWidgets.cpp), so the lock rides
    // the production path rather than a stand-in for it.
    MMShipInitTest_ResetProbes();
    ShipInit::Init(probePath);
    if (MMShipInitTest_CVarProbeRuns() <= 0) {
        return Fail(2, "a CVar change through OoT's ShipInit::Init did not run MM's registrar for the same key — "
                       "MM's map still has no CVar-change driver (#539)");
    }

    // ---- leg 3: the "*" pseudo-path is NOT forwarded ----------------------
    // Non-vacuous: RegisterShipInitFunc pushes every registrant into "*" too,
    // so the CVar probe IS under it. OoT drives "*" from boot and from preset
    // loads; forwarding it would run MM's whole registrar set before MM has
    // booted and again on every OoT preset load.
    MMShipInitTest_ResetProbes();
    MM_ShipInit_OnCVarChanged("*");
    if (MMShipInitTest_CVarProbeRuns() != 0) {
        return Fail(3, "the '*' wildcard was forwarded to MM's map — OoT's boot and preset paths would run MM's "
                       "whole registrar set, including before MM has booted (#539)");
    }

    // ---- leg 4: the "IS_RANDO" pseudo-path is NOT forwarded ---------------
    // Non-vacuous via the MM-side IS_RANDO probe. MM drives its own IS_RANDO
    // from Rando/Rando.cpp on MM's save-load chain; forwarding OoT's would arm
    // MM's rando hooks against MM's previous save on an OoT-only file load,
    // which is what the #439 arm-state locks exist to prevent.
    MMShipInitTest_ResetProbes();
    MM_ShipInit_OnCVarChanged("IS_RANDO");
    if (MMShipInitTest_RandoProbeRuns() != 0) {
        return Fail(4, "'IS_RANDO' was forwarded to MM's map — an OoT rando file load would re-arm MM's rando hooks "
                       "off MM's stale save state (#539)");
    }

    // ---- leg 5: a key MM does not register on does not grow MM's map ------
    // ShipInit::Init indexes with operator[], which inserts. Forwarding every
    // OoT-only key a player ever touches through that would accrete one empty
    // vector per key for the life of the process.
    const char* oneOffKey = "gTest.ShipInitDriverKeyMMNeverRegisters";
    MM_ShipInit_OnCVarChanged(oneOffKey);
    if (MM_ShipInit_RegistrarCountForPath(oneOffKey) != 0) {
        return Fail(5, "forwarding an unregistered key inserted it into MM's map");
    }

    // Leave the counters clean for whatever runs next in AllTests.
    MMShipInitTest_ResetProbes();

    // ---- leg 6: the Autosave CVar re-arms MM's REAL RegisterAutosave (#614) ---
    // Legs 1-5 above never invoke a production registrar with live side
    // effects — leg 1 only counts registrars keyed on the RememberSaveLocation
    // key, it never calls Init on it. This leg is different on purpose: #614's
    // bug was that RegisterAutosave sat OUTSIDE MM's map, so counting alone
    // cannot distinguish "registered but never re-armed" from "never
    // registered at all" the way it can for a key with a synthetic probe
    // sitting next to the real registrar. Only running the real thing and
    // reading its own settled registry state proves the map entry exists AND
    // that the driver reaches it.
    //
    // Reality check first, mirroring leg 1: a registrar really is keyed on the
    // converged Autosave path. If this is 0, #614 either regressed or the two
    // games' spellings of the converged key drifted, and everything below
    // would be exercising a no-op.
    const int autosaveRegistrars = MM_ShipInit_RegistrarCountForPath(kAutosaveKey);
    if (autosaveRegistrars <= 0) {
        printf("[TEST] MM registrar count for '%s' is %d\n", kAutosaveKey, autosaveRegistrars);
        MMShipInitTest_DumpCVarKeys();
        return Fail(6, "no MM registrar is keyed on the converged Autosave key — RegisterAutosave is still outside "
                       "MM's ShipInit map and re-arming it is a no-op (#614)");
    }

    // Drive with the CVar OFF first and settle: RegisterAutosave's hooks must
    // not be left registered from an earlier state (this process's — or an
    // earlier --test-all row's — prior CVar value).
    CVarSetInteger(kAutosaveKey, 0);
    MM_ShipInit_OnCVarChanged(kAutosaveKey);
    const int settledOff = MMShipInitTest_AutosaveDrawFinishSettledCount();
    if (settledOff != 0) {
        printf("[TEST] OnGameStateDrawFinish settled count with Autosave OFF: %d\n", settledOff);
        return Fail(7, "RegisterAutosave left an OnGameStateDrawFinish registrant behind with the Autosave CVar "
                       "OFF (#614)");
    }

    // THE FIX. Turn it ON through the exact call the unified menu makes
    // (CVarSetInteger, then the same ShipInit::Init(path) entry point leg 2
    // proved reaches MM). Before #614, MM's map had no entry here at all, so
    // this call is a lookup miss — settledOn stays 0, not 1.
    CVarSetInteger(kAutosaveKey, 1);
    MM_ShipInit_OnCVarChanged(kAutosaveKey);
    const int settledOn = MMShipInitTest_AutosaveDrawFinishSettledCount();
    if (settledOn != 1) {
        printf("[TEST] OnGameStateDrawFinish settled count with Autosave ON: %d\n", settledOn);
        return Fail(8, "changing the Autosave CVar through the unified menu path did not re-arm MM's registrar — "
                       "RegisterAutosave is still outside MM's ShipInit map (#614)");
    }

    // ARM-STATE CHECK: re-drive with the CVar unchanged (a second click, or
    // AllTests re-running this row). RegisterAutosave unregisters both hooks
    // unconditionally before conditionally re-registering them — the same
    // shape as COND_HOOK — so this must settle back to 1, never stack to 2.
    MM_ShipInit_OnCVarChanged(kAutosaveKey);
    const int settledOnAgain = MMShipInitTest_AutosaveDrawFinishSettledCount();
    if (settledOnAgain != 1) {
        printf("[TEST] OnGameStateDrawFinish settled count after a second re-arm: %d\n", settledOnAgain);
        return Fail(9, "re-arming MM's Autosave registrar stacked hooks instead of staying idempotent (#614)");
    }

    // Leave it disarmed for whatever runs next in AllTests.
    CVarSetInteger(kAutosaveKey, 0);
    MM_ShipInit_OnCVarChanged(kAutosaveKey);

    printf("[TEST] PASS: a CVar change on the unified menu path re-arms MM's registrars (%d on the converged key), "
           "while the '*' and 'IS_RANDO' pseudo-paths stay MM-driven; the real Autosave registrar (%d on its "
           "converged key) re-arms idempotently too (#614)\n",
           convergedRegistrars, autosaveRegistrars);
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
