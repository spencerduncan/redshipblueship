/**
 * @file foreign_items.c
 * @brief Foreign-item placement table accessors (Phase 3.0 Lane C1, #392).
 *
 * See foreign_items.h for the model. Like shared_items.c, this TU is
 * deliberately free of game headers: it manipulates the ADR-0002 tagged types
 * and gComboCtx only, so it compiles into redship_common and both games (and
 * the ROM-free test harness) call it directly. The pinned POOL half of the
 * header (Combo_GetForeignItemPool / Combo_GetForeignItemName /
 * OoT_ForeignItem_Give) is defined OoT-side, where the RG_* enumerators are
 * in scope.
 */

#include "foreign_items.h"
#include <stdio.h>
#include <string.h>

bool Combo_ForeignPairingActive(void) {
    return gComboCtx.sourceIsRando && gComboCtx.sharedRandoSettingsHash != 0;
}

// ============================================================================
// Combo-level settings (ADR 0011): defaults, resolution, freeze, digest, diff
// ============================================================================
//
// See foreign_items.h for the model and for why every value space here is
// pinned and append-only. This TU stays game-header-free, so the whole surface
// is a function of gComboCtx and of the pinned tables — which is also what lets
// the golden-vector lock drive the REAL encoder rather than a paraphrase.

void Combo_ComboSettingsDefaults(ComboSettingsRecord* out) {
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->formatVersion = (uint8_t)RSBS_COMBO_SETTINGS_FORMAT_VERSION;
    // Accepted answer O2: the shipped default is what already ships. A default
    // that differs from shipped behaviour silently changes every new world at
    // the moment the setting lands, and the change is invisible in a diff.
    out->direction = (uint8_t)RSBS_COMBO_DIR_BOTH;
    // Accepted answer O4: two per-direction counts, each 1..CAP. The cap is the
    // default because that is precisely what both passes place today —
    // min(pool, CAP, candidates) — so the record changes no world.
    out->poolSizeOoT = (uint8_t)RSBS_FOREIGN_PLACEMENT_CAP;
    out->poolSizeMM = (uint8_t)RSBS_FOREIGN_PLACEMENT_CAP;
    // Accepted answer O7, and the same "reproduce today" rule: today's pools are
    // filtered by no class at all, so the default is every ALLOCATED bit.
    out->itemClassOoT = (uint16_t)RSBS_ITEMCLASS_ALL_V1;
    out->itemClassMM = (uint16_t)RSBS_ITEMCLASS_ALL_V1;
    // ADR 0010 D1 / answer O11: GOAL stays beat-both (OoTMM's own goal likewise
    // defaults to 'both'), and the shipped rung is the strictest Ship of
    // Harkinian ships — the proved, no-tricks rung, never the base `none`.
    // Both terms are inert today: their consumers are ADR 0010's increments.
    // They are frozen anyway, because a term added to the identity later cannot
    // describe a world that was already created.
    out->goal = (uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH;
    out->logicRung = (uint8_t)RSBS_COMBO_RUNG_BEATABLE;
    // spare0/spare1 stay 0: unallocated at formatVersion 1, and a nonzero spare
    // in a version-1 record is corruption the divergence diff reports.
}

void Combo_ResolveComboSettings(ComboSettingsRecord* out) {
    if (out == NULL) {
        return;
    }
    // INCREMENT 1 RESOLVES TO THE DEFAULTS. The tier-4 authoring keys
    // (gCombo.Rando.Direction, .PoolSize.OoT/.MM, .ItemClass.OoT/.MM) are
    // increment 2's work and do not exist in any store yet, so "what the player
    // chose" and "what ships" are the same record today. When those keys land,
    // the CVar read goes HERE and every caller follows unchanged — which is the
    // whole reason this is a named resolver rather than an inlined copy of the
    // defaults at each site (the same one-resolver discipline
    // Rando::Foreign::ResolveProfileValues holds for MM's profile).
    Combo_ComboSettingsDefaults(out);
}

bool Combo_ForeignPairingRequested(void) {
    // The FUTURE tense. Derived from the resolved settings and never from
    // gComboCtx's stamp, so it is answerable before generation by construction
    // (ADR 0009 decision 2). Do not collapse it into Combo_ForeignPairingActive.
    ComboSettingsRecord live;
    Combo_ResolveComboSettings(&live);
    return live.direction != (uint8_t)RSBS_COMBO_DIR_OFF;
}

bool Combo_ComboSettingsFrozen(void) {
    // The PRESENT tense, and the exact twin of Combo_MMProfileFrozen: a
    // src/common fact, never a gSaveContext read (ADR 0008 rule 5).
    return gComboCtx.comboSettings.formatVersion != 0;
}

uint8_t Combo_ComboDirection(void) {
    if (Combo_ComboSettingsFrozen()) {
        return gComboCtx.comboSettings.direction;
    }
    ComboSettingsRecord live;
    Combo_ResolveComboSettings(&live);
    return live.direction;
}

bool Combo_ComboDirectionArms(uint8_t originGame) {
    const uint8_t dir = Combo_ComboDirection();
    if (dir == (uint8_t)RSBS_COMBO_DIR_BOTH) {
        return true;
    }
    if (originGame == (uint8_t)GAME_OOT) {
        return dir == (uint8_t)RSBS_COMBO_DIR_FORWARD;
    }
    if (originGame == (uint8_t)GAME_MM) {
        return dir == (uint8_t)RSBS_COMBO_DIR_REVERSE;
    }
    return false;
}

int Combo_ComboPoolSizeFor(uint8_t originGame) {
    if (originGame != (uint8_t)GAME_OOT && originGame != (uint8_t)GAME_MM) {
        return 0;
    }

    // The frozen record is the authority for a created world; an UNFROZEN one
    // falls back to the resolved (shipped) size. That fallback is load-bearing,
    // not defensive: a zero-extended legacy record reads poolSize 0, and
    // honouring it would silently generate a paired world with no crossings at
    // all — the opposite of what a pre-carve world was generated with.
    ComboSettingsRecord rec;
    if (Combo_ComboSettingsFrozen()) {
        rec = gComboCtx.comboSettings;
    } else {
        Combo_ResolveComboSettings(&rec);
    }

    int size = (originGame == (uint8_t)GAME_OOT) ? (int)rec.poolSizeOoT : (int)rec.poolSizeMM;
    // A count that can exceed a table's capacity is a setting that lies
    // (accepted answer O4), and neither option may bump the cap in place. So the
    // clamp lives here, once, rather than at each placement pass.
    if (size > (int)RSBS_FOREIGN_PLACEMENT_CAP) {
        size = (int)RSBS_FOREIGN_PLACEMENT_CAP;
    }
    if (size < 1) {
        // Only reachable from a corrupt/absent frozen record, since the
        // resolver never produces 0. Falling back to the cap keeps a damaged
        // record from silently deleting the crossings rather than refusing.
        size = (int)RSBS_FOREIGN_PLACEMENT_CAP;
    }
    return size;
}

uint16_t Combo_ComboItemClassFor(uint8_t originGame) {
    if (originGame != (uint8_t)GAME_OOT && originGame != (uint8_t)GAME_MM) {
        return 0;
    }

    // The frozen record is the authority for a created world. Unlike the pool
    // SIZE, a frozen ZERO is honoured verbatim: inside a formatted record
    // "itemClass == 0" is a legitimate "no classes armed for this direction"
    // (ADR 0011 decision 3.3), and clamping it up to the defaults would silently
    // re-arm a direction the player turned off. The size clamp is different only
    // because 0 is not a value poolSize can legitimately carry.
    if (Combo_ComboSettingsFrozen()) {
        return (originGame == (uint8_t)GAME_OOT) ? gComboCtx.comboSettings.itemClassOoT
                                                 : gComboCtx.comboSettings.itemClassMM;
    }

    // UNFROZEN: the shipped default (every allocated bit). Load-bearing for the
    // same reason the pool-size fallback is — a zero-extended legacy record must
    // not resolve to "no eligible source items at all".
    ComboSettingsRecord live;
    Combo_ResolveComboSettings(&live);
    return (originGame == (uint8_t)GAME_OOT) ? live.itemClassOoT : live.itemClassMM;
}

int Combo_ForeignPoolClassMembersFor(uint8_t originGame, uint16_t classMask, int* outIndices, int maxIndices) {
    const ComboForeignItemDef* pool = NULL;
    const int poolCount = Combo_GetForeignItemPoolFor(originGame, &pool);
    if (poolCount <= 0 || pool == NULL) {
        return 0;
    }

    // IN POOL ORDER, because pool order is world-visible: the forward pass
    // assigns pool[members[i]] to the i-th drawn host, so regrouping by class
    // here would re-order every already-generated world's crossings.
    //
    // An UNCLASSIFIED row (itemClass == 0) matches no mask and is therefore
    // never drawn. That is deliberate rather than defensive — a row nobody
    // classified is a row nobody adjudicated the criteria for — and the
    // ForeignItemClass lock refuses one at CI rather than at an operator's
    // generation.
    int selected = 0;
    for (int i = 0; i < poolCount; i++) {
        if ((pool[i].itemClass & classMask) == 0) {
            continue;
        }
        if (outIndices != NULL) {
            if (selected >= maxIndices) {
                break;
            }
            outIndices[selected] = i;
        }
        selected++;
    }
    return selected;
}

int Combo_ForeignPoolDrawFor(uint8_t originGame, int* outIndices, int maxIndices) {
    return Combo_ForeignPoolClassMembersFor(originGame, Combo_ComboItemClassFor(originGame), outIndices, maxIndices);
}

const char* Combo_ForeignCriterionName(uint8_t criterion) {
    switch (criterion) {
        case RSBS_FOREIGN_CRIT_NONE:
            return "(none)";
        case RSBS_FOREIGN_CRIT_REAL_ITEM:
            return "real-item";
        case RSBS_FOREIGN_CRIT_NOT_JUNK:
            return "not-junk";
        case RSBS_FOREIGN_CRIT_UNCONDITIONAL_GIVE:
            return "unconditional-give";
        case RSBS_FOREIGN_CRIT_NO_WORLD_EVENT:
            return "no-world-event";
        case RSBS_FOREIGN_CRIT_REWARD:
            return "reward-not-punishment";
        case RSBS_FOREIGN_CRIT_NOT_SHARED_RESOURCE:
            return "not-shared-resource";
        default:
            return "(unknown)";
    }
}

const char* Combo_ForeignItemClassName(uint16_t classBit) {
    switch (classBit) {
        case RSBS_ITEMCLASS_PROGRESSION:
            return "progression";
        case RSBS_ITEMCLASS_SONGS:
            return "songs";
        case RSBS_ITEMCLASS_MASKS:
            return "masks";
        case RSBS_ITEMCLASS_DUNGEON_ITEMS:
            return "dungeon-items";
        case RSBS_ITEMCLASS_DUNGEON_REWARD:
            return "dungeon-reward";
        case RSBS_ITEMCLASS_SIDEQUEST:
            return "sidequest";
        default:
            return "(unknown)";
    }
}

// ---- canonical() and the whole-pair fingerprint (decision 1.4) -------------

void Combo_ComboSettingsCanonical(const ComboSettingsRecord* rec, uint8_t* out) {
    if (out == NULL) {
        return;
    }
    ComboSettingsRecord zero;
    if (rec == NULL) {
        memset(&zero, 0, sizeof(zero));
        rec = &zero;
    }
    // FIELD BY FIELD, DECLARATION ORDER, LITTLE-ENDIAN, A BYTE AT A TIME.
    // Deliberately NOT memcpy(out, rec, sizeof(*rec)): that would make the
    // world's identity a property of the toolchain's struct layout and of the
    // host's endianness, in a fingerprint whose other two terms are both
    // string-encoded specifically to avoid that — and in the exact term #574
    // wants to exchange between peers. ADR 0007 §2 already paid for this lesson.
    out[0] = rec->formatVersion;
    out[1] = rec->direction;
    out[2] = rec->poolSizeOoT;
    out[3] = rec->poolSizeMM;
    out[4] = (uint8_t)(rec->itemClassOoT & 0xFFu);
    out[5] = (uint8_t)((rec->itemClassOoT >> 8) & 0xFFu);
    out[6] = (uint8_t)(rec->itemClassMM & 0xFFu);
    out[7] = (uint8_t)((rec->itemClassMM >> 8) & 0xFFu);
    out[8] = rec->goal;
    out[9] = rec->logicRung;
    out[10] = rec->spare0;
    out[11] = rec->spare1;
}

// FNV-1a 32 — the project's hash (SohUtils::Hash / Ship_Hash / the relay's
// sourceKey all compute exactly this), reimplemented over bytes rather than
// over a std::string so the fingerprint is computable from this game-header-free
// C TU. Same offset basis, same prime; a string and its bytes hash identically.
static uint32_t ComboFnv1a(const uint8_t* bytes, size_t len) {
    uint32_t h = 0x811C9DC5u;
    for (size_t i = 0; i < len; i++) {
        h ^= (uint32_t)bytes[i];
        h *= 0x01000193u;
    }
    return h;
}

static void ComboAppendLe32(uint8_t* out, uint32_t value) {
    out[0] = (uint8_t)(value & 0xFFu);
    out[1] = (uint8_t)((value >> 8) & 0xFFu);
    out[2] = (uint8_t)((value >> 16) & 0xFFu);
    out[3] = (uint8_t)((value >> 24) & 0xFFu);
}

uint32_t Combo_ComputeComboSettingsHash(const ComboSettingsRecord* rec, uint32_t sharedRandoSettingsHash,
                                        uint32_t mmProfileDigest) {
    // canonical(12) || ':' || LE32(settingsHash) || ':' || LE32(mmProfileDigest)
    uint8_t buf[RSBS_COMBO_SETTINGS_CANONICAL_LEN + 1u + 4u + 1u + 4u];
    Combo_ComboSettingsCanonical(rec, buf);
    buf[RSBS_COMBO_SETTINGS_CANONICAL_LEN] = (uint8_t)':';
    ComboAppendLe32(&buf[RSBS_COMBO_SETTINGS_CANONICAL_LEN + 1u], sharedRandoSettingsHash);
    buf[RSBS_COMBO_SETTINGS_CANONICAL_LEN + 5u] = (uint8_t)':';
    ComboAppendLe32(&buf[RSBS_COMBO_SETTINGS_CANONICAL_LEN + 6u], mmProfileDigest);

    uint32_t digest = ComboFnv1a(buf, sizeof(buf));
    if (digest == 0) {
        // Zero displaces, exactly as DigestFromIdentity does: zero is this
        // field's growth-contract "unset", so a real identity hashing to 0 would
        // read as "not frozen" and become an undetectable mismatch. One
        // collision in 2^32 against a certainty of a false negative.
        digest = 0x43425348u; // 'CBSH'
    }
    return digest;
}

uint32_t Combo_StampComboSettingsHash(void) {
    if (!Combo_ComboSettingsFrozen()) {
        return 0;
    }
    gComboCtx.comboSettingsHash = Combo_ComputeComboSettingsHash(
        &gComboCtx.comboSettings, gComboCtx.sharedRandoSettingsHash, gComboCtx.mmProfileDigest);
    return gComboCtx.comboSettingsHash;
}

uint32_t Combo_FreezeComboSettings(const ComboSettingsRecord* rec) {
    ComboSettingsRecord frozen;
    if (rec != NULL) {
        frozen = *rec;
    } else {
        Combo_ResolveComboSettings(&frozen);
    }
    // A caller must not be able to freeze a record that reads as ABSENT: the
    // occupancy tag is what makes the other eleven bytes usable at all.
    frozen.formatVersion = (uint8_t)RSBS_COMBO_SETTINGS_FORMAT_VERSION;
    gComboCtx.comboSettings = frozen;

    // Computed LAST, over the record and BOTH half-digests. The caller is
    // responsible for having stamped those first (decision 4.1's order); this
    // is where that ordering constraint becomes observable, because a hash taken
    // before mmProfileDigest exists folds a zero that will never be true again.
    const uint32_t digest = Combo_StampComboSettingsHash();
    fprintf(stderr,
            "[Combo] combo settings FROZEN: dir=%u poolOoT=%u poolMM=%u classOoT=%04X classMM=%04X goal=%u rung=%u "
            "(fingerprint %08X over settingsHash=%08X mmProfile=%08X)\n",
            (unsigned)frozen.direction, (unsigned)frozen.poolSizeOoT, (unsigned)frozen.poolSizeMM,
            (unsigned)frozen.itemClassOoT, (unsigned)frozen.itemClassMM, (unsigned)frozen.goal,
            (unsigned)frozen.logicRung, (unsigned)digest, (unsigned)gComboCtx.sharedRandoSettingsHash,
            (unsigned)gComboCtx.mmProfileDigest);
    return digest;
}

int Combo_FreezeLegacyComboSettings(void) {
    if (!Combo_ForeignPairingActive()) {
        // Nothing to freeze rules FOR. An unpaired file must not acquire a combo
        // identity it has no world to describe.
        return 0;
    }
    if (Combo_ComboSettingsFrozen()) {
        // A SECOND crossing COMPARES rather than re-freezes. Without this early
        // return the transitional writer would be a self-healing overwrite —
        // exactly what ResolvePairedProfile refuses to be, and what would make
        // every arrival-time divergence silently disappear.
        return 0;
    }

    ComboSettingsRecord defaults;
    Combo_ComboSettingsDefaults(&defaults);
    fprintf(stderr, "[Combo] combo settings: no creation-time record (pre-ADR-0011 pair); freezing the SHIPPED "
                    "DEFAULTS at this first crossing — this pair was generated when there was only one rule set\n");
    Combo_FreezeComboSettings(&defaults);
    return 1;
}

// ---- Field-level divergence ------------------------------------------------

uint32_t Combo_ComboSettingsDivergenceBetween(const ComboSettingsRecord* frozen, const ComboSettingsRecord* live) {
    if (frozen == NULL || live == NULL) {
        return 0;
    }
    if (frozen->formatVersion == 0) {
        // ABSENT, not divergent. Exempt from comparison until the O5 writer
        // freezes it (decision 4.2) — this is the one state where "no bits set"
        // does not mean "the same rules".
        return 0;
    }
    if (frozen->formatVersion > (uint8_t)RSBS_COMBO_SETTINGS_FORMAT_VERSION) {
        // A record from a NEWER build. Its fields cannot be compared, because
        // this build does not know which of them are authoritative. Refuse;
        // never guess.
        return RSBS_COMBO_DIVERGE_UNREADABLE;
    }

    // Every field below exists at formatVersion 1, so no per-field version gate
    // is needed yet. When a field is added at version N, guard it on
    // `min(frozen->formatVersion, live->formatVersion) >= N` — comparing it
    // against an older record would refuse a world for carrying rules that did
    // not exist when it was made.
    uint32_t bits = 0;
    if (frozen->direction != live->direction) {
        bits |= RSBS_COMBO_DIVERGE_DIRECTION;
    }
    if (frozen->poolSizeOoT != live->poolSizeOoT) {
        bits |= RSBS_COMBO_DIVERGE_POOL_SIZE_OOT;
    }
    if (frozen->poolSizeMM != live->poolSizeMM) {
        bits |= RSBS_COMBO_DIVERGE_POOL_SIZE_MM;
    }
    if (frozen->itemClassOoT != live->itemClassOoT) {
        bits |= RSBS_COMBO_DIVERGE_ITEM_CLASS_OOT;
    }
    if (frozen->itemClassMM != live->itemClassMM) {
        bits |= RSBS_COMBO_DIVERGE_ITEM_CLASS_MM;
    }
    if (frozen->goal != live->goal) {
        bits |= RSBS_COMBO_DIVERGE_GOAL;
    }
    if (frozen->logicRung != live->logicRung) {
        bits |= RSBS_COMBO_DIVERGE_LOGIC_RUNG;
    }
    if (frozen->spare0 != live->spare0) {
        bits |= RSBS_COMBO_DIVERGE_SPARE0;
    }
    if (frozen->spare1 != live->spare1) {
        bits |= RSBS_COMBO_DIVERGE_SPARE1;
    }
    return bits;
}

uint32_t Combo_ComboSettingsDivergenceFor(const ComboSettingsRecord* frozen, uint32_t storedHash,
                                          uint32_t sharedRandoSettingsHash, uint32_t mmProfileDigest) {
    if (frozen == NULL || frozen->formatVersion == 0) {
        return 0; // absent, exempt (decision 4.2)
    }
    ComboSettingsRecord live;
    Combo_ResolveComboSettings(&live);
    uint32_t bits = Combo_ComboSettingsDivergenceBetween(frozen, &live);

    // The fingerprint cross-check. Skipped for an UNREADABLE record, where a
    // recomputation would be meaningless anyway (this build cannot know which
    // of that record's fields the newer format made authoritative).
    if ((bits & RSBS_COMBO_DIVERGE_UNREADABLE) == 0) {
        const uint32_t expected = Combo_ComputeComboSettingsHash(frozen, sharedRandoSettingsHash, mmProfileDigest);
        if (storedHash != expected) {
            bits |= RSBS_COMBO_DIVERGE_FINGERPRINT;
        }
    }
    return bits;
}

uint32_t Combo_ComboSettingsDivergence(void) {
    return Combo_ComboSettingsDivergenceFor(&gComboCtx.comboSettings, gComboCtx.comboSettingsHash,
                                            gComboCtx.sharedRandoSettingsHash, gComboCtx.mmProfileDigest);
}

const char* Combo_ComboSettingsDivergenceFieldName(uint32_t bit) {
    switch (bit) {
        case RSBS_COMBO_DIVERGE_DIRECTION:
            return "direction";
        case RSBS_COMBO_DIVERGE_POOL_SIZE_OOT:
            return "poolSizeOoT";
        case RSBS_COMBO_DIVERGE_POOL_SIZE_MM:
            return "poolSizeMM";
        case RSBS_COMBO_DIVERGE_ITEM_CLASS_OOT:
            return "itemClassOoT";
        case RSBS_COMBO_DIVERGE_ITEM_CLASS_MM:
            return "itemClassMM";
        case RSBS_COMBO_DIVERGE_GOAL:
            return "goal";
        case RSBS_COMBO_DIVERGE_LOGIC_RUNG:
            return "logicRung";
        case RSBS_COMBO_DIVERGE_SPARE0:
            return "spare0";
        case RSBS_COMBO_DIVERGE_SPARE1:
            return "spare1";
        case RSBS_COMBO_DIVERGE_UNREADABLE:
            return "formatVersion";
        case RSBS_COMBO_DIVERGE_FINGERPRINT:
            return "comboSettingsHash";
        default:
            return "(unknown)";
    }
}

int Combo_ComboSettingsDivergenceDescribe(uint32_t bits, char* out, size_t len) {
    if (out != NULL && len > 0) {
        out[0] = '\0';
    }
    if (bits == 0) {
        if (out != NULL && len > 0) {
            // "(none)" rather than an empty string: a refusal message that
            // interpolates an empty field list reads as a truncated sentence,
            // which is how "loud and explainable" degrades back into "loud".
            const char* none = "(none)";
            size_t i = 0;
            while (none[i] != '\0' && i + 1u < len) {
                out[i] = none[i];
                i++;
            }
            out[i] = '\0';
        }
        return 0;
    }

    int named = 0;
    size_t used = 0;
    for (int b = 0; b < 32; b++) {
        const uint32_t bit = 1u << b;
        if ((bits & bit) == 0) {
            continue;
        }
        const char* name = Combo_ComboSettingsDivergenceFieldName(bit);
        if (out != NULL && len > 0) {
            if (named > 0 && used + 2u < len) {
                out[used++] = ',';
                out[used++] = ' ';
            }
            size_t i = 0;
            while (name[i] != '\0' && used + 1u < len) {
                out[used++] = name[i++];
            }
            out[used] = '\0';
        }
        named++;
    }
    return named;
}

void Combo_ComboSettingsSummary(ComboSettingsSummary* out) {
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    // Same predicate the MM profile summary uses: "a paired world exists" is a
    // post-condition of a successful generation, not an intent. An unpaired
    // world must not present gComboCtx's zeros as real rules.
    out->paired = Combo_ForeignPairingActive();
    if (!out->paired) {
        return;
    }
    out->frozen = Combo_ComboSettingsFrozen();
    out->record = gComboCtx.comboSettings;
    out->comboSettingsHash = gComboCtx.comboSettingsHash;
}

// ============================================================================
// Pool registry, indexed by origin game (ADR 0009 decision 3)
// ============================================================================
//
// Each pool's table is defined in the TU where its enum is in scope and lands
// here through a file-scope registration, so neither game has to be linkable
// from the other and a build with only one pool present still resolves. See
// foreign_items.h for why (originGame, name) is the key and a bare name is not.

static const ComboForeignItemDef* sForeignPools[RSBS_FOREIGN_POOL_ORIGIN_COUNT];
static int sForeignPoolCounts[RSBS_FOREIGN_POOL_ORIGIN_COUNT];

void Combo_RegisterForeignItemPool(uint8_t originGame, const ComboForeignItemDef* pool, int count) {
    if (originGame == (uint8_t)GAME_NONE || originGame >= RSBS_FOREIGN_POOL_ORIGIN_COUNT) {
        fprintf(stderr, "[ForeignItem] pool registration rejected: origin %u is not a game id-space\n",
                (unsigned)originGame);
        return;
    }
    if (pool == NULL && count == 0) {
        // Explicit un-register. Exists so a test can install a synthetic pool
        // for an origin whose real pool TU is not linked yet and then put the
        // registry back, rather than leaving process-global state behind for
        // whichever test runs next.
        sForeignPools[originGame] = NULL;
        sForeignPoolCounts[originGame] = 0;
        return;
    }
    if (pool == NULL || count <= 0) {
        fprintf(stderr, "[ForeignItem] pool registration rejected: empty pool for origin %u\n", (unsigned)originGame);
        return;
    }
    if (sForeignPools[originGame] != NULL) {
        // Two tables claiming one id-space is exactly the ambiguity this
        // surface exists to prevent; it must not pass silently even though the
        // last writer wins.
        fprintf(stderr, "[ForeignItem] pool for origin %u re-registered (%d entries replace %d)\n",
                (unsigned)originGame, count, sForeignPoolCounts[originGame]);
    }
    sForeignPools[originGame] = pool;
    sForeignPoolCounts[originGame] = count;
}

int Combo_GetForeignItemPoolFor(uint8_t originGame, const ComboForeignItemDef** outPool) {
    if (originGame == (uint8_t)GAME_NONE || originGame >= RSBS_FOREIGN_POOL_ORIGIN_COUNT) {
        return 0;
    }
    if (sForeignPools[originGame] == NULL) {
        return 0; // that origin's pool TU is not linked into this build
    }
    if (outPool != NULL) {
        *outPool = sForeignPools[originGame];
    }
    return sForeignPoolCounts[originGame];
}

int Combo_GetForeignItemPool(const ComboForeignItemDef** outPool) {
    return Combo_GetForeignItemPoolFor((uint8_t)GAME_OOT, outPool);
}

const char* Combo_GetForeignItemName(SharedItem item) {
    // Origin-aware by construction: the SharedItem carries its own tag, so the
    // only thing the origin dimension changes is WHICH pool gets walked. An
    // untagged item resolves to no pool and therefore to no name, which is the
    // correct answer rather than a lucky match in whichever table came first.
    const ComboForeignItemDef* pool = NULL;
    const int poolCount = Combo_GetForeignItemPoolFor(item.originGame, &pool);
    for (int i = 0; i < poolCount; i++) {
        if (pool[i].item.originGame == item.originGame && pool[i].item.id == item.id) {
            return pool[i].name;
        }
    }
    return NULL;
}

const char* Combo_GetForeignItemArticle(SharedItem item) {
    // Same origin-keyed walk as the name lookup — deliberately a separate entry
    // point rather than a second out-param, so the spoiler surfaces (which want
    // the bare name) are not forced to think about articles at all.
    const ComboForeignItemDef* pool = NULL;
    const int poolCount = Combo_GetForeignItemPoolFor(item.originGame, &pool);
    for (int i = 0; i < poolCount; i++) {
        if (pool[i].item.originGame == item.originGame && pool[i].item.id == item.id) {
            return pool[i].article;
        }
    }
    return NULL;
}

const char* Combo_GetForeignItemIconName(SharedItem item) {
    // The icon column of the SAME origin-keyed walk. A pool entry may carry a
    // NULL icon (its origin's toast renders text-only), so a NULL result here
    // means either "not in the pool" or "no icon for this entry" — both of which
    // the arrival toast treats identically, so they need not be distinguished.
    const ComboForeignItemDef* pool = NULL;
    const int poolCount = Combo_GetForeignItemPoolFor(item.originGame, &pool);
    for (int i = 0; i < poolCount; i++) {
        if (pool[i].item.originGame == item.originGame && pool[i].item.id == item.id) {
            return pool[i].iconName;
        }
    }
    return NULL;
}

bool Combo_GetForeignItemByNameFor(uint8_t originGame, const char* name, SharedItem* outItem) {
    // The spoiler-LOAD inverse. Reused (not re-derived) so reconstruction shares
    // one source of truth with generation, and so a raw RG_*/RI_* is never
    // fabricated on the far side (ADR 0002). Scoped to ONE origin's pool: the
    // display names are not unique across pools ("Lens of Truth" is in both), so a
    // merged scan would resolve to a wrong origin tag rather than to nothing.
    if (name == NULL) {
        return false;
    }
    const ComboForeignItemDef* pool = NULL;
    const int poolCount = Combo_GetForeignItemPoolFor(originGame, &pool);
    for (int i = 0; i < poolCount; i++) {
        if (pool[i].name != NULL && strcmp(pool[i].name, name) == 0) {
            if (outItem != NULL) {
                *outItem = pool[i].item;
            }
            return true;
        }
    }
    return false;
}

bool Combo_GetForeignItemByName(const char* name, SharedItem* outItem) {
    return Combo_GetForeignItemByNameFor((uint8_t)GAME_OOT, name, outItem);
}

// ============================================================================
// Placement tables — one per DIRECTION (#493, ADR 0009 decision 3)
// ============================================================================
//
// There are two tables and they are separate key spaces, not one array split
// in two:
//
//   gComboCtx.foreignPlacements     keyed by MM   RandoCheckId -> OoT item
//   gComboCtx.foreignPlacementsOoT  keyed by OoT  RandomizerCheck -> MM item
//
// An OoT RC and an MM RC are unrelated enumerations that collide freely as raw
// u16s, which is exactly why one table with no host discriminator would
// false-positive across directions. ComboForeignPlacement cannot gain a host
// byte in place (its size and member offsets are static_asserted .redsave
// format), so the DIRECTION IS THE ACCESSOR: the table you pass is the host
// discriminator, and no lookup ever consults the other one.
//
// The bodies below are shared between directions on purpose. #493 names the
// duplication of five accessors as the accepted cost of the parallel carve;
// taking the table as a parameter pays it once instead of five times, so a fix
// to the duplicate scan cannot land in one direction and miss the other.

// A direction, for the log lines only. Behavior must never branch on this.
static const char* ForeignHostName(const ComboForeignPlacement* table) {
    return (table == gComboCtx.foreignPlacementsOoT) ? "OoT" : "MM";
}

static int ForeignPlaceInto(ComboForeignPlacement* table, uint16_t hostCheckId, SharedItem item) {
    if (hostCheckId == 0 || item.originGame == (uint8_t)GAME_NONE) {
        return -1; // check id 0 is each game's RC_UNKNOWN and never hosts; an untagged item must not enter
    }

    // ONE pass finds both the duplicate and the first free slot. Splitting it
    // into two passes is how a cross-block duplicate escapes when a second
    // block is carved later (see the RSBS_FOREIGN_PLACEMENT_CAP note in
    // context.h) — keep them fused.
    int firstFree = -1;
    for (int i = 0; i < (int)RSBS_FOREIGN_PLACEMENT_CAP; i++) {
        ComboForeignPlacement* slot = &table[i];
        if (slot->item.originGame == (uint8_t)GAME_NONE) {
            if (firstFree < 0) {
                firstFree = i;
            }
            continue;
        }
        if (slot->mmCheckId == hostCheckId) {
            fprintf(stderr, "[ForeignItem] placement rejected: %s check %u already hosts origin=%u id=%u\n",
                    ForeignHostName(table), (unsigned)hostCheckId, (unsigned)slot->item.originGame,
                    (unsigned)slot->item.id);
            return -1; // one check hosts at most one foreign item
        }
    }

    if (firstFree < 0) {
        fprintf(stderr, "[ForeignItem] placement dropped: %s table full (%u slots), check=%u id=%u\n",
                ForeignHostName(table), RSBS_FOREIGN_PLACEMENT_CAP, (unsigned)hostCheckId, (unsigned)item.id);
        return -1;
    }

    ComboForeignPlacement* dst = &table[firstFree];
    dst->mmCheckId = hostCheckId;
    dst->item = item;
    fprintf(stderr, "[ForeignItem] placed origin=%u id=%u at %s check %u (slot %d)\n", (unsigned)item.originGame,
            (unsigned)item.id, ForeignHostName(table), (unsigned)hostCheckId, firstFree);
    return firstFree;
}

static const SharedItem* ForeignLookupIn(const ComboForeignPlacement* table, uint16_t hostCheckId) {
    if (hostCheckId == 0) {
        return NULL;
    }
    for (int i = 0; i < (int)RSBS_FOREIGN_PLACEMENT_CAP; i++) {
        const ComboForeignPlacement* slot = &table[i];
        if (slot->item.originGame != (uint8_t)GAME_NONE && slot->mmCheckId == hostCheckId) {
            return &slot->item;
        }
    }
    return NULL;
}

static int ForeignCountIn(const ComboForeignPlacement* table) {
    int count = 0;
    for (int i = 0; i < (int)RSBS_FOREIGN_PLACEMENT_CAP; i++) {
        if (table[i].item.originGame != (uint8_t)GAME_NONE) {
            count++;
        }
    }
    return count;
}

static void ForeignClear(ComboForeignPlacement* table) {
    for (int i = 0; i < (int)RSBS_FOREIGN_PLACEMENT_CAP; i++) {
        table[i].mmCheckId = 0;
        table[i].item.originGame = (uint8_t)GAME_NONE;
        table[i].item.flags = 0;
        table[i].item.id = 0;
    }
}

// ---- Forward direction: MM checks host OoT items -----------------------------

int Combo_SetForeignPlacement(uint16_t mmCheckId, SharedItem item) {
    return ForeignPlaceInto(gComboCtx.foreignPlacements, mmCheckId, item);
}

const SharedItem* Combo_GetForeignPlacementForCheck(uint16_t mmCheckId) {
    return ForeignLookupIn(gComboCtx.foreignPlacements, mmCheckId);
}

int Combo_CountForeignPlacements(void) {
    return ForeignCountIn(gComboCtx.foreignPlacements);
}

void Combo_ClearForeignPlacements(void) {
    ForeignClear(gComboCtx.foreignPlacements);
}

// ---- Reverse direction: OoT checks host MM items -----------------------------

int Combo_SetForeignPlacementOoT(uint16_t ootCheckId, SharedItem item) {
    return ForeignPlaceInto(gComboCtx.foreignPlacementsOoT, ootCheckId, item);
}

const SharedItem* Combo_GetForeignPlacementForOoTCheck(uint16_t ootCheckId) {
    return ForeignLookupIn(gComboCtx.foreignPlacementsOoT, ootCheckId);
}

int Combo_CountForeignPlacementsOoT(void) {
    return ForeignCountIn(gComboCtx.foreignPlacementsOoT);
}

void Combo_ClearForeignPlacementsOoT(void) {
    ForeignClear(gComboCtx.foreignPlacementsOoT);
}
