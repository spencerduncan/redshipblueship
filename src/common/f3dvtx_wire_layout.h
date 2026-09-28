/**
 * @file f3dvtx_wire_layout.h
 * @brief Compile-time pin of the 16-byte vertex record both games' Array readers fill (#604).
 *
 * Every extracted `*Vtx_*` resource is an 'OARR' Array whose Vertex path reads
 * ten fields, in this order and at these widths, into a Fast::F3DVtx:
 *
 *     ob[0..2]  3 x s16   bytes  0..5
 *     flag      u16       bytes  6..7
 *     tc[0..1]  2 x s16   bytes  8..11
 *     cn[0..3]  4 x u8    bytes 12..15
 *
 * The renderer consumes the parsed vector as raw N64 `Vtx` memory, so these
 * offsets are the wire record as well as the struct. This header is included by
 * the translation unit that exports each game's Array reader
 * (games/oot/soh/OTRGlobals.cpp, games/mm/2s2h/GameExports_SingleExe.cpp) and by
 * src/common/array_reader_agreement.cpp, so each target checks the layout under
 * its OWN compile definitions: a `GBI_FLOATS` (float `ob`) or a reordered struct
 * in either game fails that game's build here.
 *
 * What this cannot see: the ORDER of the `reader->Read*()` calls inside each
 * reader. Two readers can agree on this struct and still fill it differently.
 * That is the load-bearing half of the #604 guard, and it is an output
 * comparison rather than a layout check: see array_reader_agreement.h.
 */
#pragma once

#include <cstddef>
#include <fast/lus_gbi.h>

static_assert(sizeof(Fast::F3DVtx) == 16, "#604: Fast::F3DVtx is no longer the 16-byte N64 vertex record the "
                                          "'OARR' Vertex readers fill and the renderer consumes as raw Vtx");
static_assert(sizeof(Fast::F3DVtx_t) == 16, "#604: Fast::F3DVtx_t is no longer 16 bytes (GBI_FLOATS defined?)");
static_assert(offsetof(Fast::F3DVtx_t, ob) == 0 && sizeof(Fast::F3DVtx_t::ob) == 6,
              "#604: F3DVtx_t::ob must be three s16 at offset 0");
static_assert(offsetof(Fast::F3DVtx_t, flag) == 6 && sizeof(Fast::F3DVtx_t::flag) == 2,
              "#604: F3DVtx_t::flag must be one u16 at offset 6");
static_assert(offsetof(Fast::F3DVtx_t, tc) == 8 && sizeof(Fast::F3DVtx_t::tc) == 4,
              "#604: F3DVtx_t::tc must be two s16 at offset 8");
static_assert(offsetof(Fast::F3DVtx_t, cn) == 12 && sizeof(Fast::F3DVtx_t::cn) == 4,
              "#604: F3DVtx_t::cn must be four u8 at offset 12");
