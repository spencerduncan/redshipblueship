/**
 * @file rsbs.h
 * @brief RedShipBlueShip N64 ABI compatibility shims
 *
 * `rsbs/` holds libultra (N64 OS / graphics-utility) functions that are
 * identical between OoT and MM and are shared across both. It is *not*
 * the unified host-side game layer — that lives in `src/common/` (game
 * lifecycle, cross-game state, entrance switching, etc.).
 *
 * See docs/adr/0001-rsbs-vs-src-common.md for the directory-role decision.
 */

#ifndef RSBS_H
#define RSBS_H

// N64 ABI shims live in rsbs/src/libultra/. Host-side cross-game logic
// lives in src/common/; do not add new files of that kind here.

#endif // RSBS_H
