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
 *   leg 1 - Goal, Crossing Direction, one pool size, one item-class mask and
 *           Shared Ocarina changed at the title screen, the file loaded
 *           through the production seam's own calls
 *                -> the pair is restored, all five keys hold the file's values,
 *                   one toast names "Goal, Crossing Direction +3", the slot is
 *                   writable, and the arrival gate does not refuse
 *   leg 2 - one MM trick and one MM option changed, same load
 *                -> the MM profile digest matches the file again, both keys hold
 *                   the file's values, a toast names both rows, and the arrival
 *                   gate does not refuse
 *   leg 2b - ONE trick changed, the settable trick with the longest label
 *                -> the toast names that trick (whole, or cut with "..."), never
 *                   a bare count
 *   leg 2c - EVERY settable trick changed
 *                -> the toast's shown names plus its "+N" add up to the true
 *                   number restored, however long the joined label list is
 *   leg 3 - the .redsave round trip in a process where Majora's Mask never
 *           booted: nothing changed -> loads, pairs, no toast, no key written
 *           (Cross-Game Rules, MM options AND MM tricks);
 *           the same with an EMPTY MM half (a file saved before MM ever ran)
 *           -> the same; and that empty-half file with a trick changed -> the
 *           file cannot restore it, so the load pairs, a toast
 *           warns now, nothing is written, and the arrival gate still refuses
 *           (the last line of defence)
 *   leg 4 - a record field no key authors (the logic rung, only another build
 *           can write it) -> REFUSED, not committed, not quarantined, a
 *           "Not paired:" toast in player words (no field identifier), and the
 *           next crossing into Majora's Mask says Termina stays un-randomized
 *           instead of skipping pairing silently; an unrefused unpaired
 *           arrival posts nothing (the control)
 *   leg 5 - a restore whose after-check fails (forced through the test hooks;
 *           unreachable by construction today) -> every key it wrote is put
 *           back as it was, set or unset, on both the Cross-Game Rules and the
 *           MM side
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
#include <cstdlib>
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
    } else {
        // Written explicitly: the shadow buffers outlive a session reset, so an
        // earlier leg's half would otherwise ride along into this file.
        std::vector<uint8_t> none(MM_SAVE_CONTEXT_SIZE, 0);
        Context_UpdateShadowCopy(GAME_MM, none.data(), none.size());
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

bool PlrContains(const std::string& haystack, const char* needle) {
    return haystack.find(needle) != std::string::npos;
}

bool TrickSettable(const ComboMMTrickDesc* d) {
    return d != nullptr && d->bound && !d->reserved;
}

/** The settable trick whose label is longest: the toast's hardest single name. */
const ComboMMTrickDesc* LongestSettableTrick() {
    const ComboMMTrickDesc* best = nullptr;
    for (int i = 0; i < Combo_MMTrickCount(); i++) {
        const ComboMMTrickDesc* d = Combo_MMTrickAt(i);
        if (TrickSettable(d) && d->label != nullptr &&
            (best == nullptr || std::strlen(d->label) > std::strlen(best->label))) {
            best = d;
        }
    }
    return best;
}

/** Any Cross-Game Rule, MM option or MM trick key the player could have set. */
bool AnyAuthoredKeyExplicit() {
    for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
        if (Combo_ComboSettingIsExplicit((ComboSettingId)i)) {
            return true;
        }
    }
    for (int i = 0; i < Combo_MMOptionCount(); i++) {
        if (Combo_MMOptionIsExplicit(Combo_MMOptionAt(i))) {
            return true;
        }
    }
    for (int i = 0; i < Combo_MMTrickCount(); i++) {
        const ComboMMTrickDesc* d = Combo_MMTrickAt(i);
        if (d != nullptr && Combo_CVarIsExplicitInt(d->cvar)) {
            return true;
        }
    }
    return false;
}

/** The "+N" a toast ends with, 0 without one. */
int ToastPlusCount(const std::string& message) {
    const std::size_t at = message.rfind(" +");
    return at == std::string::npos ? 0 : std::atoi(message.c_str() + at + 2);
}

/** How many names a toast message shows before its "+N" (0 for a bare count). */
int ToastShownNames(const std::string& message) {
    const std::size_t at = message.rfind(" +");
    const std::string names = at == std::string::npos ? message : message.substr(0, at);
    if (names.empty() || (names[0] >= '0' && names[0] <= '9')) {
        return 0;
    }
    int n = 1;
    for (std::size_t i = names.find(", "); i != std::string::npos; i = names.find(", ", i + 2)) {
        n++;
    }
    return n;
}

/** The toast names @p label: the whole label, or its first characters cut with
 *  "..." (at least eight of them, so a fragment is still recognisable). */
bool ToastNamesLabel(const std::string& message, const char* label) {
    const std::string whole(label);
    if (message.find(whole) != std::string::npos) {
        return true;
    }
    const std::size_t dots = message.find("...");
    if (dots == std::string::npos || dots < 8) {
        return false;
    }
    const std::size_t start = message.rfind(", ", dots);
    const std::size_t from = start == std::string::npos ? 0 : start + 2;
    const std::string fragment = message.substr(from, dots - from);
    return fragment.size() >= 8 && whole.compare(0, fragment.size(), fragment) == 0;
}

/** SoH's toast shape (docs/ui-style-guide.md section 10): the overlay draws the
 *  prefix and the message on ONE line and never wraps, so together they stay
 *  within about 53 characters. */
bool FitsOneLine(const Toast& toast) {
    return toast.prefix.size() + 1 + toast.message.size() <= 53;
}

// ---------------------------------------------------------------------------
// Leg 1: Cross-Game Rules changed at the title screen.
// ---------------------------------------------------------------------------
int LegCrossGameRules() {
    ClearAuthoredKeys();
    // Three more rules through the same table: a pool size, a class mask and
    // Shared Ocarina, each set to a legal value that is not the shipped one.
    const int32_t filePool = Combo_ComboSettingDefault(COMBO_SETTING_POOL_SIZE_OOT) == 1 ? 2 : 1;
    const int32_t fileClass =
        Combo_ComboSettingDefault(COMBO_SETTING_ITEM_CLASS_MM) == 0 ? (int32_t)RSBS_ITEMCLASS_PROGRESSION : 0;
    const int32_t fileOcarina = Combo_ComboSettingDefault(COMBO_SETTING_SHARED_OCARINA) != 0 ? 0 : 1;
    if (Combo_ComboSettingSet(COMBO_SETTING_GOAL, (int32_t)RSBS_COMBO_GOAL_BEAT_EITHER) != 1 ||
        Combo_ComboSettingSet(COMBO_SETTING_DIRECTION, (int32_t)RSBS_COMBO_DIR_FORWARD) != 1 ||
        Combo_ComboSettingSet(COMBO_SETTING_POOL_SIZE_OOT, filePool) != 1 ||
        Combo_ComboSettingSet(COMBO_SETTING_ITEM_CLASS_MM, fileClass) != 1 ||
        Combo_ComboSettingSet(COMBO_SETTING_SHARED_OCARINA, fileOcarina) != 1) {
        return Fail(10, "leg 1 setup: could not author the five rules for the file");
    }
    if (int rc = CreatePairedFile(false, nullptr)) {
        return rc;
    }

    // Back at the title screen the player picks the shipped rules again: two
    // keys set explicitly, three cleared back to unset.
    Relaunch();
    Combo_ComboSettingSet(COMBO_SETTING_GOAL, (int32_t)RSBS_COMBO_GOAL_BEAT_BOTH);
    Combo_ComboSettingSet(COMBO_SETTING_DIRECTION, (int32_t)RSBS_COMBO_DIR_BOTH);
    Combo_ComboSettingClear(COMBO_SETTING_POOL_SIZE_OOT);
    Combo_ComboSettingClear(COMBO_SETTING_ITEM_CLASS_MM);
    Combo_ComboSettingClear(COMBO_SETTING_SHARED_OCARINA);

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
        return Fail(13,
                    "leg 1: the live keys still hold the session's values (goal %d, direction %d) — the file's "
                    "rules must win at load",
                    (int)goal, (int)direction);
    }
    const int32_t pool = Combo_ComboSettingResolved(COMBO_SETTING_POOL_SIZE_OOT);
    const int32_t klass = Combo_ComboSettingResolved(COMBO_SETTING_ITEM_CLASS_MM);
    const int32_t ocarina = Combo_ComboSettingResolved(COMBO_SETTING_SHARED_OCARINA);
    printf("[TEST] leg 1 OBSERVED: poolSizeOoT=%d (file %d) itemClassMM=%d (file %d) sharedOcarina=%d (file %d)\n",
           (int)pool, (int)filePool, (int)klass, (int)fileClass, (int)ocarina, (int)fileOcarina);
    if (pool != filePool || klass != fileClass || ocarina != fileOcarina) {
        return Fail(18, "leg 1: a pool size, class mask or Shared Ocarina key does not hold the file's value");
    }
    if (Combo_ComboSettingsDivergence() != 0) {
        return Fail(14, "leg 1: the loaded pair still diverges from the live resolution");
    }
    if (writable != 1 || RsbsSave_HasQuarantine(kSlot) != 0 || RsbsSave_HasSave(kSlot) != 1) {
        return Fail(15, "leg 1: the slot is latched, quarantined or gone after a restoring load");
    }
    if (!toast.any || !PlrContains(toast.prefix, "Restored from file") || !PlrContains(toast.message, "Goal") ||
        !PlrContains(toast.message, "Crossing Direction") ||
        ToastShownNames(toast.message) + ToastPlusCount(toast.message) != 5 || !FitsOneLine(toast)) {
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
           rc, paired ? 1 : 0, matches ? 1 : 0, trickOn ? 1 : 0, (int)heartsNow, writable, toast.any ? "'" : "(none)",
           toast.any ? toast.prefix.c_str() : "", toast.any ? " " : "", toast.any ? toast.message.c_str() : "", gate);

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
    // Options before tricks, and one short line: the first restored row by name,
    // the trick counted ("+1"); stderr lists both.
    if (!toast.any || !PlrContains(toast.prefix + " " + toast.message, "Majora's Mask") ||
        !PlrContains(toast.prefix, "Restored") || !PlrContains(toast.message, "Starting Hearts") ||
        ToastPlusCount(toast.message) != 1 || !FitsOneLine(toast)) {
        return Fail(27, "leg 2: no toast names what the load restored (prefix '%s', message '%s')",
                    toast.prefix.c_str(), toast.message.c_str());
    }
    if (gate != 0 || RsbsSave_IsSlotWritable(kSlot) != 1) {
        return Fail(28, "leg 2: the arrival gate refused a file that loaded");
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Leg 2b: ONE trick changed, the one with the longest label.
// ---------------------------------------------------------------------------
int LegMmTrickOnly() {
    const ComboMMTrickDesc* trick = LongestSettableTrick();
    if (trick == nullptr) {
        return Fail(50, "leg 2b setup: no settable MM trick");
    }
    ClearAuthoredKeys();
    Combo_MMTrickSetValue(trick, true);
    if (int rc = CreatePairedFile(false, nullptr)) {
        return rc;
    }
    Relaunch();
    Combo_MMTrickClear(trick);

    const int rc = LoadThroughProductionSeam();
    const bool paired = Combo_ForeignPairingActive();
    const bool trickOn = Combo_MMTrickGetValue(trick);
    const Toast toast = LastToast();
    const int gate = paired ? MM_Rando_GateCrossGameArrival() : -1;
    printf("[TEST] leg 2b OBSERVED: trick '%s' (%zu chars) load rc=%d paired=%d trick=%d toast=%s%s%s%s "
           "arrivalGate=%d\n",
           trick->label, std::strlen(trick->label), rc, paired ? 1 : 0, trickOn ? 1 : 0, toast.any ? "'" : "(none)",
           toast.any ? toast.prefix.c_str() : "", toast.any ? " " : "", toast.any ? toast.message.c_str() : "", gate);
    if (!paired || !trickOn || gate != 0) {
        return Fail(51, "leg 2b: a trick-only change did not restore, or the arrival refused it");
    }
    if (!toast.any || !PlrContains(toast.prefix + " " + toast.message, "Majora's Mask") ||
        !ToastNamesLabel(toast.message, trick->label) || ToastPlusCount(toast.message) != 0 || !FitsOneLine(toast)) {
        return Fail(52, "leg 2b: the toast does not name the one trick it restored (prefix '%s', message '%s')",
                    toast.prefix.c_str(), toast.message.c_str());
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Leg 2c: every settable trick changed: the "+N" must be the true count.
// ---------------------------------------------------------------------------
int LegMmManyTricks() {
    ClearAuthoredKeys();
    int settable = 0;
    std::size_t joined = 0;
    for (int i = 0; i < Combo_MMTrickCount(); i++) {
        const ComboMMTrickDesc* d = Combo_MMTrickAt(i);
        if (TrickSettable(d)) {
            Combo_MMTrickSetValue(d, true);
            settable++;
            joined += std::strlen(d->label) + 2;
        }
    }
    if (settable < 2) {
        return Fail(55, "leg 2c setup: fewer than two settable MM tricks (%d)", settable);
    }
    if (int rc = CreatePairedFile(false, nullptr)) {
        return rc;
    }
    Relaunch();
    for (int i = 0; i < Combo_MMTrickCount(); i++) {
        const ComboMMTrickDesc* d = Combo_MMTrickAt(i);
        if (TrickSettable(d)) {
            Combo_MMTrickClear(d);
        }
    }

    const int rc = LoadThroughProductionSeam();
    const bool paired = Combo_ForeignPairingActive();
    const Toast toast = LastToast();
    const int shown = ToastShownNames(toast.message);
    const int plus = ToastPlusCount(toast.message);
    printf("[TEST] leg 2c OBSERVED: %d settable tricks (joined labels ~%zu chars) load rc=%d paired=%d toast=%s%s%s%s "
           "shown=%d plus=%d\n",
           settable, joined, rc, paired ? 1 : 0, toast.any ? "'" : "(none)", toast.any ? toast.prefix.c_str() : "",
           toast.any ? " " : "", toast.any ? toast.message.c_str() : "", shown, plus);
    if (!paired) {
        return Fail(56, "leg 2c: restoring every trick left the file unpaired");
    }
    if (!toast.any || shown < 1 || shown + plus != settable || !FitsOneLine(toast)) {
        return Fail(57, "leg 2c: the toast's names (%d) and '+%d' do not add up to the %d tricks restored ('%s %s')",
                    shown, plus, settable, toast.prefix.c_str(), toast.message.c_str());
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
        // Every key a page authors: Cross-Game Rules, MM options AND MM tricks.
        const bool anyKeyWritten = AnyAuthoredKeyExplicit();
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
    if (!toast.any || !PlrContains(toast.prefix, "Not restored") ||
        !PlrContains(toast.message, "Majora's Mask options") || !FitsOneLine(toast)) {
        return Fail(38,
                    "leg 3: an MM profile the file cannot restore was not flagged at load (prefix '%s', "
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

    // Control: an unpaired arrival with no refused slot (a vanilla file) says
    // nothing; the toast below is about a REFUSED paired file only.
    RsbsSave_SetActiveSlot(kSlot);
    PlantSentinel();
    const int controlGate = MM_Rando_GateCrossGameArrival();
    const Toast controlToast = LastToast();
    printf("[TEST] leg 4 control OBSERVED: unpaired arrival, no refused slot: gate=%d toast=%s%s%s%s\n", controlGate,
           controlToast.any ? "'" : "(none)", controlToast.any ? controlToast.prefix.c_str() : "",
           controlToast.any ? " " : "", controlToast.any ? controlToast.message.c_str() : "");
    if (controlGate != 0 || controlToast.any) {
        return Fail(44, "leg 4 control: an unpaired arrival with no refused slot posted a toast or refused");
    }

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
    // Player words, not the record's field identifier (that stays on stderr),
    // and the consequence: the file is not paired. Recorded, not returned, so
    // the arrival below is observed in the same run.
    int legRc = 0;
    if (!toast.any || !PlrContains(toast.prefix, "Not paired") || !PlrContains(toast.message, "another build") ||
        PlrContains(toast.message, "logicRung") || !FitsOneLine(toast)) {
        legRc = Fail(43,
                     "leg 4: the refusal toast does not say, in player words, that the file is not paired "
                     "(prefix '%s', message '%s')",
                     toast.prefix.c_str(), toast.message.c_str());
    }

    // The OoT file plays on (the caller cannot un-open it). The next crossing
    // into Majora's Mask must say what that means, not skip pairing silently.
    PlantSentinel();
    const int arrivalGate = MM_Rando_GateCrossGameArrival();
    const Toast arrival = LastToast();
    printf("[TEST] leg 4 arrival OBSERVED: gate=%d toast=%s%s%s%s\n", arrivalGate, arrival.any ? "'" : "(none)",
           arrival.any ? arrival.prefix.c_str() : "", arrival.any ? " " : "",
           arrival.any ? arrival.message.c_str() : "");
    if (arrivalGate != 0) {
        return Fail(45, "leg 4: the unpaired arrival refused (%d); it plays vanilla Termina", arrivalGate);
    }
    if (!arrival.any || !PlrContains(arrival.prefix, "Not paired") || !PlrContains(arrival.message, "Termina") ||
        !FitsOneLine(arrival)) {
        return Fail(46,
                    "leg 4: the arrival after a refused load is silent — no toast says Termina plays "
                    "un-randomized (prefix '%s', message '%s')",
                    arrival.prefix.c_str(), arrival.message.c_str());
    }
    return legRc;
}

// ---------------------------------------------------------------------------
// Leg 5: a restore whose after-check fails leaves the keys as they were.
// ---------------------------------------------------------------------------
int LegRollback() {
    // Cross-Game Rules: the file holds Goal BEAT_EITHER and Direction FORWARD;
    // the session holds Goal BEAT_BOTH (set) and Direction unset.
    ClearAuthoredKeys();
    Combo_ComboSettingSet(COMBO_SETTING_GOAL, (int32_t)RSBS_COMBO_GOAL_BEAT_EITHER);
    Combo_ComboSettingSet(COMBO_SETTING_DIRECTION, (int32_t)RSBS_COMBO_DIR_FORWARD);
    if (int rc = CreatePairedFile(false, nullptr)) {
        return rc;
    }
    Relaunch();
    Combo_ComboSettingSet(COMBO_SETTING_GOAL, (int32_t)RSBS_COMBO_GOAL_BEAT_BOTH);
    Combo_ComboSettingClear(COMBO_SETTING_DIRECTION);

    RsbsSave_ForceLoadRestoreVerifyFailForTest(1);
    const int rc = LoadThroughProductionSeam();
    RsbsSave_ForceLoadRestoreVerifyFailForTest(0);
    const int32_t goal = Combo_ComboSettingResolved(COMBO_SETTING_GOAL);
    const bool goalSet = Combo_ComboSettingIsExplicit(COMBO_SETTING_GOAL);
    const bool directionSet = Combo_ComboSettingIsExplicit(COMBO_SETTING_DIRECTION);
    const Toast toast = LastToast();
    printf("[TEST] leg 5 (rules) OBSERVED: load rc=%d goal=%d goalSet=%d directionSet=%d toast=%s%s%s%s\n", rc,
           (int)goal, goalSet ? 1 : 0, directionSet ? 1 : 0, toast.any ? "'" : "(none)",
           toast.any ? toast.prefix.c_str() : "", toast.any ? " " : "", toast.any ? toast.message.c_str() : "");
    // Recorded, not returned, so the MM half below is observed in the same run.
    int legRc = 0;
    if (rc != RSBS_LOAD_REFUSED) {
        legRc = Fail(60, "leg 5: a restore whose after-check failed was not refused");
    } else if (goal != (int32_t)RSBS_COMBO_GOAL_BEAT_BOTH || !goalSet || directionSet) {
        legRc = Fail(61,
                     "leg 5: a refused restore left the file's rules in the keys (goal %d, goalSet %d, "
                     "directionSet %d)",
                     (int)goal, goalSet ? 1 : 0, directionSet ? 1 : 0);
    } else if (!toast.any || !PlrContains(toast.prefix, "Not paired") || !FitsOneLine(toast)) {
        legRc = Fail(62, "leg 5: the refused restore posted no refusal toast");
    }

    // MM: the file holds the trick on and Starting Hearts 5; the session holds
    // the trick unset and Starting Hearts 3 (set).
    const ComboMMTrickDesc* trick = FirstSettableTrick();
    const ComboMMOptionDesc* hearts = Combo_MMOptionById((uint16_t)RO_STARTING_HEALTH);
    if (trick == nullptr || hearts == nullptr) {
        return Fail(63, "leg 5 setup: no settable MM trick, or no Starting Hearts row");
    }
    ClearAuthoredKeys();
    Combo_MMTrickSetValue(trick, true);
    Combo_MMOptionSetValue(hearts, 5);
    if (int rc2 = CreatePairedFile(false, nullptr)) {
        return rc2;
    }
    Relaunch();
    Combo_MMTrickClear(trick);
    Combo_MMOptionSetValue(hearts, 3);

    MM_Rando_ForceProfileRestoreVerifyFailForTest(1);
    const int mmRc = LoadThroughProductionSeam();
    MM_Rando_ForceProfileRestoreVerifyFailForTest(0);
    const bool trickSet = Combo_CVarIsExplicitInt(trick->cvar);
    const int32_t heartsNow = Combo_MMOptionGetValue(hearts);
    const Toast mmToast = LastToast();
    printf("[TEST] leg 5 (MM) OBSERVED: load rc=%d trickSet=%d startingHearts=%d toast=%s%s%s%s\n", mmRc,
           trickSet ? 1 : 0, (int)heartsNow, mmToast.any ? "'" : "(none)", mmToast.any ? mmToast.prefix.c_str() : "",
           mmToast.any ? " " : "", mmToast.any ? mmToast.message.c_str() : "");
    if (trickSet || heartsNow != 3) {
        return Fail(64, "leg 5: a failed MM restore left the file's values in the keys (trickSet %d, hearts %d)",
                    trickSet ? 1 : 0, (int)heartsNow);
    }
    if (!mmToast.any || !PlrContains(mmToast.prefix, "Not restored")) {
        return Fail(65, "leg 5: the failed MM restore was not flagged");
    }
    return legRc;
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
    int (*const legs[])() = { LegCrossGameRules, LegMmProfile,        LegMmTrickOnly, LegMmManyTricks,
                              LegRoundTrip,      LegUnrestorableRule, LegRollback };
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
