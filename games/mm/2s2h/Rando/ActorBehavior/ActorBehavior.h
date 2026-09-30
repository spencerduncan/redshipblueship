#ifndef RANDO_ACTOR_BEHAVIOR_H
#define RANDO_ACTOR_BEHAVIOR_H

#include "Rando/Rando.h"
#include <string>

namespace Rando {

namespace ActorBehavior {

void Init();
void OnFileLoad();

RandoCheckId GetObjectRandoCheckId(void* actor);
void SetObjectRandoCheckId(const void* object, RandoCheckId rc);

// #800: a shop slot may host a foreign (OoT) item while physically holding MM's
// junk cover. Both defined in EnGirlA.cpp; outside a paired world they are the
// native answers unchanged.
// The name a shop line gives the item slot `rc` sells: the hosted foreign item's
// (with its article when `withArticle`), else `nativeName`.
std::string ShopOfferedItemName(RandoCheckId rc, const std::string& nativeName, bool withArticle);
// A milk-counter slot (EnIn/EnTab) is sold for this cycle, or, hosting a foreign
// item whose crossing was delivered, for the rest of the game.
bool ShopCounterSlotSold(RandoCheckId rc);

void InitDmChar01Behavior();
void InitDmChar05Behavior();
void InitDmChar08Behavior();
void InitDmHinaBehavior();
void InitDmStkBehavior();
void InitDoorWarp1VBehavior();
void InitEnAkindonutsBehavior();
void InitEnAlBehavior();
void InitEnAnBehavior();
void InitEnAob01Behavior();
void InitEnAzBehavior();
void InitEnBabaBehavior();
void InitEnBalBehavior();
void InitEnBjtBehavior();
void InitEnBomBowlManBehavior();
void InitEnBoxBehavior();
void InitEnCowBehavior();
void InitEnDaiBehavior();
void InitEnDnhBehavior();
void InitEnElfgrpBehavior();
void InitEnElforgBehavior();
void InitEnFish2Behavior();
void InitEnemyDropBehavior();
void InitEnFsnBehavior();
void InitEnFuBehavior();
void InitEnGamelupyBehavior();
void InitEnGb2Behavior();
void InitEnGegBehavior();
void InitEnGgBehavior();
void InitEnGinkoBehavior();
void InitEnGirlABehavior();
void InitEnGKBehavior();
void InitEnGoBehavior();
void InitEnGsBehavior();
void InitEnHgBehavior();
void InitEnInBehavior();
void InitEnInvadepohBehavior();
void InitEnItem00Behavior();
void InitEnJgameTsnBehavior();
void InitEnJgBehavior();
void InitEnJsBehavior();
void InitEnKgyBehavior();
void InitEnKitanBehavior();
void InitEnKnightBehavior();
void InitEnKujiyaBehavior();
void InitEnLiftNutsBehavior();
void InitEnMa4Behavior();
void InitEnMaYtoBehavior();
void InitEnMinifrogBehavior();
void InitEnMnkBehavior();
void InitEnNbBehavior();
void InitEnOsnBehavior();
void InitEnOtBehavior();
void InitEnOwlBehavior();
void InitEnPmBehavior();
void InitEnRuppecrowBehavior();
void InitEnRzBehavior();
void InitEnScopenutsBehavior();
void InitEnSellnutsBehavior();
void InitEnShnBehavior();
void InitEnSiBehavior();
void InitEnSob1Behavior();
void InitEnSshBehavior();
void InitEnStoneheishiBehavior();
void InitEnSyatekiManBehavior();
void InitEnTabBehavior();
void InitEnTakarayaBehavior();
void InitEnTalkBehavior();
void InitEnTotoBehavior();
void InitEnTrtBehavior();
void InitEnYbBehavior();
void InitEnZogBehavior();
void InitEnZotBehavior();
void InitEnZowBehavior();
void InitItemBHeartBehavior();
void InitItemGetBehavior();
void InitObjKibakoBehavior();
void InitObjGrassBehavior();
void InitObjMoonStoneBehavior();
void InitObjSnowballBehavior();
void InitObjTaruBehavior();
void InitObjTsuboBehavior();
void InitObjWarpstoneBehavior();
void InitPlayerBehavior();
void InitSoulsBehavior();
void InitTrapsBehavior();

} // namespace ActorBehavior

} // namespace Rando

#endif
