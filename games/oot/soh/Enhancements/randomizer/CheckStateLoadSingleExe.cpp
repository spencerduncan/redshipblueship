/**
 * CheckStateLoadSingleExe.cpp — OoT's check state follows the save half that
 * was loaded (#849, and the load leg of #803, which runs the same way).
 *
 * WHERE THE STATE LIVES. Whether an OoT check is obtained is
 * Rando::ItemLocation::status on the heap Rando::Context (HasObtained() is
 * COLLECTED or SAVED). It is not in the SaveContext: the .sav keeps it in its
 * own tracker section (randomizer_check_tracker.cpp, SaveTrackerData), and a
 * section-only save writes a COLLECTED check as SCUMMED, which does not count
 * as obtained.
 *
 * WHAT GOES WRONG. A load whose .redsave holds a newer whole commit than the
 * .sav (every reload after a crossing since #846, an MM owl save, OoT's exit
 * snapshot) applies the .redsave's OoT half over the .sav's base section
 * (SaveManager.cpp, OoT_Combo_OnLoadFileSeam). The tracker section stays as the
 * .sav has it, so the statuses describe the .sav's base and not the half that
 * is now live:
 *
 *   - forward (#849, and #803's load leg): a heart piece, small key, heart container or shuffled
 *     freestanding item collected before the crossing has its flag set in the
 *     loaded half but reads SCUMMED. Their despawn tests read the status
 *     (hook_handlers.cpp VB_ITEM00_DESPAWN / VB_ITEM_B_HEART_DESPAWN,
 *     ShuffleFreestanding.cpp), so the item is back in the world, and the
 *     check trackers show the check as not found.
 *   - reverse: a check the .sav holds as SAVED whose flag the loaded half
 *     does not have (a half that went back). Opening the chest again fires the
 *     flag-set hook, which queues nothing for an obtained location
 *     (RandomizerOnSceneFlagSetHandler), so the item is never given. No
 *     production load is known to produce this: the half is only applied when
 *     the .redsave's commit is newer than the .sav's (a .sav-newer pair is
 *     refused, save.cpp EvaluateSlot), and save flags only get set in play.
 *     The rule covers it anyway, so a half that does go back cannot strand an
 *     item.
 *
 * THE RULE. In a rando file every give is queued when its location's
 * collection flag goes from unset to set (RandomizerOnFlagSetHandler and
 * RandomizerOnSceneFlagSetHandler map the flag back to the check through the
 * same collection check read here), and every OoT location that holds an item
 * has a collection flag a save half holds. So for each check whose own
 * collection flag DIFFERS between the half the statuses describe and the half
 * that was loaded, the loaded half decides:
 *
 *   - set in the loaded half and not obtained: RCSHOW_SAVED (it is in a
 *     committed save half);
 *   - unset in the loaded half and obtained: RCSHOW_SCUMMED (SoH's own "you
 *     collected it, but the save you loaded does not have it", which does not
 *     count as obtained).
 *
 * A check whose flag is the same in both halves is not touched: its status is
 * what SoH's own load gives it. That keeps a check that file creation marks
 * obtained without setting its flag (StartingItemGive: the Master Sword on an
 * adult start) obtained; a plain "status := flag" rule would undo it and let it
 * be given again. Checks with no flag a save half holds (none of them holds an
 * item: RC_GANON and RC_TRIFORCE_COMPLETED), a temporary collectible flag,
 * checks the flag-set hooks do not resolve to (a flag two locations share
 * resolves to the one the dungeon's quest selects) and locations outside the
 * seed (nothing placed) are not touched either.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstddef>
#include <cstdint>

#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/location.h"
#include "soh/Enhancements/randomizer/item_location.h"

extern "C" {
#include <z64.h>
#include "macros.h"    // ARRAY_COUNTU
#include "variables.h" // gGsFlagsMasks / gGsFlagsShifts (z_inventory.c)
}

#define NOGDI // avoid various windows defines that conflict with things in z64.h
#include <spdlog/spdlog.h>

// hook_handlers.cpp: the flag -> check maps the flag-set hooks queue gives
// through (C++ linkage there; no header of their own).
RandomizerCheck GetRandomizerCheckFromFlag(int16_t flagType, int16_t flag);
RandomizerCheck GetRandomizerCheckFromSceneFlag(int16_t sceneNum, int16_t flagType, int16_t flag);

namespace {

// One bit of a u16 flag array as the Flags_Get* functions read it.
template <typename T, size_t N> bool InfFlag(const T (&words)[N], uint32_t flag, bool* outSet) {
    const uint32_t word = flag >> 4;
    if (word >= N) {
        return false;
    }
    *outSet = (words[word] & (1u << (flag & 0xF))) != 0;
    return true;
}

// The flag a location is given by, as the flag-set hooks receive it.
struct CollectionFlag {
    int16_t type;
    int16_t scene;
    int16_t flag;
};

// Fills *out with `loc`'s collection flag and *outSet with whether `save` holds
// it. false when no save half holds that flag.
bool CollectionFlagIn(const SaveContext& save, const Rando::Location& loc, CollectionFlag* out, bool* outSet) {
    const Rando::SpoilerCollectionCheck cc = loc.GetCollectionCheck();
    out->scene = cc.scene;
    out->flag = (int16_t)cc.flag;
    switch (cc.type) {
        case SPOILER_CHK_CHEST:
            if (cc.scene >= ARRAY_COUNTU(save.sceneFlags) || cc.flag >= 32) {
                return false;
            }
            out->type = FLAG_SCENE_TREASURE;
            *outSet = (save.sceneFlags[cc.scene].chest & (1u << cc.flag)) != 0;
            return true;
        case SPOILER_CHK_COLLECTABLE:
            // 0x20 and up are the per-visit temp flags, which no save holds.
            if (cc.scene >= ARRAY_COUNTU(save.sceneFlags) || cc.flag >= 32) {
                return false;
            }
            out->type = FLAG_SCENE_COLLECTIBLE;
            *outSet = (save.sceneFlags[cc.scene].collect & (1u << cc.flag)) != 0;
            return true;
        case SPOILER_CHK_GOLD_SKULLTULA: {
            // En_Si sets SET_GS_FLAGS((params & 0x1F00) >> 8, params & 0xFF) and
            // fires FLAG_GS_TOKEN with the whole params, which is what the hook
            // maps back to the check (z_en_si.c).
            const uint32_t params = (uint32_t)loc.GetActorParams();
            const uint32_t index = (params & 0x1F00) >> 8;
            const uint32_t mask = params & 0xFF;
            if ((index >> 2) >= ARRAY_COUNTU(save.gsFlags) || mask == 0) {
                return false;
            }
            const uint32_t bits =
                ((uint32_t)save.gsFlags[index >> 2] & gGsFlagsMasks[index & 3]) >> gGsFlagsShifts[index & 3];
            out->type = FLAG_GS_TOKEN;
            out->flag = (int16_t)params;
            *outSet = (bits & mask) != 0;
            return true;
        }
        case SPOILER_CHK_ITEM_GET_INF:
            out->type = FLAG_ITEM_GET_INF;
            return InfFlag(save.itemGetInf, cc.flag, outSet);
        case SPOILER_CHK_EVENT_CHK_INF:
            out->type = FLAG_EVENT_CHECK_INF;
            return InfFlag(save.eventChkInf, cc.flag, outSet);
        case SPOILER_CHK_INF_TABLE:
            out->type = FLAG_INF_TABLE;
            return InfFlag(save.infTable, cc.flag, outSet);
        case SPOILER_CHK_RANDOMIZER_INF:
            out->type = FLAG_RANDOMIZER_INF;
            return InfFlag(save.ship.randomizerInf, cc.flag, outSet);
        default: // SPOILER_CHK_NONE, SPOILER_CHK_GRAVEDIGGER
            return false;
    }
}

// Would the flag-set hooks queue THIS check when that flag is set? The same maps
// they use, so a flag two locations share resolves the way the give does.
bool HooksResolveTo(RandomizerCheck rc, const CollectionFlag& f) {
    if (f.type == FLAG_SCENE_TREASURE || f.type == FLAG_SCENE_COLLECTIBLE) {
        return GetRandomizerCheckFromSceneFlag(f.scene, f.type, f.flag) == rc;
    }
    return GetRandomizerCheckFromFlag(f.type, f.flag) == rc;
}

} // namespace

/**
 * Make OoT's check statuses follow `loaded`, the save half that is now live,
 * where it differs from `described`, the half the statuses were loaded beside
 * (the .sav's base section). See the file comment for the rule. Rando files
 * only: a vanilla file's tracker marks checks through its own maps
 * (CheckTrackerFlagSet) and gives nothing through these statuses, so on a
 * vanilla file the tracker's display can still lag the loaded half.
 *
 * @return the number of checks changed; *outFound / *outNotFound (either may
 *         be NULL) split it. 0 with no heap Rando::Context.
 */
extern "C" int OoT_Combo_ReconcileCheckStateToLoadedHalf(const SaveContext* described, const SaveContext* loaded,
                                                         int* outFound, int* outNotFound) {
    int found = 0;
    int notFound = 0;
    auto ctx = Rando::Context::GetInstance();
    if (ctx != nullptr && described != nullptr && loaded != nullptr && loaded->ship.quest.id == QUEST_RANDOMIZER) {
        for (size_t i = 1; i < (size_t)RC_MAX; i++) { // 0 == RC_UNKNOWN_CHECK
            const RandomizerCheck rc = (RandomizerCheck)i;
            Rando::ItemLocation* itemLoc = ctx->GetItemLocation(rc);
            const Rando::Location* loc = Rando::StaticData::GetLocation(rc);
            if (itemLoc == nullptr || loc == nullptr || itemLoc->GetPlacedRandomizerGet() == RG_NONE) {
                continue;
            }
            CollectionFlag f{};
            bool wasSet = false;
            bool isSet = false;
            if (!CollectionFlagIn(*described, *loc, &f, &wasSet) || !CollectionFlagIn(*loaded, *loc, &f, &isSet) ||
                wasSet == isSet || isSet == itemLoc->HasObtained() || !HooksResolveTo(rc, f)) {
                continue;
            }
            if (isSet) {
                itemLoc->SetCheckStatus(RCSHOW_SAVED);
                found++;
            } else {
                itemLoc->SetCheckStatus(RCSHOW_SCUMMED);
                notFound++;
            }
        }
    }
    if (found + notFound > 0) {
        SPDLOG_INFO("RSBS: OoT's check state follows the loaded half: {} marked found, {} marked not found (#849)",
                    found, notFound);
    }
    if (outFound != nullptr) {
        *outFound = found;
    }
    if (outNotFound != nullptr) {
        *outNotFound = notFound;
    }
    return found + notFound;
}

#endif // RSBS_SINGLE_EXECUTABLE
