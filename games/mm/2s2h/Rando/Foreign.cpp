/**
 * Rando::Foreign — MM-side foreign-item placement + give-path core
 * (Phase 3.0 Lane C1, #392).
 *
 * Placement model (the C0 handoff's design, ADR 0002 at every boundary):
 * after MM's own fill has populated RANDO_SAVE_CHECKS, deterministically pick
 * N junk-holding shuffled checks and mark them as hosting the pinned OoT
 * progression items. The MM save table KEEPS RI_JUNK at those checks — a raw
 * RG_* never enters an MM table — while gComboCtx.foreignPlacements records
 * "check X hosts SharedItem{GAME_OOT, id}". If the placement table is ever
 * absent (a pre-C1 .redsave zero-extends to an empty table), the hosting
 * checks degrade to the junk they physically hold: nothing crashes, nothing
 * aliases.
 *
 * Determinism: selection uses a LOCAL xorshift32 stream seeded from the
 * paired-world identity (master seed + settings digest + MM final seed), so
 * it can never be perturbed by other Ship_Random consumers, and candidate
 * order comes from std::map's sorted iteration. Same gComboCtx + same MM
 * options => same placements, which is what the SeedDeterminism fold locks.
 *
 * Paired-world generation failure (a fill dead-end under the derived seed) is
 * NOT retried here: OnFileCreate's existing catch reverts the save to vanilla,
 * exactly as upstream does for any failed generation. The derivation is
 * attempt-free on purpose — a retry counter would make the world identity
 * depend on runtime state the digest cannot see. If a pinned CI seed
 * dead-ends, the fix is to pin a different seed, not to bend the derivation.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include "Foreign.h"
#include "Rando/Rando.h"
#include "2s2h/ShipUtils.h"

// src/common — placement table + pinned pool surface, and the A1 producer
// (Combo_RecordSharedItem). Included OUTSIDE any extern "C" block: they pull
// context.h, whose <type_traits> include must not be wrapped in C linkage
// (see context.h's header comment).
#include "foreign_items.h"
#include "shared_items.h"

#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

extern "C" {
#include "variables.h"
}

namespace Rando {
namespace Foreign {

bool PairingActive() {
    return Combo_ForeignPairingActive();
}

std::string PairedInputSeedString() {
    // Plain decimal of the master seed with a stable prefix: deterministic,
    // filename-safe (the spoiler lands at randomizer/<inputSeed>.json), and
    // recognizable in logs/artifacts as a paired-world file.
    return "RSBSPAIR" + std::to_string(gComboCtx.sharedRandoSeed);
}

static std::string MMOptionsString() {
    // The persisted options ARE the finalized MM settings profile — the loop
    // below must run after OnFileCreate copied them into the save. std::map
    // iteration gives a stable option order.
    std::string s;
    for (auto& [randoOptionId, randoStaticOption] : Rando::StaticData::Options) {
        s += std::to_string(RANDO_SAVE_OPTIONS[randoOptionId]);
        s += ';';
    }
    return s;
}

uint32_t MixPairedFinalSeed() {
    // Mirrors OoT's Playthrough_Init: Hash(str(master seed) + settings), so
    // the same master seed reproduces the MM world only under the same MM
    // options — the "one seed + one pinned settings profile" contract.
    return Ship_Hash(std::to_string(gComboCtx.sharedRandoSeed) + MMOptionsString());
}

// Local, self-contained PRNG for placement selection (see the file header).
static uint32_t sSelectState;

static uint32_t SelectNext() {
    uint32_t x = sSelectState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    sSelectState = (x != 0) ? x : 0xB5297A4Du;
    return sSelectState;
}

int PlaceForeignItems() {
    Combo_ClearForeignPlacements();

    if (!PairingActive()) {
        return 0;
    }

    const ComboForeignItemDef* pool = nullptr;
    const int poolCount = Combo_GetForeignItemPool(&pool);
    if (poolCount <= 0 || pool == nullptr) {
        return 0;
    }

    // Candidates: shuffled checks the fill left holding junk, in ascending
    // RandoCheckId order (std::map).
    std::vector<RandoCheckId> candidates;
    for (auto& [randoCheckId, randoStaticCheck] : Rando::StaticData::Checks) {
        if (randoStaticCheck.randoCheckId == RC_UNKNOWN) {
            continue;
        }
        if (RANDO_SAVE_CHECKS[randoCheckId].shuffled && RANDO_SAVE_CHECKS[randoCheckId].randoItemId == RI_JUNK) {
            candidates.push_back(randoCheckId);
        }
    }

    sSelectState =
        Ship_Hash(std::to_string(gComboCtx.sharedRandoSeed) + ":" + std::to_string(gComboCtx.sharedRandoSettingsHash) +
                  ":" + std::to_string(gSaveContext.save.shipSaveInfo.rando.finalSeed) + ":foreign-v1");
    if (sSelectState == 0) {
        sSelectState = 0xB5297A4Du;
    }

    int placed = 0;
    for (int i = 0; i < poolCount && !candidates.empty(); i++) {
        const size_t pick = (size_t)(SelectNext() % (uint32_t)candidates.size());
        const RandoCheckId hostCheck = candidates[pick];
        candidates.erase(candidates.begin() + (std::ptrdiff_t)pick);

        if (Combo_SetForeignPlacement((uint16_t)hostCheck, pool[i].item) >= 0) {
            placed++;
            fprintf(stderr, "[MM] foreign placement: '%s' hosted at MM check %s\n", pool[i].name,
                    Rando::StaticData::Checks[hostCheck].name);
        }
    }

    if (placed < poolCount) {
        fprintf(stderr, "[MM] foreign placement: only %d of %d pool items placed (junk candidates exhausted)\n", placed,
                poolCount);
    }
    return placed;
}

static const SharedItem* PlacementFor(RandoCheckId randoCheckId) {
    if (randoCheckId == RC_UNKNOWN) {
        return nullptr;
    }
    return Combo_GetForeignPlacementForCheck((uint16_t)randoCheckId);
}

bool IsForeignCheck(RandoCheckId randoCheckId) {
    return PlacementFor(randoCheckId) != nullptr;
}

const char* ForeignNameForCheck(RandoCheckId randoCheckId) {
    const SharedItem* item = PlacementFor(randoCheckId);
    if (item == nullptr) {
        return nullptr;
    }
    return Combo_GetForeignItemName(*item);
}

bool RecordForeignPickup(RandoCheckId randoCheckId) {
    const SharedItem* item = PlacementFor(randoCheckId);
    if (item == nullptr) {
        return false;
    }
    // Durable immediately (Combo_RecordSharedItem writes the serialized array,
    // so an MM save+quit before the next switch cannot lose the pickup — the
    // stage/commit outbox is RAM-only, see shared_items.h). The producer
    // de-dups an identical un-redeemed entry, so a re-fired give cannot
    // double-record.
    return Combo_RecordSharedItem((GameId)item->originGame, item->id) >= 0;
}

} // namespace Foreign
} // namespace Rando

// ============================================================================
// ROM-free test bridge (redship tier; src/common/tests/test_foreign_items.c).
// Drives the SAME give-path core the CheckQueue lambda calls, so the lock
// covers the real recording path, not a copy.
// ============================================================================
extern "C" int MM_Rando_Foreign_RecordPickup(uint16_t randoCheckId) {
    return Rando::Foreign::RecordForeignPickup((RandoCheckId)randoCheckId) ? 1 : 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
