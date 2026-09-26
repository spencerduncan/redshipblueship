/**
 * @file crossing_store.c
 * @brief The cross-game placement store (ADR 0010 O7). See crossing_store.h for
 *        the decision, the format and the three writers' rules.
 *
 * Game-header free, like foreign_items.c and combo_logic.c: every row is a host
 * check id plus an origin-tagged SharedItem, so this file compiles into the
 * common library and the ROM-free tier drives it directly.
 *
 * THREADING: game thread only. The scratch buffers are file-static (the largest
 * is the serialized block at RSBS_CROSSING_BLOCK_MAX_SIZE), which is only safe
 * because of that; SaveManager::StageCommit serializes on the game thread and
 * hands the bytes to the write phase, so the save worker never reads the store.
 */

#include "crossing_store.h"
#include "foreign_items.h" // Combo_ForeignPairingActive

#include <stdio.h>
#include <string.h>

// Index 0 = OoT hosts (MM-origin items), 1 = MM hosts (OoT-origin items).
static ComboCrossing sRows[2][RSBS_CROSSING_STORE_CAP];
static int sCount[2];
// FROZEN vs UNSET (crossing_store.h, "FROZEN EMPTY IS NOT UNSET"): the counts
// alone cannot tell a world frozen with no crossings from a store nobody wrote.
static bool sFrozen;
// Bumped by every write; see Combo_Crossings_WriteGeneration.
static uint32_t sWriteGeneration;

// Parse / capture scratch, so a refused write never touches sRows.
static ComboCrossing sScratch[2][RSBS_CROSSING_STORE_CAP];
static int sScratchCount[2];

// Duplicate-host scratch: one bit per u16 host id.
static uint8_t sHostSeen[65536 / 8];

// Serialized image for the digest.
static uint8_t sImage[RSBS_CROSSING_BLOCK_MAX_SIZE];

static int CrossingSide(uint8_t hostGame) {
    if (hostGame == (uint8_t)GAME_OOT) {
        return 0;
    }
    if (hostGame == (uint8_t)GAME_MM) {
        return 1;
    }
    return -1;
}

static uint8_t CrossingHostGame(int side) {
    return side == 0 ? (uint8_t)GAME_OOT : (uint8_t)GAME_MM;
}

const char* Combo_Crossings_StatusName(int status) {
    switch (status) {
        case RSBS_CROSSING_OK:
            return "ok";
        case RSBS_CROSSING_ERR_CAPACITY:
            return "capacity";
        case RSBS_CROSSING_ERR_BAD_ROW:
            return "bad-row";
        case RSBS_CROSSING_ERR_DUPLICATE_HOST:
            return "duplicate-host";
        case RSBS_CROSSING_ERR_NOT_PAIRED:
            return "not-paired";
        case RSBS_CROSSING_ERR_DIVERGED:
            return "diverged";
        case RSBS_CROSSING_ERR_MALFORMED:
            return "malformed";
        case RSBS_CROSSING_ERR_COORDINATOR:
            return "coordinator-refused";
        default:
            return status >= 0 ? "ok" : "unknown";
    }
}

void Combo_Crossings_Clear(void) {
    sCount[0] = 0;
    sCount[1] = 0;
    memset(sRows, 0, sizeof(sRows));
    sFrozen = false;
    ++sWriteGeneration;
}

bool Combo_Crossings_IsFrozen(void) {
    return sFrozen;
}

uint32_t Combo_Crossings_WriteGeneration(void) {
    return sWriteGeneration;
}

int Combo_Crossings_Count(GameId hostGame) {
    const int side = CrossingSide((uint8_t)hostGame);
    return side < 0 ? 0 : sCount[side];
}

bool Combo_Crossings_At(GameId hostGame, int index, ComboCrossing* out) {
    const int side = CrossingSide((uint8_t)hostGame);
    if (side < 0 || index < 0 || index >= sCount[side]) {
        return false;
    }
    if (out != NULL) {
        *out = sRows[side][index];
    }
    return true;
}

const SharedItem* Combo_Crossings_Lookup(GameId hostGame, uint16_t hostCheck) {
    const int side = CrossingSide((uint8_t)hostGame);
    if (side < 0 || hostCheck == 0) {
        return NULL;
    }
    // Linear, and deliberately so: at most RSBS_CROSSING_STORE_CAP (1024) u16
    // compares per miss. It is reached through the give-path accessors, so it
    // runs per pickup (CheckQueue) and also from MM's per-actor/per-draw
    // foreign-check probes (EnBox, DrawItem): a bounded scan of an 8 KiB table,
    // not a measurable cost. An index would be a second structure to keep
    // coherent with every writer.
    for (int i = 0; i < sCount[side]; ++i) {
        if (sRows[side][i].hostCheck == hostCheck) {
            return &sRows[side][i].item;
        }
    }
    return NULL;
}

/**
 * The row rules every writer enforces, over ONE host game's list: at most the
 * cap, no host 0 (both games' RC_UNKNOWN), an origin that is a real game and is
 * NOT the host's own (an own-origin row is not a crossing; it belongs to the
 * host game's own save), and one row per host.
 */
static int CrossingValidateSide(const ComboCrossing* rows, int count, uint8_t hostGame) {
    if (count < 0 || count > (int)RSBS_CROSSING_STORE_CAP) {
        return RSBS_CROSSING_ERR_CAPACITY;
    }
    if (count > 0 && rows == NULL) {
        return RSBS_CROSSING_ERR_BAD_ROW;
    }
    memset(sHostSeen, 0, sizeof(sHostSeen));
    for (int i = 0; i < count; ++i) {
        const ComboCrossing* r = &rows[i];
        const uint8_t origin = r->item.originGame;
        if (r->hostCheck == 0 || CrossingSide(origin) < 0 || origin == hostGame) {
            return RSBS_CROSSING_ERR_BAD_ROW;
        }
        const uint16_t h = r->hostCheck;
        if ((sHostSeen[h >> 3] & (uint8_t)(1u << (h & 7))) != 0) {
            return RSBS_CROSSING_ERR_DUPLICATE_HOST;
        }
        sHostSeen[h >> 3] |= (uint8_t)(1u << (h & 7));
    }
    return RSBS_CROSSING_OK;
}

static bool CrossingRowEqual(const ComboCrossing* a, const ComboCrossing* b) {
    return a->hostCheck == b->hostCheck && a->itemClass == b->itemClass && a->item.originGame == b->item.originGame &&
           a->item.flags == b->item.flags && a->item.id == b->item.id;
}

static void CrossingCommit(const ComboCrossing* oot, int ootCount, const ComboCrossing* mm, int mmCount) {
    Combo_Crossings_Clear();
    for (int i = 0; i < ootCount; ++i) {
        sRows[0][i] = oot[i];
    }
    for (int i = 0; i < mmCount; ++i) {
        sRows[1][i] = mm[i];
    }
    sCount[0] = ootCount;
    sCount[1] = mmCount;
    sFrozen = true;
    ++sWriteGeneration;
}

int Combo_Crossings_Replace(const ComboCrossing* ootHosted, int ootCount, const ComboCrossing* mmHosted,
                            int mmCount) {
    int rc = CrossingValidateSide(ootHosted, ootCount, (uint8_t)GAME_OOT);
    if (rc == RSBS_CROSSING_OK) {
        rc = CrossingValidateSide(mmHosted, mmCount, (uint8_t)GAME_MM);
    }
    if (rc != RSBS_CROSSING_OK) {
        fprintf(stderr, "[Crossings] replace REFUSED (%s); the store is unchanged\n", Combo_Crossings_StatusName(rc));
        return rc;
    }

    if (sFrozen) {
        // FROZEN, possibly EMPTY. The resident set is the world's identity; a
        // second writer may only agree with it. A world frozen with zero
        // crossings refuses a non-empty set exactly as a populated one refuses
        // a different one: the counts alone cannot say which, so the flag does.
        bool same = (sCount[0] == ootCount && sCount[1] == mmCount);
        for (int i = 0; same && i < ootCount; ++i) {
            same = CrossingRowEqual(&sRows[0][i], &ootHosted[i]);
        }
        for (int i = 0; same && i < mmCount; ++i) {
            same = CrossingRowEqual(&sRows[1][i], &mmHosted[i]);
        }
        if (!same) {
            fprintf(stderr,
                    "[Crossings] replace REFUSED: a different crossing set (%d OoT-hosted, %d MM-hosted) is already "
                    "frozen for this world; divergence is refused, never honoured. The store is unchanged\n",
                    sCount[0], sCount[1]);
            return RSBS_CROSSING_ERR_DIVERGED;
        }
        return ootCount + mmCount;
    }

    CrossingCommit(ootHosted, ootCount, mmHosted, mmCount);
    return ootCount + mmCount;
}

int Combo_Crossings_CaptureFromCoordinator(void) {
    if (!Combo_ForeignPairingActive()) {
        Combo_Crossings_Clear();
        fprintf(stderr, "[Crossings] capture REFUSED: no live cross-game pairing; the store is left empty\n");
        return RSBS_CROSSING_ERR_NOT_PAIRED;
    }

    for (int side = 0; side < 2; ++side) {
        const uint8_t host = CrossingHostGame(side);
        const int placed = Combo_Logic_PlacementCount((GameId)host);
        sScratchCount[side] = 0;
        for (int i = 0; i < placed; ++i) {
            ComboLogicPlacement p;
            if (!Combo_Logic_PlacementAt((GameId)host, i, &p) || p.item.originGame == host) {
                continue;
            }
            if (sScratchCount[side] >= (int)RSBS_CROSSING_STORE_CAP) {
                // Refused, never truncated: a store that kept the first cap rows
                // would hand the give path a world that is not the one generated.
                Combo_Crossings_Clear();
                fprintf(stderr, "[Crossings] capture REFUSED: more than %d crossings hosted in %s; store left empty\n",
                        (int)RSBS_CROSSING_STORE_CAP, Game_ToString((GameId)host));
                return RSBS_CROSSING_ERR_CAPACITY;
            }
            ComboCrossing* c = &sScratch[side][sScratchCount[side]++];
            c->hostCheck = p.hostCheck;
            c->itemClass = p.itemClass;
            c->item = p.item;
        }
    }

    int rc = CrossingValidateSide(sScratch[0], sScratchCount[0], (uint8_t)GAME_OOT);
    if (rc == RSBS_CROSSING_OK) {
        rc = CrossingValidateSide(sScratch[1], sScratchCount[1], (uint8_t)GAME_MM);
    }
    if (rc != RSBS_CROSSING_OK) {
        Combo_Crossings_Clear();
        fprintf(stderr, "[Crossings] capture REFUSED (%s); store left empty\n", Combo_Crossings_StatusName(rc));
        return rc;
    }

    CrossingCommit(sScratch[0], sScratchCount[0], sScratch[1], sScratchCount[1]);
    fprintf(stderr, "[Crossings] captured %d OoT-hosted and %d MM-hosted crossings (digest %08X)\n", sCount[0],
            sCount[1], (unsigned)Combo_Crossings_Digest());
    return sCount[0] + sCount[1];
}

int Combo_Crossings_HydrateCoordinator(void) {
    static ComboLogicPlacement rows[2][RSBS_CROSSING_STORE_CAP];
    for (int side = 0; side < 2; ++side) {
        for (int i = 0; i < sCount[side]; ++i) {
            rows[side][i].hostCheck = sRows[side][i].hostCheck;
            rows[side][i].itemClass = sRows[side][i].itemClass;
            rows[side][i].item = sRows[side][i].item;
        }
    }
    if (!Combo_Logic_HydrateTables(rows[0], sCount[0], rows[1], sCount[1])) {
        fprintf(stderr, "[Crossings] hydrate REFUSED by the coordinator; its tables are unchanged\n");
        return RSBS_CROSSING_ERR_COORDINATOR;
    }
    return sCount[0] + sCount[1];
}

// ---- The codec ----------------------------------------------------------------

static void PutU16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static void PutU32(uint8_t* p, uint32_t v) {
    PutU16(p, (uint16_t)(v & 0xFFFFu));
    PutU16(p + 2, (uint16_t)((v >> 16) & 0xFFFFu));
}

static uint16_t GetU16(const uint8_t* p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t GetU32(const uint8_t* p) {
    return (uint32_t)GetU16(p) | ((uint32_t)GetU16(p + 2) << 16);
}

size_t Combo_Crossings_SerializedSize(void) {
    return (size_t)RSBS_CROSSING_BLOCK_HEADER_SIZE +
           (size_t)(sCount[0] + sCount[1]) * (size_t)RSBS_CROSSING_RECORD_SIZE;
}

/** The one encoder: `rows[side]` / `counts[side]`, already bounded by the caller. */
static size_t CrossingSerializeRows(const ComboCrossing* const rows[2], const int counts[2], uint8_t* out,
                                    size_t cap) {
    const size_t total =
        (size_t)RSBS_CROSSING_BLOCK_HEADER_SIZE + (size_t)(counts[0] + counts[1]) * (size_t)RSBS_CROSSING_RECORD_SIZE;
    if (out == NULL || cap < total) {
        return 0;
    }
    memcpy(out, RSBS_CROSSING_BLOCK_MAGIC, 4);
    PutU16(out + 4, (uint16_t)RSBS_CROSSING_BLOCK_FORMAT);
    PutU16(out + 6, (uint16_t)RSBS_CROSSING_RECORD_SIZE);
    PutU16(out + 8, (uint16_t)counts[0]);
    PutU16(out + 10, (uint16_t)counts[1]);
    PutU32(out + 12, 0u);
    uint8_t* p = out + RSBS_CROSSING_BLOCK_HEADER_SIZE;
    for (int side = 0; side < 2; ++side) {
        for (int i = 0; i < counts[side]; ++i) {
            const ComboCrossing* r = &rows[side][i];
            PutU16(p + 0, r->hostCheck);
            PutU16(p + 2, r->itemClass);
            p[4] = r->item.originGame;
            p[5] = r->item.flags;
            PutU16(p + 6, r->item.id);
            p += RSBS_CROSSING_RECORD_SIZE;
        }
    }
    return total;
}

size_t Combo_Crossings_Serialize(uint8_t* out, size_t cap) {
    const ComboCrossing* const rows[2] = { sRows[0], sRows[1] };
    return CrossingSerializeRows(rows, sCount, out, cap);
}

static uint32_t CrossingFnv(const uint8_t* bytes, size_t n) {
    uint32_t h = 0x811C9DC5u;
    for (size_t i = 0; i < n; ++i) {
        h ^= bytes[i];
        h *= 0x01000193u;
    }
    return h;
}

uint32_t Combo_Crossings_Digest(void) {
    return CrossingFnv(sImage, Combo_Crossings_Serialize(sImage, sizeof(sImage)));
}

uint32_t Combo_Crossings_DigestRows(const ComboCrossing* ootHosted, int ootCount, const ComboCrossing* mmHosted,
                                    int mmCount) {
    if (ootCount < 0 || ootCount > (int)RSBS_CROSSING_STORE_CAP || mmCount < 0 ||
        mmCount > (int)RSBS_CROSSING_STORE_CAP || (ootCount > 0 && ootHosted == NULL) ||
        (mmCount > 0 && mmHosted == NULL)) {
        return 0u;
    }
    const ComboCrossing* const rows[2] = { ootHosted, mmHosted };
    const int counts[2] = { ootCount, mmCount };
    return CrossingFnv(sImage, CrossingSerializeRows(rows, counts, sImage, sizeof(sImage)));
}

int Combo_Crossings_BlockSize(const uint8_t* header, size_t headerLen, size_t* outTotal) {
    if (header == NULL || headerLen < RSBS_CROSSING_BLOCK_HEADER_SIZE) {
        return RSBS_CROSSING_ERR_MALFORMED;
    }
    if (memcmp(header, RSBS_CROSSING_BLOCK_MAGIC, 4) != 0 || GetU16(header + 4) != RSBS_CROSSING_BLOCK_FORMAT ||
        GetU16(header + 6) != RSBS_CROSSING_RECORD_SIZE || GetU32(header + 12) != 0u) {
        return RSBS_CROSSING_ERR_MALFORMED;
    }
    const uint16_t ootCount = GetU16(header + 8);
    const uint16_t mmCount = GetU16(header + 10);
    if (ootCount > RSBS_CROSSING_STORE_CAP || mmCount > RSBS_CROSSING_STORE_CAP) {
        return RSBS_CROSSING_ERR_CAPACITY;
    }
    if (outTotal != NULL) {
        *outTotal = (size_t)RSBS_CROSSING_BLOCK_HEADER_SIZE +
                    ((size_t)ootCount + (size_t)mmCount) * (size_t)RSBS_CROSSING_RECORD_SIZE;
    }
    return RSBS_CROSSING_OK;
}

/** Parse + validate into sScratch; the store is not touched. */
static int CrossingParse(const uint8_t* block, size_t len) {
    size_t total = 0;
    int rc = Combo_Crossings_BlockSize(block, len, &total);
    if (rc != RSBS_CROSSING_OK) {
        return rc;
    }
    if (len != total) {
        return RSBS_CROSSING_ERR_MALFORMED;
    }
    sScratchCount[0] = (int)GetU16(block + 8);
    sScratchCount[1] = (int)GetU16(block + 10);
    const uint8_t* p = block + RSBS_CROSSING_BLOCK_HEADER_SIZE;
    for (int side = 0; side < 2; ++side) {
        for (int i = 0; i < sScratchCount[side]; ++i) {
            ComboCrossing* r = &sScratch[side][i];
            r->hostCheck = GetU16(p + 0);
            r->itemClass = GetU16(p + 2);
            r->item.originGame = p[4];
            r->item.flags = p[5];
            r->item.id = GetU16(p + 6);
            p += RSBS_CROSSING_RECORD_SIZE;
        }
    }
    rc = CrossingValidateSide(sScratch[0], sScratchCount[0], (uint8_t)GAME_OOT);
    if (rc == RSBS_CROSSING_OK) {
        rc = CrossingValidateSide(sScratch[1], sScratchCount[1], (uint8_t)GAME_MM);
    }
    return rc;
}

int Combo_Crossings_ValidateBlock(const uint8_t* block, size_t len) {
    return CrossingParse(block, len);
}

int Combo_Crossings_LoadBlock(const uint8_t* block, size_t len) {
    if (block == NULL && len == 0) {
        // A pre-crossing file: its world has none, and that is FROZEN, not unset.
        CrossingCommit(NULL, 0, NULL, 0);
        return RSBS_CROSSING_OK;
    }
    const int rc = CrossingParse(block, len);
    if (rc != RSBS_CROSSING_OK) {
        fprintf(stderr, "[Crossings] load REFUSED (%s); the store is unchanged\n", Combo_Crossings_StatusName(rc));
        return rc;
    }
    CrossingCommit(sScratch[0], sScratchCount[0], sScratch[1], sScratchCount[1]);
    return RSBS_CROSSING_OK;
}
