/**
 * @file array_reader_agreement.h
 * @brief Do the two games' 'OARR' Array readers turn the same bytes into the same vertices? (#604)
 *
 * Both ports ship an Array reader (SOH:: and S2H::ResourceFactoryBinaryArrayV0).
 * One process holds both, and which one parses a given vertex array depends on
 * who registered the loader slot and which archive served the file (the Array
 * slot is dispatched per archive once MM has initialized, like Room, Cutscene
 * and Path; before that, and for any archive not recorded as MM's — the curated
 * cross-game archive included — OoT's reader parses it). Garbled geometry, not
 * an error, is what a divergence on the Vertex path would produce, so it has to
 * be caught before anything draws.
 *
 * The check is an OUTPUT comparison: parse the same payload through both
 * readers and compare the vertex memory each hands the renderer
 * (IResource::GetRawPointer / GetPointerSize). A layout static_assert alone
 * would pass unchanged if one reader's ten Read*() calls were reordered, which
 * is exactly the mis-parse being guarded (f3dvtx_wire_layout.h pins the layout
 * half).
 *
 * Game-header-free (ADR 0002): each game hands over its reader through a
 * creator defined in its own TU (OoT_CreateArrayFactory in
 * games/oot/soh/OTRGlobals.cpp, MM_CreateArrayFactory in
 * games/mm/2s2h/GameExports_SingleExe.cpp), the same pattern as
 * OoT_CreatePathFactory.
 */
#pragma once

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <ship/utils/binarytools/endianness.h>

namespace Ship {
class ResourceFactory;
}

namespace rsbs::array_readers {

// ArrayResourceType::Vertex in both ports' resource/type/Array.h (the enum is
// the exporter's resource-type list; Vertex is its 26th entry). Only the
// synthetic payload uses it: a wrong value sends both readers down the scalar
// path, which yields zero vertices and fails the non-vacuity check loudly.
constexpr uint32_t kArrayTypeVertex = 25;

// A synthetic 'OARR' Vertex payload (the bytes after the 64-byte OTR header),
// little-endian, whose vertices give every field a distinct, non-zero,
// sign-exercising value and make tc[0] != tc[1] and every cn[] byte differ, so
// any swap, width change or skipped read in one reader changes the output.
std::vector<char> SyntheticVertexPayload();
// How many vertices SyntheticVertexPayload() carries.
size_t SyntheticVertexCount();

struct ParsedArray {
    bool ok = false;          // the reader returned a resource and did not throw
    std::string error;        // why not, when !ok
    std::vector<uint8_t> raw; // GetRawPointer() .. + GetPointerSize()
};

// Parse `payload` (no OTR header) through `factory` exactly as ResourceLoader
// would hand it over: a fresh BinaryReader over a copy of the bytes with the
// given byte order, and BINARY-format init data carrying `path`.
ParsedArray ParseArrayPayload(const std::shared_ptr<Ship::ResourceFactory>& factory, const std::vector<char>& payload,
                              Ship::Endianness byteOrder, const std::string& path);

// True when both readers parse `payload` into the same, non-empty vertex
// memory. Otherwise false, with `diag` naming the reader that failed or the
// first differing vertex and field.
bool ReadersAgree(const std::shared_ptr<Ship::ResourceFactory>& a, const char* aName,
                  const std::shared_ptr<Ship::ResourceFactory>& b, const char* bName, const std::vector<char>& payload,
                  Ship::Endianness byteOrder, const std::string& path, std::string* diag);

} // namespace rsbs::array_readers

extern "C" {
#endif

// The boot guard: OoT's and MM's real readers over the synthetic payload.
// Returns 0 when they agree; otherwise prints a FATAL diagnostic to stderr and
// returns 1 (rsbs/src/main.cpp refuses to boot). Needs no Ship::Context.
int Combo_ArrayReaders_BootCheck(void);

#ifdef __cplusplus
}
#endif
