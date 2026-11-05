/**
 * Rando::HintWoth — MM's Way of the Hero gossip-stone hint (#1439).
 *
 * Single-exe only. The free stone's Way of the Hero variant, in MM's voice, over
 * the pair's frozen Way of the Hero (src/common's combo_woth.h: the areas of both
 * games holding a host the frozen goal cannot do without, persisted in the
 * .redsave as gComboCtx.comboWoth). Defined in HintWothSingleExe.cpp.
 */
#ifndef RANDO_HINT_WOTH_H
#define RANDO_HINT_WOTH_H

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdint>
#include <string>

#include "combo_area.h" // src/common: ComboArea, the area the variant names (#1475)

namespace Rando {
namespace HintWoth {

/** A free stone that kept its item hint (no foolish variant) speaks its Way of
 *  the Hero variant one time in this many, when the world has an area on the
 *  path at all: about 27% of free stones, beside the foolish variant's 20%, as
 *  OoTMM's default distribution gives WotH more stones than foolish (9 hints to
 *  5, hints.ts SETTINGS_DEFAULT_HINTS). */
constexpr int kFreeStoneOneIn = 3;

/**
 * What the free gossip stone seeded `stoneSeed` says INSTEAD of its item hint:
 * "They say %y<area>%w is on the %yWay of the Hero%w." for one area of either
 * game on the pair's Way of the Hero (an OoT area named as OoT names it, an MM
 * area with its article), or "" when this stone keeps its item hint (or the world
 * froze no Way of the Hero: one created before #1439, or under No Logic).
 *
 * Picked when the stone is read, from a generator of its own seeded like the item
 * pick (the same stone seed, under its own domain tag), over the frozen record: a
 * pure function of the world and the stone. It never touches MM's global
 * Ship_Random stream, so the item pick and the stone's flavour line never move.
 *
 * #1475: `*outArea` (may be NULL) receives the area named, for the combined Hint
 * Tracker's resolver; untouched when "" is returned.
 */
std::string FreeStoneText(uint32_t stoneSeed, ComboArea* outArea = nullptr);

} // namespace HintWoth
} // namespace Rando

#endif // RSBS_SINGLE_EXECUTABLE

#endif // RANDO_HINT_WOTH_H
