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
 *   leg 1 - Goal, Crossing Direction and Shared Ocarina changed at the title
 *           screen, in a file whose record also carries a non-default pool
 *           size and a non-default MM item-class mask (a world created before
 *           #801 and #834 retired those rows), the file loaded through the
 *           production seam's own calls
 *                -> the pair is restored, all three keys hold the file's
 *                   values, the record keeps its own pool byte and class mask,
 *                   one toast names "Goal, Crossing Direction +1", the slot is
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
 *           booted: nothing changed -> the file select accepts, the load
 *           pairs, no toast, no key written (Cross-Game Rules, MM options AND
 *           MM tricks); the same with an EMPTY MM half (a file saved before MM
 *           ever ran), unchanged and with a trick changed -> REFUSED at the
 *           file select ("This file has no Majora's Mask world", #836), the
 *           Save Files page says the same, and the open path's load refuses too
 *   leg 4 - a record field no key authors (the logic rung, only another build
 *           can write it) -> REFUSED at the file select in player words ("File
 *           made by another build"), the page says the same, the open path's
 *           load refuses, nothing is committed or quarantined; an unpaired
 *           arrival with no refused slot posts nothing (the control). The old
 *           arrival half ("Termina stays un-randomized" after a refused load)
 *           is gone with the leg it checked: a refused file is never opened
 *   leg 5 - a restore whose after-check fails (forced through the test hooks;
 *           unreachable by construction today) -> the file select ACCEPTED it
 *           and the load refuses (the backstop's trigger, #836): every key it
 *           wrote is put back as it was, set or unset, on both the Cross-Game
 *           Rules and the MM side, and the slot records the refusal's words
 *   leg 6 - #836: the file select's probe, one leg per refusal the open path
 *           makes (every structural reason, commit skew, identity damage, a
 *           missing or unpaired record on a randomizer file, an MM profile the
 *           file cannot restore): the reason, the words, and NOTHING MOVED (the
 *           slot file's bytes, no .tmp or .bak, gComboCtx byte-identical, every
 *           Cross-Game Rule / MM option / MM trick key, the active slot)
 *   leg 7 - #836: what the probe accepts: a healthy file; a restorable rules
 *           divergence plus an MM-profile divergence (the probe writes no key
 *           and the load that follows restores both); a vanilla file with no
 *           record
 *
 * Every leg loads through Context_InvalidateSessionOnSlotLoad +
 * RsbsSave_SetActiveSlot + RsbsSave_LoadSlotForOpen, the OnLoadFile seam's exact
 * sequence, and asserts STATE.
 *
 * MM-side because the MM half is authored the way creation authors it
 * (Rando::Foreign::ResolvePairedProfile writes the options and tricks into the
 * save) and because the arrival gate is MM's.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdarg>
#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <filesystem>
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
#include "combo_save_files_view.h"
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

/** The Combo > Save Files page's status for the test slot, read the way the
 *  page reads it (ReadMeta, then the model's row). */
std::string SaveFilesStatus() {
    return Combo_SaveFiles_RowFor(kSlot, rsbs::SaveManager::Instance().ReadMeta(kSlot)).status;
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
        Rando::Foreign::ResolvePairedProfile(/*paired=*/true);
        // THIS pair's world: the seed the master seed derives from the options
        // just resolved into the half (rung 0), as OnFileCreate stamps it. The
        // arrival's pair-membership check (#564 V11) refuses any other seed.
        gSaveContext.save.shipSaveInfo.rando.finalSeed = Rando::Foreign::MixPairedFinalSeedForAttempt(0);
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

/** games/oot/soh/SaveManager.cpp's OnLoadFile seam, call for call: the open
 *  path's load of a randomizer file (#836). A refusal here is the backstop's
 *  trigger; the seam records it rather than acting on the return. */
int LoadThroughProductionSeam() {
    PlantSentinel();
    Context_InvalidateSessionOnSlotLoad();
    RsbsSave_SetActiveSlot(kSlot);
    return RsbsSave_LoadSlotForOpen(kSlot, 0, /*isRandoFile=*/1);
}

// ---------------------------------------------------------------------------
// #836: the file select's probe, and "nothing moved".
// ---------------------------------------------------------------------------

/** Everything the probe must leave alone: the slot file's bytes, the scratch
 *  directory's other files (a .tmp or .bak is a write or a rename), gComboCtx,
 *  every Cross-Game Rule / MM option / MM trick key (set or unset, and its
 *  value), and the active slot. */
struct World {
    bool slotExists = false;
    std::vector<uint8_t> slotBytes;
    int otherFiles = 0;
    std::vector<uint8_t> combo;
    std::vector<int32_t> keys;
    int activeSlot = -1;
};

std::vector<uint8_t> PlrReadAll(const std::string& path) {
    std::vector<uint8_t> out;
    FILE* f = fopen(path.c_str(), "rb");
    if (f == nullptr) {
        return out;
    }
    uint8_t buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        out.insert(out.end(), buf, buf + n);
    }
    fclose(f);
    return out;
}

void PlrWriteAll(const std::string& path, const std::vector<uint8_t>& bytes) {
    FILE* f = fopen(path.c_str(), "wb");
    if (f != nullptr) {
        fwrite(bytes.data(), 1, bytes.size(), f);
        fclose(f);
    }
}

World Snap() {
    World w;
    const std::string path = rsbs::SaveManager::Instance().SlotPath(kSlot);
    std::error_code ec;
    w.slotExists = std::filesystem::is_regular_file(path, ec);
    if (w.slotExists) {
        w.slotBytes = PlrReadAll(path);
    }
    for (const auto& entry : std::filesystem::directory_iterator(kScratchSaveDir, ec)) {
        const std::string name = entry.path().filename().string();
        if (name.size() < 8 || name.compare(name.size() - 8, 8, ".redsave") != 0) {
            w.otherFiles++;
        }
    }
    w.combo.assign(reinterpret_cast<const uint8_t*>(&gComboCtx),
                   reinterpret_cast<const uint8_t*>(&gComboCtx) + sizeof(ComboContext));
    for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
        w.keys.push_back(Combo_ComboSettingIsExplicit((ComboSettingId)i) ? 1 : 0);
        w.keys.push_back(Combo_ComboSettingResolved((ComboSettingId)i));
    }
    for (int i = 0; i < Combo_MMOptionCount(); i++) {
        const ComboMMOptionDesc* d = Combo_MMOptionAt(i);
        w.keys.push_back(Combo_MMOptionIsExplicit(d) ? 1 : 0);
        w.keys.push_back(Combo_MMOptionGetValue(d));
    }
    for (int i = 0; i < Combo_MMTrickCount(); i++) {
        const ComboMMTrickDesc* d = Combo_MMTrickAt(i);
        w.keys.push_back(d != nullptr && Combo_CVarIsExplicitInt(d->cvar) ? 1 : 0);
        w.keys.push_back(d != nullptr && Combo_MMTrickGetValue(d) ? 1 : 0);
    }
    w.activeSlot = RsbsSave_GetActiveSlot();
    return w;
}

/** "" when nothing moved, else what did. */
std::string WhatMoved(const World& a, const World& b) {
    if (a.slotExists != b.slotExists || a.slotBytes != b.slotBytes) {
        return "the slot file";
    }
    if (a.otherFiles != b.otherFiles) {
        return "a .tmp or .bak beside the slot file";
    }
    if (a.combo != b.combo) {
        return "gComboCtx";
    }
    if (a.keys != b.keys) {
        return "a Cross-Game Rule, MM option or MM trick key";
    }
    if (a.activeSlot != b.activeSlot) {
        return "the active slot";
    }
    return "";
}

struct ProbeResult {
    int outcome = -1;
    std::string words;
    std::string moved;
};

/** The file select's probe of the test slot, with the world snapshotted around
 *  it. A non-zero @p ootSavGeneration stands in for the .sav's mirrored stamp. */
ProbeResult Probe(int isRandoFile, uint32_t ootSavGeneration = 0) {
    const World before = Snap();
    char words[96] = { 0 };
    ProbeResult r;
    r.outcome = RsbsSave_ProbeSlotForOpen(kSlot, ootSavGeneration, isRandoFile, words, sizeof(words));
    r.words = words;
    r.moved = WhatMoved(before, Snap());
    return r;
}

// The .redsave's byte layout, for the refusal legs (save.h): a 32-byte header,
// then Tier-1 (comboSize), Tier-2 (ootSize), Tier-3 (mmSize), Tier-4; the CRC at
// header offset 28 covers everything after the header.
constexpr size_t kHdrVersion = 8;
constexpr size_t kHdrSlot = 13;
constexpr size_t kHdrComboSize = 16;
constexpr size_t kHdrCrc = 28;
constexpr size_t kTier1 = 32;

uint32_t PlrU32At(const std::vector<uint8_t>& f, size_t at) {
    uint32_t v = 0;
    memcpy(&v, f.data() + at, sizeof(v));
    return v;
}

void PlrPutU32(std::vector<uint8_t>& f, size_t at, uint32_t v) {
    memcpy(f.data() + at, &v, sizeof(v));
}

void PlrRecrc(std::vector<uint8_t>& f) {
    PlrPutU32(f, kHdrCrc, rsbs::SaveManager::Crc32(f.data() + 32, f.size() - 32));
}

size_t Tier3Offset(const std::vector<uint8_t>& f) {
    return kTier1 + PlrU32At(f, kHdrComboSize) + PlrU32At(f, kHdrComboSize + 4);
}

size_t Tier4Offset(const std::vector<uint8_t>& f) {
    return Tier3Offset(f) + PlrU32At(f, kHdrComboSize + 8);
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

/** SoH's toast shape (docs/ui-style-guide.md section 10b): the overlay draws the
 *  prefix and the message on ONE line and never wraps. The load keeps to 48
 *  characters (save.cpp's kLoadToastBudget); the pixels are the UI snapshot's
 *  toast/load-* pages at the 832-px profile. */
bool FitsOneLine(const Toast& toast) {
    return toast.prefix.size() + 1 + toast.message.size() <= 48;
}

// ---------------------------------------------------------------------------
// Leg 1: Cross-Game Rules changed at the title screen.
// ---------------------------------------------------------------------------
int LegCrossGameRules() {
    ClearAuthoredKeys();
    // One more rule through the same table: Shared Ocarina, set to a legal
    // value that is not the shipped one.
    const int32_t fileOcarina = Combo_ComboSettingDefault(COMBO_SETTING_SHARED_OCARINA) != 0 ? 0 : 1;
    if (Combo_ComboSettingSet(COMBO_SETTING_GOAL, (int32_t)RSBS_COMBO_GOAL_BEAT_EITHER) != 1 ||
        Combo_ComboSettingSet(COMBO_SETTING_DIRECTION, (int32_t)RSBS_COMBO_DIR_FORWARD) != 1 ||
        Combo_ComboSettingSet(COMBO_SETTING_SHARED_OCARINA, fileOcarina) != 1) {
        return Fail(10, "leg 1 setup: could not author the three rules for the file");
    }
    // The record also carries a pool size and an item-class mask nobody can
    // author any more: the file was created before #801 with "Max OoT Items"
    // moved and before #834 with "MM Classes" changed. No key can restore
    // either, and none has to: the session holds no value to diverge.
    ComboSettingsRecord older;
    Combo_ResolveComboSettings(&older);
    const uint8_t filePool = older.poolSizeOoT == 3u ? 2u : 3u;
    older.poolSizeOoT = filePool;
    const uint16_t fileClass =
        older.itemClassMM == (uint16_t)RSBS_ITEMCLASS_PROGRESSION ? 0u : (uint16_t)RSBS_ITEMCLASS_PROGRESSION;
    older.itemClassMM = fileClass;
    if (int rc = CreatePairedFile(false, &older)) {
        return rc;
    }

    // Back at the title screen the player picks the shipped rules again: two
    // keys set explicitly, one cleared back to unset.
    Relaunch();
    Combo_ComboSettingSet(COMBO_SETTING_GOAL, (int32_t)RSBS_COMBO_GOAL_BEAT_BOTH);
    Combo_ComboSettingSet(COMBO_SETTING_DIRECTION, (int32_t)RSBS_COMBO_DIR_BOTH);
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
    const int32_t pool = (int32_t)gComboCtx.comboSettings.poolSizeOoT;
    const int32_t klass = (int32_t)gComboCtx.comboSettings.itemClassMM;
    const int32_t ocarina = Combo_ComboSettingResolved(COMBO_SETTING_SHARED_OCARINA);
    printf("[TEST] leg 1 OBSERVED: poolSizeOoT=%d (file %d) itemClassMM=%d (file %d) sharedOcarina=%d (file %d)\n",
           (int)pool, (int)filePool, (int)klass, (int)fileClass, (int)ocarina, (int)fileOcarina);
    if (pool != filePool || klass != (int32_t)fileClass || ocarina != fileOcarina) {
        return Fail(18, "leg 1: the record's pool byte or class mask, or the Shared Ocarina key, does not hold "
                        "the file's value");
    }
    if (Combo_ComboSettingsDivergence() != 0) {
        return Fail(14, "leg 1: the loaded pair still diverges from the live resolution");
    }
    if (writable != 1 || RsbsSave_HasQuarantine(kSlot) != 0 || RsbsSave_HasSave(kSlot) != 1) {
        return Fail(15, "leg 1: the slot is latched, quarantined or gone after a restoring load");
    }
    if (!toast.any || !PlrContains(toast.prefix, "Restored from file") || !PlrContains(toast.message, "Goal") ||
        !PlrContains(toast.message, "Crossing Direction") ||
        ToastShownNames(toast.message) + ToastPlusCount(toast.message) != 3 || !FitsOneLine(toast)) {
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
    // An authored MM half: loads, pairs, says nothing, writes no key.
    {
        ClearAuthoredKeys();
        if (int rc = CreatePairedFile(false, nullptr)) {
            return rc;
        }
        const uint32_t fileDigest = gComboCtx.mmProfileDigest;
        Relaunch();

        const ProbeResult probe = Probe(1);
        const int rc = LoadThroughProductionSeam();
        const bool paired = Combo_ForeignPairingActive();
        const Toast toast = LastToast();
        // Every key a page authors: Cross-Game Rules, MM options AND MM tricks.
        const bool anyKeyWritten = AnyAuthoredKeyExplicit();
        const int gate = paired ? MM_Rando_GateCrossGameArrival() : -1;
        printf("[TEST] leg 3 (authored MM half) OBSERVED: probe=%d load rc=%d paired=%d toast=%s keyWritten=%d "
               "arrivalGate=%d\n",
               probe.outcome, rc, paired ? 1 : 0, toast.any ? toast.prefix.c_str() : "(none)", anyKeyWritten ? 1 : 0,
               gate);
        if (probe.outcome != RSBS_LOAD_OK || rc != RSBS_LOAD_OK || !paired || gComboCtx.mmProfileDigest != fileDigest) {
            return Fail(30, "leg 3: an unchanged paired file did not round-trip into a paired session");
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

    // An EMPTY MM half (a pre-#680 file that never crossed), unchanged and then
    // with a trick changed: the file has no paired Termina world, so it is
    // refused at the file select and by the open path's load (#836). It used to
    // load, pair, and be refused at the first crossing (Termina vanilla).
    const ComboMMTrickDesc* trick = FirstSettableTrick();
    if (trick == nullptr) {
        return Fail(35, "leg 3 setup: no settable MM trick");
    }
    for (int changed = 0; changed <= 1; changed++) {
        ClearAuthoredKeys();
        if (int rc = CreatePairedFile(true, nullptr)) {
            return rc;
        }
        Relaunch();
        if (changed) {
            Combo_MMTrickSetValue(trick, true);
        }
        const ProbeResult probe = Probe(1);
        const int reason = RsbsSave_GetSlotRefuseReason(kSlot);
        const std::string page = SaveFilesStatus();
        const int rc = LoadThroughProductionSeam();
        const bool paired = Combo_ForeignPairingActive();
        printf("[TEST] leg 3 (empty MM half%s) OBSERVED: probe=%d words='%s' reason=%d moved='%s' page='%s' load "
               "rc=%d paired=%d\n",
               changed ? ", trick changed" : "", probe.outcome, probe.words.c_str(), reason, probe.moved.c_str(),
               page.c_str(), rc, paired ? 1 : 0);
        if (probe.outcome != RSBS_LOAD_REFUSED || reason != (int)RSBS_REFUSE_GENERATION ||
            probe.words != "This file has no Majora's Mask world" || !probe.moved.empty()) {
            return Fail(36 + changed,
                        "leg 3: a paired file with an empty MM half was not refused at the file select, or the probe "
                        "moved %s",
                        probe.moved.empty() ? "nothing" : probe.moved.c_str());
        }
        if (page != "Not paired: " + probe.words) {
            return Fail(38, "leg 3: the Save Files page reads '%s', not the probe's words", page.c_str());
        }
        if (rc != RSBS_LOAD_REFUSED || paired || RsbsSave_IsSlotWritable(kSlot) != 0) {
            return Fail(39, "leg 3: the open path's load committed a paired file with an empty MM half");
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Leg 4: a record field no key authors.
// ---------------------------------------------------------------------------
int LegUnrestorableRule() {
    ClearAuthoredKeys();

    // Control: an unpaired arrival with no refused slot (a vanilla file) says
    // nothing and refuses nothing.
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

    // The file select refuses it (#836): the file is not opened.
    const ProbeResult probe = Probe(1);
    const std::string page = SaveFilesStatus();
    printf("[TEST] leg 4 OBSERVED: probe=%d words='%s' reason=%d moved='%s' quarantined=%d page='%s'\n", probe.outcome,
           probe.words.c_str(), RsbsSave_GetSlotRefuseReason(kSlot), probe.moved.c_str(), RsbsSave_HasQuarantine(kSlot),
           page.c_str());
    if (probe.outcome != RSBS_LOAD_REFUSED || RsbsSave_GetSlotRefuseReason(kSlot) != (int)RSBS_REFUSE_IDENTITY ||
        RsbsSave_IsSlotWritable(kSlot) != 0) {
        return Fail(40, "leg 4: a rule no key can restore was not refused and latched at the file select");
    }
    if (probe.words != "File made by another build" || PlrContains(probe.words, "logicRung") || !probe.moved.empty()) {
        return Fail(43, "leg 4: the refusal's words are '%s' (want player words), or the probe moved %s",
                    probe.words.c_str(), probe.moved.empty() ? "nothing" : probe.moved.c_str());
    }
    if (page != "Not paired: " + probe.words) {
        return Fail(71, "leg 4: the Save Files page reads '%s' but the file select said '%s'", page.c_str(),
                    probe.words.c_str());
    }

    // The open path's load refuses it too, commits nothing and quarantines
    // nothing: a healthy file is never renamed for a session divergence.
    const int rc = LoadThroughProductionSeam();
    printf("[TEST] leg 4 OBSERVED: open-path load rc=%d paired=%d frozen=%d quarantined=%d\n", rc,
           Combo_ForeignPairingActive() ? 1 : 0, Combo_ComboSettingsFrozen() ? 1 : 0, RsbsSave_HasQuarantine(kSlot));
    if (rc != RSBS_LOAD_REFUSED || Combo_ComboSettingsFrozen() || Combo_ForeignPairingActive()) {
        return Fail(41, "leg 4: the open path's load committed the record");
    }
    if (RsbsSave_HasQuarantine(kSlot) != 0 || RsbsSave_HasSave(kSlot) != 1) {
        return Fail(42, "leg 4: a healthy file was quarantined for a session divergence");
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Leg 5: the probe accepts and the load refuses (the backstop's trigger): a
// restore whose after-check fails leaves the keys as they were.
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
    const ProbeResult probe = Probe(1);
    const int rc = LoadThroughProductionSeam();
    RsbsSave_ForceLoadRestoreVerifyFailForTest(0);
    const int32_t goal = Combo_ComboSettingResolved(COMBO_SETTING_GOAL);
    const bool goalSet = Combo_ComboSettingIsExplicit(COMBO_SETTING_GOAL);
    const bool directionSet = Combo_ComboSettingIsExplicit(COMBO_SETTING_DIRECTION);
    const std::string words = RsbsSave_SlotRefusalWords(kSlot);
    printf("[TEST] leg 5 (rules) OBSERVED: probe=%d moved='%s' load rc=%d goal=%d goalSet=%d directionSet=%d "
           "words='%s'\n",
           probe.outcome, probe.moved.c_str(), rc, (int)goal, goalSet ? 1 : 0, directionSet ? 1 : 0, words.c_str());
    // Recorded, not returned, so the MM half below is observed in the same run.
    int legRc = 0;
    if (probe.outcome != RSBS_LOAD_OK || !probe.moved.empty()) {
        legRc = Fail(66, "leg 5: the probe did not accept a restorable divergence, or it moved %s",
                     probe.moved.empty() ? "nothing" : probe.moved.c_str());
    } else if (rc != RSBS_LOAD_REFUSED) {
        legRc = Fail(60, "leg 5: a restore whose after-check failed was not refused");
    } else if (goal != (int32_t)RSBS_COMBO_GOAL_BEAT_BOTH || !goalSet || directionSet) {
        legRc = Fail(61,
                     "leg 5: a refused restore left the file's rules in the keys (goal %d, goalSet %d, "
                     "directionSet %d)",
                     (int)goal, goalSet ? 1 : 0, directionSet ? 1 : 0);
    } else if (words != "Cross-game rules differ" || Combo_ForeignPairingActive()) {
        legRc = Fail(62, "leg 5: the refused load recorded the words '%s', or committed the pair", words.c_str());
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
    const ProbeResult mmProbe = Probe(1);
    const int mmRc = LoadThroughProductionSeam();
    MM_Rando_ForceProfileRestoreVerifyFailForTest(0);
    const bool trickSet = Combo_CVarIsExplicitInt(trick->cvar);
    const int32_t heartsNow = Combo_MMOptionGetValue(hearts);
    const std::string mmWords = RsbsSave_SlotRefusalWords(kSlot);
    printf("[TEST] leg 5 (MM) OBSERVED: probe=%d moved='%s' load rc=%d trickSet=%d startingHearts=%d words='%s'\n",
           mmProbe.outcome, mmProbe.moved.c_str(), mmRc, trickSet ? 1 : 0, (int)heartsNow, mmWords.c_str());
    if (mmProbe.outcome != RSBS_LOAD_OK || !mmProbe.moved.empty()) {
        return Fail(67, "leg 5: the probe did not accept a restorable MM divergence, or it moved %s",
                    mmProbe.moved.empty() ? "nothing" : mmProbe.moved.c_str());
    }
    if (trickSet || heartsNow != 3) {
        return Fail(64, "leg 5: a failed MM restore left the file's values in the keys (trickSet %d, hearts %d)",
                    trickSet ? 1 : 0, (int)heartsNow);
    }
    if (mmRc != RSBS_LOAD_REFUSED || mmWords != "Majora's Mask options differ" || Combo_ForeignPairingActive()) {
        return Fail(65, "leg 5: the failed MM restore did not refuse the load (rc %d, words '%s')", mmRc,
                    mmWords.c_str());
    }
    return legRc;
}

// ---------------------------------------------------------------------------
// Leg 6 (#836): one probe leg per refusal the open path makes. Each asserts the
// reason and the words, and that the probe moved nothing.
// ---------------------------------------------------------------------------
int ExpectRefused(int code, const char* what, const ProbeResult& probe, RsbsRefuseReason reason, const char* words) {
    const int got = RsbsSave_GetSlotRefuseReason(kSlot);
    printf("[TEST] leg 6 (%s) OBSERVED: probe=%d reason=%d words='%s' moved='%s' writable=%d\n", what, probe.outcome,
           got, probe.words.c_str(), probe.moved.c_str(), RsbsSave_IsSlotWritable(kSlot));
    if (probe.outcome != RSBS_LOAD_REFUSED || got != (int)reason || probe.words != words ||
        RsbsSave_IsSlotWritable(kSlot) != 0) {
        return Fail(code, "leg 6 (%s): want REFUSED, reason %d, words '%s'", what, (int)reason, words);
    }
    if (!probe.moved.empty()) {
        return Fail(code, "leg 6 (%s): the probe moved %s", what, probe.moved.c_str());
    }
    return 0;
}

/** A healthy paired file, then @p mutate on its bytes (the CRC recomputed when
 *  @p recrc), then a fresh launch. */
template <typename F> int PairedFileThen(bool recrc, F mutate) {
    ClearAuthoredKeys();
    if (int rc = CreatePairedFile(false, nullptr)) {
        return rc;
    }
    const std::string path = rsbs::SaveManager::Instance().SlotPath(kSlot);
    std::vector<uint8_t> bytes = PlrReadAll(path);
    mutate(bytes);
    if (recrc) {
        PlrRecrc(bytes);
    }
    PlrWriteAll(path, bytes);
    Relaunch();
    return 0;
}

int LegProbeRefusals() {
    int rc = 0;
    auto note = [&rc](int legRc) {
        if (rc == 0) {
            rc = legRc;
        }
    };
    const std::string path = rsbs::SaveManager::Instance().SlotPath(kSlot);
    struct Structural {
        const char* what;
        RsbsRefuseReason reason;
        bool recrc;
        void (*mutate)(std::vector<uint8_t>&);
    };
    const Structural structural[] = {
        { "header", RSBS_REFUSE_HEADER, false, [](std::vector<uint8_t>& f) { f[0] ^= 0xFF; } },
        { "version", RSBS_REFUSE_VERSION, false, [](std::vector<uint8_t>& f) { PlrPutU32(f, kHdrVersion, 99u); } },
        { "tier size", RSBS_REFUSE_TIER_SIZE, false,
          [](std::vector<uint8_t>& f) { PlrPutU32(f, kHdrComboSize, 0x00FFFFFFu); } },
        { "wrong slot", RSBS_REFUSE_WRONG_SLOT, false, [](std::vector<uint8_t>& f) { f[kHdrSlot] = 2; } },
        { "truncated", RSBS_REFUSE_TRUNCATED, false, [](std::vector<uint8_t>& f) { f.resize(kTier1 + 16); } },
        { "crc", RSBS_REFUSE_CRC, false, [](std::vector<uint8_t>& f) { f[Tier3Offset(f) - 7] ^= 0x5A; } },
        { "combo magic", RSBS_REFUSE_COMBO_MAGIC, true,
          [](std::vector<uint8_t>& f) { f[kTier1 + offsetof(ComboContext, magic)] ^= 0x20; } },
        { "crossings", RSBS_REFUSE_CROSSINGS, true, [](std::vector<uint8_t>& f) { f[Tier4Offset(f)] ^= 0xFF; } },
    };
    int code = 80;
    for (const Structural& s : structural) {
        if (int setup = PairedFileThen(s.recrc, s.mutate)) {
            return setup;
        }
        note(ExpectRefused(code++, s.what, Probe(1), s.reason, Combo_SaveFiles_RefuseText(s.reason)));
    }

    // Unreadable: something that exists at the slot path and cannot be opened.
    {
        ClearAuthoredKeys();
        Relaunch();
        RsbsSave_DeleteSave(kSlot);
        RsbsSave_ResetSlotSessionState();
        std::error_code ec;
        std::filesystem::create_directory(path, ec);
        note(ExpectRefused(code++, "unreadable", Probe(1), RSBS_REFUSE_UNREADABLE,
                           Combo_SaveFiles_RefuseText(RSBS_REFUSE_UNREADABLE)));
        std::filesystem::remove(path, ec);
    }

    // Commit skew: the .sav mirrors a newer generation than the record.
    {
        if (int setup = PairedFileThen(false, [](std::vector<uint8_t>&) {})) {
            return setup;
        }
        const std::vector<uint8_t> bytes = PlrReadAll(path);
        const uint32_t fileGen = PlrU32At(bytes, kTier1 + offsetof(ComboContext, commitGeneration));
        note(ExpectRefused(code++, "commit skew", Probe(1, fileGen + 5u), RSBS_REFUSE_COMMIT_SKEW,
                           Combo_SaveFiles_RefuseText(RSBS_REFUSE_COMMIT_SKEW)));
    }

    // Identity damage: a fingerprint the record does not produce.
    {
        if (int setup = PairedFileThen(true, [](std::vector<uint8_t>& f) {
                const size_t at = kTier1 + offsetof(ComboContext, comboSettingsHash);
                PlrPutU32(f, at, PlrU32At(f, at) ^ 0x00010000u);
            })) {
            return setup;
        }
        note(ExpectRefused(code++, "identity damage", Probe(1), RSBS_REFUSE_IDENTITY,
                           RsbsSave_LoadToastRefusalMessage(RSBS_LOAD_TOAST_REFUSED_DAMAGED)));
    }

    // Missing: no .redsave at all, and a record with no pairing, on a
    // randomizer .sav (SoH's Copy, a lost record, the permanent unpaired state).
    {
        ClearAuthoredKeys();
        Relaunch();
        RsbsSave_DeleteSave(kSlot);
        RsbsSave_ResetSlotSessionState();
        note(ExpectRefused(code++, "no record, randomizer file", Probe(1), RSBS_REFUSE_MISSING,
                           "Cross-game record is missing"));
        if (int setup = PairedFileThen(true, [](std::vector<uint8_t>& f) {
                f[kTier1 + offsetof(ComboContext, sourceIsRando)] = 0;
                PlrPutU32(f, kTier1 + offsetof(ComboContext, sharedRandoSettingsHash), 0u);
            })) {
            return setup;
        }
        note(ExpectRefused(code++, "unpaired record, randomizer file", Probe(1), RSBS_REFUSE_MISSING,
                           "Cross-game record is missing"));
    }

    // An MM profile the file cannot restore: its MM half is not a randomizer
    // save, so it records no options, and the session changed a trick.
    {
        const ComboMMTrickDesc* trick = FirstSettableTrick();
        if (trick == nullptr) {
            return Fail(98, "leg 6 setup: no settable MM trick");
        }
        if (int setup = PairedFileThen(true, [](std::vector<uint8_t>& f) {
                const size_t at = Tier3Offset(f) + offsetof(SaveContext, save.shipSaveInfo.saveType);
                f[at] = (uint8_t)SAVETYPE_VANILLA;
            })) {
            return setup;
        }
        Combo_MMTrickSetValue(trick, true);
        note(ExpectRefused(code++, "MM profile unrestorable", Probe(1), RSBS_REFUSE_IDENTITY,
                           "Majora's Mask options differ"));
    }
    return rc;
}

// ---------------------------------------------------------------------------
// Leg 7 (#836): what the probe accepts. A healthy file; a restorable
// Cross-Game Rules divergence plus an MM-profile divergence (the probe writes
// no key, and the load that follows restores both); a vanilla file with no
// record.
// ---------------------------------------------------------------------------
int LegProbeAccepts() {
    // Healthy.
    ClearAuthoredKeys();
    if (int rc = CreatePairedFile(false, nullptr)) {
        return rc;
    }
    Relaunch();
    const ProbeResult healthy = Probe(1);
    printf("[TEST] leg 7 (healthy) OBSERVED: probe=%d moved='%s'\n", healthy.outcome, healthy.moved.c_str());
    if (healthy.outcome != RSBS_LOAD_OK || !healthy.moved.empty()) {
        return Fail(110, "leg 7: the probe refused a healthy paired file, or moved %s",
                    healthy.moved.empty() ? "nothing" : healthy.moved.c_str());
    }

    // Both divergences.
    const ComboMMTrickDesc* trick = FirstSettableTrick();
    const ComboMMOptionDesc* hearts = Combo_MMOptionById((uint16_t)RO_STARTING_HEALTH);
    if (trick == nullptr || hearts == nullptr) {
        return Fail(111, "leg 7 setup: no settable MM trick, or no Starting Hearts row");
    }
    ClearAuthoredKeys();
    Combo_ComboSettingSet(COMBO_SETTING_GOAL, (int32_t)RSBS_COMBO_GOAL_BEAT_EITHER);
    Combo_MMTrickSetValue(trick, true);
    Combo_MMOptionSetValue(hearts, 5);
    if (int rc = CreatePairedFile(false, nullptr)) {
        return rc;
    }
    Relaunch();
    Combo_ComboSettingSet(COMBO_SETTING_GOAL, (int32_t)RSBS_COMBO_GOAL_BEAT_BOTH);
    Combo_MMTrickClear(trick);
    Combo_MMOptionSetValue(hearts, 3);
    const ProbeResult diverged = Probe(1);
    const int rc = LoadThroughProductionSeam();
    const int32_t goal = Combo_ComboSettingResolved(COMBO_SETTING_GOAL);
    const bool trickOn = Combo_MMTrickGetValue(trick);
    const int32_t heartsNow = Combo_MMOptionGetValue(hearts);
    printf("[TEST] leg 7 (diverged) OBSERVED: probe=%d moved='%s' load rc=%d paired=%d goal=%d trick=%d "
           "startingHearts=%d\n",
           diverged.outcome, diverged.moved.c_str(), rc, Combo_ForeignPairingActive() ? 1 : 0, (int)goal,
           trickOn ? 1 : 0, (int)heartsNow);
    if (diverged.outcome != RSBS_LOAD_OK || !diverged.moved.empty()) {
        return Fail(112,
                    "leg 7: the probe refused a divergence the file can restore, or moved %s (it must write "
                    "no key)",
                    diverged.moved.empty() ? "nothing" : diverged.moved.c_str());
    }
    if (rc != RSBS_LOAD_OK || !Combo_ForeignPairingActive() || goal != (int32_t)RSBS_COMBO_GOAL_BEAT_EITHER ||
        !trickOn || heartsNow != 5) {
        return Fail(113, "leg 7: the load after the probe did not restore the file's rules and MM profile");
    }

    // A vanilla file with no record opens (and the load arms its first write).
    ClearAuthoredKeys();
    Relaunch();
    RsbsSave_DeleteSave(kSlot);
    RsbsSave_ResetSlotSessionState();
    const ProbeResult vanilla = Probe(0);
    printf("[TEST] leg 7 (vanilla, no record) OBSERVED: probe=%d moved='%s'\n", vanilla.outcome, vanilla.moved.c_str());
    if (vanilla.outcome != RSBS_LOAD_ABSENT || !vanilla.moved.empty()) {
        return Fail(114, "leg 7: the probe refused a vanilla file with no cross-game record");
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
    int (*const legs[])() = { LegCrossGameRules,   LegMmProfile, LegMmTrickOnly,   LegMmManyTricks, LegRoundTrip,
                              LegUnrestorableRule, LegRollback,  LegProbeRefusals, LegProbeAccepts };
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
