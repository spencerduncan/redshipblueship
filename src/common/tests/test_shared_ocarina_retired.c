/**
 * @file test_shared_ocarina_retired.c
 * @brief ROM-free lock for the RETIRED Shared Ocarina rule (#879, operator
 *        ruling 2026-10-09 item 1; it replaced #668's SharedOcarina row).
 *
 * CTest row SharedOcarinaRetired (label "redship"), dispatch
 * "shared-ocarina-retired".
 *
 * WHY THE RULE WENT. MM starts every paired world holding its Ocarina of Time
 * (2Ship's starting list, frozen at creation), so with the rule on the only
 * thing it ever did was carry that free instrument back to Hyrule as a Fairy
 * Ocarina no check awarded (#879's scoping comment, "The leak, exactly"). The
 * operator removed the option: the Cross-Game Rules row, its key and its effect
 * go; an older world whose frozen record carries the bit loads as before, with
 * the bit ignored (the #801 / #834 retirement shape).
 *
 * WHAT THIS ROW OWNS:
 *
 *   1. THE KIND IS NEVER ARMED. RSBS_SHARED_RES_OCARINA_TIER stays allocated
 *      (kind 18 is .redsave format, append-only), but
 *      Combo_SharedResourceKindArmed answers false for it whatever the frozen
 *      record's RSBS_COMBO_FLAG_SHARED_OCARINA bit says, frozen or not.
 *
 *   2. AN OLDER WORLD WITH THE BIT SET LOADS WITHOUT REFUSAL. The bit stays
 *      RESERVED in the record and in comboSettingsHash (the fingerprint still
 *      pins it), but no key authors it any more, so a session can hold no value
 *      that diverges from the file's: the arrival / load compare is healthy, and
 *      a bit flipped after the stamp is still fingerprint damage.
 *
 *   3. IT HARVESTS NOTHING AND APPLIES NOTHING. Both halves pass through the one
 *      arming gate, so neither game's ocarina reaches the pool, and a slot an
 *      older .redsave already holds (written while the rule was live) is inert:
 *      no apply reads it.
 *
 *   4. EVERY OTHER KIND IS UNTOUCHED by the bit.
 */

#define SOR_ASSERT(cond, msg)                   \
    do {                                        \
        if (!(cond)) {                          \
            printf("[TEST] FAIL: %s\n", (msg)); \
            ComboContext_Init();                \
            return TEST_FAIL;                   \
        }                                       \
    } while (0)

/** The pool's value for a kind, or -1 when the resource has never been shared
 *  (which is a DIFFERENT fact from "shared, and zero"). */
static int SharedOcarinaRetiredPool(uint8_t kind) {
    uint16_t value = 0;
    if (!Combo_GetSharedResource(kind, &value)) {
        return -1;
    }
    return (int)value;
}

/** Freeze a paired world's record the way a creation event (or a .redsave
 *  load) does, with the retired bit set or clear. This row runs with no
 *  Ship::Context, so no key exists and the live resolution is the shipped
 *  defaults: exactly a session that can no longer author the rule. */
static uint32_t SharedOcarinaRetiredFreeze(bool bit) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x0CA21A79u;
    gComboCtx.sharedRandoSettingsHash = 0x0CA5E879u;
    gComboCtx.mmProfileDigest = 0x4D4D0879u;

    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    rec.comboFlags = bit ? (uint8_t)RSBS_COMBO_FLAG_SHARED_OCARINA : 0u;
    return Combo_FreezeComboSettings(&rec);
}

TestResult Test_SharedOcarinaRetired(void) {
    printf("[TEST] shared-ocarina-retired: the Shared Ocarina rule is gone; an older world's bit loads and shares "
           "nothing (#879)\n");

    // ---- (0) The kind and the bit stay allocated (format, not behaviour) ---
    SOR_ASSERT((uint8_t)RSBS_SHARED_RES_OCARINA_TIER == 18u,
               "RSBS_SHARED_RES_OCARINA_TIER moved off 18 -- kinds are .redsave format: retire, never renumber");
    SOR_ASSERT(RSBS_COMBO_FLAG_SHARED_OCARINA == 0x01u,
               "RSBS_COMBO_FLAG_SHARED_OCARINA moved off 0x01 -- the bit stays RESERVED, older worlds carry it");

    // ---- (1) The kind is never armed, whatever the bit says ----------------
    {
        ComboContext_Init();
        const bool unfrozen = Combo_SharedResourceKindArmed(RSBS_SHARED_RES_OCARINA_TIER);
        SharedOcarinaRetiredFreeze(false);
        const bool frozenOff = Combo_SharedResourceKindArmed(RSBS_SHARED_RES_OCARINA_TIER);
        SharedOcarinaRetiredFreeze(true);
        const bool frozenOn = Combo_SharedResourceKindArmed(RSBS_SHARED_RES_OCARINA_TIER);
        printf("[TEST] shared-ocarina-retired: (1) ocarina kind armed: unfrozen=%d, record bit clear=%d, record bit "
               "set=%d (expected 0/0/0)\n",
               unfrozen ? 1 : 0, frozenOff ? 1 : 0, frozenOn ? 1 : 0);
        SOR_ASSERT(!unfrozen && !frozenOff && !frozenOn,
                   "(1) RSBS_SHARED_RES_OCARINA_TIER is armed -- the retired Shared Ocarina rule still shares an "
                   "instrument (#879)");
    }

    // ---- (2) A world created with the bit set loads without refusal --------
    {
        const uint32_t stamp = SharedOcarinaRetiredFreeze(true);
        ComboSettingsRecord plain;
        Combo_ComboSettingsDefaults(&plain);
        SOR_ASSERT(gComboCtx.comboSettings.comboFlags == (uint8_t)RSBS_COMBO_FLAG_SHARED_OCARINA,
                   "(2) the freeze dropped the older world's bit -- the record must keep its own bytes verbatim");
        SOR_ASSERT(stamp == Combo_ComputeComboSettingsHash(&gComboCtx.comboSettings, gComboCtx.sharedRandoSettingsHash,
                                                           gComboCtx.mmProfileDigest) &&
                       stamp != Combo_ComputeComboSettingsHash(&plain, gComboCtx.sharedRandoSettingsHash,
                                                               gComboCtx.mmProfileDigest),
                   "(2) the bit left the older world's identity -- comboSettingsHash must still pin it");

        // The session compare (every MM arrival) and the stored-identity compare
        // (the .redsave load, SaveManager::LoadSlot) both have to agree with
        // the file: no key authors the bit, so nothing can diverge from it.
        const uint32_t session = Combo_ComboSettingsDivergence();
        const uint32_t load = Combo_ComboSettingsDivergenceFor(
            &gComboCtx.comboSettings, stamp, gComboCtx.sharedRandoSettingsHash, gComboCtx.mmProfileDigest);
        char named[192];
        Combo_ComboSettingsDivergenceDescribe(session | load, named, sizeof(named));
        printf("[TEST] shared-ocarina-retired: (2) older world with the bit set: arrival divergence 0x%X, load "
               "divergence 0x%X (%s; expected none)\n",
               (unsigned)session, (unsigned)load, named);
        SOR_ASSERT(session == 0 && load == 0,
                   "(2) a world created with Shared Ocarina on is refused -- the retired rule's bit must load as "
                   "before, ignored (#879)");

        // The fingerprint still pins the bit: flipped after the stamp, it is
        // damage to the stored identity, exactly as before.
        gComboCtx.comboSettings.comboFlags = 0u;
        const uint32_t flipped = Combo_ComboSettingsDivergence();
        SOR_ASSERT((flipped & RSBS_COMBO_DIVERGE_FINGERPRINT) != 0 && Combo_ComboSettingsDivergenceIsDamage(flipped),
                   "(2) the bit changed after the stamp did not read as damage to the stored identity");
    }

    // ---- (3) It harvests nothing and applies nothing ------------------------
    {
        SharedOcarinaRetiredFreeze(true);
        Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_OCARINA_TIER, 1u); // MM's starting Ocarina of Time
        Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_OCARINA_TIER, 2u);
        const int pool = SharedOcarinaRetiredPool(RSBS_SHARED_RES_OCARINA_TIER);
        const int slots = Combo_CountSharedResources();
        uint16_t ootLive = 0u;
        const bool ootApplied = Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_OCARINA_TIER, 2u, &ootLive);
        printf("[TEST] shared-ocarina-retired: (3) bit set, both games harvested: pool=%d slots=%d; OoT apply=%d "
               "live=%u (expected -1/0/0/0)\n",
               pool, slots, ootApplied ? 1 : 0, (unsigned)ootLive);
        SOR_ASSERT(pool < 0 && slots == 0,
                   "(3) an ocarina harvest reached the pool -- MM's starting Ocarina would still arrive in Hyrule as a "
                   "Fairy Ocarina no check awarded (#879)");
        SOR_ASSERT(!ootApplied && ootLive == 0u, "(3) OoT's apply handed out an ocarina with no pool behind it");

        // An older .redsave written while the rule was live holds a slot. It is
        // inert: neither apply reads it, and a harvest does not grow it.
        SharedOcarinaRetiredFreeze(true);
        gComboCtx.sharedResources[0].kind = (uint8_t)RSBS_SHARED_RES_OCARINA_TIER;
        gComboCtx.sharedResources[0].flags = (uint8_t)RSBS_SHARED_RES_F_MONOTONIC;
        gComboCtx.sharedResources[0].value = 1u;
        uint16_t ootFromSlot = 0u;
        uint16_t mmFromSlot = 0u;
        const bool ootSlotApplied = Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_OCARINA_TIER, 2u, &ootFromSlot);
        const bool mmSlotApplied = Combo_ApplySharedResource(GAME_MM, RSBS_SHARED_RES_OCARINA_TIER, 1u, &mmFromSlot);
        Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_OCARINA_TIER, 2u);
        printf("[TEST] shared-ocarina-retired: (3) an older save's ocarina slot (1): OoT apply=%d live=%u, MM apply=%d "
               "live=%u, slot after an OoT harvest of 2 = %d (expected 0/0/0/0/1)\n",
               ootSlotApplied ? 1 : 0, (unsigned)ootFromSlot, mmSlotApplied ? 1 : 0, (unsigned)mmFromSlot,
               SharedOcarinaRetiredPool(RSBS_SHARED_RES_OCARINA_TIER));
        SOR_ASSERT(!ootSlotApplied && ootFromSlot == 0u && !mmSlotApplied && mmFromSlot == 0u,
                   "(3) an older save's ocarina slot was applied -- the retired rule's pool must be inert");
        SOR_ASSERT(SharedOcarinaRetiredPool(RSBS_SHARED_RES_OCARINA_TIER) == 1,
                   "(3) a harvest grew an older save's ocarina slot");
    }

    // ---- (4) Every other kind is untouched by the bit -----------------------
    {
        SharedOcarinaRetiredFreeze(true);
        Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_HOOKSHOT_TIER, 2u);
        uint16_t mmHookshot = 0u;
        SOR_ASSERT(SharedOcarinaRetiredPool(RSBS_SHARED_RES_HOOKSHOT_TIER) == 2 &&
                       Combo_ApplySharedResource(GAME_MM, RSBS_SHARED_RES_HOOKSHOT_TIER, 1u, &mmHookshot) &&
                       mmHookshot == 1u,
                   "(4) the retired bit suppressed the hookshot tier, which every world shares");
    }

    // AllTests runs every dispatch entry in ONE process, and this row freezes
    // gComboCtx and populates the pool. Leave it as it was found.
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();

    printf("[TEST] PASS: the ocarina kind is never armed; a world created with Shared Ocarina on loads healthy, keeps "
           "its bit, and shares nothing\n");
    return TEST_PASS;
}

#undef SOR_ASSERT
