/**
 * @file Tricks.h
 * @brief The MM trick ROW TABLE, its accessors, and the `MM_TRICK()` predicate
 *        region conditions consult (#578 part 1; ADR 0010 O9, D6, §3.2-3.3).
 *
 * The KEY SPACE and the whole design rationale — attribution (OoTMM, MIT; and
 * mm-rando as an explicit NON-source), why the row shape is SoH's two-axis one
 * rather than OoTMM's flat one, what `reserved` means, why the enum is
 * append-only, and where the frozen bits live — are in `TrickIds.h`, which this
 * header includes. Read that first.
 *
 * THE SPLIT IS FORCED, NOT STYLISTIC. `games/mm/include/z64save.h` needs
 * `MMRT_MAX` to size `RandoSaveInfo::randoSaveTricks`, and z64save.h is pulled
 * in by C translation units and — in C++ ones — from INSIDE `extern "C" { ... }`
 * blocks (`#include "variables.h"` is written that way across the tree). A
 * header reachable from there must therefore contain no `<map>`, no `<cstdint>`
 * and no namespace: C linkage around a libstdc++ header is the exact hazard
 * src/common/context.h's own header comment warns about. So the plain enums live
 * in TrickIds.h (no includes at all, C-safe), and everything C++ lives here.
 */
#ifndef RANDO_STATIC_DATA_TRICKS_H
#define RANDO_STATIC_DATA_TRICKS_H

#include "TrickIds.h"

#ifdef __cplusplus

#include <cstdint>
#include <map>

namespace Rando {

namespace StaticData {

/** One trick, in SoH's two-axis form (TrickIds.h explains the two axes). */
struct RandoStaticTrick {
    MMRandoTrickId mmRandoTrickId;
    /** Stringified enumerator, e.g. "MMRT_KEG_EXPLOSIVES" — the spoiler-stable
     *  name and the key the pane's evidence prints. */
    const char* name;
    /** Authoring CVar, "gRando.Tricks.MMRT_<KEY>". Read ONLY by the profile
     *  resolution and the pane; never by a region condition (TrickIds.h). */
    const char* cvar;
    MMRandoTrickArea area;
    /** Bitwise OR of `MMRandoTrickTag`; never 0. */
    uint32_t tags;
    /** True when the trick needs an OoT-side item MM cannot hold yet. A
     *  reserved trick is inert: `IsTrickEnabled` returns false for it whatever
     *  the save and the CVars say. */
    bool reserved;
    const char* displayName;
    const char* tooltip;
    /** Why the row is reserved; NULL exactly when `reserved` is false. */
    const char* reservedReason;
};

/** Every declared key, keyed by id. `Tricks.size() == MMRT_MAX` is a lock. */
extern std::map<MMRandoTrickId, RandoStaticTrick> Tricks;

/** `MMRT_MAX` when no row carries that name. */
MMRandoTrickId GetTrickIdFromName(const char* name);

/** Display name for an area; never NULL (out-of-range yields a placeholder,
 *  because every caller is a printf-family or ImGui text call). */
const char* GetTrickAreaName(MMRandoTrickArea area);

/** Display name for a single tag BIT; never NULL. */
const char* GetTrickTagName(MMRandoTrickTag tag);

/**
 * THE predicate: is this trick enabled for the file currently loaded?
 *
 * Reads the frozen per-file set (`randoSaveTricks`), bounds-checked, and returns
 * false for any reserved key regardless of what is stored. This is what
 * `MM_TRICK()` expands to, and the only trick accessor a region condition may
 * use.
 */
bool IsTrickEnabled(MMRandoTrickId mmRandoTrickId);

/**
 * Resolve a trick's authored value from its CVar — the AUTHORING read, for the
 * profile resolution and the options pane only. A reserved key always resolves
 * false, so a CVar written before a key became reserved cannot arm it.
 */
bool ResolveTrickFromCVar(MMRandoTrickId mmRandoTrickId);

} // namespace StaticData

} // namespace Rando

/**
 * The form region conditions use: `MM_TRICK(MMRT_KEG_EXPLOSIVES) && ...`.
 *
 * A macro rather than a bare call so trick consultation reads like `HAS_ITEM()`
 * and `CAN_BE_GORON` in the same expressions, and so the single accessor can be
 * swapped without touching the ~60 bindings part 2 will author.
 */
#define MM_TRICK(trickId) (Rando::StaticData::IsTrickEnabled(trickId))

#endif // __cplusplus

#endif // RANDO_STATIC_DATA_TRICKS_H
