/**
 * @file ForcedGatesSingleExe.cpp
 * @brief The IS_RANDO half of three upstream 2S2H gates the single exe drops
 *        (#1215, #1221).
 *
 * Upstream forces these on for every rando file from three TUs under
 * 2s2h/Enhancements, each registering `CVAR || IS_RANDO` through a file-scope
 * RegisterShipInitFunc and nothing else. In the single exe those TUs sit in the
 * plain `2ship_enh` archive, outside ship__Enh_registrars, so the linker drops
 * them (none is in redship.map) and a rando file runs the vanilla behaviour:
 *
 *   - Cheats/UnbreakableRazorSword.cpp: VB_LOWER_RAZOR_SWORD_DURABILITY. About
 *     100 landed hits break the seed's Razor Sword into the Kokiri Sword
 *     (z_player.c func_8083FFEC), and RI_PROGRESSIVE_SWORD then resolves from
 *     the Kokiri Sword and gives a second Razor (#1215).
 *   - Cycle/OceansideWalletAnyDay.cpp: VB_GIVE_ITEM_FROM_OFFER. On day 2 or 3
 *     the Oceanside Spider House man offers a rupee and never sets
 *     WEEKEVENTREG_RECEIVED_OCEANSIDE_WALLET_UPGRADE, RC_OCEAN_SPIDER_HOUSE_WALLET's
 *     flag, while COLLECTED_REWARD closes the reward for the cycle (#1221).
 *   - Cycle/OceansideSpiderHouseSquatter.cpp: OnFlagSet. Mikau's grave sets
 *     BUYER_MOVED_IN once 30 Oceanside tokens are held, which removes the man
 *     who gives the reward (#1221).
 *
 * WHY A PORT AND NOT A LINK. Each of those TUs also reads its own CVar live
 * (gCheats.UnbreakableRazorSword, gEnhancements.Cycle.OceansideWalletAnyDay,
 * gEnhancements.Cycle.StopOceansideSpiderHouseSquatter). No shared menu row owns
 * any of the three keys (src/common/cvar_shared_keys.h; BenMenu is excluded), so
 * linking them would arm three settings nothing in the single exe draws, the
 * #966 class. Only the IS_RANDO half is registered here, keyed on "IS_RANDO"
 * alone, and the bodies are upstream's. Outside rando nothing registers: the
 * vanilla Razor still breaks and the vanilla reward still depends on the day.
 *
 * The rando's OfferGetItem catch-all (OfferGetItem.cpp, ACTOR_EN_STH) still
 * blocks the offer itself. The wallet gate does not read the verdict: it sets
 * the check's flag, and Rando::MiscBehavior::OnFlagSet makes the check eligible
 * from that, in either registration order.
 *
 * None of the three bodies reads the play state. Each hook is dispatched in the
 * single exe: VB_* through MM_GameHooks_ExecuteVBShould (z_player.c, z_actor.c),
 * OnFlagSet through MM_GameHooks_ExecuteOnFlagSet (z_actor.c
 * Flags_SetWeekEventReg). Locked by MMRegistrarCoverage phase 12.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "Rando/Rando.h"

extern "C" {
#include "variables.h"
#include "overlays/actors/ovl_En_Sth/z_en_sth.h"
}

namespace {

void RegisterRandoForcedGates() {
    // Cheats/UnbreakableRazorSword.cpp's IS_RANDO half.
    COND_VB_SHOULD(VB_LOWER_RAZOR_SWORD_DURABILITY, IS_RANDO, { *should = false; });

    // Cycle/OceansideWalletAnyDay.cpp's IS_RANDO half: the reward is the next
    // wallet tier on any day, and the check's flag is set when it is offered.
    COND_VB_SHOULD(VB_GIVE_ITEM_FROM_OFFER, IS_RANDO, {
        GetItemId* item = va_arg(args, GetItemId*);
        Actor* actor = va_arg(args, Actor*);

        if (actor == nullptr || actor->id != ACTOR_EN_STH ||
            STH_GET_TYPE(actor) != STH_TYPE_OCEANSIDE_SPIDER_HOUSE_GREET) {
            return;
        }

        if (!CHECK_WEEKEVENTREG(WEEKEVENTREG_RECEIVED_OCEANSIDE_WALLET_UPGRADE)) {
            switch (CUR_UPG_VALUE(UPG_WALLET)) {
                case 0:
                    *item = GI_WALLET_ADULT;
                    break;
                case 1:
                    *item = GI_WALLET_GIANT;
                    break;
                default:
                    *item = GI_RUPEE_SILVER;
                    break;
            }
            SET_WEEKEVENTREG(WEEKEVENTREG_RECEIVED_OCEANSIDE_WALLET_UPGRADE);
            STH_GI_ID(actor) = *item;
        }
    });

    // Cycle/OceansideSpiderHouseSquatter.cpp's IS_RANDO half: the squatter
    // moves in only once the reward has been collected.
    COND_HOOK(OnFlagSet, IS_RANDO, [](FlagType flagType, u32 flag) {
        if (flagType != FLAG_WEEK_EVENT_REG) {
            return;
        }
        if (flag == WEEKEVENTREG_OCEANSIDE_SPIDER_HOUSE_BUYER_MOVED_IN) {
            // Quietly unset it, unless the reward was received.
            if (!CHECK_WEEKEVENTREG(WEEKEVENTREG_OCEANSIDE_SPIDER_HOUSE_COLLECTED_REWARD)) {
                CLEAR_WEEKEVENTREG(WEEKEVENTREG_OCEANSIDE_SPIDER_HOUSE_BUYER_MOVED_IN);
            }
        } else if (flag == WEEKEVENTREG_OCEANSIDE_SPIDER_HOUSE_COLLECTED_REWARD) {
            // The reward is collected, so the squatter may move in now.
            SET_WEEKEVENTREG(WEEKEVENTREG_OCEANSIDE_SPIDER_HOUSE_BUYER_MOVED_IN);
        }
    });
}

} // namespace

static RegisterShipInitFunc initFunc(RegisterRandoForcedGates, { "IS_RANDO" });

#endif // RSBS_SINGLE_EXECUTABLE
