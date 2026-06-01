/**
 * @file save.h
 * @brief Unified cross-game save file (.redsave) for the single executable
 *        (Phase 2 T6, issue #35).
 *
 * A `.redsave` file holds, in one slot, three tiers:
 *   Tier 0  RsbsSaveHeader   (fixed 32 bytes: magic, version, slot, sizes, crc)
 *   Tier 1  ComboContext     (cross-game flags / shared items / last game)
 *   Tier 2  OoT SaveContext  (OOT_SAVE_CONTEXT_SIZE bytes)
 *   Tier 3  MM  SaveContext  (MM_SAVE_CONTEXT_SIZE bytes)
 *
 * This is the HEADLESS CORE: the format, the SaveManager, and round-trip /
 * validation unit tests. It serializes the cross-game shadow copies that the
 * context layer already maintains (Context_GetOoTSaveContext /
 * Context_GetMMSaveContext / gComboCtx) and restores them via
 * Context_UpdateShadowCopy, so it depends only on src/common and never on
 * either game's z64save.h. The OoT/MM save-flow hooks (OnSaveFile/OnLoadFile)
 * and the unified file-select panel are a deliberate follow-up; see issue #35.
 *
 * Format notes:
 * - Little-endian, host-native. Both shipped targets are LE (x86-64 / arm64);
 *   the header records endian=1 and Load asserts it so a future big-endian port
 *   fails loudly instead of silently corrupting.
 * - A `.redsave` is tied to the SaveContext struct sizes of the build that
 *   wrote it: the header stores each tier's byte length and Load rejects a file
 *   whose tier sizes or version do not match the current build (fail-safe, no
 *   partial load) rather than memcpy-ing a mismatched blob.
 */

#ifndef RSBS_COMMON_SAVE_H
#define RSBS_COMMON_SAVE_H

#include "game.h"     // GameId, OOT_SAVE_CONTEXT_SIZE, MM_SAVE_CONTEXT_SIZE
#include "context.h"  // ComboContext, gComboCtx, Context_* shadow API

#include <stdint.h>

// On-disk constants (visible to both C and C++).
#define RSBS_SAVE_MAGIC      "REDSHIP1"  // 8 bytes, NOT NUL-terminated on disk
#define RSBS_SAVE_VERSION    1u          // bump on any layout change
#define RSBS_SAVE_ENDIAN_LE  1
#define RSBS_SAVE_MAX_SLOTS  3           // matches each game's MaxFiles

#ifdef __cplusplus

#include <cstddef>
#include <iosfwd>
#include <string>

namespace rsbs {

#pragma pack(push, 1)
/**
 * Tier-0 header. Fixed 32 bytes, packed so its layout is stable across
 * compilers. All multi-byte integers little-endian.
 */
struct RsbsSaveHeader {
    char     magic[8];    // "REDSHIP1"
    uint32_t version;     // RSBS_SAVE_VERSION
    uint8_t  endian;      // RSBS_SAVE_ENDIAN_LE (1)
    uint8_t  slot;        // 0..RSBS_SAVE_MAX_SLOTS-1
    uint16_t headerSize;  // sizeof(RsbsSaveHeader) == 32 (forward-compat probe)
    uint32_t comboSize;   // Tier-1 bytes == sizeof(ComboContext)
    uint32_t ootSize;     // Tier-2 bytes == OOT_SAVE_CONTEXT_SIZE at write time
    uint32_t mmSize;      // Tier-3 bytes == MM_SAVE_CONTEXT_SIZE at write time
    uint32_t crc32;       // CRC32 over Tiers 1..3 (the payload after the header)
};
#pragma pack(pop)

static_assert(sizeof(RsbsSaveHeader) == 32, "RsbsSaveHeader must be exactly 32 bytes");

/**
 * Reads/writes the three-tier unified save file. Process-wide singleton.
 *
 * Serialization source/sink is the context layer's shadow copies, so a Save
 * captures whatever cross-game state is currently resident and a Load primes
 * both shadows + gComboCtx for the next cross-game switch to pick up.
 */
class SaveManager {
public:
    static SaveManager& Instance();

    bool Save(int slot);
    bool Load(int slot);
    bool HasSave(int slot) const;
    void DeleteSave(int slot);

    /**
     * Directory the .redsave files live in. Defaults to "Save" relative to the
     * working directory so the headless --test path (which never creates a
     * Ship::Context) can round-trip to a writable location. The game-integration
     * follow-up injects the real per-app save directory here.
     */
    void SetSaveDirectory(const std::string& dir);
    std::string SlotPath(int slot) const;

    static uint32_t Crc32(const uint8_t* data, size_t len);

private:
    SaveManager() = default;

    // Validates magic / version / endian / headerSize / tier sizes against the
    // current build. Returns true and fills outHeader only on a fully valid
    // header; never mutates live state.
    bool DeserializeHeader(std::istream& in, RsbsSaveHeader& outHeader) const;

    std::string mSaveDir = "Save";
};

}  // namespace rsbs

#endif  // __cplusplus

// ---- C shim for game-side hook code (used by the deferred integration PR) ----
#ifdef __cplusplus
extern "C" {
#endif

int  RsbsSave_Save(int slot);
int  RsbsSave_Load(int slot);
int  RsbsSave_HasSave(int slot);
void RsbsSave_DeleteSave(int slot);

#ifdef __cplusplus
}
#endif

#endif  // RSBS_COMMON_SAVE_H
