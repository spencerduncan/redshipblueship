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
 * session. The view labels the result live only while OoT is the running
 * game ("Updated live"), and stale otherwise ("As of the last game switch").
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

#include <cstdint>
#include <map>
#include <memory>
#include <string>

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

bool OoTTrackerSummary(ComboTrackerGameSummary* out) {
    auto ctx = Rando::Context::GetInstance();
    if (ctx == nullptr || out == nullptr) {
        return false;
    }

    int shuffled = 0;
    int obtained = 0;
    int skipped = 0;
    for (size_t i = 1; i < (size_t)RC_MAX; i++) { // 0 == RC_UNKNOWN_CHECK
        Rando::ItemLocation* loc = ctx->GetItemLocation(i);
        if (loc->GetPlacedRandomizerGet() == RG_NONE) {
            continue; // not part of this seed (IsLocationShuffled's own predicate)
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
    out->hasWorld = shuffled > 0;
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
    out->shuffled = loc->GetPlacedRandomizerGet() != RG_NONE;
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

// The ROM-free lock's stand-ins for the two things it cannot have: a loaded save
// (GameInteractor::IsSaveLoaded needs a play state and a player) and a save
// folder to write. -1 is production; 0 or 1 is the lock's answer to "is a save
// loaded", and then the persist step is counted instead of run.
int sSkipTestSaveLoaded = -1;
int sSkipTestPersists = 0;

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
    if (sSkipTestSaveLoaded >= 0) {
        return sSkipTestSaveLoaded != 0;
    }
    return GameInteractor::IsSaveLoaded() && IS_RANDO;
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
    Rando::ItemLocation* loc = Rando::Context::GetInstance()->GetItemLocation((size_t)checkId);
    // The checks SoH's button is drawn on: part of the seed, and not found.
    if (loc->GetPlacedRandomizerGet() == RG_NONE || StatusObtained(loc->GetCheckStatus())) {
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
 * 0 or 1 answers "is a save loaded" for OoTTrackerSkipWritable (the current game
 * and the heap context are still checked for real), and each persist is then
 * counted rather than written to a save folder. @return the persists counted
 * since the previous call, which resets the count.
 */
extern "C" int OoT_TrackerAdapter_TestSkipSeam(int saveLoaded) {
    sSkipTestSaveLoaded = saveLoaded < 0 ? -1 : (saveLoaded != 0 ? 1 : 0);
    const int persists = sSkipTestPersists;
    sSkipTestPersists = 0;
    return persists;
}

#endif // RSBS_SINGLE_EXECUTABLE
