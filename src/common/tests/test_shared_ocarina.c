/**
 * @file test_shared_ocarina.c
 * @brief ROM-free lock for the SHARED OCARINA tier (#668): one instrument
 *        across both games, armed by a frozen combo-level setting.
 *
 * CTest row SharedOcarina (label "redship"), dispatch "shared-ocarina".
 *
 * WHAT THIS ROW OWNS, and why it is not a leg of shared-resources. Every other
 * RSBS_SHARED_RES_* kind is unconditional: #525 decided that both games have
 * one wallet, one health bar and one hookshot, and nothing in the save can turn
 * that off. RSBS_SHARED_RES_OCARINA_TIER is the first kind whose existence is a
 * PLAYER'S CHOICE, frozen into ComboSettingsRecord.comboFlags at file creation
 * (ADR 0011 decision 4; one-game semantics). That makes two claims testable
 * here that are meaningless for the other seventeen:
 *
 *   1. WITH THE OPTION OFF NOTHING HAPPENS AT ALL -- no slot is created, no
 *      watermark is taken, no apply writes. This is the digest guarantee in its
 *      operational form: an existing world must reproduce byte for byte, and a
 *      pool slot that appears in the `.redsave` of a world that never asked for
 *      one is exactly the kind of silent change the freeze exists to prevent.
 *
 *   2. THE GATE IS SYMMETRIC. A one-sided gate corrupts: harvest and apply are
 *      not inverses (apply ASSIGNS for a consumable), so a kind that can be
 *      harvested but not applied -- or the reverse -- leaks in one direction.
 *      Combo_SharedResourceKindArmed is consulted by BOTH entry points, in
 *      src/common, once; this row drives both halves through it.
 *
 * THE TIER AND THE MAPPING (see foreign_items.h RSBS_COMBO_FLAG_SHARED_OCARINA):
 *   0 = no ocarina, 1 = OoT's Fairy Ocarina / MM's Ocarina of Time,
 *   2 = OoT's Ocarina of Time.
 * OoT's ceiling is 2 and MM's is 1, exactly the hookshot's 2-vs-1 shape, and
 * max-merge handles it natively: MM's single ocarina can never demote an
 * Ocarina of Time the pool already holds, and can never PROMOTE OoT past the
 * Fairy Ocarina either. The "never lowers" leg below is what distinguishes a
 * MONOTONIC tier from a CONSUMABLE one -- every other property the two share.
 */

#define SO_ASSERT(cond, msg)                    \
    do {                                        \
        if (!(cond)) {                          \
            printf("[TEST] FAIL: %s\n", (msg)); \
            ComboContext_Init();                \
            return TEST_FAIL;                   \
        }                                       \
    } while (0)

/** The pool's value for a kind, or -1 when the resource has never been shared
 *  (which is a DIFFERENT fact from "shared, and zero"). */
static int SharedOcarinaPool(uint8_t kind) {
    uint16_t value = 0;
    if (!Combo_GetSharedResource(kind, &value)) {
        return -1;
    }
    return (int)value;
}

/** Arm (or disarm) the option the way a creation event does: resolve the
 *  shipped defaults, set the flag, freeze. Freezing rather than writing a CVar
 *  is deliberate -- this row runs with no Ship::Context, and the frozen record
 *  is the surface every consumer actually reads once a world exists. */
static void SharedOcarinaFreezeOption(bool on) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x0CA21A00u;
    gComboCtx.sharedRandoSettingsHash = 0x0CA5E77Au;
    gComboCtx.mmProfileDigest = 0x4D4D0CA7u;

    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    rec.comboFlags = on ? (uint8_t)RSBS_COMBO_FLAG_SHARED_OCARINA : 0u;
    Combo_FreezeComboSettings(&rec);
}

TestResult Test_SharedOcarina(void) {
    printf("[TEST] shared-ocarina: one instrument across both games, armed by the frozen combo setting (#668)\n");

    // ---- (0) The kind exists and the watermark table covers it -------------
    // RSBS_SHARED_RES_KIND_COUNT sizes the RAM watermark table and bounds
    // IsRealKind; a kind appended without bumping it is silently dropped by
    // every harvest, which looks exactly like "the option is off".
    SO_ASSERT((uint8_t)RSBS_SHARED_RES_OCARINA_TIER == 18u,
              "RSBS_SHARED_RES_OCARINA_TIER moved off 18 -- these values are .redsave format, append-only");
    SO_ASSERT(RSBS_SHARED_RES_KIND_COUNT == 19u,
              "RSBS_SHARED_RES_KIND_COUNT does not cover the ocarina tier; the harvest would be dropped");

    // ---- (1) OPTION OFF: nothing crosses, and nothing is even recorded -----
    // The digest guarantee, operationally. Not merely "MM does not get an
    // ocarina": no slot, so the `.redsave` of a world that never asked for the
    // option is byte-identical to one written before the option existed.
    {
        SharedOcarinaFreezeOption(false);
        SO_ASSERT(!Combo_ComboSharedOcarina(), "a record with comboFlags 0 must report the option OFF");

        Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_OCARINA_TIER, 2u);
        SO_ASSERT(SharedOcarinaPool(RSBS_SHARED_RES_OCARINA_TIER) < 0,
                  "a disarmed harvest created a pool slot -- an unasked-for world just grew a shared resource");
        SO_ASSERT(Combo_CountSharedResources() == 0,
                  "a disarmed harvest occupied a shared-resource slot; with the option off nothing may change");

        uint16_t mmLive = 0;
        SO_ASSERT(!Combo_ApplySharedResource(GAME_MM, RSBS_SHARED_RES_OCARINA_TIER, 1u, &mmLive),
                  "a disarmed apply reported a shared value");
        SO_ASSERT(mmLive == 0, "a disarmed apply wrote the live value");
    }

    // ---- (2) OPTION ON, OoT -> MM: any OoT ocarina gives MM the instrument --
    // The operator's direction verbatim ("there should be an option to have the
    // ocarina be shared in both games"): tier 1 is the FAIRY ocarina, and MM
    // still ends up holding its only ocarina. MM's ceiling of 1 is what does the
    // mapping -- the tier crosses, never an item id.
    {
        SharedOcarinaFreezeOption(true);
        SO_ASSERT(Combo_ComboSharedOcarina(), "a record with the flag set must report the option ON");

        Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_OCARINA_TIER, 1u);
        SO_ASSERT(SharedOcarinaPool(RSBS_SHARED_RES_OCARINA_TIER) == 1,
                  "an armed OoT harvest of the Fairy Ocarina did not reach the pool");

        uint16_t mmLive = 0;
        SO_ASSERT(Combo_ApplySharedResource(GAME_MM, RSBS_SHARED_RES_OCARINA_TIER, 1u, &mmLive),
                  "an armed MM apply found no shared ocarina");
        SO_ASSERT(mmLive == 1, "MM must hold its Ocarina of Time after ANY OoT ocarina crosses");
    }

    // ---- (3) OPTION ON, MM -> OoT: the reverse leg, and the NON-promotion ---
    // MM's one ocarina maps onto OoT's LOWER rung. Mapping it onto the top rung
    // would hand a player who only ever found the Fairy Ocarina the Ocarina of
    // Time -- the Door of Time key -- for the price of a Termina round trip. The
    // shared model transports what the player HAS; it never invents an upgrade,
    // which is the same rule that keeps MM's tier-1 hookshot from becoming a
    // longshot in OoT.
    {
        SharedOcarinaFreezeOption(true);
        Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_OCARINA_TIER, 1u);
        SO_ASSERT(SharedOcarinaPool(RSBS_SHARED_RES_OCARINA_TIER) == 1, "an armed MM harvest did not reach the pool");

        uint16_t ootLive = 0;
        SO_ASSERT(Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_OCARINA_TIER, 2u, &ootLive),
                  "an armed OoT apply found no shared ocarina");
        SO_ASSERT(ootLive == 1,
                  "MM's ocarina promoted OoT past the Fairy Ocarina; the shared tier must never invent an upgrade");
    }

    // ---- (4) MONOTONIC: a lower harvest after a full apply never lowers -----
    // The ONLY property that distinguishes a MONOTONIC tier from a CONSUMABLE
    // one. Every other leg above would pass identically for a consumable, and a
    // consumable ocarina would let a Termina round trip walk an Ocarina of Time
    // back down to a Fairy Ocarina -- permanently, since nothing re-raises it.
    {
        SharedOcarinaFreezeOption(true);
        Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_OCARINA_TIER, 2u);
        SO_ASSERT(SharedOcarinaPool(RSBS_SHARED_RES_OCARINA_TIER) == 2, "OoT's Ocarina of Time did not reach the pool");

        uint16_t mmLive = 0;
        SO_ASSERT(Combo_ApplySharedResource(GAME_MM, RSBS_SHARED_RES_OCARINA_TIER, 1u, &mmLive),
                  "MM's apply found no shared ocarina");
        SO_ASSERT(mmLive == 1, "MM must clamp the pool's 2 to its own ceiling of 1");

        // MM now harvests what it actually holds -- 1 -- on the way out.
        Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_OCARINA_TIER, 1u);
        SO_ASSERT(SharedOcarinaPool(RSBS_SHARED_RES_OCARINA_TIER) == 2,
                  "an MM harvest of 1 demoted the pool's Ocarina of Time -- the tier is MONOTONIC, max-merge only");

        uint16_t ootLive = 1u; // the player left OoT holding only the Fairy Ocarina
        SO_ASSERT(Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_OCARINA_TIER, 2u, &ootLive),
                  "OoT's apply found no shared ocarina");
        SO_ASSERT(ootLive == 2, "the Ocarina of Time did not survive a Termina round trip");

        // And an apply never LOWERS a live value that already exceeds the pool.
        SharedOcarinaFreezeOption(true);
        Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_OCARINA_TIER, 1u);
        uint16_t ahead = 2u;
        SO_ASSERT(Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_OCARINA_TIER, 2u, &ahead) && ahead == 2,
                  "an apply lowered a live tier that was already above the pool");
    }

    // ---- (5) The slot records the MONOTONIC discipline ---------------------
    // The flags byte is descriptive rather than authoritative (`kind` is the
    // authority), but a save inspector and the netplay side both read it, and a
    // kind added to the enum without adding it to IsMonotonicKind's switch would
    // silently delta-harvest an ocarina.
    {
        SharedOcarinaFreezeOption(true);
        Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_OCARINA_TIER, 1u);
        bool found = false;
        for (int i = 0; i < (int)RSBS_SHARED_RESOURCE_TOTAL_CAP; i++) {
            const ComboSharedResource* entry = (i < (int)RSBS_SHARED_RESOURCE_CAP)
                                                   ? &gComboCtx.sharedResources[i]
                                                   : &gComboCtx.sharedResourcesExt[i - (int)RSBS_SHARED_RESOURCE_CAP];
            if (entry->kind == (uint8_t)RSBS_SHARED_RES_OCARINA_TIER) {
                found = true;
                SO_ASSERT((entry->flags & RSBS_SHARED_RES_F_MONOTONIC) != 0,
                          "the ocarina slot is not flagged MONOTONIC -- it would delta-harvest like a rupee count");
            }
        }
        SO_ASSERT(found, "no ocarina slot after an armed harvest");
    }

    // ---- (6) THE GATE IS SYMMETRIC, and it follows the FROZEN record -------
    // Turning the option off with a pool already present must stop BOTH halves.
    // A harvest-only gate would leave the apply writing an instrument into a
    // world whose rules say it has none; an apply-only gate would keep growing a
    // pool nothing reads. And the predicate must read the RECORD, not a live
    // resolution: after creation the rules are identity, and a session that
    // resolved differently is refused, never honoured.
    {
        SharedOcarinaFreezeOption(true);
        Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_OCARINA_TIER, 2u);
        SO_ASSERT(SharedOcarinaPool(RSBS_SHARED_RES_OCARINA_TIER) == 2, "the leg needs a populated pool");

        // Disarm in place, leaving the pool where it is: the shape a `.redsave`
        // written under the option ON would take if its record said OFF.
        gComboCtx.comboSettings.comboFlags = 0u;
        SO_ASSERT(!Combo_ComboSharedOcarina(), "the predicate did not follow the frozen record");

        uint16_t mmLive = 0;
        SO_ASSERT(!Combo_ApplySharedResource(GAME_MM, RSBS_SHARED_RES_OCARINA_TIER, 1u, &mmLive) && mmLive == 0,
                  "a disarmed apply still served a populated pool");
        Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_OCARINA_TIER, 1u);
        SO_ASSERT(SharedOcarinaPool(RSBS_SHARED_RES_OCARINA_TIER) == 2, "a disarmed harvest still wrote the pool");
    }

    // ---- (7) Every OTHER kind is unaffected by the flag --------------------
    // The gate is one kind's, not a global switch: a bug that keyed it on the
    // wrong comparison would silently disable the wallet and the health bar,
    // which no other row in this tier would notice (they all run with the option
    // in its default state).
    {
        SharedOcarinaFreezeOption(false);
        Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_HOOKSHOT_TIER, 2u);
        SO_ASSERT(SharedOcarinaPool(RSBS_SHARED_RES_HOOKSHOT_TIER) == 2,
                  "the shared-ocarina gate suppressed the hookshot tier");
        uint16_t mmHookshot = 0;
        SO_ASSERT(Combo_ApplySharedResource(GAME_MM, RSBS_SHARED_RES_HOOKSHOT_TIER, 1u, &mmHookshot) &&
                      mmHookshot == 1,
                  "the shared-ocarina gate suppressed the hookshot apply");
    }

    // AllTests runs every dispatch entry in ONE process, and this row freezes
    // gComboCtx and populates the pool. Leave it as it was found.
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();

    printf("[TEST] PASS: the ocarina is one shared monotonic instrument when the frozen setting arms it, and "
           "nothing at all when it does not\n");
    return TEST_PASS;
}

#undef SO_ASSERT
