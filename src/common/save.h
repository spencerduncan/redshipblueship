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

// ---- Slot state: ABSENT / VALID / REFUSED are three DIFFERENT facts ---------
// (#533). A .redsave that fails validation used to leave the session in a state
// byte-for-byte identical to "this slot has no cross-game state at all"; the
// next OoT autosave then rename-overwrote the mostly-intact original with a
// blank Tier-1 and an all-zero Tier-3 — MM's ONLY persistence — unrecoverably.
// REFUSED is now first-class: the refused file is quarantined (renamed aside,
// never overwritten in place), the refusing session latches the slot against
// writes, and the file panel renders REFUSED distinctly from empty.
typedef enum RsbsSlotState {
    RSBS_SLOT_ABSENT = 0,   // no slot file (and this session refused nothing)
    RSBS_SLOT_VALID = 1,    // slot file present, header passes every compat check
    RSBS_SLOT_REFUSED = 2,  // file failed validation, in place or already quarantined
} RsbsSlotState;

// Why a load/validation was refused. Doubles as the quarantine filename tag so
// the renamed-aside evidence names its own diagnosis.
typedef enum RsbsRefuseReason {
    RSBS_REFUSE_NONE = 0,
    RSBS_REFUSE_UNREADABLE,   // file exists but cannot be opened/read
    RSBS_REFUSE_HEADER,       // short header / bad magic / endianness / headerSize
    RSBS_REFUSE_VERSION,      // outside [RSBS_SAVE_VERSION_MIN, RSBS_SAVE_VERSION]
    RSBS_REFUSE_TIER_SIZE,    // a stored tier size exceeds this build's capacity
    RSBS_REFUSE_WRONG_SLOT,   // header.slot != the slot this path belongs to
    RSBS_REFUSE_TRUNCATED,    // a tier read came up short
    RSBS_REFUSE_CRC,          // payload CRC mismatch
    RSBS_REFUSE_COMBO_MAGIC,  // Tier-1 bytes are not a ComboContext
    // #498/#564: the MM option profile resolved at a cross-game arrival does
    // not match the identity frozen at the pair's creation
    // (gComboCtx.mmProfileDigest). Unlike every reason above, the slot FILE is
    // healthy — the running SESSION diverged — so this refusal latches and
    // surfaces without quarantining anything (see RefuseSlotIdentity).
    RSBS_REFUSE_IDENTITY,
} RsbsRefuseReason;

// What a load attempt actually did — richer than the old bool, because ABSENT
// and REFUSED must never collapse into one "false" again.
typedef enum RsbsLoadOutcome {
    RSBS_LOAD_OK = 0,       // loaded and committed; slot armed for writes
    RSBS_LOAD_ABSENT = 1,   // no slot file; nothing loaded; slot armed (safe to create)
    RSBS_LOAD_REFUSED = 2,  // validation failed; evidence quarantined; writes latched
} RsbsLoadOutcome;

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

    // #533: REFUSED distinct from ABSENT. `state` folds together what is on
    // disk right now AND what this session refused (a quarantined slot's file
    // is gone from the slot path, but the slot is REFUSED, not empty).
    // `refuseReason` names the failing check; `hasQuarantine` reports whether
    // renamed-aside evidence (*.bak) sits next to the slot path, which
    // survives process restarts even after `state` degrades to ABSENT.
    RsbsSlotState    state;
    RsbsRefuseReason refuseReason;
    bool             hasQuarantine;
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
     * Write the resident cross-game state into `slot` — IF the armed-session
     * latch allows it (#533). A slot is writable only after this session
     * successfully loaded, created, or erased it; anything else refuses, so a
     * session that REFUSED a slot's .redsave can never rename-overwrite the
     * evidence with a blank Tier-1 + all-zero Tier-3.
     */
    bool Save(int slot);

    /**
     * Full-validation load. RSBS_LOAD_OK commits and arms the slot;
     * RSBS_LOAD_ABSENT arms the slot for its first write (opening an empty
     * slot IS the create path); RSBS_LOAD_REFUSED quarantines the failing file
     * (renamed aside with a reason suffix, never overwritten in place) and
     * latches the slot against writes for the rest of the session. A refusal
     * is sticky: re-opening the now-empty slot path stays REFUSED until the
     * player explicitly erases the slot or a load actually succeeds.
     */
    RsbsLoadOutcome LoadSlot(int slot);

    /** Compatibility wrapper: LoadSlot(slot) == RSBS_LOAD_OK. */
    bool Load(int slot);

    bool HasSave(int slot) const;

    /**
     * Explicit player-initiated erase: removes the slot file, its temp file,
     * and any quarantined evidence, then ARMS the slot (erasing it this
     * session is one of the three legitimate ways to make it writable) and
     * clears any refusal record.
     */
    void DeleteSave(int slot);

    /**
     * The file-create seam's arming call (#533): OoT's Sram_InitSave runs this
     * for the slot the new file occupies, BEFORE its first Save_SaveFile fires
     * the OnSaveFile -> RsbsSave_Save chain. If a slot file exists but fails
     * validation, it is quarantined FIRST — creating a file over a corrupt
     * .redsave must preserve the evidence, not rename-overwrite it — then the
     * slot is armed and any refusal record cleared.
     */
    void ArmSlotOnCreate(int slot);

    /** Armed-session latch state: true iff Save(slot) would be allowed to write. */
    bool IsSlotWritable(int slot) const;

    /**
     * #498/#564: record that THIS SESSION's resolved state diverged from the
     * identity the slot's pair was created under (the arrival digest-mismatch
     * producer). Latches the slot against writes and surfaces REFUSED with
     * RSBS_REFUSE_IDENTITY, exactly like a refused load — but quarantines
     * NOTHING: the on-disk .redsave is healthy and is precisely what must be
     * protected from the divergent session's captures. Released by the same
     * three events as any refusal (successful load, explicit erase, create).
     * Out-of-range slots (including -1, "no active slot") are a no-op.
     */
    void RefuseSlotIdentity(int slot);

    /**
     * ABSENT / VALID / REFUSED for the slot, combining the on-disk file (header
     * compat checks only — no CRC, this is called per-frame) with this
     * session's refusal record, which wins: a quarantined slot reads REFUSED
     * even though its slot path is now empty.
     */
    RsbsSlotState GetSlotState(int slot) const;

    /** Why the slot is REFUSED (RSBS_REFUSE_NONE when it is not). */
    RsbsRefuseReason GetSlotRefuseReason(int slot) const;

    /** True iff renamed-aside quarantine evidence (*.bak) exists for the slot. */
    bool HasQuarantine(int slot) const;

    /**
     * Restore process-start latch state: all slots unarmed, no refusal records.
     * Headless tests use this to simulate a fresh session; production code has
     * no business calling it (the latch deliberately survives session
     * invalidation — it is a fact about the PROCESS, not about a session).
     */
    void ResetSlotSessionState();

    /** Human-readable label for a refuse reason (for the file panel). */
    static const char* RefuseReasonLabel(RsbsRefuseReason reason);

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

    // Everything ReadSlotFile stages before any commit decision is made.
    struct SlotFileData;

    enum class SlotReadResult { Absent, Ok, Refused };

    // Validates magic / version / endian / headerSize / slot and that all
    // three stored tier sizes fit this build's capacities. Stored sizes may be
    // SMALLER (older builds wrote shorter tiers; Load zero-extends) but never
    // larger. Returns true and fills outHeader only on a fully valid header;
    // never mutates live state. `expectedSlot` >= 0 additionally requires
    // header.slot to match — a cloud-synced or hand-copied file that claims a
    // different slot must not silently attach one pair's identity + MM world
    // to another OoT file (#533 / V13).
    //
    // `verbose` gates the rejection logging. Load() passes true — a user-
    // initiated load that silently does nothing is the failure mode this whole
    // path exists to prevent. HasSave/ReadMeta pass false: ReadMeta runs for
    // every slot on every file-select frame, so logging there would be a
    // per-frame spam loop, not a diagnostic.
    bool DeserializeHeader(std::istream& in, int expectedSlot, RsbsSaveHeader& outHeader, bool verbose,
                           RsbsRefuseReason* outReason) const;

    // Reads + FULLY validates (header, slot, tier reads, CRC, inner magic) the
    // slot file into `out` without touching live state. The one validator both
    // LoadSlot and the create-seam probe share, so "what counts as refused"
    // cannot fork between them.
    SlotReadResult ReadSlotFile(int slot, SlotFileData& out, RsbsRefuseReason& outReason, bool verbose) const;

    // Renames the slot file aside as `<slot>.refused-<reason>[-N].bak`,
    // never overwriting an existing quarantine file (dedupe suffix). On rename
    // failure the file stays in place — still safe, because the caller latches
    // the slot and Save() then refuses to touch it.
    void QuarantineSlotFile(int slot, RsbsRefuseReason reason);

    std::string mSaveDir = "Save";

    // -1 == no slot established this session. See SetActiveSlot.
    int mActiveSlot = -1;

    // #533 armed-session latch. A slot becomes writable ONLY via this
    // session's own successful Load, an explicit erase, or the file-create
    // seam — never by default. Deliberately NOT reset by session
    // invalidation: "this process successfully established slot N" stays true
    // across in-process session changes, while a refusal stays sticky.
    bool             mSlotArmed[RSBS_SAVE_MAX_SLOTS]{};
    RsbsRefuseReason mSlotRefused[RSBS_SAVE_MAX_SLOTS]{};

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
 * #533 REFUSED-state surface. LoadSlot is RsbsSave_Load with the three-way
 * outcome preserved (returns RsbsLoadOutcome); ArmSlotOnCreate is the
 * file-create seam's arming call (quarantines a failing existing file first);
 * GetSlotState / GetSlotRefuseReason / HasQuarantine feed the file panel;
 * IsSlotWritable reports the armed-session latch. ResetSlotSessionState
 * restores process-start latch state and exists for the headless tests.
 */
int  RsbsSave_LoadSlot(int slot);
void RsbsSave_ArmSlotOnCreate(int slot);
int  RsbsSave_IsSlotWritable(int slot);
void RsbsSave_RefuseSlotIdentity(int slot);
int  RsbsSave_GetSlotState(int slot);
int  RsbsSave_GetSlotRefuseReason(int slot);
int  RsbsSave_HasQuarantine(int slot);
void RsbsSave_ResetSlotSessionState(void);

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
