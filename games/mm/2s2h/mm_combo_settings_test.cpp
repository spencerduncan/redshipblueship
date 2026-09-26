/**
 * ROM-free lock for the COMBO-LEVEL arrival identity gate (ADR 0011 increment
 * 1, #498). CTest label "redship" (display-free); row `mm-combo-settings-gate`
 * in src/common/test_runner.cpp.
 *
 * Lives MM-side because the surface under test is the REAL arrival gate
 * (`MM_Rando_GateCrossGameArrival` + `MM_Rando_HydrateCrossGameArrival`), the
 * pair the Happy Mask Shop hand-off runs through
 * (games/mm/2s2h/GameExports_SingleExe.cpp, called from
 * MM_Play_ConsumeStartupEntrance) — and because reaching its combo leg means
 * first satisfying its MM-profile leg, which needs MM's option tables.
 *
 * ============================================================================
 * WHAT THIS LOCKS, AND WHY IT IS THE POINT OF THE CARVE
 * ============================================================================
 *
 * ADR 0011 spends 16 bytes on a RECORD instead of ADR 0009's reserved 4-byte
 * digest, and one of the two justifications is that a refusal can then say
 * WHICH rule diverged. The ADR itself records the disposition: that
 * justification only stands if something BUILDS the capability, so
 * `Combo_ComboSettingsDivergence` is a scheduled increment-1 task with a test
 * lock that asserts the refusal NAMES the diverged field rather than merely
 * refusing. This is that lock. Without it the twelve bytes buy display alone.
 *
 *   leg 1 — direction alone diverges  -> REFUSED, and the toast names
 *                                        "direction"; the frozen record is NOT
 *                                        self-healed and no world is generated
 *   leg 2 — a legacy (formatVersion 0) paired file taken through one crossing
 *                                     -> comes back at formatVersion 1 with the
 *                                        shipped defaults and a nonzero
 *                                        fingerprint, and is NOT refused
 *   leg 3 — the same file taken through a SECOND crossing
 *                                     -> compares rather than re-freezing
 *   leg 4 — a frozen TRIFORCE HUNT (ADR 0010 O10) whose MM half is not what
 *           MM's option CVars resolve now
 *                                     -> the refusal NAMES "triforceHunt";
 *                                        the same world whose MM half does
 *                                        match refuses without that name.
 *           No session can author the goal yet, so both halves of the leg
 *           also carry the "goal" field divergence; the leg is about which
 *           refusal names triforceHunt, which only the gate's MM half
 *           compare (GameExports_SingleExe.cpp) can decide.
 *
 * NON-VACUITY. A gate that refused everything would pass leg 1 and prove
 * nothing. Two things close that, in different places and deliberately so:
 * leg 2 drives the same gate to completion WITHOUT a refusal, and the
 * `combo-settings-divergence` row (src/common/tests/test_combo_settings.c)
 * proves the diff returns 0 for a healthy pair — which is decisive here
 * because the gate's condition is literally
 * `Combo_ComboSettingsDivergence() != 0`.
 *
 * WHY LEG 2/3 PASS hadFrozenState = 1. The transitional writer sits ahead of
 * every "this MM save already exists" early return on purpose (all of them are
 * still crossings, and a legacy pair that always has a restored MM session
 * would otherwise never freeze at all). Passing 1 exercises exactly that
 * placement AND keeps this row ROM-free — and since ADR 0010 increment 2 the
 * arrival cannot dispatch a fill at all, which every leg asserts through
 * MM_Rando_OnSaveInitDispatchCount rather than assuming.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

#include "Rando/Rando.h"
#include "Rando/StaticData/StaticData.h"

#include <libultraship/bridge/consolevariablebridge.h>

// src/common — outside any extern "C" block; these headers manage their own
// linkage (matching Foreign.cpp / mm_spoiler_identity_test.cpp).
#include "foreign_items.h"
#include "save.h"                  // the #533 REFUSED surface this gate reports through
#include "notification_bridge.h"   // the player-visible half of that surface
#include "combo_mm_options_view.h" // MM_Rando_ComputeProfileStamp — the profile leg's input
#include "triforce_hunt.h"         // Combo_TriforceFreezeAtCreation — leg 4's frozen record

extern "C" {
#include "variables.h"
// ADR 0010 increment 2 split the single arrival function in two: the GATE
// compares (and runs the O5 transitional freeze) BEFORE the frozen MM half is
// consumed, and the HYDRATE half repairs/reports/refuses-a-missing-half after.
// RunArrival below drives them in z_play.c's exact order.
int MM_Rando_GateCrossGameArrival(void);
void MM_Rando_HydrateCrossGameArrival(int hadFrozenState, int refused);
// The generation entry point's dispatch counter. ADR 0010 increment 2 deletes
// generation from the arrival outright, and this is the observable that keeps it
// deleted: every leg below asserts it did not move.
uint32_t MM_Rando_OnSaveInitDispatchCount(void);
}

namespace {

int Fail(int code, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[MM-COMBO-SETTINGS] FAIL(%d): ", code);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    fflush(stderr);
    return code;
}

// Drive an arrival exactly as MM_Play_ConsumeStartupEntrance does — gate, then
// (unless refused) the consume, then hydrate — and ASSERT that no generation was
// dispatched along the way. `hadFrozenState` stands in for
// Combo_ConsumeFrozenState's return: this row never restores a real blob, so it
// passes the answer directly the way the pre-split test passed it to the single
// function.
//
// @return 1 when the gate REFUSED, 0 otherwise; -1 when generation was
//         dispatched, which is a contract violation rather than an outcome.
int RunArrival(int hadFrozenState) {
    const uint32_t beforeDispatches = MM_Rando_OnSaveInitDispatchCount();
    const int refused = MM_Rando_GateCrossGameArrival();
    MM_Rando_HydrateCrossGameArrival(hadFrozenState, refused);
    if (MM_Rando_OnSaveInitDispatchCount() != beforeDispatches) {
        Fail(99, "the arrival DISPATCHED GENERATION (OnSaveInit) — ADR 0010 increment 2 deletes that dispatch; the "
                 "MM half is authored at OoT's file-create seam and the arrival may only hydrate or refuse");
        return -1;
    }
    return refused;
}

constexpr int kSlot = 0;
const char* const kScratchSaveDir = "rsbs_test_saves_combo_settings";

// Planted as the "last toast" before every leg: the overlay's store has no
// drain entry point, so "a toast exists" would otherwise be satisfiable by an
// earlier leg's — the exact vacuous pass mm_spoiler_identity_test.cpp closes
// the same way.
const char* const kToastSentinel = "rsbs498-no-toast-was-emitted";

constexpr uint32_t kSeed = 0x0498C0DEu;
constexpr uint32_t kSettingsHash = 0x5E770011u;

/** Publish the live pairing carrier the way Playthrough_Init stamps it, with an
 *  MM profile digest that MATCHES what this session resolves — otherwise the
 *  profile leg refuses first and the combo leg is never reached. */
void ArmPairing() {
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = kSeed;
    gComboCtx.sharedRandoSettingsHash = kSettingsHash;
    gComboCtx.mmProfileDigest = MM_Rando_ComputeProfileStamp();
}

/** The bootstrap state the arrival gate expects: a vanilla MM save with no
 *  rando world under it (what TitleSetup's MM_Sram_InitNewSave authors). */
void ArmVanillaBootstrapSave() {
    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_VANILLA;
    gSaveContext.save.shipSaveInfo.rando.finalSeed = 0;
}

void ResetRefusalSurface() {
    RsbsSave_ResetSlotSessionState();
    RsbsSave_SetActiveSlot(kSlot);
    RsbsSave_ArmSlotOnCreate(kSlot);

    ComboNotification sentinel;
    memset(&sentinel, 0, sizeof(sentinel));
    sentinel.prefix = "";
    sentinel.message = kToastSentinel;
    sentinel.remainingTime = 1.0f;
    // Muted for the same reason the refusal toasts are: the overlay's ding is
    // OoT's Audio_PlaySoundGeneral, and this runs display-free.
    sentinel.mute = 1;
    OoT_Notification_Emit(&sentinel);
}

/** The refusal surface as a whole: latched slot, RSBS_REFUSE_IDENTITY, no
 *  quarantine (the .redsave FILE is healthy — the SESSION diverged), and a
 *  toast that NAMES @p term. */
int AssertRefusedNaming(int baseCode, const char* leg, const char* term) {
    if (RsbsSave_IsSlotWritable(kSlot) != 0) {
        return Fail(baseCode,
                    "%s: the active slot was not latched — a session running different cross-game rules can still "
                    "capture itself into the healthy pair's .redsave",
                    leg);
    }
    if (RsbsSave_GetSlotRefuseReason(kSlot) != (int)RSBS_REFUSE_IDENTITY) {
        return Fail(baseCode + 1, "%s: refusal reason is %d, expected RSBS_REFUSE_IDENTITY", leg,
                    RsbsSave_GetSlotRefuseReason(kSlot));
    }
    if (RsbsSave_HasQuarantine(kSlot) != 0) {
        return Fail(baseCode + 2, "%s: an identity refusal quarantined the slot file — the file is healthy", leg);
    }

    ComboNotification toast;
    memset(&toast, 0, sizeof(toast));
    if (OoT_Notification_PeekLastForTest(&toast) != 1) {
        return Fail(baseCode + 3, "%s: no toast reached the shared overlay — stderr is not a player-visible surface",
                    leg);
    }
    const std::string message = toast.message != nullptr ? toast.message : "";
    const std::string prefix = toast.prefix != nullptr ? toast.prefix : "";
    if (message == kToastSentinel) {
        return Fail(baseCode + 3, "%s: the overlay still holds this leg's sentinel — no refusal toast was emitted",
                    leg);
    }
    if (prefix.find("REFUSED") == std::string::npos) {
        return Fail(baseCode + 3, "%s: the toast's prefix ('%s') does not read as a refusal", leg, prefix.c_str());
    }
    if (message.find(term) == std::string::npos) {
        return Fail(baseCode + 4,
                    "%s: the refusal does not NAME the diverged rule '%s' (message: '%s'). A refusal that cannot "
                    "say which rule diverged is the un-repairable case ADR 0009 accepted only for want of an "
                    "alternative — and it is the whole reason ADR 0011 carves a record instead of a digest",
                    leg, term, message.c_str());
    }
    return 0;
}

} // namespace

extern "C" int MM_ComboSettingsGate_RunHeadless(void) {
    printf("[TEST] mm-combo-settings-gate: the arrival refuses a divergent combo record and NAMES the rule; a "
           "legacy pair freezes the shipped defaults instead (#498, ADR 0011)\n");

    rsbs::SaveManager::Instance().SetSaveDirectory(kScratchSaveDir);

    // ------------------------------------------------------------------------
    // Leg 1 — `direction` alone diverges: REFUSED, naming the field.
    // ------------------------------------------------------------------------
    {
        ComboContext_Init();
        ArmPairing();
        ArmVanillaBootstrapSave();
        ResetRefusalSurface();

        // Freeze a record that differs from the live resolution in ONE field, so
        // a refusal that named several fields (or a generic one) is a fail.
        // Frozen through the real freeze so the fingerprint stays consistent —
        // otherwise the fingerprint bit would fire too and the leg would pass
        // for the wrong reason.
        ComboSettingsRecord divergent;
        Combo_ResolveComboSettings(&divergent);
        divergent.direction = (uint8_t)RSBS_COMBO_DIR_FORWARD; // live resolves BOTH
        Combo_FreezeComboSettings(&divergent);

        const uint32_t bits = Combo_ComboSettingsDivergence();
        if (bits != RSBS_COMBO_DIVERGE_DIRECTION) {
            return Fail(1, "leg 1 setup: expected exactly the direction bit, got %04X", (unsigned)bits);
        }

        if (RunArrival(/*hadFrozenState=*/0) < 0) {
            return 99;
        }

        const int rc = AssertRefusedNaming(10, "leg 1 (direction diverged)", "direction");
        if (rc != 0) {
            return rc;
        }
        // NEVER self-healed: overwriting the frozen record with the divergent
        // resolution would make every divergence disappear the instant it was
        // detected.
        if (gComboCtx.comboSettings.direction != (uint8_t)RSBS_COMBO_DIR_FORWARD) {
            return Fail(15, "leg 1: the refusal SELF-HEALED the frozen record — divergence is corruption to "
                            "refuse, never a value to overwrite");
        }
        // And no world was authored under the divergent rules.
        if (gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO) {
            return Fail(16, "leg 1: a paired MM world was generated under refused rules");
        }
    }

    // ------------------------------------------------------------------------
    // Leg 2 — a LEGACY paired file freezes the shipped defaults at its first
    // crossing (accepted answer O5), and is NOT refused.
    // ------------------------------------------------------------------------
    {
        ComboContext_Init();
        ArmPairing();
        ArmVanillaBootstrapSave();
        ResetRefusalSurface();

        if (Combo_ComboSettingsFrozen()) {
            return Fail(20, "leg 2 setup: the record must start ABSENT (a pre-ADR-0011 pair)");
        }

        // hadFrozenState: a restored MM session — the path the transitional
        // writer has to sit AHEAD of, because a legacy pair that always restores
        // an existing MM save would otherwise stay at formatVersion 0 forever,
        // permanently exempt from comparison.
        if (RunArrival(/*hadFrozenState=*/1) < 0) {
            return 99;
        }

        if (!Combo_ComboSettingsFrozen()) {
            return Fail(21, "leg 2: one crossing did not freeze a legacy pair — 4.4 would describe a behaviour "
                            "nothing builds, and every pre-carve file would stay exempt from comparison forever");
        }
        if (gComboCtx.comboSettings.formatVersion != (uint8_t)RSBS_COMBO_SETTINGS_FORMAT_VERSION) {
            return Fail(22, "leg 2: the transitional write did not stamp the current format version");
        }
        ComboSettingsRecord defaults;
        Combo_ComboSettingsDefaults(&defaults);
        if (memcmp(&gComboCtx.comboSettings, &defaults, sizeof(defaults)) != 0) {
            return Fail(23, "leg 2: the transitional write did not freeze the SHIPPED DEFAULTS");
        }
        if (gComboCtx.comboSettingsHash == 0) {
            return Fail(24, "leg 2: the transitional write left the fingerprint at 0 (= 'not frozen')");
        }
        if (Combo_ComboSettingsDivergence() != 0) {
            return Fail(25, "leg 2: the freshly frozen legacy record reports divergence against its own session");
        }
        if (RsbsSave_IsSlotWritable(kSlot) != 1) {
            return Fail(26, "leg 2: a legacy pair was REFUSED — that orphans every already-written paired "
                            ".redsave to detect a divergence that cannot have happened");
        }

        // --------------------------------------------------------------------
        // Leg 3 — a SECOND crossing COMPARES rather than re-freezing.
        // --------------------------------------------------------------------
        const uint32_t frozenHash = gComboCtx.comboSettingsHash;
        if (RunArrival(/*hadFrozenState=*/1) < 0) {
            return 99;
        }
        if (gComboCtx.comboSettingsHash != frozenHash ||
            memcmp(&gComboCtx.comboSettings, &defaults, sizeof(defaults)) != 0) {
            return Fail(30, "leg 3: a second crossing rewrote the frozen record — the transitional writer became a "
                            "self-heal");
        }
        if (RsbsSave_IsSlotWritable(kSlot) != 1) {
            return Fail(31, "leg 3: a second crossing of a healthy pair was refused");
        }
    }

    // ------------------------------------------------------------------------
    // Leg 4 — the O10 triforce record's MM half, compared at the arrival.
    // ------------------------------------------------------------------------
    {
        // MM's hunt OFF in the CVars (the option defaults), so MM's live half
        // resolves to (0, 0) and the profile stamp below is taken over it.
        CVarClear(Rando::StaticData::Options[RO_SHUFFLE_TRIFORCE_PIECES].cvar);
        CVarClear(Rando::StaticData::Options[RO_TRIFORCE_PIECES_MAX].cvar);
        CVarClear(Rando::StaticData::Options[RO_TRIFORCE_PIECES_REQUIRED].cvar);
        const ComboTriforceHalf ootHalf = { 5, 3 };
        const ComboTriforceHalf mmOff = { 0, 0 };
        const ComboTriforceHalf mmMoved = { 4, 3 };

        for (int moved = 0; moved <= 1; moved++) {
            ComboContext_Init();
            ArmPairing();
            ArmVanillaBootstrapSave();
            ResetRefusalSurface();
            ComboSettingsRecord hunt;
            Combo_ResolveComboSettings(&hunt);
            hunt.goal = (uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT;
            Combo_FreezeComboSettings(&hunt);
            if (Combo_TriforceFreezeAtCreation(&ootHalf, moved != 0 ? &mmMoved : &mmOff) != RSBS_TRIFORCE_OK ||
                !Combo_TriforceRecordPresent(&gComboCtx.comboTriforce)) {
                return Fail(40, "leg 4 setup: the triforce-hunt record did not freeze");
            }
            if ((Combo_ComboSettingsDivergence() & RSBS_COMBO_DIVERGE_TRIFORCE) != 0) {
                return Fail(41, "leg 4 setup: the stored record already diverges before the MM half is compared");
            }

            if (RunArrival(/*hadFrozenState=*/0) < 0) {
                return 99;
            }
            if (moved != 0) {
                const int rc = AssertRefusedNaming(42, "leg 4 (MM's triforce half moved)", "triforceHunt");
                if (rc != 0) {
                    return rc;
                }
            } else {
                // Refused (by the goal no session can author yet), but NOT for
                // the triforce record: MM's half matches what MM resolves.
                const int rc = AssertRefusedNaming(47, "leg 4 (MM's triforce half matches)", "goal");
                if (rc != 0) {
                    return rc;
                }
                ComboNotification toast;
                memset(&toast, 0, sizeof(toast));
                OoT_Notification_PeekLastForTest(&toast);
                const std::string message = toast.message != nullptr ? toast.message : "";
                if (message.find("triforceHunt") != std::string::npos) {
                    return Fail(52,
                                "leg 4: an MM half that MATCHES the frozen record was refused as triforceHunt "
                                "(message: '%s')",
                                message.c_str());
                }
            }
        }
    }

    ComboContext_Init();
    RsbsSave_ResetSlotSessionState();
    printf("[TEST] PASS: the arrival gate refuses a divergent combo record by name and freezes a legacy pair's "
           "shipped defaults\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
