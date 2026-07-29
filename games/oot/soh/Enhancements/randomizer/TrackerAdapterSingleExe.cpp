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
 * game, "as of suspend" otherwise.
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
#include <memory>
#include <string>

#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/static_data.h"
// SeedContext.h only FORWARD-declares Rando::Logic, and the test seam below has
// to call Logic::SetContext to cut the Context<->Logic ownership cycle.
#include "soh/Enhancements/randomizer/logic.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"

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
    return true;
}

} // namespace

extern "C" void OoT_TrackerAdapter_Register(void) {
    static const ComboOoTTrackerOps kOps = {
        OoTTrackerSummary,
        OoTTrackerCheckCount,
        OoTTrackerCheckAt,
        OoTTrackerCheckName,
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

#endif // RSBS_SINGLE_EXECUTABLE
