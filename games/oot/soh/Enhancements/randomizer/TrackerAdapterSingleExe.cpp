/**
 * TrackerAdapterSingleExe.cpp — OoT's combo-tracker adapter: a narrow
 * extern-C accessor surface over the heap Rando::Context (#458; ADR 0002).
 *
 * OoT's check status does NOT live in the SaveContext blob: it lives on the
 * heap, in Rando::Context's itemLocationTable, and OOT_SAVE_CONTEXT_SIZE has
 * ~1KB slack, so the MM-style blob route is structurally unavailable. What
 * makes the accessor route sound is the suspend contract: a cross-game switch
 * SUSPENDS OoT (audio+graph only) rather than shutting it down, so the heap —
 * and every status the trackers wrote into it — survives the entire MM
 * session. The view reports the result live only while OoT is the running
 * game, and stale otherwise.
 *
 * NULL-SAFETY IS THE CONTRACT. Rando::Context::GetInstance() is a weak_ptr
 * lock — NULL until something creates the context (an MM-first session that
 * never boots OoT, and every ROM-free harness). Every accessor below answers
 * "unavailable" for that case instead of dereferencing; the ROM-free lock
 * drives exactly that path.
 *
 * Everything leaves this TU as flat data — counts, u16 ids, const char*
 * pointers into static storage — through the vtable declared in
 * src/common/combo_tracker_view.h; no RG_ or RC_ enumerator crosses out
 * (ADR 0002).
 *
 * Lives in soh/Enhancements/randomizer/ (soh_rando, WHOLE_ARCHIVE), and is
 * additionally referenced by name from Combo_TrackerWindow_Init — no elision
 * mode leaves the tracker with a silently empty OoT panel.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
#include <z64.h>
#include "variables.h" // gSaveContext.language, read only while OoT is the active game
}

#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/randomizer/randomizer_check_objects.h"

#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/static_data.h"
// SeedContext.h only FORWARD-declares Rando::Logic, and the test seam below has
// to call Logic::SetContext to cut the Context<->Logic ownership cycle.
#include "soh/Enhancements/randomizer/logic.h"
#include "soh/Enhancements/randomizer/randomizer_check_tracker.h" // the skip toggle's native-tracker refresh (#458 U5)
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/SaveManager.h" // the skip toggle's tracker-data save (#458 U5)

// src/common. Included outside any extern "C" block: the header manages its
// own linkage (matching ForeignItemsSingleExe.cpp).
#include "combo_tracker_view.h"

// randomizer_check_tracker.cpp (#1126): whether SoH's Check Tracker has loaded its
// settings for the file being played.
extern "C" int OoT_CheckTracker_SettingsLoaded(void);

namespace {

/**
 * "Obtained" projection. OoT's RandomizerCheckStatus is one ordered enum with
 * UI-only states (SEEN/IDENTIFIED/SCUMMED) that have no MM analogue; the
 * tracker projects it to the same obtained/skipped booleans MM's table
 * carries natively and does NOT export the raw enum — a merged status space
 * is the #458 discussion's named irreversible mistake, so the MVP ships the
 * lossy projection only and keeps the raw states private to this TU.
 */
bool StatusObtained(RandomizerCheckStatus status) {
    return status == RCSHOW_COLLECTED || status == RCSHOW_SAVED;
}

// The listing lock's stand-ins (#1126; OoT_TrackerAdapter_TestListSeam below):
// -1 is production; 0 or 1 is the lock's answer to "are SoH's Check Tracker
// settings loaded", and the stand-in for SoH's predicate then lists exactly
// sListTestVisible. sListTestCalls counts every question put to the predicate.
int sListTestLoaded = -1;
std::vector<uint16_t> sListTestVisible;
int sListTestCalls = 0;

// The ROM-free lock's stand-ins for the two things it cannot have: a loaded save
// (GameInteractor::IsSaveLoaded needs a play state and a player) and a save
// folder to write (#458 U5; the skip toggle below, and the list's gate, #1126).
// -1 is production; 0 is "no save is loaded"; 1 is "a randomizer save is
// loaded"; 2 is "a save is loaded", with IS_RANDO read for real. With any of
// them the skip's persist step is counted instead of run.
int sSkipTestSaveLoaded = -1;
int sSkipTestPersists = 0;

// The reachability lock's stand-in (#1129; OoT_TrackerAdapter_TestAvailableSeam
// below): -1 is production; 0 or 1 answers "is SoH's available set current"
// (Enable Available Checks on and a recalculation run), which a ROM-free tier
// cannot reach for real (the recalculation needs OoT's region graph).
int sAvailTestCurrent = -1;

/**
 * Whether OoT's live save is a loaded randomizer file:
 * GameInteractor::IsSaveLoaded() (a play state, a player, normal game mode, a
 * file slot) and IS_RANDO. Asked only while OoT is the running game, because
 * both read OoT's globals and gSaveContext is MM's while MM runs.
 */
bool OoTRandoFileLoaded(void) {
    if (sSkipTestSaveLoaded >= 0) {
        return sSkipTestSaveLoaded == 1 || (sSkipTestSaveLoaded == 2 && IS_RANDO);
    }
    return GameInteractor::IsSaveLoaded() && IS_RANDO;
}

/**
 * WHICH CHECKS THE PANEL LISTS (#1126): what SoH's Check Tracker lists,
 * CheckTracker::IsVisibleInCheckTracker (randomizer_check_tracker.cpp), so the
 * rows and the "Checks: n / m" summary equal that tracker's list and its total,
 * excluded locations, hint stones and unshuffled shop slots left out, and its
 * Always Show Gold Skulltulas and Hide Unshuffled Shop Item Checks settings
 * applied. That predicate is not free-standing, so it is asked only when it can
 * answer (OoTTrackerListLive):
 *   - It reads statics only CheckTrackerLoadGame fills (LoadSettings), and
 *     IsCheckShuffled dereferences OTRGlobals::Instance->gRandomizer for every
 *     shop location before its IS_RANDO test (no OTRGlobals in a ROM-free tier).
 *     Until SoH's tracker has loaded (OoT_CheckTracker_SettingsLoaded: its
 *     `initialized`, which Teardown clears at a file exit) the panel lists the
 *     checks the seed placed an item on, as it did before, and asks nothing.
 *   - SoH's tracker loads for EVERY file (CheckTrackerLoadGame returns early only
 *     for Boss Rush), and its predicate has a vanilla branch that lists
 *     vanilla-completion checks; `initialized` also outlives a paired file that
 *     MM returned to title from (OoT's OnExitGame, which tears it down, never
 *     runs). The panel is for randomizer files only (#1126 ruling D11), so the
 *     predicate is asked only while OoT plays a loaded randomizer file
 *     (OoTRandoFileLoaded: SoH's own draw gate, IsSaveLoaded, plus IS_RANDO); a
 *     vanilla file lists the placed checks, which ClearItemLocations has emptied.
 *   - It reads IS_RANDO, gSaveContext's quest id. Both ports overlay one live save
 *     (unified_save.c) and the quest id sits past the end of Majora's Mask's save
 *     blob, so while Majora's Mask runs the byte is whatever Ocarina of Time left
 *     there, never written by MM and never part of MM's own file. So the
 *     predicate is asked only while OoT is the running game, and the answers
 *     are kept: the stale panel lists, for the same seed, what SoH's tracker
 *     listed when OoT last ran; a panel never read while OoT ran falls back to
 *     the placed checks.
 *   - It is a long chain (shop slots identify their item twice), and SoH asks it
 *     only on a load or a filter change. The summary, read once per draw before
 *     the rows, asks it for every check and keeps the answers; the rows and the
 *     skip toggle read that list, and ask only when no list is kept for the seed.
 */
static_assert(offsetof(SaveContext, ship.quest.id) >= MM_SAVE_CONTEXT_SIZE,
              "IS_RANDO's byte moved inside Majora's Mask's save blob: revisit OoTCheckListed's MM rule");

bool OoTTrackerSettingsLoaded(void) {
    if (sListTestLoaded >= 0) {
        return sListTestLoaded != 0;
    }
    return OoT_CheckTracker_SettingsLoaded() != 0;
}

bool OoTTrackerPredicate(RandomizerCheck rc) {
    sListTestCalls++;
    if (sListTestLoaded >= 0) {
        return std::find(sListTestVisible.begin(), sListTestVisible.end(), (uint16_t)rc) != sListTestVisible.end();
    }
    return CheckTracker::IsVisibleInCheckTracker(rc);
}

// The list as of OoT's last live read: one byte per check (1 listed), for the
// seed sListedSeed. Empty until a live read, and emptied whenever OoT runs
// without a randomizer file or SoH's tracker is torn down.
std::vector<uint8_t> sListed;
uint32_t sListedSeed = 0;

// SoH's predicate can answer now: OoT plays a loaded randomizer file and SoH's
// tracker has loaded its settings for it. The current game is asked first.
bool OoTTrackerListLive(void) {
    return Context_GetCurrentGame() == GAME_OOT && OoTRandoFileLoaded() && OoTTrackerSettingsLoaded();
}

bool OoTListKept(Rando::Context* ctx) {
    return sListed.size() == (size_t)RC_MAX && sListedSeed == ctx->GetSeed();
}

// Ask SoH's predicate for every check and keep the answers (live only).
void OoTRefreshListed(Rando::Context* ctx) {
    sListed.assign((size_t)RC_MAX, 0);
    sListedSeed = ctx->GetSeed();
    for (size_t i = 1; i < (size_t)RC_MAX; i++) { // 0 == RC_UNKNOWN_CHECK
        sListed[i] = OoTTrackerPredicate((RandomizerCheck)i) ? 1 : 0;
    }
}

bool OoTCheckListed(Rando::Context* ctx, RandomizerCheck rc) {
    if (OoTTrackerListLive()) {
        if (!OoTListKept(ctx)) {
            OoTRefreshListed(ctx);
        }
        return sListed[rc] != 0;
    }
    if (Context_GetCurrentGame() == GAME_OOT || !OoTTrackerSettingsLoaded()) {
        sListed.clear(); // no randomizer file in OoT, or SoH's tracker torn down: nothing to keep
    } else if (OoTListKept(ctx)) {
        return sListed[rc] != 0; // MM played: the list as of OoT's last live read
    }
    return ctx->GetItemLocation(rc)->GetPlacedRandomizerGet() != RG_NONE;
}

bool OoTTrackerSummary(ComboTrackerGameSummary* out) {
    auto ctx = Rando::Context::GetInstance();
    if (ctx == nullptr || out == nullptr) {
        return false;
    }

    if (OoTTrackerListLive()) {
        OoTRefreshListed(ctx.get()); // once per draw; the rows that follow read it
    }
    int placed = 0;
    int shuffled = 0;
    int obtained = 0;
    int skipped = 0;
    for (size_t i = 1; i < (size_t)RC_MAX; i++) { // 0 == RC_UNKNOWN_CHECK
        Rando::ItemLocation* loc = ctx->GetItemLocation(i);
        if (loc->GetPlacedRandomizerGet() != RG_NONE) {
            placed++;
        }
        if (!OoTCheckListed(ctx.get(), (RandomizerCheck)i)) {
            continue; // not on SoH's Check Tracker's list (see OoTCheckListed)
        }
        shuffled++;
        if (StatusObtained(loc->GetCheckStatus())) {
            obtained++;
        } else if (loc->GetIsSkipped()) {
            skipped++;
        }
    }

    // freshness is deliberately left alone — the view owns it (see the vtable
    // contract in combo_tracker_view.h).
    // A randomized world is resident when the seed placed an item (#1126: not
    // when something is listed, which no longer equals "placed").
    out->hasWorld = placed > 0;
    out->seed = ctx->GetSeed();
    out->totalChecks = (int)RC_MAX;
    out->shuffled = shuffled;
    out->obtained = obtained;
    out->skipped = skipped;
    return true;
}

int OoTTrackerCheckCount(void) {
    return (Rando::Context::GetInstance() != nullptr) ? (int)RC_MAX : 0;
}

const char* OoTTrackerCheckName(uint16_t checkId) {
    if (checkId >= (uint16_t)RC_MAX) {
        return nullptr;
    }
    // locationTable is static storage, so the c_str() stays valid; before
    // InitLocationTable runs (OoT never booted) the names are empty strings,
    // reported as NULL so the renderer falls back to the id.
    Rando::Location* loc = Rando::StaticData::GetLocation((RandomizerCheck)checkId);
    if (loc == nullptr) {
        return nullptr;
    }
    const std::string& name = loc->GetName();
    return name.empty() ? nullptr : name.c_str();
}

/**
 * The common status (#458 U4; combo_tracker_view.h, ComboTrackerCheckStatus),
 * in the order SoH's check tracker tests them (DrawLocation): collected, saved,
 * then the skip flag, then seen/identified, scummed, unchecked.
 */
uint8_t ProjectStatus(RandomizerCheckStatus status, bool skipped) {
    if (status == RCSHOW_COLLECTED) {
        return COMBO_TRACKER_CHECK_COLLECTED;
    }
    if (status == RCSHOW_SAVED) {
        return COMBO_TRACKER_CHECK_SAVED;
    }
    if (skipped) {
        return COMBO_TRACKER_CHECK_SKIPPED;
    }
    if (status == RCSHOW_SEEN || status == RCSHOW_IDENTIFIED) {
        return COMBO_TRACKER_CHECK_SEEN;
    }
    if (status == RCSHOW_SCUMMED) {
        return COMBO_TRACKER_CHECK_SCUMMED;
    }
    return COMBO_TRACKER_CHECK_UNCHECKED;
}

/**
 * An area's name, as SoH's check tracker heads it (GetRCAreaName). That returns
 * a copy, so each name is kept here once, in node-stable storage the row can
 * point into.
 */
const char* OoTAreaName(RandomizerCheckArea area) {
    static std::map<int, std::string> sNames;
    auto it = sNames.find((int)area);
    if (it == sNames.end()) {
        it = sNames.emplace((int)area, RandomizerCheckObjects::GetRCAreaName(area)).first;
    }
    return it->second.empty() ? nullptr : it->second.c_str();
}

/**
 * The language SoH's check tracker names items in. It reads gSaveContext.language,
 * which is OoT's only while OoT is the active game (both ports overlay one live
 * save, unified_save.c); otherwise the language setting OoT copies it from at
 * load (SaveManager.cpp).
 */
uint8_t OoTTrackerLanguage(void) {
    if (Context_GetCurrentGame() == GAME_OOT) {
        return gSaveContext.language;
    }
    return (uint8_t)CVarGetInteger(CVAR_SETTING("Languages"), LANGUAGE_ENG);
}

/**
 * The item a found check's row names: SoH's check tracker's rule
 * (randomizer_check_tracker.cpp, PlacedItemTrackerName, #796) without its
 * " (MM)" suffix, which the window adds from `*outGame`. A paired check that hosts
 * an MM item physically holds a cover, so the crossed item comes first; otherwise
 * the item placed there. The storage is static (the foreign describer's names, the
 * item table's Text), so the pointer outlives the row.
 */
const char* OoTPlacedItemName(Rando::ItemLocation* loc, RandomizerCheck rc, uint8_t* outGame) {
    if (Combo_ForeignPairingActive()) {
        if (const SharedItem* crossed = Combo_GetForeignPlacementForOoTCheck((uint16_t)rc)) {
            if (const char* name = Combo_GetForeignItemName(*crossed)) {
                *outGame = (uint8_t)GAME_MM;
                return name;
            }
        }
    }
    const std::string& name = loc->GetPlacedItem().GetName().GetForLanguage(OoTTrackerLanguage());
    if (name.empty()) {
        return nullptr;
    }
    *outGame = (uint8_t)GAME_OOT;
    return name.c_str();
}

bool OoTTrackerCheckAt(int index, ComboTrackerCheckRow* out) {
    auto ctx = Rando::Context::GetInstance();
    if (ctx == nullptr || out == nullptr || index < 0 || index >= (int)RC_MAX) {
        return false;
    }
    Rando::ItemLocation* loc = ctx->GetItemLocation((size_t)index);
    out->checkId = (uint16_t)index;
    out->name = OoTTrackerCheckName((uint16_t)index);
    out->shuffled = OoTCheckListed(ctx.get(), (RandomizerCheck)index);
    out->obtained = StatusObtained(loc->GetCheckStatus());
    out->skipped = loc->GetIsSkipped();

    // #458 U4: status, area and the found item, each as SoH's tracker has it.
    out->status = ProjectStatus(loc->GetCheckStatus(), out->skipped);
    if (out->name != nullptr) { // a filled location table, so the area is real
        const Rando::Location* staticLoc = Rando::StaticData::GetLocation((RandomizerCheck)index);
        const RandomizerCheckArea area = staticLoc->GetArea();
        out->areaKey = (uint16_t)area;
        out->areaName = OoTAreaName(area);
        // Under its area header SoH's tracker prints the short name (DrawLocation:
        // GetShortName()); locationTable is static storage, so c_str() stays valid.
        const std::string& shortName = staticLoc->GetShortName();
        out->shortName = shortName.empty() ? nullptr : shortName.c_str();
    }
    // The name shows once the check is found: SoH prints the placed item for a
    // collected, saved or scummed check, whatever its skip flag. A seen or
    // identified check's item SoH reveals only under hint settings this window
    // does not read, so it stays unnamed here: the window never shows more than
    // SoH's tracker.
    out->placedItemName = nullptr;
    out->placedItemGame = (uint8_t)GAME_NONE;
    const RandomizerCheckStatus status = loc->GetCheckStatus();
    if (status == RCSHOW_COLLECTED || status == RCSHOW_SAVED || status == RCSHOW_SCUMMED) {
        uint8_t game = (uint8_t)GAME_NONE;
        out->placedItemName = OoTPlacedItemName(loc, (RandomizerCheck)index, &game);
        out->placedItemGame = out->placedItemName != nullptr ? game : (uint8_t)GAME_NONE;
    }
    return true;
}

// ---- #458 U5: the skip toggle ------------------------------------------------

// The ROM-free lock's stand-ins, sSkipTestSaveLoaded and sSkipTestPersists, are
// declared above the list's gate (OoTRandoFileLoaded), which shares them.

/**
 * A skip write needs OoT running a loaded rando file: the heap then belongs to
 * the save being played, and gSaveContext is OoT's (both ports overlay one live
 * save, so it is MM's while MM runs). The current game is asked first, because
 * IsSaveLoaded reads gSaveContext.
 *
 * This is NOT SoH's own Check Tracker draw condition, which is
 * `GameInteractor::IsSaveLoaded() && initialized` (DrawElement; "Waiting for
 * file load..." otherwise). The predicate here has no `initialized` test (that
 * flag is file-local to randomizer_check_tracker.cpp, and Teardown clears it on
 * OnExitGame), and it adds IS_RANDO. A write while the native tracker is torn
 * down still lands: UpdateAllOrdering then sorts an empty area table, and
 * SaveSection persists the heap flag, which LoadFile reads back on the next load.
 */
bool OoTTrackerSkipWritable(void) {
    if (Context_GetCurrentGame() != GAME_OOT || Rando::Context::GetInstance() == nullptr) {
        return false;
    }
    return OoTRandoFileLoaded();
}

/**
 * What SoH's skip button does after flipping the flag (DrawLocation): its own
 * area counts and order follow, and its tracker-data section is saved. The order
 * pass also recalculates every area's totals (UpdateOrdering), which is what the
 * button's hand-adjusted counters amount to; hook_handlers.cpp's foreign pickup
 * persists the same section the same way.
 */
void OoTTrackerPersistSkip(void) {
    CheckTracker::UpdateAllOrdering();
    if (sSkipTestSaveLoaded >= 0) {
        sSkipTestPersists++;
        return;
    }
    SaveManager::Instance->SaveSection(gSaveContext.fileNum, SECTION_ID_TRACKER_DATA, true);
}

bool OoTTrackerSetSkipped(uint16_t checkId, bool skipped) {
    if (!OoTTrackerSkipWritable() || checkId == 0 || checkId >= (uint16_t)RC_MAX) {
        return false;
    }
    auto ctx = Rando::Context::GetInstance();
    Rando::ItemLocation* loc = ctx->GetItemLocation((size_t)checkId);
    // The checks SoH's button is drawn on: on its list (#1126), and not found.
    if (!OoTCheckListed(ctx.get(), (RandomizerCheck)checkId) || StatusObtained(loc->GetCheckStatus())) {
        return false;
    }
    if (loc->GetIsSkipped() == skipped) {
        return true;
    }
    loc->SetIsSkipped(skipped);
    OoTTrackerPersistSkip();
    return true;
}

} // namespace

extern "C" void OoT_TrackerAdapter_CaptureAvailability(void) {
}

extern "C" void OoT_TrackerAdapter_Register(void) {
    static const ComboOoTTrackerOps kOps = {
        OoTTrackerSummary,   OoTTrackerCheckCount,   OoTTrackerCheckAt,
        OoTTrackerCheckName, OoTTrackerSkipWritable, OoTTrackerSetSkipped,
    };
    Combo_Tracker_RegisterOoT(&kOps);
}

// ============================================================================
// ROM-free lock support (redship --test combo-tracker-view)
// ============================================================================
//
// The lock lives in src/common/tests and must not see an RC_ or RG_
// enumerator, but exercising the adapter against AUTHORED heap data requires
// placing items — so the authoring happens here, in the TU where the enums
// are legal, and only flat ids cross back. The seam holds the context alive
// via a file-static shared_ptr; mContext is a weak_ptr, so releasing it here
// genuinely returns Rando::Context::GetInstance() to NULL and the lock can
// prove the never-booted path is re-checked per call rather than latched.

namespace {
std::shared_ptr<Rando::Context> sTrackerTestWorld;
} // namespace

/**
 * Author a minimal heap world: three placed checks — one collected, one
 * untouched, one skipped — under `seed`. Returns the number placed and writes
 * their flat check ids to outIds[3] (collected, untouched, skipped, in that
 * order) so the common-side lock can assert row content without OoT headers.
 */
extern "C" int OoT_TrackerAdapter_TestAuthorWorld(uint32_t seed, uint16_t outIds[3]) {
    // SetCheckStatus/SetIsSkipped dispatch GameInteractor hooks; the harness
    // has no instance, so create one exactly as the VB-veto test helpers do
    // (GameInteractor_Hooks.cpp). No hooks are registered, so dispatch is a
    // no-op.
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }

    sTrackerTestWorld = Rando::Context::CreateInstance();
    sTrackerTestWorld->SetSeed(seed);

    // The rows' names, areas and placed-item names (#458 U4) come from OoT's
    // static location and item tables, which only OoT's bring-up fills; a
    // ROM-free tier never ran it. Fill whichever is still empty (both are static
    // tables that a fill overwrites with the same contents, and InitItemTable
    // reads the Context created above).
    if (Rando::StaticData::GetLocation(RC_KF_KOKIRI_SWORD_CHEST)->GetName().empty()) {
        Rando::StaticData::InitLocationTable();
    }
    if (Rando::StaticData::RetrieveItem(RG_KOKIRI_SWORD).GetName().GetEnglish().empty()) {
        Rando::StaticData::InitItemTable();
    }

    struct {
        RandomizerCheck rc;
        RandomizerCheckStatus status;
        bool skipped;
    } authored[3] = {
        { RC_KF_KOKIRI_SWORD_CHEST, RCSHOW_SAVED, false },
        { RC_KF_MIDOS_TOP_LEFT_CHEST, RCSHOW_UNCHECKED, false },
        { RC_KF_MIDOS_TOP_RIGHT_CHEST, RCSHOW_UNCHECKED, true },
    };
    for (int i = 0; i < 3; i++) {
        Rando::ItemLocation* loc = sTrackerTestWorld->GetItemLocation(authored[i].rc);
        loc->SetPlacedItem(RG_KOKIRI_SWORD);
        loc->SetCheckStatus(authored[i].status);
        loc->SetIsSkipped(authored[i].skipped);
        if (outIds != nullptr) {
            outIds[i] = (uint16_t)authored[i].rc;
        }
    }
    return 3;
}

/**
 * Drop the authored world. GetInstance() returns NULL again afterwards — but
 * only because the back-edge is cut first: Context::CreateInstance hands the
 * context to its own Logic by SHARED pointer (SeedContext.cpp:93 ->
 * Logic::SetContext, whose member is a std::shared_ptr<Context>, logic.h:171),
 * and Context owns that Logic, so the pair keeps itself alive. Dropping our
 * reference alone leaves mContext un-expired forever and the never-booted path
 * becomes unreachable for the rest of the process.
 */
extern "C" void OoT_TrackerAdapter_TestReleaseWorld(void) {
    if (sTrackerTestWorld != nullptr) {
        sTrackerTestWorld->GetLogic()->SetContext(nullptr);
    }
    sTrackerTestWorld.reset();
}

/**
 * Mark OoT check @p rc collected (or back to unchecked) in whichever heap
 * Rando::Context is live: the authored world above, or a real generated one
 * (ComboCrossingViewsWorld marks a crossing host found through this, #755).
 * @return the check's previous obtained projection (1 or 0), or -1 with no
 *         heap context or an out-of-range id.
 */
extern "C" int OoT_TrackerAdapter_TestSetCollected(uint16_t rc, int collected) {
    auto ctx = Rando::Context::GetInstance();
    if (ctx == nullptr || rc == 0 || rc >= (uint16_t)RC_MAX) {
        return -1;
    }
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor(); // SetCheckStatus dispatches a hook
    }
    Rando::ItemLocation* loc = ctx->GetItemLocation((size_t)rc);
    const int was = StatusObtained(loc->GetCheckStatus()) ? 1 : 0;
    loc->SetCheckStatus(collected != 0 ? RCSHOW_COLLECTED : RCSHOW_UNCHECKED);
    return was;
}

/**
 * The skip toggle's lock seam (#458 U5): @p saveLoaded -1 restores production;
 * 0 or 1 answers "is a randomizer save loaded" for OoTRandoFileLoaded (the skip
 * toggle's and, #1126, the list's gate; the current game and the heap context
 * are still checked for real), and 2 answers "a save is loaded" with IS_RANDO
 * read for real. Each persist is then counted rather than written to a save
 * folder. @return the persists counted since the previous call, which resets
 * the count.
 */
extern "C" int OoT_TrackerAdapter_TestSkipSeam(int saveLoaded) {
    sSkipTestSaveLoaded = saveLoaded < 0 ? -1 : (saveLoaded == 2 ? 2 : (saveLoaded != 0 ? 1 : 0));
    const int persists = sSkipTestPersists;
    sSkipTestPersists = 0;
    return persists;
}

/**
 * The listing lock's seam (#1126): @p settingsLoaded -1 restores production; 0
 * or 1 answers "are SoH's Check Tracker settings loaded" for the adapter, and
 * with 1 a stand-in for SoH's IsVisibleInCheckTracker lists exactly the @p count
 * check ids at @p visible (the real predicate dereferences OTRGlobals::Instance,
 * which a ROM-free tier does not have). @return the questions put to the
 * predicate (the stand-in or the real one) since the previous call, which resets
 * the count.
 */
extern "C" int OoT_TrackerAdapter_TestListSeam(int settingsLoaded, const uint16_t* visible, int count) {
    sListTestLoaded = settingsLoaded < 0 ? -1 : (settingsLoaded != 0 ? 1 : 0);
    sListTestVisible.clear();
    if (visible != nullptr && count > 0) {
        sListTestVisible.assign(visible, visible + count);
    }
    const int calls = sListTestCalls;
    sListTestCalls = 0;
    return calls;
}

/**
 * The reachability lock's seam (#1129): @p current -1 restores production; 0 or 1
 * answers "is SoH's available set current" for the adapter (Enable Available
 * Checks on and a recalculation run; OoT_CheckTracker_AvailabilityCurrent in
 * production). The list's gate (OoT played, a randomizer file, SoH's tracker
 * settings) is still asked through its own seams.
 */
extern "C" void OoT_TrackerAdapter_TestAvailableSeam(int current) {
    sAvailTestCurrent = current < 0 ? -1 : (current != 0 ? 1 : 0);
}

/**
 * Author ItemLocation::IsAvailable for OoT check @p rc in whichever heap
 * Rando::Context is live (#1129), as a recalculation would. @return the previous
 * value (1 or 0), or -1 with no heap context or an out-of-range id.
 */
extern "C" int OoT_TrackerAdapter_TestSetAvailable(uint16_t rc, int available) {
    auto ctx = Rando::Context::GetInstance();
    if (ctx == nullptr || rc == 0 || rc >= (uint16_t)RC_MAX) {
        return -1;
    }
    Rando::ItemLocation* loc = ctx->GetItemLocation((size_t)rc);
    const int was = loc->IsAvailable() ? 1 : 0;
    loc->SetAvailable(available != 0);
    return was;
}

#endif // RSBS_SINGLE_EXECUTABLE
