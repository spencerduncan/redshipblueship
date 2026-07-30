/**
 * @file shared_resources.c
 * @brief Shared cross-game resource merge core (#525).
 *
 * See shared_resources.h for the model, the two merge disciplines and why the
 * watermark is mandatory rather than an optimization.
 *
 * This TU is intentionally free of game headers — it touches
 * gComboCtx.sharedResources and a small RAM-only watermark table and nothing
 * else — so it compiles into the shared redship_common library and both games'
 * per-game shims call it directly with values they read out of their own
 * gSaveContext. The unit conversions and field names stay on the game side,
 * where the headers are; the merge rules stay here, where there is exactly one
 * copy of them.
 */

#include "shared_resources.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// The RAM-only watermark table: how much of the shared pool is currently
// MATERIALIZED in the live save, per resource.
//
// Indexed by KIND, not by slot: slot positions are found by scan and are not
// promised to be stable, while a kind is a fixed enumerator. `owner` is the
// game the watermark describes — GAME_NONE means "no game has been applied to
// for this resource in this process", which is the first-harvest seed case the
// header spells out.
// ============================================================================

typedef struct {
    uint16_t value; // the live value at the last apply/harvest sync point
    uint8_t owner;  // GameId the watermark belongs to; GAME_NONE = not live
} SharedResWatermark;

static SharedResWatermark sWatermarks[RSBS_SHARED_RES_KIND_COUNT];

static bool IsRealGame(GameId game) {
    return game == GAME_OOT || game == GAME_MM;
}

static bool IsRealKind(uint8_t kind) {
    return kind != RSBS_SHARED_RES_NONE && kind < RSBS_SHARED_RES_KIND_COUNT;
}

/**
 * Which discipline `kind` merges under. This switch is the ONE authoritative
 * statement of the split — the RSBS_SHARED_RES_F_MONOTONIC flag byte stored on
 * the slot is descriptive (so a reader or a save inspector can tell them apart
 * without this table), never the thing decided on.
 */
static bool IsMonotonicKind(uint8_t kind) {
    switch (kind) {
        case RSBS_SHARED_RES_WALLET_TIER:
        case RSBS_SHARED_RES_HEALTH_QUARTERS:
        case RSBS_SHARED_RES_DOUBLE_DEFENSE:
        case RSBS_SHARED_RES_MAGIC_LEVEL:
        case RSBS_SHARED_RES_QUIVER_TIER:
        case RSBS_SHARED_RES_BOMB_BAG_TIER:
        case RSBS_SHARED_RES_STICK_TIER:
        case RSBS_SHARED_RES_NUT_TIER:
            return true;
        default:
            // The spent quantities — rupees, current health, current magic, and
            // the five ammo counts — are delta-harvested.
            return false;
    }
}

// ============================================================================
// TWO PHYSICAL BLOCKS, ONE LOGICAL ARRAY.
//
// gComboCtx.sharedResources[8] and gComboCtx.sharedResourcesExt[12] are
// separate fields at separate .redsave offsets — they must be, because
// widening the first in place would move the second (and everything carved
// after it) off the offset every shipped save stored it at. But nothing above
// this line should have to know that: a "slot" here is a LOGICAL index into
// the concatenation, and every scan below covers the whole range in one pass.
//
// This resolver is the only place the split is visible. Keeping it that way is
// the point — a scan that stops at the first block's end would split a kind
// across blocks (two slots claiming one resource) or report "full" with twelve
// free slots behind it.
// ============================================================================

static ComboSharedResource* SlotAt(int logical) {
    if (logical < 0 || logical >= (int)RSBS_SHARED_RESOURCE_TOTAL_CAP) {
        return NULL;
    }
    if (logical < (int)RSBS_SHARED_RESOURCE_CAP) {
        return &gComboCtx.sharedResources[logical];
    }
    return &gComboCtx.sharedResourcesExt[logical - (int)RSBS_SHARED_RESOURCE_CAP];
}

// Locate the occupied slot holding `kind`, or -1 when the resource has never
// been shared. Slots are unordered; there is at most one per kind because
// FindOrCreateSlot always looks first — across BOTH blocks.
static int FindSlot(uint8_t kind) {
    for (int i = 0; i < (int)RSBS_SHARED_RESOURCE_TOTAL_CAP; i++) {
        if (SlotAt(i)->kind == kind) {
            return i;
        }
    }
    return -1;
}

// Locate `kind`'s slot, claiming a free one (value 0) if it has none. Returns
// -1 only when every slot is taken by some OTHER resource. Nothing is ever
// evicted: unlike a shared ITEM, a shared resource has no "redeemed" state, so
// there is no slot that is safe to reclaim — a full array means the resource
// set outgrew both blocks, which is a carve-time bug, not a runtime condition.
// The blocks are sized with room for the whole class.
static int FindOrCreateSlot(uint8_t kind) {
    int slot = FindSlot(kind);
    if (slot >= 0) {
        return slot;
    }
    for (int i = 0; i < (int)RSBS_SHARED_RESOURCE_TOTAL_CAP; i++) {
        ComboSharedResource* entry = SlotAt(i);
        if (entry->kind == RSBS_SHARED_RES_NONE) {
            entry->kind = kind;
            entry->flags = IsMonotonicKind(kind) ? RSBS_SHARED_RES_F_MONOTONIC : 0u;
            entry->value = 0u;
            return i;
        }
    }
    fprintf(stderr,
            "[combo] shared-resource slots full across both blocks; dropping kind=%u (carve a THIRD "
            "block from reserved[] and span it too — never widen an existing block in place)\n",
            (unsigned)kind);
    return -1;
}

// ============================================================================
// The canonical heart quantity. See the header for why hearts are ONE number.
// ============================================================================

uint16_t Combo_MakeHealthQuarters(uint16_t capacity, uint16_t pieces) {
    uint32_t quarters = (uint32_t)capacity + (uint32_t)pieces * 4u;
    if (quarters > RSBS_SHARED_RES_MAX_HEALTH_QUARTERS) {
        quarters = RSBS_SHARED_RES_MAX_HEALTH_QUARTERS;
    }
    return (uint16_t)quarters;
}

void Combo_SplitHealthQuarters(uint16_t quarters, uint16_t* outCapacity, uint16_t* outPieces) {
    if (quarters > RSBS_SHARED_RES_MAX_HEALTH_QUARTERS) {
        quarters = (uint16_t)RSBS_SHARED_RES_MAX_HEALTH_QUARTERS;
    }
    if (outCapacity != NULL) {
        *outCapacity = (uint16_t)((quarters / 16u) * 16u);
    }
    if (outPieces != NULL) {
        *outPieces = (uint16_t)((quarters % 16u) / 4u);
    }
}

void Combo_HarvestSharedResource(GameId game, uint8_t kind, uint16_t liveValue) {
    if (!IsRealGame(game) || !IsRealKind(kind)) {
        return;
    }

    // Read occupancy BEFORE any slot is created: "has this resource ever been
    // shared" is the discriminator the first-harvest seed turns on, and
    // FindOrCreateSlot would destroy the answer.
    const bool everShared = (FindSlot(kind) >= 0);

    if (IsMonotonicKind(kind)) {
        if (!everShared && liveValue == 0) {
            // Nothing to record. Deliberately does NOT create the slot: every
            // game harvests its whole resource set at each suspend, and a
            // player who has no wallet upgrade and no double defense yet would
            // otherwise occupy slots with zeros that read as "shared" — making
            // a fresh world indistinguishable from a played one, which is the
            // growth contract's "zero means unset" rule turned inside out.
            return;
        }
        const int slot = FindOrCreateSlot(kind);
        if (slot < 0) {
            return;
        }
        ComboSharedResource* entry = SlotAt(slot);
        if (liveValue > entry->value) {
            entry->value = liveValue;
        }
        return;
    }

    // CONSUMABLE: delta against the watermark, never a raw copy.
    SharedResWatermark* wm = &sWatermarks[kind];
    if (wm->owner != (uint8_t)game) {
        // No live watermark for this game — either a cold boot or a .redsave
        // load. Seed per the header's rule: an occupied slot means the pool was
        // written by a harvest that already counted this balance (seed at live,
        // delta zero), an empty slot means nothing was ever shared (seed at
        // zero, so the whole balance enters the pool).
        wm->value = everShared ? liveValue : 0u;
        wm->owner = (uint8_t)game;
    }

    const int32_t delta = (int32_t)liveValue - (int32_t)wm->value;
    wm->value = liveValue;
    if (delta == 0) {
        // Nothing changed. Deliberately does NOT create the slot: an untouched
        // resource stays unshared, which keeps a zero-extended legacy record
        // and a genuinely-never-used resource indistinguishable, as the growth
        // contract requires.
        return;
    }

    int slot = FindOrCreateSlot(kind);
    if (slot < 0) {
        return;
    }
    ComboSharedResource* entry = SlotAt(slot);
    int32_t next = (int32_t)entry->value + delta;
    if (next < 0) {
        next = 0;
    }
    if (next > 0xFFFF) {
        next = 0xFFFF;
    }
    entry->value = (uint16_t)next;
}

bool Combo_ApplySharedResource(GameId game, uint8_t kind, uint16_t cap, uint16_t* liveValue) {
    if (!IsRealGame(game) || !IsRealKind(kind) || liveValue == NULL) {
        return false;
    }

    const int slot = FindSlot(kind);
    if (slot < 0) {
        // Never shared: nothing to apply, and deliberately NO watermark write.
        // Seeding lives in exactly one place — the harvest rule above — because
        // two seeding rules is how the halves disagree. Leaving the watermark
        // unset here means the next harvest applies that one rule and sees an
        // empty slot, so the game's existing balance joins the pool as genuinely
        // new money. That is the same answer a cold boot gets, which is what
        // makes "which side went first" stop mattering.
        return false;
    }

    const uint16_t shared = SlotAt(slot)->value;
    const uint16_t applied = (shared > cap) ? cap : shared;

    if (IsMonotonicKind(kind)) {
        // Raise only. A capacity the arriving game already exceeds is its own
        // business and its next harvest folds it back into the pool.
        if (applied > *liveValue) {
            *liveValue = applied;
        }
        return true;
    }

    // CONSUMABLE. Record what was ACTUALLY materialized, not what the pool
    // holds: the difference is the money that does not fit in this game's
    // wallet, and it must stay in the pool rather than be harvested away.
    *liveValue = applied;
    sWatermarks[kind].value = applied;
    sWatermarks[kind].owner = (uint8_t)game;
    return true;
}

bool Combo_GetSharedResource(uint8_t kind, uint16_t* outValue) {
    if (!IsRealKind(kind) || outValue == NULL) {
        return false;
    }
    const int slot = FindSlot(kind);
    if (slot < 0) {
        return false;
    }
    *outValue = SlotAt(slot)->value;
    return true;
}

int Combo_CountSharedResources(void) {
    int count = 0;
    for (int i = 0; i < (int)RSBS_SHARED_RESOURCE_TOTAL_CAP; i++) {
        if (SlotAt(i)->kind != RSBS_SHARED_RES_NONE) {
            count++;
        }
    }
    return count;
}

void Combo_ResetSharedResourceWatermarks(void) {
    memset(sWatermarks, 0, sizeof(sWatermarks));
}
