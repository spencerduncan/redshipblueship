/**
 * Rando::HintArea — where an MM hint says a check is (epic #575, child H2).
 *
 * Every MM hint that names a place (the gossip stones, and the Oath, remains,
 * Hookshot and gold dust hints) fills its location through ReplaceLocation, in
 * both builds:
 *
 *  - Single-exe: the check's area in the shared vocabulary (src/common's
 *    combo_area.h; MM's half is OoTMM's 46 regions, mapped by MM's table in
 *    HintAreaSingleExe.cpp), named with its article, and the area's OoTMM
 *    preposition for "{{prepos}}" ("is hidden in the Southern Swamp", "around
 *    Goron Village", "on Clock Tower Roof"), as OoTMM's item-region hint reads.
 *    A check with no area (an enemy drop, RC_UNKNOWN) is "nowhere", OoTMM's
 *    word for region 0, with no preposition.
 *  - Otherwise: upstream's exact expression, the check's scene name.
 *
 * Most call sites sit inside COND_ID_HOOK arguments, where a preprocessor
 * directive is ill-formed (MSVC C2121). So they call ReplaceLocation in both
 * builds, and a template names its preposition as RANDO_HINT_PREPOS("<upstream's
 * word>"): the single-exe build swaps in "{{prepos}}", every other build keeps
 * upstream's literal text.
 */
#ifndef RANDO_HINT_AREA_H
#define RANDO_HINT_AREA_H

#include <string>

#include "2s2h/Rando/StaticData/StaticData.h"

#ifdef RSBS_SINGLE_EXECUTABLE
#define RANDO_HINT_PREPOS(upstream) "{{prepos}}"
#else
#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/ShipUtils.h"
#include "2s2h/Rando/Rando.h"
#define RANDO_HINT_PREPOS(upstream) upstream
#endif

namespace Rando {
namespace HintArea {

#ifdef RSBS_SINGLE_EXECUTABLE
/** Replace `key` in `*msg` with the name of `randoCheckId`'s area, and
 *  "{{prepos}}" with that area's preposition; an area without one (nowhere)
 *  drops "{{prepos}} " instead. Defined in HintAreaSingleExe.cpp. */
void ReplaceLocation(std::string* msg, const char* key, RandoCheckId randoCheckId);

/** Whether `randoCheckId` has an area a hint can name: false for the 55 enemy
 *  drops and RC_UNKNOWN. A gossip stone hints only such checks. */
bool HasArea(RandoCheckId randoCheckId);

/** #575 H3c: replace `key` in `*msg` with where MM item `randoItemId` is, as
 *  ReplaceLocation does for the MM check Rando::FindItemPlacement finds. The
 *  static hints (Oath, remains, Hookshot, gold dust) fill their location
 *  through it. Defined in HintAreaSingleExe.cpp. */
void ReplaceItemLocation(std::string* msg, const char* key, RandoItemId randoItemId);
#else
inline void ReplaceLocation(std::string* msg, const char* key, RandoCheckId randoCheckId) {
    CustomMessage::Replace(msg, key, Ship_GetSceneName(Rando::StaticData::Checks[randoCheckId].sceneId));
}

inline void ReplaceItemLocation(std::string* msg, const char* key, RandoItemId randoItemId) {
    ReplaceLocation(msg, key, Rando::FindItemPlacement(randoItemId));
}
#endif

} // namespace HintArea
} // namespace Rando

#endif // RANDO_HINT_AREA_H
