/**
 * ROM-free lock for LOADING a paired file whose rules the session walked away
 * from (#781). CTest label "redship" (display-free); row `paired-load-restore`
 * in src/common/test_runner.cpp.
 *
 * ============================================================================
 * THE DEFECT
 * ============================================================================
 *
 * One-game semantics: every combo-level decision freezes when the paired file
 * is created. But the two "frozen" predicates (Combo_ComboSettingsFrozen,
 * Combo_MMProfileFrozen) read the RESIDENT identity, which a fresh launch and
 * the return to the title screen both drop, so Combo > Cross-Game Rules, MM
 * Randomizer and MM Tricks are editable between sessions. A player who changes
 * one "for the next file" and then loads an existing paired file met:
 *
 *  - a changed Cross-Game Rule: SaveManager::LoadSlot compared the rules by
 *    field and returned RSBS_LOAD_REFUSED on stderr only; the one production
 *    caller (OoT's OnLoadFile seam, games/oot/soh/SaveManager.cpp) discards the
 *    return, so the OoT file opened with the pairing identity already dropped
 *    by Context_InvalidateSessionOnSlotLoad. No toast; the next MM arrival
 *    silently took the no-paired-world leg: a paired file played unpaired.
 *  - a changed MM option or trick: nothing at load (the MM profile was not
 *    compared there at all); the file loaded, and the first crossing into
 *    Termina was REFUSED mid-play (MM_Rando_GateCrossGameArrival), Termina
 *    vanilla, the slot latched.
 *
 * ============================================================================
 * THE RULE THIS LOCKS: FROZEN WINS AT LOAD
 * ============================================================================
 *
 * The file's rules are the rules; the live keys are staging for the next file.
 * A load whose keys diverge in a way the file can answer puts the file's values
 * back into the keys and says so on the notification overlay, and the arrival
 * then agrees with the file. What the file cannot answer is still refused (a
 * cross-game field no key authors) or flagged (an MM identity input the file
 * does not record), visibly.
 *
 *   leg 1 - Goal and Crossing Direction changed at the title screen, the file
 *           loaded through the production seam's own calls
 *                -> the pair is restored, the keys hold the file's values, one
 *                   toast names "Goal, Crossing Direction", the slot is
 *                   writable, and the arrival gate does not refuse
 *   leg 2 - one MM trick and one MM option changed, same load
 *                -> the MM profile digest matches the file again, both keys hold
 *                   the file's values, a toast names both rows, and the arrival
 *                   gate does not refuse
 *   leg 3 - the .redsave round trip in a process where Majora's Mask never
 *           booted: nothing changed -> loads, pairs, no toast, no key written;
 *           the same with an EMPTY MM half (a file saved before MM ever ran)
 *           -> the same; and that empty-half file with a trick changed -> the
 *           file cannot restore it, so the load pairs, a refusal-coloured toast
 *           warns now, nothing is written, and the arrival gate still refuses
 *           (the last line of defence)
 *   leg 4 - a record field no key authors (the logic rung, only another build
 *           can write it) -> REFUSED, not committed, not quarantined, and a
 *           REFUSED toast names "logicRung"
 *
 * Every leg loads through Context_InvalidateSessionOnSlotLoad +
 * RsbsSave_SetActiveSlot + RsbsSave_LoadSlotChecked, the OnLoadFile seam's exact
 * sequence, and asserts STATE, never the return value alone: the production
 * caller discards it, which is the whole reason the old refusal was invisible.
 *
 * MM-side because the MM half is authored the way creation authors it
 * (Rando::Foreign::ResolvePairedProfile writes the options and tricks into the
 * save) and because the arrival gate is MM's.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "Rando/Rando.h"
#include "Rando/Foreign.h"
#include "Rando/StaticData/StaticData.h"

#include <libultraship/bridge/consolevariablebridge.h>

#include "foreign_items.h"
#include "save.h"
#include "notification_bridge.h"
#include "combo_settings_view.h"
#include "combo_mm_options_view.h"
#include "combo_mm_tricks_view.h"
#include "combo_mm_options_page.h"
#include "game.h"

extern "C" {
#include "variables.h"
int MM_Rando_GateCrossGameArrival(void);
}

namespace {

constexpr int kSlot = 0;
const char* const kScratchSaveDir = "rsbs_test_saves_paired_load_restore";
const char* const kToastSentinel = "rsbs781-no-toast-was-emitted";
constexpr uint32_t kSeed = 0x0781C0DEu;
constexpr uint32_t kSettingsHash = 0x5E770781u;

int Fail(int code, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[PAIRED-LOAD-RESTORE] FAIL(%d): ", code);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    fflush(stderr);
    return code;
}

void PlantSentinel() {
    ComboNotification sentinel;
    memset(&sentinel, 0, sizeof(sentinel));
    sentinel.prefix = "";
    sentinel.message = kToastSentinel;
    sentinel.remainingTime = 1.0f;
    sentinel.mute = 1;
    OoT_Notification_Emit(&sentinel);
}

struct Toast {
    bool any = false;
    std::string prefix;
    std::string message;
};

Toast LastToast() {
    Toast t;
    ComboNotification n;
    memset(&n, 0, sizeof(n));
    if (OoT_Notification_PeekLastForTest(&n) == 1) {
        t.prefix = n.prefix != nullptr ? n.prefix : "";
        t.message = n.message != nullptr ? n.message : "";
        t.any = t.message != kToastSentinel;
    }
    return t;
}

void Relaunch();

/** Every key this row writes, back to unset. The writers refuse while a world
 *  is resident, so the session is dropped first, as the title screen does. */
void ClearAuthoredKeys() {
    Relaunch();
    for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
        Combo_ComboSettingClear((ComboSettingId)i);
    }
    for (int i = 0; i < Combo_MMOptionCount(); i++) {
        Combo_MMOptionClear(Combo_MMOptionAt(i));
    }
    for (int i = 0; i < Combo_MMTrickCount(); i++) {
        Combo_MMTrickClear(Combo_MMTrickAt(i));
    }
}

/** A fresh launch: nothing resident, no slot armed. */
void Relaunch() {
    RsbsSave_ResetSlotSessionState();
    ComboContext_Init();
    Context_InitFrozenStates();
}

/**
 * Create and save a paired file from whatever the keys hold now, the way the
 * creation event does it: stamp the pairing identity and the MM profile digest,
 * author the MM half (ResolvePairedProfile writes the resolved options and trick
 * set into the save), freeze the combo record, commit the slot. With
 * @p emptyMmHalf the MM shadow stays all-zero instead: a file saved before
 * Majora's Mask ever ran.
 */
int CreatePairedFile(bool emptyMmHalf, const ComboSettingsRecord* recordOverride) {
    Relaunch();
    RsbsSave_DeleteSave(kSlot); // erasing arms the slot for its first write (#533)

    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = kSeed;
    gComboCtx.sharedRandoSettingsHash = kSettingsHash;
    gComboCtx.mmProfileDigest = MM_Rando_ComputeProfileStamp();

    if (!emptyMmHalf) {
        gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
        gSaveContext.save.shipSaveInfo.rando.finalSeed = kSeed;
        Rando::Foreign::ResolvePairedProfile(/*paired=*/true);
        Context_UpdateShadowCopy(GAME_MM, &gSaveContext, sizeof(gSaveContext));
    }
    // OoT's half is a stand-in: every load below passes generation 0 (a .sav
    // with no mirrored generation, exempt), so this blob is never armed.
    std::vector<uint8_t> oot(OOT_SAVE_CONTEXT_SIZE, 0x5A);
    Context_UpdateShadowCopy(GAME_OOT, oot.data(), oot.size());

    ComboSettingsRecord record;
    Combo_ResolveComboSettings(&record);
    if (recordOverride != nullptr) {
        record = *recordOverride;
    }
    Combo_FreezeComboSettings(&record);

    if (RsbsSave_Save(kSlot) != 1) {
        return Fail(1, "setup: the paired file did not save");
    }
    // Scribble the live MM save so nothing below can pass off leftovers: the
    // load must read the FILE's half.
    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_VANILLA;
    memset(gSaveContext.save.shipSaveInfo.rando.randoSaveOptions, 0,
           sizeof(gSaveContext.save.shipSaveInfo.rando.randoSaveOptions));
    memset(gSaveContext.save.shipSaveInfo.rando.randoSaveTricks, 0,
           sizeof(gSaveContext.save.shipSaveInfo.rando.randoSaveTricks));
    return 0;
}

/** games/oot/soh/SaveManager.cpp's OnLoadFile seam, call for call. It discards
 *  the return; so does every assertion below except the report. */
int LoadThroughProductionSeam() {
    PlantSentinel();
    Context_InvalidateSessionOnSlotLoad();
    RsbsSave_SetActiveSlot(kSlot);
    return RsbsSave_LoadSlotChecked(kSlot, 0);
}

const ComboMMTrickDesc* FirstSettableTrick() {
    for (int i = 0; i < Combo_MMTrickCount(); i++) {
        const ComboMMTrickDesc* d = Combo_MMTrickAt(i);
        if (d != nullptr && d->bound && !d->reserved) {
            return d;
        }
    }
    return nullptr;
}

bool Contains(const std::string& haystack, const char* needle) {
    return haystack.find(needle) != std::string::npos;
}

// ---------------------------------------------------------------------------
// Leg 1: Cross-Game Rules changed at the title screen.
// ---------------------------------------------------------------------------
int LegCrossGameRules() {
    ClearAuthoredKeys();
    if (Combo_ComboSettingSet(COMBO_SETTING_GOAL, (int32_t)RSBS_COMBO_GOAL_BEAT_EITHER) != 1 ||
        Combo_ComboSettingSet(COMBO_SETTING_DIRECTION, (int32_t)RSBS_COMBO_DIR_FORWARD) != 1) {
        return Fail(10, "leg 1 setup: could not author Goal and Crossing Direction for the file");
    }
    if (int rc = CreatePairedFile(false, nullptr)) {
        return rc;
    }

    // Back at the title screen the player picks the shipped rules again.
    Relaunch();
    Combo_ComboSettingSet(COMBO_SETTING_GOAL, (int32_t)RSBS_COMBO_GOAL_BEAT_BOTH);
    Combo_ComboSettingSet(COMBO_SETTING_DIRECTION, (int32_t)RSBS_COMBO_DIR_BOTH);

    const int rc = LoadThroughProductionSeam();
    const bool paired = Combo_ForeignPairingActive();
    const bool frozen = Combo_ComboSettingsFrozen();
    const int32_t goal = Combo_ComboSettingResolved(COMBO_SETTING_GOAL);
    const int32_t direction = Combo_ComboSettingResolved(COMBO_SETTING_DIRECTION);
    const int writable = RsbsSave_IsSlotWritable(kSlot);
    const Toast toast = LastToast();
    const int gate = paired ? MM_Rando_GateCrossGameArrival() : -1;
    printf("[TEST] leg 1 OBSERVED: load rc=%d (discarded by the production caller) paired=%d frozen=%d goal=%d "
           "direction=%d writable=%d toast=%s%s%s%s arrivalGate=%d\n",
           rc, paired ? 1 : 0, frozen ? 1 : 0, (int)goal, (int)direction, writable, toast.any ? "'" : "(none)",
           toast.any ? toast.prefix.c_str() : "", toast.any ? " " : "", toast.any ? toast.message.c_str() : "", gate);

    if (!paired) {
        return Fail(11, "leg 1: the paired file loaded UNPAIRED — the OoT file plays on and the next MM arrival "
                        "silently skips pairing (#781)");
    }
    if (!frozen || gComboCtx.comboSettings.goal != (uint8_t)RSBS_COMBO_GOAL_BEAT_EITHER ||
        gComboCtx.comboSettings.direction != (uint8_t)RSBS_COMBO_DIR_FORWARD) {
        return Fail(12, "leg 1: the loaded record is not the file's own");
    }
    if (goal != (int32_t)RSBS_COMBO_GOAL_BEAT_EITHER || direction != (int32_t)RSBS_COMBO_DIR_FORWARD) {
        return Fail(13, "leg 1: the live keys still hold the session's values (goal %d, direction %d) — the file's "
                        "rules must win at load",
                    (int)goal, (int)direction);
    }
    if (Combo_ComboSettingsDivergence() != 0) {
        return Fail(14, "leg 1: the loaded pair still diverges from the live resolution");
    }
    if (writable != 1 || RsbsSave_HasQuarantine(kSlot) != 0 || RsbsSave_HasSave(kSlot) != 1) {
        return Fail(15, "leg 1: the slot is latched, quarantined or gone after a restoring load");
    }
    if (!toast.any || !Contains(toast.prefix, "Cross-Game Rules restored") || !Contains(toast.message, "Goal") ||
        !Contains(toast.message, "Crossing Direction")) {
        return Fail(16, "leg 1: no toast names what the load restored (prefix '%s', message '%s')",
                    toast.prefix.c_str(), toast.message.c_str());
    }
    if (gate != 0 || RsbsSave_IsSlotWritable(kSlot) != 1) {
        return Fail(17, "leg 1: the arrival gate refused a file that loaded");
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Leg 2: one MM trick and one MM option changed at the title screen.
// ---------------------------------------------------------------------------
int LegMmProfile() {
    const ComboMMTrickDesc* trick = FirstSettableTrick();
    const ComboMMOptionDesc* hearts = Combo_MMOptionById((uint16_t)RO_STARTING_HEALTH);
    if (trick == nullptr || hearts == nullptr) {
        return Fail(20, "leg 2 setup: no settable MM trick, or no Starting Hearts row");
    }
    ClearAuthoredKeys();
    Combo_MMTrickSetValue(trick, true);
    Combo_MMOptionSetValue(hearts, 5);
    if (!Combo_MMTrickGetValue(trick) || Combo_MMOptionGetValue(hearts) != 5) {
        return Fail(21, "leg 2 setup: could not author the trick and Starting Hearts for the file");
    }
    if (int rc = CreatePairedFile(false, nullptr)) {
        return rc;
    }
    const uint32_t fileDigest = gComboCtx.mmProfileDigest;

    Relaunch();
    Combo_MMTrickSetValue(trick, false);
    Combo_MMOptionSetValue(hearts, 3);
    if (MM_Rando_ComputeProfileStamp() == fileDigest) {
        return Fail(22, "leg 2 setup: changing a trick and an option did not move the live profile digest");
    }

    const int rc = LoadThroughProductionSeam();
    const bool paired = Combo_ForeignPairingActive();
    const bool matches = MM_Rando_ComputeProfileStamp() == gComboCtx.mmProfileDigest;
    const bool trickOn = Combo_MMTrickGetValue(trick);
    const int32_t heartsNow = Combo_MMOptionGetValue(hearts);
    const int writable = RsbsSave_IsSlotWritable(kSlot);
    const Toast toast = LastToast();
    const int gate = paired ? MM_Rando_GateCrossGameArrival() : -1;
    printf("[TEST] leg 2 OBSERVED: load rc=%d paired=%d liveProfileMatchesFile=%d trick=%d startingHearts=%d "
           "writable=%d toast=%s%s%s%s arrivalGate=%d (1 = REFUSED)\n",
           rc, paired ? 1 : 0, matches ? 1 : 0, trickOn ? 1 : 0, (int)heartsNow, writable,
           toast.any ? "'" : "(none)", toast.any ? toast.prefix.c_str() : "", toast.any ? " " : "",
           toast.any ? toast.message.c_str() : "", gate);

    if (!paired || gComboCtx.mmProfileDigest != fileDigest) {
        return Fail(23, "leg 2: the load did not restore the pair's MM identity");
    }
    if (!matches) {
        return Fail(24, "leg 2: the live MM profile still differs from the file's after the load — the first "
                        "crossing into Termina will be refused mid-play (#781)");
    }
    if (!trickOn || heartsNow != 5) {
        return Fail(25, "leg 2: the trick (%d) or Starting Hearts (%d) does not hold the file's value", trickOn ? 1 : 0,
                    (int)heartsNow);
    }
    if (writable != 1) {
        return Fail(26, "leg 2: a restoring load latched the slot");
    }
    if (!toast.any || !Contains(toast.prefix, "Majora's Mask options restored") ||
        !Contains(toast.message, "Starting Hearts") || !Contains(toast.message, trick->label)) {
        return Fail(27, "leg 2: no toast names what the load restored (prefix '%s', message '%s')",
                    toast.prefix.c_str(), toast.message.c_str());
    }
    if (gate != 0 || RsbsSave_IsSlotWritable(kSlot) != 1) {
        return Fail(28, "leg 2: the arrival gate refused a file that loaded");
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Leg 3: the round trip with Majora's Mask never booted.
// ---------------------------------------------------------------------------
int LegRoundTrip() {
    for (int emptyHalf = 0; emptyHalf <= 1; emptyHalf++) {
        ClearAuthoredKeys();
        if (int rc = CreatePairedFile(emptyHalf != 0, nullptr)) {
            return rc;
        }
        const uint32_t fileDigest = gComboCtx.mmProfileDigest;
        Relaunch();

        const int rc = LoadThroughProductionSeam();
        const bool paired = Combo_ForeignPairingActive();
        const Toast toast = LastToast();
        bool anyKeyWritten = false;
        for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
            anyKeyWritten = anyKeyWritten || Combo_ComboSettingIsExplicit((ComboSettingId)i);
        }
        for (int i = 0; i < Combo_MMOptionCount(); i++) {
            anyKeyWritten = anyKeyWritten || Combo_MMOptionIsExplicit(Combo_MMOptionAt(i));
        }
        const int gate = paired ? MM_Rando_GateCrossGameArrival() : -1;
        printf("[TEST] leg 3 (%s MM half) OBSERVED: load rc=%d paired=%d toast=%s keyWritten=%d arrivalGate=%d\n",
               emptyHalf ? "empty" : "authored", rc, paired ? 1 : 0, toast.any ? toast.prefix.c_str() : "(none)",
               anyKeyWritten ? 1 : 0, gate);
        if (rc != RSBS_LOAD_OK || !paired || gComboCtx.mmProfileDigest != fileDigest) {
            return Fail(30 + emptyHalf, "leg 3: an unchanged paired file did not round-trip into a paired session");
        }
        if (toast.any) {
            return Fail(32, "leg 3: a load that changed nothing posted a toast ('%s %s')", toast.prefix.c_str(),
                        toast.message.c_str());
        }
        if (anyKeyWritten) {
            return Fail(33, "leg 3: a load that changed nothing wrote a key");
        }
        if (gate != 0) {
            return Fail(34, "leg 3: the arrival gate refused an unchanged file");
        }
    }

    // The empty-half file once more, with a trick changed. The file records no
    // options, so it cannot say what the trick was: the load pairs and warns
    // now; nothing is written; the arrival gate still refuses.
    const ComboMMTrickDesc* trick = FirstSettableTrick();
    if (trick == nullptr) {
        return Fail(35, "leg 3 setup: no settable MM trick");
    }
    Relaunch();
    Combo_MMTrickSetValue(trick, true);
    const int rc = LoadThroughProductionSeam();
    const bool paired = Combo_ForeignPairingActive();
    const Toast toast = LastToast();
    const bool trickOn = Combo_MMTrickGetValue(trick);
    const int gate = paired ? MM_Rando_GateCrossGameArrival() : -1;
    printf("[TEST] leg 3 (empty MM half, trick changed) OBSERVED: load rc=%d paired=%d trick=%d toast=%s%s%s%s "
           "arrivalGate=%d\n",
           rc, paired ? 1 : 0, trickOn ? 1 : 0, toast.any ? "'" : "(none)", toast.any ? toast.prefix.c_str() : "",
           toast.any ? " " : "", toast.any ? toast.message.c_str() : "", gate);
    if (rc != RSBS_LOAD_OK || !paired) {
        return Fail(36, "leg 3: an unrestorable MM profile refused the load or left it unpaired");
    }
    if (!trickOn) {
        return Fail(37, "leg 3: the load wrote a trick value the file does not record");
    }
    if (!toast.any || !Contains(toast.prefix, "at risk") || !Contains(toast.message, "Majora's Mask options")) {
        return Fail(38, "leg 3: an MM profile the file cannot restore was not flagged at load (prefix '%s', "
                        "message '%s')",
                    toast.prefix.c_str(), toast.message.c_str());
    }
    if (gate != 1) {
        return Fail(39, "leg 3: the arrival gate, the last line of defence, did not refuse a diverged profile");
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Leg 4: a record field no key authors.
// ---------------------------------------------------------------------------
int LegUnrestorableRule() {
    ClearAuthoredKeys();
    ComboSettingsRecord foreign;
    Combo_ResolveComboSettings(&foreign);
    foreign.logicRung = (uint8_t)(foreign.logicRung + 1u); // only a different build can have written this
    if (int rc = CreatePairedFile(false, &foreign)) {
        return rc;
    }
    Relaunch();

    const int rc = LoadThroughProductionSeam();
    const bool paired = Combo_ForeignPairingActive();
    const Toast toast = LastToast();
    printf("[TEST] leg 4 OBSERVED: load rc=%d paired=%d refuseReason=%d quarantined=%d toast=%s%s%s%s\n", rc,
           paired ? 1 : 0, RsbsSave_GetSlotRefuseReason(kSlot), RsbsSave_HasQuarantine(kSlot),
           toast.any ? "'" : "(none)", toast.any ? toast.prefix.c_str() : "", toast.any ? " " : "",
           toast.any ? toast.message.c_str() : "");
    if (rc != RSBS_LOAD_REFUSED || RsbsSave_GetSlotRefuseReason(kSlot) != (int)RSBS_REFUSE_IDENTITY ||
        RsbsSave_IsSlotWritable(kSlot) != 0) {
        return Fail(40, "leg 4: a rule no key can restore was not refused and latched");
    }
    if (Combo_ComboSettingsFrozen()) {
        return Fail(41, "leg 4: a refused load committed the record");
    }
    if (RsbsSave_HasQuarantine(kSlot) != 0 || RsbsSave_HasSave(kSlot) != 1) {
        return Fail(42, "leg 4: a healthy file was quarantined for a session divergence");
    }
    if (!toast.any || !Contains(toast.prefix, "REFUSED") || !Contains(toast.message, "logicRung")) {
        return Fail(43, "leg 4: the refusal is invisible — no toast names the rule (prefix '%s', message '%s')",
                    toast.prefix.c_str(), toast.message.c_str());
    }
    return 0;
}

} // namespace

extern "C" int MM_PairedLoadRestore_RunHeadless(void) {
    printf("[TEST] paired-load-restore: loading a paired file restores its own Cross-Game Rules and MM profile, "
           "says so, and never plays the file unpaired (#781)\n");
    if (!Combo_ComboSettingStoreAvailable()) {
        return Fail(2, "no CVar store: every leg would pass vacuously on the defaults");
    }
    Combo_MMOptionsPages_Init();
    rsbs::SaveManager::Instance().SetSaveDirectory(kScratchSaveDir);

    // The live MM SaveContext is borrowed as the creation event's scratch; put
    // it back afterwards.
    auto savedMm = std::make_unique<SaveContext>();
    memcpy(savedMm.get(), &gSaveContext, sizeof(SaveContext));

    // Every leg runs even after one fails, so a single run reports each leg's
    // observed state; the first failure is the row's result.
    int rc = 0;
    int (*const legs[])() = { LegCrossGameRules, LegMmProfile, LegRoundTrip, LegUnrestorableRule };
    for (auto leg : legs) {
        const int legRc = leg();
        if (rc == 0) {
            rc = legRc;
        }
    }

    Relaunch();
    ClearAuthoredKeys();
    RsbsSave_DeleteSave(kSlot);
    RsbsSave_ResetSlotSessionState();
    memcpy(&gSaveContext, savedMm.get(), sizeof(SaveContext));
    if (rc != 0) {
        return rc;
    }
    printf("[TEST] PASS: a paired file's own rules win at load, visibly, and the arrival agrees with a file that "
           "loaded\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
