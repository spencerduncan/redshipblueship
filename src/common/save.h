/**
 * @file save.h
 * @brief Unified cross-game save file (.redsave) for the single executable
 *        (Phase 2 T6, issue #35).
 *
 * A `.redsave` file holds, in one slot, three tiers:
 *   Tier 0  RsbsSaveHeader   (fixed 32 bytes: magic, version, slot, sizes, crc)
 *   Tier 1  ComboContext     (cross-game flags / shared items / last game)
 *   Tier 2  OoT SaveContext  (header.ootSize bytes; OOT_SAVE_CONTEXT_SIZE when written by this build)
 *   Tier 3  MM  SaveContext  (header.mmSize bytes; MM_SAVE_CONTEXT_SIZE when written by this build)
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
 * - The header stores each tier's byte length, so the file is self-describing:
 *   Load reads the STORED sizes, not this build's constants. This applies to
 *   ALL THREE tiers, Tier-1 included. A tier that is shorter than this build's
 *   size is a file from an older build — its bytes are a prefix of the same
 *   layout, so it is accepted and zero-extended. A tier LARGER than this
 *   build's capacity, or a version outside [RSBS_SAVE_VERSION_MIN,
 *   RSBS_SAVE_VERSION], is rejected (fail-safe, no partial load) AND logged.
 * - Tier-1 is written at a FIXED RSBS_COMBO_CONTEXT_RECORD_SIZE (struct plus
 *   zero padding), so appending fields to ComboContext — which Lane A's
 *   origin-tagged sharedItems requires — does not change the serialized size
 *   at all and does not orphan existing saves. See context.h for the growth
 *   contract that keeps the prefix property true.
 */

#ifndef RSBS_COMMON_SAVE_H
#define RSBS_COMMON_SAVE_H

#include "game.h"     // GameId, OOT_SAVE_CONTEXT_SIZE, MM_SAVE_CONTEXT_SIZE
#include "context.h"  // ComboContext, gComboCtx, Context_* shadow API

#include <stdint.h>

// On-disk constants (visible to both C and C++).
#define RSBS_SAVE_MAGIC      "REDSHIP1"  // 8 bytes, NOT NUL-terminated on disk
#define RSBS_SAVE_VERSION    2u          // version THIS build writes
// Oldest version this build can still read. v1 files predate the fixed-size
// Tier-1 record: their comboSize is the raw sizeof(ComboContext) of the build
// that wrote them, which is a prefix of the current layout and therefore
// zero-extends cleanly. Widen this window rather than bumping VERSION alone —
// bumping alone is what silently orphans every existing .redsave.
#define RSBS_SAVE_VERSION_MIN 1u
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
#include <mutex>
#include <string>
#include <vector>

namespace rsbs {

/**
 * Per-slot summary, cheap to compute (one header read + a handful of byte
 * pulls). Built so the unified file-select panel can render a slot without
 * loading + committing the whole ~200KB payload. `valid` is true iff the header
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
    uint32_t comboSize;   // Tier-1 bytes as stored (RSBS_COMBO_CONTEXT_RECORD_SIZE at write time)
    uint32_t ootSize;     // Tier-2 bytes as stored (== OOT_SAVE_CONTEXT_SIZE at write time)
    uint32_t mmSize;      // Tier-3 bytes as stored (== MM_SAVE_CONTEXT_SIZE at write time)
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

    /**
     * THE .redsave COMMIT CHOKE POINT (#537/#531, #564 "one commit
     * discipline"). Every durable .redsave write is a two-phase commit:
     *
     *   1. StageCommit() — GAME THREAD ONLY. Marshals ONE coherent snapshot of
     *      the whole cross-game state (Tier-1 gComboCtx + the OoT shadow + the
     *      MM shadow) into an internal staging buffer, and stamps the next
     *      MONOTONIC commit generation into gComboCtx.commitGeneration first
     *      so the snapshot carries it. The caller must have refreshed the
     *      ACTIVE game's shadow (Context_UpdateShadowCopy from the live
     *      gSaveContext) and set gComboCtx.sourceGame on the same thread,
     *      immediately before staging — that is what makes the snapshot
     *      single-instant. Returns the stamped generation, or 0 on refusal
     *      (shadows absent). Never touches the filesystem.
     *
     *   2. WriteStagedCommit(slot) — ANY THREAD. Serializes the most recently
     *      staged snapshot to the slot file. Reads ONLY the immutable staged
     *      copy — never gComboCtx, never the live shadows — so a worker
     *      thread can run it while the game thread keeps mutating live state
     *      (the #537 tear becomes unrepresentable). Returns false and logs if
     *      nothing has been staged this session.
     *
     * Save(slot) below is the one-call form (stage + write on the calling
     * thread) and therefore inherits StageCommit's game-thread contract.
     * There is deliberately NO other writer: SaveManager serializes staged
     * snapshots only.
     */
    uint32_t StageCommit();
    bool WriteStagedCommit(int slot);

    /**
     * Stage + write in one call, on the calling thread. GAME THREAD ONLY (it
     * stages; see StageCommit). This is the route for synchronous commits: the
     * MM-side capture funnel, OoT's exit-time snapshot, tests, and the debug
     * menu's "Save to slot".
     */
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
     * The unified slot the CURRENT SESSION is attached to, or -1 for none.
     *
     * Why this has to exist at all: a .redsave write needs a slot index, and
     * the only code that ever knew one was OoT's save hooks, which get it
     * handed to them as `fileNum`. MM cannot ask the same question. A
     * cross-game MM session (entered through the Happy Mask Shop) runs with
     * gSaveContext.fileNum pinned to the 0xFF sentinel for its ENTIRE life —
     * ConsoleLogo_Init sets it on every MM boot, and the only writers of a real
     * 0..2 slot live in MM's own file select, which a portal arrival never
     * enters. So MM has no slot of its own to save into, and before this there
     * was nowhere for it to look one up.
     *
     * Session state, deliberately NOT serialized: which slot is open is a fact
     * about the running process, not about the save file. It is set by
     * whichever game last established a slot identity (OoT's load / save
     * hooks) and read by any game that needs to persist without one of its own.
     *
     * Out-of-range values are normalized to -1 so a caller that passes MM's
     * 0xFF sentinel by mistake gets "no slot" rather than a silent write to
     * a nonexistent slot.
     */
    void SetActiveSlot(int slot);
    int GetActiveSlot() const;

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

    // Validates magic / version / endian / headerSize and that all three
    // stored tier sizes fit this build's capacities. Stored sizes may be
    // SMALLER (older builds wrote shorter tiers; Load zero-extends) but never
    // larger. Returns true and fills outHeader only on a fully valid header;
    // never mutates live state.
    //
    // `verbose` gates the rejection logging. Load() passes true — a user-
    // initiated load that silently does nothing is the failure mode this whole
    // path exists to prevent. HasSave/ReadMeta pass false: ReadMeta runs for
    // every slot on every file-select frame, so logging there would be a
    // per-frame spam loop, not a diagnostic.
    bool DeserializeHeader(std::istream& in, RsbsSaveHeader& outHeader, bool verbose) const;

    // Serializes the given snapshot to the slot file (temp-write + rename).
    // The ONLY function that writes .redsave bytes; both commit phases and
    // Save() funnel here with an immutable snapshot.
    bool WriteSlotFile(int slot, const ComboContext& combo, const uint8_t* ootBlob, const uint8_t* mmBlob);

    std::string mSaveDir = "Save";

    // -1 == no slot established this session. See SetActiveSlot.
    int mActiveSlot = -1;

    // The staged commit snapshot (see StageCommit/WriteStagedCommit). Guarded
    // by mStageMtx: the game thread overwrites it at each stage, the worker
    // copies it out for serialization. The buffers are allocated at first
    // stage and reused.
    struct StagedCommit {
        ComboContext combo{};
        std::vector<uint8_t> oot;
        std::vector<uint8_t> mm;
        uint32_t generation = 0;
        bool valid = false;
    };
    StagedCommit mStaged;
    mutable std::mutex mStageMtx;

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
 * Two-phase commit shims (#537/#531; see SaveManager::StageCommit).
 * RsbsSave_StageCommit marshals the game-thread snapshot and returns the
 * stamped monotonic generation (0 on refusal); RsbsSave_WriteStagedCommit
 * serializes the staged snapshot from any thread, reading no live state.
 * RsbsSave_Save == stage + write on the calling thread (game thread only).
 */
uint32_t RsbsSave_StageCommit(void);
int      RsbsSave_WriteStagedCommit(int slot);

/**
 * Load-time freshness comparison between the two durable artifacts of the ONE
 * save (#531/#564 V16 interim). Call AFTER a successful RsbsSave_Load, with
 * the commit generation the OoT .sav JSON mirrors ("rsbsCommitGeneration", 0
 * when absent). Compares it against the just-loaded Tier-1 generation.
 *
 * Returns 0 when the artifacts agree (or either side predates the stamp,
 * which is exempt), +1 when the .redsave is NEWER than the .sav (the #531
 * shape: an MM-side commit landed after OoT's last save point — OoT's world
 * is stale relative to the combo records), -1 when the .sav is NEWER (the
 * .redsave write was lost, or a foreign file was swapped in).
 *
 * TODO(#533): a nonzero result should surface through the REFUSED/diagnostic
 * machinery once it exists. Until then this logs loudly and the caller
 * continues — each artifact remains authoritative for its own tiers, i.e. the
 * newer generation's tiers are implicitly preferred, because refusing without
 * #533's quarantine would convert detection into data loss (#564 V19).
 */
int RsbsSave_CheckCommitGenerationSkew(int slot, uint32_t ootSavGeneration);

/**
 * Session-scoped "which slot is open" (see SaveManager::SetActiveSlot).
 * RsbsSave_GetActiveSlot returns -1 when no slot has been established, which
 * every caller must treat as "do not write" rather than as slot 0.
 */
void RsbsSave_SetActiveSlot(int slot);
int  RsbsSave_GetActiveSlot(void);

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
