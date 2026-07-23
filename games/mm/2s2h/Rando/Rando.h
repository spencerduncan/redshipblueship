#ifndef RANDO_H
#define RANDO_H

#include "StaticData/StaticData.h"
#include "Types.h"
#include "variables.h"

#define IS_RANDO (gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO)
#define RANDO_SAVE_CHECKS gSaveContext.save.shipSaveInfo.rando.randoSaveChecks
#define RANDO_SAVE_OPTIONS gSaveContext.save.shipSaveInfo.rando.randoSaveOptions
#define RANDO_EVENTS gSaveContext.save.shipSaveInfo.rando.randoEvents

namespace Rando {

void Init();
void DrawItem(RandoItemId randoItemId, Actor* actor = nullptr);
#ifdef RSBS_SINGLE_EXECUTABLE
// Lane 6 (#494 slice 6): the particle aura that marks a check as holding a
// cross-game item, so a player can SEE that before collecting it. An aura
// rather than a replacement model because the host class already conveys item
// class through its own geometry (a chest's ornate corner/lock set) and should
// keep doing so. Defined in DrawItem.cpp; no-op without an actor.
void DrawForeignCheckAura(Actor* actor);
#endif
void GiveItem(RandoItemId randoItemId);
void RemoveItem(RandoItemId randoItemId);
RandoItemId CurrentJunkItem();
bool IsItemObtainable(RandoItemId randoItemId, RandoCheckId randoCheckId = RC_UNKNOWN);
RandoItemId ConvertItem(RandoItemId randoItemId, RandoCheckId randoCheckId = RC_UNKNOWN);
RandoCheckId FindItemPlacement(RandoItemId randoItemId);
void RegisterMenu();

void GrantStartingItems();
std::vector<RandoItemId> GetStartingItemsFromSpoiler(nlohmann::json& spoiler);
void SetStartingItemsInSpoiler(nlohmann::json& spoiler, std::vector<RandoItemId>& startingItems);
std::vector<RandoItemId> GetStartingItemsFromSave(RandoSaveInfo& randoSaveInfo);
void SetStartingItemsInSave(RandoSaveInfo& randoSaveInfo, std::vector<RandoItemId>& startingItems);
std::vector<RandoItemId> GetStartingItemsFromConfig();
void SetStartingItemsInConfig(std::vector<RandoItemId>& startingItems);

} // namespace Rando

#endif
