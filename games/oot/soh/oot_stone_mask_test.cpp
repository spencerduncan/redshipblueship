/**
 * @file oot_stone_mask_test.cpp
 * OoT's half of the Stone Mask locks (#979, epic #975 P4's second mask; the
 * rules are RSBS_XITEM_STONE_MASK_OOT and RSBS_XITEM_SHARED_STONE in
 * src/common/foreign_items.h, the item is
 * soh/Enhancements/randomizer/StoneMaskOoT.cpp).
 *
 * THE HELPERS the redship rows `stone-mask-oot-rule` and `stone-mask-oot-items`
 * (src/common/tests/test_stone_mask_oot.c) read OoT's tables, its live save, its
 * REAL logic and the Stone Mask's REAL update seam through. They only put the
 * save, a heap Actor or a private logic probe in a known state and read it back;
 * every decision is the production code's. They live here because only an OoT
 * TU has OoT's ItemIDs, actor ids, `Actor`, `gSaveContext` and Rando::Logic in
 * scope.
 */

#include <cstdint>
#include <cstring>
#include <memory>

// At file scope, not inside an extern "C" block: z64.h includes <memory> under
// C++ (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
#include "variables.h"
#include "macros.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/logic.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/StoneMaskOoT.h"

extern "C" s32 Flags_GetRandomizerInf(RandomizerInf flag);
extern "C" void Flags_SetRandomizerInf(RandomizerInf flag);
extern "C" void Flags_UnsetRandomizerInf(RandomizerInf flag);

namespace {

// Two heap actors the update seam is driven on: only the id, the params and the
// four player-relative fields are read or written.
std::unique_ptr<Actor> sStoneActors[2];

// What Actor_UpdateAll computed for the armed actors, before any update.
constexpr s16 kTrueYaw = 0x1234;
constexpr f32 kTrueXz = 20.0f;
constexpr f32 kTrueY = -3.0f;
constexpr f32 kTrueXyzSq = 409.0f;

Actor* StoneActor(int slot) {
    if (slot < 0 || slot > 1) {
        return nullptr;
    }
    if (!sStoneActors[slot]) {
        sStoneActors[slot] = std::make_unique<Actor>();
    }
    return sStoneActors[slot].get();
}

/** OoTMM's shouldActorIgnorePlayer list (generator src/oot/actors.c), spelled
 *  again here as the oracle the production list is compared with. */
bool OotmmIgnoresWearer(int id, int params) {
    if (id == ACTOR_EN_AM) {
        return params != 0;
    }
    static const int kListed[] = {
        ACTOR_EN_NY,       ACTOR_EN_SB,       ACTOR_EN_RR,      ACTOR_EN_TORCH2,   ACTOR_EN_FZ,      ACTOR_EN_WEIYER,
        ACTOR_EN_EIYER,    ACTOR_EN_BB,       ACTOR_EN_ANUBICE, ACTOR_EN_TP,       ACTOR_EN_BA,      ACTOR_EN_GOMA,
        ACTOR_EN_DODOJR,   ACTOR_EN_SKJ,      ACTOR_EN_CROW,    ACTOR_EN_HINTNUTS, ACTOR_EN_WALLMAS, ACTOR_EN_REEBA,
        ACTOR_EN_FLOORMAS, ACTOR_EN_DEKUNUTS, ACTOR_EN_DNS,     ACTOR_EN_MB,       ACTOR_EN_TITE,    ACTOR_EN_PEEHAT,
        ACTOR_EN_FIREFLY,  ACTOR_EN_DODONGO,  ACTOR_EN_SW,      ACTOR_EN_VM,       ACTOR_EN_ST,      ACTOR_EN_GE1,
        ACTOR_EN_GE2,      ACTOR_EN_BILI,     ACTOR_EN_VALI,    ACTOR_EN_HEISHI2,  ACTOR_EN_HEISHI3, ACTOR_EN_HEISHI4,
    };
    for (const int listed : kListed) {
        if (id == listed) {
            return true;
        }
    }
    return false;
}

} // namespace

/** The Stone Mask's custom ItemID, and the Blast Mask's before it. */
extern "C" uint8_t OoT_StoneMaskTest_ItemId(void) {
    return (uint8_t)ITEM_MASK_STONE;
}

extern "C" uint8_t OoT_StoneMaskTest_BlastItemId(void) {
    return (uint8_t)ITEM_MASK_BLAST;
}

/** OoT's pool id for the Stone Mask (RG_STONE_MASK), and whether the item table
 *  names it an advancement item (what makes the bag class it PROGRESSION). */
extern "C" int OoT_StoneMaskTest_PoolId(void) {
    return (int)RG_STONE_MASK;
}

extern "C" int OoT_StoneMaskTest_IsAdvancement(void) {
    return Rando::StaticData::RetrieveItem(RG_STONE_MASK).IsAdvancement() ? 1 : 0;
}

/** Give (1) or take (0) OoT's ownership of the Stone Mask in the live save: its
 *  RAND_INF flag, which its get-item sets. */
extern "C" void OoT_StoneMaskTest_SetOwned(int held) {
    if (held != 0) {
        Flags_SetRandomizerInf(RAND_INF_OBTAINED_STONE_MASK);
    } else {
        Flags_UnsetRandomizerInf(RAND_INF_OBTAINED_STONE_MASK);
    }
}

extern "C" int OoT_StoneMaskTest_Owned(void) {
    return Flags_GetRandomizerInf(RAND_INF_OBTAINED_STONE_MASK) ? 1 : 0;
}

/** Give (1) or take (0) the Blast Mask's flag, for the cycle-order leg. */
extern "C" void OoT_StoneMaskTest_SetBlastOwned(int held) {
    if (held != 0) {
        Flags_SetRandomizerInf(RAND_INF_OBTAINED_BLAST_MASK);
    } else {
        Flags_UnsetRandomizerInf(RAND_INF_OBTAINED_BLAST_MASK);
    }
}

/**
 * The production ignore list against OoTMM's, over every OoT actor id with the
 * params 0 and 1: the number of answers that differ (0 when they agree), the
 * first differing id in @p firstBad (-1 when none) and the ids the list holds
 * with params 1 in @p listed.
 */
extern "C" int OoT_StoneMaskTest_IgnoreListMismatches(int* firstBad, int* listed) {
    int mismatches = 0;
    int count = 0;
    if (firstBad != nullptr) {
        *firstBad = -1;
    }
    for (int id = 0; id < (int)ACTOR_ID_MAX; id++) {
        for (int params = 0; params <= 1; params++) {
            const int got = OoT_StoneMask_IgnoresWearer((int16_t)id, (int16_t)params);
            const int want = OotmmIgnoresWearer(id, params) ? 1 : 0;
            if (got != want) {
                if (mismatches++ == 0 && firstBad != nullptr) {
                    *firstBad = id;
                }
            }
            count += (params == 1 && got == 1) ? 1 : 0;
        }
    }
    if (listed != nullptr) {
        *listed = count;
    }
    return mismatches;
}

/**
 * Arm heap actor @p slot (0 or 1) the way Actor_UpdateAll leaves an actor just
 * before its update: @p kind 0 a Skulltula (listed), 1 an Armos statue (the Armos
 * is listed only as the enemy), 2 an Armos enemy, else a Lizalfos (not listed);
 * the four player-relative fields at known true values.
 */
extern "C" void OoT_StoneMaskTest_ArmActor(int slot, int kind) {
    Actor* actor = StoneActor(slot);
    if (actor == nullptr) {
        return;
    }
    std::memset(actor, 0, sizeof(Actor));
    switch (kind) {
        case 0:
            actor->id = ACTOR_EN_ST;
            break;
        case 1:
            actor->id = ACTOR_EN_AM;
            actor->params = 0; // ARMOS_STATUE
            break;
        case 2:
            actor->id = ACTOR_EN_AM;
            actor->params = 1; // ARMOS_ENEMY
            break;
        default:
            actor->id = ACTOR_EN_ZF;
            break;
    }
    actor->yawTowardsPlayer = kTrueYaw;
    actor->xzDistToPlayer = kTrueXz;
    actor->yDistToPlayer = kTrueY;
    actor->xyzDistToPlayerSq = kTrueXyzSq;
}

/** The production seam on heap actor @p slot (StoneMaskOoT.cpp's hooks call
 *  exactly these). */
extern "C" int OoT_StoneMaskTest_Before(int slot, uint32_t frame, int updates) {
    return OoT_StoneMask_BeforeActorUpdate(StoneActor(slot), frame, updates);
}

extern "C" void OoT_StoneMaskTest_After(int slot) {
    OoT_StoneMask_AfterActorUpdate(StoneActor(slot));
}

/** 1 when heap actor @p slot holds the true fields it was armed with; 2 when it
 *  holds OoTMM's "far away" (the three distances 10000); 0 otherwise. */
extern "C" int OoT_StoneMaskTest_Seen(int slot) {
    const Actor* actor = StoneActor(slot);
    if (actor == nullptr) {
        return 0;
    }
    if (actor->yawTowardsPlayer == kTrueYaw && actor->xzDistToPlayer == kTrueXz && actor->yDistToPlayer == kTrueY &&
        actor->xyzDistToPlayerSq == kTrueXyzSq) {
        return 1;
    }
    if (actor->xzDistToPlayer == 10000.0f && actor->yDistToPlayer == 10000.0f && actor->xyzDistToPlayerSq == 10000.0f) {
        return 2;
    }
    return 0;
}

/**
 * OoT's REAL logic for the Stone Mask: a private Rando::Logic over a zeroed save
 * holding nothing but the mask's ownership flag (@p held), as a child or an
 * adult (@p child), under whatever cross-game item rules the caller froze.
 * @p which: 0 CanPassEnemy(RE_BIG_SKULLTULA), 1 CanPassEnemy(RE_GERUDO_GUARD),
 * 2 CanPassEnemy(RE_BREAK_ROOM_GUARD), 3 CanUse(RG_STONE_MASK), 4 BlastOrSmash
 * (the Stone Mask is no explosive), 5 CanPassEnemy(RE_BIG_SKULLTULA) without the
 * Stone Mask's credit (OoTMM's get_past_skultullas_no_stone, the MQ Forest
 * basement pot room's way in), else HasItem(RG_STONE_MASK).
 * 1 or 0; -1 with no randomizer context.
 */
extern "C" int OoT_StoneMaskTest_Logic(int held, int child, int which) {
    std::shared_ptr<Rando::Context> ctx = Rando::Context::GetInstance();
    bool created = false;
    if (ctx == nullptr) {
        ctx = Rando::Context::CreateInstance();
        created = true;
    }
    if (ctx == nullptr) {
        return -1;
    }
    auto save = std::make_unique<SaveContext>();
    std::memset(save.get(), 0, sizeof(SaveContext));
    // Empty pockets: a zeroed slot holds ITEM_DEKU_STICK (0), which the logic's
    // inexact inventory checks (CheckInventory(item, false)) read as held.
    std::memset(save->inventory.items, ITEM_NONE, sizeof(save->inventory.items));
    auto probe = std::make_unique<Rando::Logic>();
    probe->SetContext(ctx);
    probe->SetSaveContext(save.get());
    probe->Reset(false);
    probe->IsChild = child != 0;
    probe->IsAdult = child == 0;
    probe->SetRandoInf((uint32_t)RAND_INF_OBTAINED_STONE_MASK, held != 0);
    bool result = false;
    switch (which) {
        case 0:
            result = probe->CanPassEnemy(RE_BIG_SKULLTULA);
            break;
        case 1:
            result = probe->CanPassEnemy(RE_GERUDO_GUARD);
            break;
        case 2:
            result = probe->CanPassEnemy(RE_BREAK_ROOM_GUARD);
            break;
        case 3:
            result = probe->CanUse(RG_STONE_MASK);
            break;
        case 4:
            result = probe->BlastOrSmash();
            break;
        case 5:
            result = probe->CanPassEnemy(RE_BIG_SKULLTULA, ED_CLOSE, true, false);
            break;
        default:
            result = probe->HasItem(RG_STONE_MASK);
            break;
    }
    probe.reset();
    if (created) {
        // Context and its Logic hold each other: let go of the one this row made.
        ctx->GetLogic()->SetContext(nullptr);
    }
    return result ? 1 : 0;
}
