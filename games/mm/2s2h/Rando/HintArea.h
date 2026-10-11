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
 *
 * Child H3c: an MM item can sit on an OoT check under the single bag (the
 * crossing store's OoT-hosted rows), where no MM check holds it. The static
 * hints then name the OoT host's area (ReplaceItemLocation), and the gossip
 * stones draw such items too (OoTHostedHintCandidates). An OoT area is named
 * with the preposition MM's sentences give it, OoTMM's for the OoT region
 * (packages/generator/src/common/text/text.c, kRegionNamesOot).
 */
#ifndef RANDO_HINT_AREA_H
#define RANDO_HINT_AREA_H

#include <cstdint>
#include <string>
#include <vector>

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

/** #575 H3c: replace `key` in `*msg` with where MM item `randoItemId` is: the
 *  MM check Rando::FindItemPlacement finds, as ReplaceLocation names it, or,
 *  when no MM check holds it, the area of the OoT check that hosts it (the
 *  first OoT-hosted row of the crossing store carrying it, insertion order).
 *  The static hints (Oath, remains, Hookshot, gold dust) fill their location
 *  through it. Defined in HintAreaSingleExe.cpp. */
void ReplaceItemLocation(std::string* msg, const char* key, RandoItemId randoItemId);

/** #575 H3c: the OoT checks a gossip stone may hint as holding an MM item: the
 *  crossing store's OoT-hosted rows, in insertion order, whose OoT check has an
 *  area. With `excludeObtained` (the purchasable hint), a host OoT's own check
 *  status reports collected is left out. */
std::vector<uint16_t> OoTHostedHintCandidates(bool excludeObtained);

/** #575 H3c: the MM item the crossing store puts on OoT check `ootCheck`, or
 *  RI_UNKNOWN when it hosts none. */
RandoItemId ItemOnOoTHost(uint16_t ootCheck);

/** #575 H3c: ReplaceLocation for an OoT check: its area, with the preposition
 *  MM's sentences give an OoT area. */
void ReplaceOoTHostLocation(std::string* msg, const char* key, uint16_t ootCheck);
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
