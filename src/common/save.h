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

// ---- C-visible per-game metadata-offset descriptor ------------------------
// Registered by each game's TU (which alone knows its SaveContext layout) so
// that save.cpp can read player-name / play-time / "valid" marker bytes from a
// slot file WITHOUT ever including either game's z64save.h. Offsets are
// relative to the start of that game's SaveContext blob as stored in the slot
// (= the same bytes Context_GetOoTSaveContext / Context_GetMMSaveContext hand
// out and that the .redsave file holds in Tier-2 / Tier-3).
typedef struct RsbsGameMetaDesc {
    uint32_t playerNameOffset;
    uint32_t playerNameLen;     // <= 8; ReadMeta clamps to 8 either way
    uint32_t playTimeOffset;    // u32 read at this offset; 0 if not applicable
    uint32_t validMarkerOffset;
    uint32_t validMarkerLen;    // 0 → treat as always-valid (skip the check)
    uint8_t  validMarker[8];    // expected bytes (e.g. "ZELDAZ", "ZELDA3")
} RsbsGameMetaDesc;

#ifdef __cplusplus

#include <cstddef>
#include <iosfwd>
#include <string>

namespace rsbs {

/**
 * Per-slot summary, cheap to compute (one header read + a handful of byte
 * pulls). Built so the unified file-select panel can render a slot without
 * loading + committing the whole 24KB payload. `valid` is true iff the header
 * passes every check Load() does, EXCLUDING CRC — slot listing must stay fast,
 * and a bad CRC will still be caught when the user actually clicks Load.
 */
struct SlotMeta {
    bool     exists;       // slot file is present and readable
    bool     valid;        // header passes all build-compat checks (no CRC)
    uint8_t  slot;         // mirrors header.slot
    GameId   lastGame;     // ComboContext.sourceGame (last game played in slot)
    char     ootName[9];   // NUL-terminated player name from the OoT blob
    char     mmName[9];    // NUL-terminated player name from the MM blob
    uint32_t ootPlayTime;  // raw u32 at the registered OoT play-time offset
    uint32_t mmPlayTime;   // raw u32 at the registered MM play-time offset
    bool     ootStarted;   // OoT validMarker bytes match (file has been started)
    bool     mmStarted;    // MM  validMarker bytes match
};

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
     * Read a cheap summary of `slot` (header + name/playtime bytes from each
     * game's blob). Does NOT validate CRC or mutate live state. If the slot
     * file is missing or the header fails any compat check, returns a
     * SlotMeta whose `exists`/`valid` reflect that; per-game fields for
     * unregistered descriptors are zeroed and `started` is left false so the
     * file-select panel renders "not started" instead of garbage names.
     */
    SlotMeta ReadMeta(int slot) const;

    /**
     * Game-side TUs register the byte offsets save.cpp needs to read each
     * game's metadata. Stored per-GameId; later calls overwrite. Passing
     * GAME_NONE / nullptr / an out-of-range game is a no-op.
     */
    void RegisterGameMeta(GameId game, const RsbsGameMetaDesc* desc);

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

    // Game-side metadata descriptors, indexed by GameId (GAME_OOT / GAME_MM).
    // mMetaPresent[i] guards mMetaDescs[i]; an unset descriptor causes
    // ReadMeta to fill that game's fields with zeros / `started=false`.
    RsbsGameMetaDesc mMetaDescs[3]{};   // GAME_NONE / GAME_OOT / GAME_MM
    bool             mMetaPresent[3]{};
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

/**
 * Register a game's metadata-offset descriptor with the SaveManager. Called
 * once per game during that game's init, from a TU that includes its own
 * z64save.h, so save.cpp never has to. Passing GAME_NONE or NULL is a no-op.
 */
void RsbsSave_RegisterGameMeta(GameId game, const RsbsGameMetaDesc* desc);

#ifdef __cplusplus
}
#endif

#endif  // RSBS_COMMON_SAVE_H
