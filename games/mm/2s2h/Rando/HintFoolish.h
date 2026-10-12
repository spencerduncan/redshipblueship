/**
 * Rando::HintFoolish — MM's foolish gossip-stone hint (epic #575, child H5a).
 *
 * Single-exe only. MM's half of the shared foolish rule (src/common's
 * combo_area.h, THE FOOLISH RULE) and the free stone's foolish variant, in MM's
 * voice. Defined in HintFoolishSingleExe.cpp.
 */
#ifndef RANDO_HINT_FOOLISH_H
#define RANDO_HINT_FOOLISH_H

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdint>
#include <string>

namespace Rando {
namespace HintFoolish {

/** A free stone speaks its foolish variant one time in this many (when the world
 *  has a foolish MM area at all); every other time it keeps its item hint. */
constexpr int kFreeStoneOneIn = 5;

/**
 * What the free gossip stone seeded `stoneSeed` says INSTEAD of its item hint:
 * "They say plundering %y<area>%w is a %pfoolish choice%w." for one foolish MM
 * area, or "" when this stone keeps its item hint (or no MM area is foolish).
 *
 * Picked when the stone is read, from a generator of its own seeded like the
 * item pick (the same stone seed, under its own domain tag), over the foolish set
 * recomputed from the frozen world: a pure function of the world and the stone,
 * chosen nowhere and persisted nowhere. It never touches MM's global Ship_Random
 * stream, so the item pick and the stone's flavour line after it never move.
 */
std::string FreeStoneText(uint32_t stoneSeed);

} // namespace HintFoolish
} // namespace Rando

#endif // RSBS_SINGLE_EXECUTABLE

#endif // RANDO_HINT_FOOLISH_H
