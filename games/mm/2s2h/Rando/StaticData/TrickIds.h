/**
 * @file TrickIds.h
 * @brief MM's per-trick vocabulary: the `MMRT_*` key space and its two
 *        classification axes, C-safe (#578 part 1; ADR 0010 O9, D6, §3.2-3.3,
 *        O11).
 *
 * PLAIN ENUMS ONLY, NO INCLUDES. `games/mm/include/z64save.h` needs `MMRT_MAX`
 * to size `RandoSaveInfo::randoSaveTricks`, and z64save.h is reached from C
 * translation units and from inside `extern "C" { #include "variables.h" }` in
 * C++ ones — so anything reachable from here must not pull a C++ standard
 * header into C linkage. The row table, its accessors and the `MM_TRICK()`
 * predicate therefore live in `Tricks.h` / `Tricks.cpp`, which include this.
 *
 * ============================================================================
 * ATTRIBUTION
 * ============================================================================
 *
 * The trick KEYS in this file and the display names / tooltips in Tricks.cpp
 * are derived from OoTMM's `packages/core/src/settings/tricks.ts`:
 *
 *     MIT License
 *     Copyright (c) 2020-2022 OoTMM Team
 *     https://github.com/OoTMM/OoTMM  (LICENSE, verbatim MIT)
 *
 * Verified key-for-key against OoTMM `master` on 2026-09-20: 85 `MM_*` keys, this
 * enum's order identical to tricks.ts source order, every display name and
 * tooltip byte-equal to the JS-evaluated string (two of them, MM_BOMBER_GUESS and
 * MM_BOMBER_BACKFLIP, contain an invalid `\s` escape upstream that JavaScript
 * renders as a bare `s`, and they are transcribed as RENDERED — "Bombers
 * Hideout"). What is taken is the community-consensus
 * TAXONOMY — which tricks exist, what they are called, and what each one means —
 * which is the expensive part and the part two independent MM communities agree
 * on. What is NOT taken is any logic wiring: every binding of a key to a region
 * condition is hand-authored against 2ship's own `RandoRegionId` graph, because
 * all three reference graphs are structurally incompatible with it (#500's
 * portability assessment).
 *
 * EXPLICIT NON-SOURCE: **mm-rando (GPL-3.0)**. Its 458 distinct trick names are
 * the best available map of "what tricks does MM even have" and its two-axis
 * categorisation is the better design — and NOTHING from it crosses into this
 * tree, not a byte and not a verbatim name. A curated list of names and
 * tooltips is a creative compilation; "we only took the strings" is not a safe
 * position into a tree that is otherwise CC0 (2Ship) + MIT-family (SoH). It was
 * used as an un-copied coverage checklist only. Do not "helpfully" import from
 * it later (#578 §1, ADR 0010 D10).
 *
 * ============================================================================
 * THE SHAPE IS SoH's, NOT OoTMM's
 * ============================================================================
 *
 * OoTMM's row is flat: `{game, name, tooltip, glitch?}`. SoH's `TrickOption`
 * (games/oot/soh/Enhancements/randomizer/option.h) carries a two-axis model —
 * an AREA plus a TAG SET — which is what lets one menu idiom serve both halves
 * of the combo, and which ADR 0010 O9 asks for literally ("an `RT_*`-equivalent
 * option table"). So a row here is
 *
 *     { key, cvar, area, tag set, reserved flag, display name, tooltip, reason }
 *
 * The AREA axis is ours: MM has no area enum (its graph is keyed by
 * `RandoRegionId`, ~700 regions), so `MMRTA_*` below is a coarse Termina
 * taxonomy authored for this table and assigned per row by hand.
 *
 * The TAG axis is ALSO ours, and that is worth saying plainly rather than
 * letting it read as ported classification. OoTMM has exactly one boolean
 * (`glitch`), and zero of the 85 MM keys set it — all four `glitch: true`
 * entries in tricks.ts are `GLITCH_OOT_*`. So `MMRTT_GLITCH` has no members
 * today and exists for part 2 and for MM's own glitch vocabulary. The
 * difficulty rung on each row was assigned by one stated mechanical rule over
 * the row's OWN tooltip text, so it is reproducible and auditable rather than
 * invented per row:
 *
 *   EXPERT       the tooltip says "frame-perfect", "very difficult", or
 *                "tight timing"
 *   ADVANCED     it says "precise" (not "somewhat precise"), "mash",
 *                "tight jump slash", or "blind"
 *   INTERMEDIATE it says "somewhat precise", "careful", "clever",
 *                "specific angle", or "may help"
 *   NOVICE       it demands no execution at all — a pure logic relaxation
 *
 * Every row also carries `MMRTT_COMBO` exactly when it is `reserved` (below).
 * Re-deriving the rung from a better rule later is a table edit, not a design
 * change; the lock only requires that every row has a non-empty tag set.
 *
 * ============================================================================
 * RESERVED KEYS — PRESENT, DECLARED, AND INERT
 * ============================================================================
 *
 * 20 of the 85 OoTMM keys REQUIRE an OoT-side item on every leg their own
 * definition states (10 Hover Boots, 4 Iron Boots — one of those also needing
 * the Megaton Hammer — 3 Din's Fire, 2 Farore's Wind, 1 Boomerang). Those are
 * OoTMM-*combo* tricks: not expressible in MM-only terms until ADR 0010
 * increment 3's single bag puts OoT items in MM's hands. They are declared here
 * with `reserved = true` so increment 3 can light them up WITHOUT renumbering
 * the key space, and `IsTrickEnabled` returns false for them unconditionally —
 * a reserved key cannot be turned on, by the pane or by anything else, so a
 * reserved row can never silently parametrize a fill.
 *
 * A trick that merely MENTIONS an OoT item as one alternative among MM-only
 * legs is NOT reserved, because its MM-only leg is expressible now: e.g.
 * `MMRT_IKANA_PILLAR_ENTRANCE_FLOAT` (Deku Mask or Hover Boots),
 * `MMRT_GBT_CENTRAL_GEYSER` (Fire + Ice Arrows or an OoT spell),
 * `MMRT_GBT_FIRELESS` (Zora Mask or Adult Link). Nor is a trick that names an
 * OoT item as something it REMOVES (`MMRT_BIO_BABA_LUCK`, "without Zora Mask or
 * Iron Boots").
 *
 * #578's body quotes 22 reserved keys from the #500 inventory taken at OoTMM
 * `e64a8652`. Re-measured row by row at `master` on 2026-09-20 the set is 20,
 * and the four differences are all reclassifications, not upstream churn:
 *   - Din's Fire 2 -> 3: the inventory undercounted. `MMRT_IGOS_DINS`,
 *     `MMRT_STAGE_LIGHTS_DIN` and `MMRT_ALIENS_DIN` all require it.
 *   - Iron Boots 5 -> 4: `MMRT_BIO_BABA_LUCK` names Iron Boots as something the
 *     trick REMOVES the need for, so its MM-only leg is expressible now.
 *   - "Scarecrow" -> live: the only scarecrow trick is `MMRT_KEG_HOOKBUNNY`, and
 *     the scarecrows it uses are MM's own (Pierre), not OoT's.
 *   - "Adult Link" -> live: `MMRT_GBT_FIRELESS` offers Zora Mask as an
 *     alternative to Adult Link, so it has an MM-only leg.
 *
 * ============================================================================
 * APPEND-ONLY
 * ============================================================================
 *
 * `MMRandoTrickId` is APPEND-ONLY and `MMRT_MAX` is its sentinel, for the same
 * reason `RandoOptionId` is (StaticData/Options.cpp's tombstone comment): the
 * enumerator's NUMBER indexes `RandoSaveInfo::randoSaveTricks` in every written
 * MM rando save, and it is folded positionally into the profile identity string
 * that every paired arrival re-derives and compares. Renumbering shifts saved
 * values and refuses healthy pairs. Add at the end, before `MMRT_MAX`; never
 * reorder, never delete (tombstone instead).
 *
 * The 85 OoTMM keys are in tricks.ts source order; `MMRT_GBT_BOSS_KEY_ICE` is
 * OURS (it has no OoTMM equivalent) and is therefore last.
 *
 * ============================================================================
 * STORAGE, FREEZE, AND THE PREDICATE
 * ============================================================================
 *
 * Trick enablement is PER-FILE RANDO STATE, frozen at creation and read-only
 * afterwards (ADR 0004 §6 state 4; ADR 0010 §3.3). It lives in
 * `gSaveContext.save.shipSaveInfo.rando.randoSaveTricks[]` — an append-only
 * extension of the MM rando save block, indexed by `MMRT_*` — written once by
 * `Rando::Foreign::ResolvePairedProfile` from the `gRando.Tricks.*` CVars, the
 * same single resolution that writes `RANDO_SAVE_OPTIONS`. `RANDO_SAVE_OPTIONS`
 * is indexed by `RO_*` and is NOT touched: the two id spaces stay separate so
 * neither can renumber the other.
 *
 * Region conditions consult exactly one predicate, `MM_TRICK(MMRT_*)`, which
 * reads that frozen array and nothing else. It deliberately does NOT read the
 * CVars: a condition that read the authoring surface would let a post-creation
 * toggle change a live world's rules, which ADR 0010 §3.3 calls divergence to
 * refuse. Bounds-checked, so an out-of-range id reads false rather than walking
 * off the array.
 */
#ifndef RANDO_STATIC_DATA_TRICK_IDS_H
#define RANDO_STATIC_DATA_TRICK_IDS_H

/**
 * MM trick keys. APPEND-ONLY — see the header comment. Derived from OoTMM's
 * tricks.ts (MIT, see ATTRIBUTION above) with the `MM_` prefix replaced by
 * `MMRT_`, in tricks.ts source order, plus our own trailing key.
 */
typedef enum {
    MMRT_HIDDEN_GROTTOS,
    MMRT_LENS,
    MMRT_TUNICS,
    MMRT_DEKU_STICK_FIGHTING,
    MMRT_PALACE_BEAN_SKIP,
    MMRT_DARMANI_WALL,
    MMRT_NO_SEAHORSE,
    MMRT_ZORA_HALL_HUMAN,
    MMRT_ICELESS_IKANA,
    MMRT_ONE_MASK_STONE_TOWER,
    MMRT_ISTT_EYEGORE,
    MMRT_SCT_NOTHING,
    MMRT_GORON_BOMB_JUMP,
    MMRT_BOMBER_GUESS,
    MMRT_CAPTAIN_SKIP,
    MMRT_ISTT_ENTRY_JUMP,
    MMRT_HARD_HOOKSHOT,
    MMRT_PFI_BOAT_HOOK,
    MMRT_PALACE_GUARD_SKIP,
    MMRT_SHT_HOT_WATER,
    MMRT_SHT_STICKS_RUN,
    MMRT_SHT_PILLARLESS,
    MMRT_SHT_PILLAR_ROOM_HOOKSHOT,
    MMRT_KEG_EXPLOSIVES,
    MMRT_DOG_RACE_CHEST_NOTHING,
    MMRT_MAJORA_LOGIC,
    MMRT_SOUTHERN_SWAMP_SCRUB_HP_GORON,
    MMRT_SOUTHERN_SWAMP_SCRUB_HP_BOOMERANG,
    MMRT_GBC_COW_LIKELIKE_ELEVATOR,
    MMRT_ZORA_HALL_SCRUB_HP_NO_DEKU,
    MMRT_ZORA_HALL_DOORS,
    MMRT_IKANA_ROOF_PARKOUR,
    MMRT_IKANA_PILLAR_ENTRANCE_FLOAT,
    MMRT_IKANA_PILLAR_ENTRANCE_JUMP,
    MMRT_POST_OFFICE_GAME,
    MMRT_WELL_HSW,
    MMRT_ISTT_CHUCHU_LESS,
    MMRT_GBT_WATERWHEEL_GORON,
    MMRT_GBT_ENTRANCE_BOW,
    MMRT_OOB_MOVEMENT,
    MMRT_ST_UPDRAFTS,
    MMRT_ESCAPE_CAGE,
    MMRT_GBT_FAIRY2_HOOK,
    MMRT_GBT_CENTRAL_GEYSER,
    MMRT_BANK_ONE_WALLET,
    MMRT_BANK_NO_WALLET,
    MMRT_CLOCK_TOWER_WAIT,
    MMRT_WFT_RUPEES_ICE,
    MMRT_ISTT_RUPEES_GORON,
    MMRT_BOMBER_BACKFLIP,
    MMRT_NCT_TINGLE,
    MMRT_GBT_FIRELESS,
    MMRT_IGOS_DINS,
    MMRT_BIO_BABA_CHU,
    MMRT_BIO_BABA_LUCK,
    MMRT_WF_SHRINE_HOVERS,
    MMRT_WFT_LOBBY_HOVERS,
    MMRT_SOARING_ZORA,
    MMRT_SOARING_HOVERS,
    MMRT_LULLABY_SKIP_IRONS,
    MMRT_PATH_SNOWHEAD_HOVERS,
    MMRT_GBT_WATERWHEEL_HOVERS,
    MMRT_GBT_CENTER_POT_IRONS,
    MMRT_GBT_RED1_HOVERS,
    MMRT_GBT_GREEN2_UPPER_HOVERS,
    MMRT_GYORG_IRONS,
    MMRT_STT_LAVA_BLOCK_HOVERS,
    MMRT_ISTT_ENTRY_HOVER,
    MMRT_GYORG_POTS_DIVE,
    MMRT_STT_POT_BOMBCHU_DIVE,
    MMRT_STOCK_POT_WAIT,
    MMRT_STAGE_LIGHTS_DIN,
    MMRT_RANCH_FARORE,
    MMRT_EVAN_FARORE,
    MMRT_KEG_TRIAL_HEATLESS,
    MMRT_KEG_HOOKBUNNY,
    MMRT_KEG_HOVERBUNNY,
    MMRT_STT_LAVA_SWITCH_HAMMER,
    MMRT_HIVE_BOMBCHU,
    MMRT_TWINMOLD_BOW,
    MMRT_TWINMOLD_FIRE_AND_ICE,
    MMRT_KEG_RED_BOULDER,
    MMRT_GBT_BABA_ENTRY_BOMBCHU,
    MMRT_CAPE_LIKE_LIKE_BOMBCHU,
    MMRT_ALIENS_DIN,
    /*
     * OURS, no OoTMM equivalent (#578 finding (b), operator ruling 2026-09-17:
     * sync and gate). Upstream 2Ship commented the Great Bay Temple
     * compass-room boss-key connection out in its PR #1661 — "intended to be
     * done as a trick(my bad) ... until trick menu is implemented" — and our
     * snapshot predates that, so redship's Glitchless assumed a trick
     * upstream's does not. The edge is now a trick-gated disjunct under this
     * key, default off: the divergence stops being an accident without the
     * route being deleted.
     */
    MMRT_GBT_BOSS_KEY_ICE,
    MMRT_MAX,
} MMRandoTrickId;

/**
 * Coarse Termina areas for grouping the table in a menu. OURS — see the header
 * comment. Append-only as well, but only because the pane sorts by it; nothing
 * persists an `MMRTA_*` value.
 */
typedef enum {
    MMRTA_GENERAL,
    MMRTA_CLOCK_TOWN,
    MMRTA_TERMINA_FIELD,
    MMRTA_SOUTHERN_SWAMP,
    MMRTA_DEKU_PALACE,
    MMRTA_WOODFALL,
    MMRTA_WOODFALL_TEMPLE,
    MMRTA_MOUNTAIN_VILLAGE,
    MMRTA_SNOWHEAD,
    MMRTA_SNOWHEAD_TEMPLE,
    MMRTA_MILK_ROAD,
    MMRTA_ROMANI_RANCH,
    MMRTA_GREAT_BAY_COAST,
    MMRTA_ZORA_CAPE,
    MMRTA_ZORA_HALL,
    MMRTA_PIRATES_FORTRESS,
    MMRTA_GREAT_BAY_TEMPLE,
    MMRTA_IKANA_CANYON,
    MMRTA_IKANA_CASTLE,
    MMRTA_STONE_TOWER,
    MMRTA_STONE_TOWER_TEMPLE,
    MMRTA_INVERTED_STONE_TOWER_TEMPLE,
    MMRTA_MOON,
    MMRTA_MAX,
} MMRandoTrickArea;

/**
 * Tag bits. A row's tag set is a bitmask so it crosses into the combo pane as
 * one integer rather than a container. Every row carries exactly one difficulty
 * rung; `MMRTT_COMBO` marks the reserved rows; `MMRTT_GLITCH` mirrors OoTMM's
 * one boolean and has no members yet (see the header comment).
 */
typedef enum {
    MMRTT_NOVICE = 1 << 0,
    MMRTT_INTERMEDIATE = 1 << 1,
    MMRTT_ADVANCED = 1 << 2,
    MMRTT_EXPERT = 1 << 3,
    MMRTT_EXPERIMENTAL = 1 << 4,
    MMRTT_GLITCH = 1 << 5,
    MMRTT_COMBO = 1 << 6,
} MMRandoTrickTag;

#define MMRTT_DIFFICULTY_MASK (MMRTT_NOVICE | MMRTT_INTERMEDIATE | MMRTT_ADVANCED | MMRTT_EXPERT)

#endif // RANDO_STATIC_DATA_TRICK_IDS_H
