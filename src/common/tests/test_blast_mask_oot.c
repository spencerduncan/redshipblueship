/**
 * @file test_blast_mask_oot.c
 * @brief Locks for the Blast Mask in Ocarina of Time (#979, epic #975 P4; OoTMM
 *        blastMaskOot, sharedMaskBlast and blastMaskCooldown).
 *
 * Two rows, split by what each one needs:
 *
 *   blast-mask-oot-rule (CTest BlastMaskOoTRule, redship tier) -- THE RULES. The
 *   three tier-4 keys (gCombo.Rando.BlastMaskOoT, SharedMaskBlast and
 *   BlastMaskCooldown), labelled with OoTMM's names, off and Default by
 *   default, in their pinned value spaces; each reaches its own part of the
 *   resolved item rules and nothing else; the shared one is dropped while the
 *   extension is off (OoTMM's `cond`), so the resolver never makes a record the
 *   validity rule refuses; the cooldown resolves from its key into the record's
 *   byte; and a session that walked away from a file holding all three restores
 *   them by name at load (frozen wins at load, #781). Then OoTMM's cooldown table
 *   (frames per value, Default being Majora's Mask's own 310), MM's No Blast Mask
 *   Cooldown composing over it, and one blast per cooldown.
 *
 *   blast-mask-oot-items (CTest BlastMaskOoTItems, redship tier) -- THE ITEM.
 *   The custom ItemID after Roc's Feather and the tables that must cover it (the
 *   lock #977 left to P4): never SLOT()-indexed (past OoT_gItemSlots' 56
 *   entries), inside gItemAgeReqs and sItemActions (PLAYER_IA_NONE), with its own
 *   curated icon in OoT_gItemIcons' custom section and a child age row. Then
 *   ownership gated by the frozen rule (Combo_ForeignMask_Owned over OoT's
 *   registered source), the child trade cycle visiting it only then, the shared
 *   member's give (the flag and an empty trade slot), THE SHARED PAIR (both games
 *   register their Blast Mask at RSBS_XITEM_SHARED_BLAST and the bag composes
 *   one row from them only under the shared rule), and OoT's REAL logic
 *   crediting the mask as a child's explosive only while the rule is frozen on.
 *
 * Not lockable headless: the B press, the explosion and the glow (they need a
 * PlayState); the playtest covers them.
 *
 * Every key is spelled from the identity prefix rather than as a literal, so
 * the tier-4 classification scan sees only the manifest's spelling.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * like its neighbours; every symbol under test is C-linkage.
 */

#include "../combo_logic.h"
#include "../combo_settings_view.h"
#include "../combo_single_bag.h"
#include "../context.h"
#include "../curated_archives.h"
#include "../cvar_shared_keys.h"
#include "../foreign_items.h"
#include "../foreign_masks.h"
#include "../shared_resources.h"
#include "../test_runner.h"
#include "test_named_items.h"

#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>

#ifdef __cplusplus
extern "C" {
#endif
// games/oot/soh/oot_blast_mask_test.cpp
uint8_t OoT_BlastMaskTest_ItemId(void);
uint8_t OoT_BlastMaskTest_RocsFeatherId(void);
int OoT_BlastMaskTest_ItemSlotsCount(void);
uint32_t OoT_BlastMaskTest_ItemIconsCount(void);
const char* OoT_BlastMaskTest_Icon(uint8_t item);
int OoT_BlastMaskTest_AgeReq(uint8_t item);
int OoT_BlastMaskTest_ChildAge(void);
int OoT_BlastMaskTest_PoolId(void);
int OoT_BlastMaskTest_IsAdvancement(void);
void OoT_BlastMaskTest_SetOwned(int held);
int OoT_BlastMaskTest_Owned(void);
int OoT_BlastMaskTest_TradeSlot(void);
void OoT_BlastMaskTest_SetTradeSlot(uint8_t item);
int OoT_BlastMaskTest_Logic(int held, int child, int which);
// games/oot/soh/Enhancements/randomizer/BlastMaskOoT.cpp
void OoT_BlastMask_RegisterRuntime(void);
// games/oot/soh/oot_foreign_mask_test.cpp
void OoT_ForeignMaskTest_ArmLive(void);
void OoT_TradeCycleTest_Set(uint8_t item, uint16_t ownedBits);
uint8_t OoT_TradeCycleTest_Next(void);
uint8_t OoT_TradeCycleTest_FirstItem(void);
int OoT_TradeCycleTest_ChildTradeCount(void);
// z_kaleido_scope_PAL.c and z_player.c (#983, #984): the two tables' entry counts
// and sItemActions' raw entry.
extern const uint32_t gItemAgeReqsCount;
extern const uint32_t gItemActionsCount;
int32_t OoT_Player_ItemActionTableEntry(uint32_t item);
#ifdef __cplusplus
}
#endif

static int sBmFailures;

#define BM_CHECK(cond, ...)                           \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            sBmFailures++;                            \
        }                                             \
    } while (0)

namespace {

// The rule bits, spelled as #976 reserved them (literals, so the lock states the
// .redsave format rather than reading it back from the header under test).
const uint32_t kBmBlastOoT = 0x00000020u;
const uint32_t kBmSharedBlast = 0x00000800u;
const uint32_t kBmSettingsHash = 0x09790979u;
const uint32_t kBmProfileDigest = 0x0BADB1A5u;
const char* const kBmIconPath = "__OTR__icon_item_static_yar/gItemIconBlastMaskTex";

/** A paired world frozen with the shipped combo record and @p flags /
 *  @p cooldown as its item rules (test_cross_item_rules.c's XirFreezeWorld). */
void BmFreezeWorld(uint32_t flags, uint8_t cooldown) {
    ComboContext_Init();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x0979B1A5u;
    gComboCtx.sharedRandoSettingsHash = kBmSettingsHash;
    gComboCtx.mmProfileDigest = kBmProfileDigest;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    ComboItemRulesRecord rules;
    memset(&rules, 0, sizeof(rules));
    rules.crossItemFlags = flags;
    rules.blastMaskCooldown = cooldown;
    Combo_FreezeComboSettingsEx(&rec, &rules);
}

void BmClearKeys() {
    Combo_ComboSettingClear(COMBO_SETTING_BLAST_MASK_OOT);
    Combo_ComboSettingClear(COMBO_SETTING_SHARED_MASK_BLAST);
    Combo_ComboSettingClear(COMBO_SETTING_BLAST_MASK_COOLDOWN);
}

/** True when the embedded manifest carries `otrPath` ("__OTR__<path>") in the
 *  mm->oot half, by its path or a directory entry. */
bool BmCuratedIntoOoT(const char* otrPath) {
    static const char kOtr[] = "__OTR__";
    if (otrPath == nullptr || strncmp(otrPath, kOtr, sizeof(kOtr) - 1) != 0) {
        return false;
    }
    const std::string path(otrPath + sizeof(kOtr) - 1);
    std::istringstream in{ std::string(rsbs::curated::EmbeddedManifest()) };
    std::string line;
    static const std::string kDir = "mm->oot ";
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.compare(0, kDir.size(), kDir) != 0) {
            continue;
        }
        const std::string entry = line.substr(kDir.size());
        const bool dir = !entry.empty() && entry.back() == '/';
        if (dir ? path.compare(0, entry.size(), entry) == 0 : path == entry) {
            return true;
        }
    }
    return false;
}

} // namespace

// ============================================================================
// blast-mask-oot-rule
// ============================================================================

TestResult BlastMaskOoTRule_Run(void) {
    printf("[TEST] blast-mask-oot-rule: the Blast Mask (OoT), Shared Blast Mask and Blast Mask Cooldown keys author "
           "the item rules, the shared one only with its extension, and a file's values come back at load (#979)\n");
    sBmFailures = 0;
    BM_CHECK(Combo_ComboSettingStoreAvailable(), "no CVar store: every key leg below would pass on the defaults");
    ComboContext_Init();
    BmClearKeys();

    // ---- (1) The keys, their labels, defaults and value spaces -------------------
    {
        const std::string prefix = RSBS::kComboIdentityKeyPrefix;
        BM_CHECK(Combo_ComboSettingKey(COMBO_SETTING_BLAST_MASK_OOT) == prefix + "BlastMaskOoT" &&
                     Combo_ComboSettingKey(COMBO_SETTING_SHARED_MASK_BLAST) == prefix + "SharedMaskBlast" &&
                     Combo_ComboSettingKey(COMBO_SETTING_BLAST_MASK_COOLDOWN) == prefix + "BlastMaskCooldown",
                 "the keys are %s, %s and %s", Combo_ComboSettingKey(COMBO_SETTING_BLAST_MASK_OOT),
                 Combo_ComboSettingKey(COMBO_SETTING_SHARED_MASK_BLAST),
                 Combo_ComboSettingKey(COMBO_SETTING_BLAST_MASK_COOLDOWN));
        BM_CHECK(strcmp(Combo_ComboSettingLabel(COMBO_SETTING_BLAST_MASK_OOT), "Blast Mask (OoT)") == 0 &&
                     strcmp(Combo_ComboSettingLabel(COMBO_SETTING_SHARED_MASK_BLAST), "Shared Blast Mask") == 0 &&
                     strcmp(Combo_ComboSettingLabel(COMBO_SETTING_BLAST_MASK_COOLDOWN), "Blast Mask Cooldown") == 0,
                 "the rows are not labelled with OoTMM's names");
        BM_CHECK(Combo_ComboSettingDefault(COMBO_SETTING_BLAST_MASK_OOT) == 0 &&
                     Combo_ComboSettingDefault(COMBO_SETTING_SHARED_MASK_BLAST) == 0 &&
                     Combo_ComboSettingDefault(COMBO_SETTING_BLAST_MASK_COOLDOWN) == 0,
                 "every Blast Mask rule must ship off and Default (OoTMM's defaults; no world moves)");
        for (const ComboSettingId id : { COMBO_SETTING_BLAST_MASK_OOT, COMBO_SETTING_SHARED_MASK_BLAST }) {
            BM_CHECK(Combo_ComboSettingValueValid(id, 0) && Combo_ComboSettingValueValid(id, 1) &&
                         !Combo_ComboSettingValueValid(id, 2) && !Combo_ComboSettingValueValid(id, -1),
                     "'%s' is not exactly 0 or 1", Combo_ComboSettingKey(id));
        }
        for (int32_t v = -1; v <= 7; v++) {
            const bool want = v >= 0 && v <= 5;
            BM_CHECK(Combo_ComboSettingValueValid(COMBO_SETTING_BLAST_MASK_COOLDOWN, v) == want,
                     "cooldown %d judged %s", (int)v, want ? "INVALID" : "valid");
        }
    }

    // ---- (2) The resolver ---------------------------------------------------------
    {
        ComboItemRulesRecord r;
        Combo_ResolveComboItemRules(&r);
        BM_CHECK((r.crossItemFlags & (kBmBlastOoT | kBmSharedBlast)) == 0u && r.blastMaskCooldown == 0u,
                 "unset keys resolved flags %08X, cooldown %u", (unsigned)r.crossItemFlags,
                 (unsigned)r.blastMaskCooldown);

        BM_CHECK(Combo_ComboSettingSet(COMBO_SETTING_BLAST_MASK_OOT, 1) == 1, "the writer refused Blast Mask (OoT)");
        Combo_ResolveComboItemRules(&r);
        BM_CHECK((r.crossItemFlags & (kBmBlastOoT | kBmSharedBlast)) == kBmBlastOoT,
                 "Blast Mask (OoT) alone resolved %08X", (unsigned)r.crossItemFlags);

        BmClearKeys();
        BM_CHECK(Combo_ComboSettingSet(COMBO_SETTING_SHARED_MASK_BLAST, 1) == 1,
                 "the writer refused Shared Blast Mask");
        Combo_ResolveComboItemRules(&r);
        BM_CHECK((r.crossItemFlags & (kBmBlastOoT | kBmSharedBlast)) == 0u && Combo_ComboItemRulesValid(&r),
                 "the Shared Blast Mask without its extension resolved %08X; OoTMM's cond makes it off, and a "
                 "record with it alone is not a world (Combo_ComboItemRulesValid)",
                 (unsigned)r.crossItemFlags);

        BM_CHECK(Combo_ComboSettingSet(COMBO_SETTING_BLAST_MASK_OOT, 1) == 1, "the writer refused Blast Mask (OoT)");
        BM_CHECK(Combo_ComboSettingSet(COMBO_SETTING_BLAST_MASK_COOLDOWN, (int32_t)RSBS_BLAST_COOLDOWN_SHORT) == 1,
                 "the writer refused a cooldown in its space");
        BM_CHECK(Combo_ComboSettingSet(COMBO_SETTING_BLAST_MASK_COOLDOWN, 6) == 0,
                 "the writer took a cooldown outside its pinned space");
        Combo_ResolveComboItemRules(&r);
        BM_CHECK((r.crossItemFlags & (kBmBlastOoT | kBmSharedBlast)) == (kBmBlastOoT | kBmSharedBlast) &&
                     r.blastMaskCooldown == (uint8_t)RSBS_BLAST_COOLDOWN_SHORT && Combo_ComboItemRulesValid(&r),
                 "all three on resolved flags %08X cooldown %u", (unsigned)r.crossItemFlags,
                 (unsigned)r.blastMaskCooldown);
        BM_CHECK((Combo_ComboItemRuleKeyedBits() & (kBmBlastOoT | kBmSharedBlast)) == (kBmBlastOoT | kBmSharedBlast),
                 "the two rule bits are not keyed (%08X)", (unsigned)Combo_ComboItemRuleKeyedBits());
        BM_CHECK((Combo_ComboSettingsRestorableMask() & RSBS_COMBO_DIVERGE_BLAST_MASK_COOLDOWN) != 0u,
                 "the cooldown's divergence is not restorable through its key");
        BmClearKeys();
    }

    // ---- (3) Frozen wins at load ----------------------------------------------------
    {
        BmFreezeWorld(kBmBlastOoT | kBmSharedBlast, (uint8_t)RSBS_BLAST_COOLDOWN_LONG);
        const uint32_t bits = Combo_ComboSettingsDivergence();
        BM_CHECK(bits == (RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS | RSBS_COMBO_DIVERGE_BLAST_MASK_COOLDOWN),
                 "a session with every key unset diverges by %08X from a file holding all three", (unsigned)bits);
        BM_CHECK(Combo_ComboSettingsCanRestoreLive(&gComboCtx.comboSettings, &gComboCtx.comboItemRules, bits) == 1,
                 "the load cannot restore the file's Blast Mask rules");
        char names[160];
        ComboSettingsKeyUndo undo;
        const int ok = Combo_ComboSettingsRestoreLive(&gComboCtx.comboSettings, &gComboCtx.comboItemRules, bits, names,
                                                      sizeof(names), &undo);
        printf("[TEST] (3) restored: %s\n", names);
        BM_CHECK(ok == 1 && strcmp(names, "Blast Mask (OoT), Shared Blast Mask, Blast Mask Cooldown") == 0,
                 "the restore wrote '%s', not the three rows in page order", names);
        BM_CHECK(Combo_ComboSettingsDivergence() == 0u, "after the restore the pair still diverges by %08X",
                 (unsigned)Combo_ComboSettingsDivergence());
        Combo_ComboSettingsRestoreUndo(&undo);
        BM_CHECK(!Combo_ComboSettingIsExplicit(COMBO_SETTING_BLAST_MASK_OOT) &&
                     !Combo_ComboSettingIsExplicit(COMBO_SETTING_BLAST_MASK_COOLDOWN),
                 "the undo did not put the keys back to unset");
        ComboContext_Init();
        BmClearKeys();
    }

    // ---- (4) OoTMM's cooldown table and one blast per cooldown ----------------------
    {
        static const uint16_t kFrames[] = { 310u, 1u, 32u, 128u, 512u, 1024u };
        for (uint8_t v = 0; v <= (uint8_t)RSBS_BLAST_COOLDOWN_MAX; v++) {
            BM_CHECK(Combo_BlastMaskCooldownFrames(v) == kFrames[v], "cooldown %u is %u frames, OoTMM's is %u",
                     (unsigned)v, (unsigned)Combo_BlastMaskCooldownFrames(v), (unsigned)kFrames[v]);
            BM_CHECK(Combo_BlastMaskCooldownFor(v, 1) == 0u && Combo_BlastMaskCooldownFor(v, 0) == kFrames[v],
                     "MM's No Blast Mask Cooldown does not compose over cooldown %u", (unsigned)v);
        }
        BM_CHECK(Combo_BlastMaskCooldownFrames(6u) == 310u && Combo_BlastMaskCooldownFrames(255u) == 310u,
                 "a cooldown outside the pinned space must read as Default");
        struct Run {
            uint16_t arm;
            int frames;
            int want;
        };
        static const Run kRuns[] = { { 310u, 620, 2 }, { 310u, 621, 3 }, { 1u, 10, 10 }, { 0u, 10, 10 } };
        for (const Run& run : kRuns) {
            uint16_t left = 0;
            int fired = 0;
            for (int f = 0; f < run.frames; f++) {
                fired += Combo_BlastMask_Step(&left, 1, run.arm);
            }
            BM_CHECK(fired == run.want, "B pressed on each of %d frames with %u armed fired %d blast(s), not %d",
                     run.frames, (unsigned)run.arm, fired, run.want);
        }
        uint16_t left = 5;
        BM_CHECK(Combo_BlastMask_Step(&left, 0, 310u) == 0 && left == 4,
                 "an idle frame must count the cooldown down and fire nothing");
        BM_CHECK(Combo_BlastMask_Step(nullptr, 1, 310u) == 0, "a null cooldown must fire nothing");
    }

    printf("[TEST] blast-mask-oot-rule: %s\n", sBmFailures == 0 ? "PASS" : "FAIL");
    return sBmFailures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// blast-mask-oot-items
// ============================================================================

TestResult Test_BlastMaskOoTItems(void) {
    printf("[TEST] blast-mask-oot-items: the Blast Mask's custom ItemID stays inside OoT's tables and out of SLOT(), "
           "is owned and cycled only under the frozen rule, pairs into one bag row under the shared rule, and OoT's "
           "logic credits it only then (#979)\n");
    sBmFailures = 0;
    BM_CHECK(OoT_ComboLogic_TestEnsureItemTableTransient() == 0, "OoT's item table is not available");
    const uint8_t blast = OoT_BlastMaskTest_ItemId();
    const uint8_t rocs = OoT_BlastMaskTest_RocsFeatherId();

    // ---- (1) THE TABLE-BOUNDS LOCK for the custom ItemIDs ----------------------------
    {
        BM_CHECK(blast == (uint8_t)(rocs + 1u), "the Blast Mask is ItemID 0x%02X, not the one after Roc's (0x%02X)",
                 (unsigned)blast, (unsigned)rocs);
        OoT_BlastMask_RegisterRuntime();
        for (unsigned id = rocs; id <= blast; id++) {
            BM_CHECK((int)id >= OoT_BlastMaskTest_ItemSlotsCount(),
                     "custom ItemID 0x%02X is inside OoT_gItemSlots (%d entries): SLOT() would name a real slot", id,
                     OoT_BlastMaskTest_ItemSlotsCount());
            BM_CHECK(id < gItemAgeReqsCount, "custom ItemID 0x%02X is past gItemAgeReqs (%u)", id,
                     (unsigned)gItemAgeReqsCount);
            BM_CHECK(id < gItemActionsCount && OoT_Player_ItemActionTableEntry(id) == 0,
                     "custom ItemID 0x%02X is past sItemActions (%u) or not PLAYER_IA_NONE", id,
                     (unsigned)gItemActionsCount);
            const char* icon = OoT_BlastMaskTest_Icon((uint8_t)id);
            BM_CHECK(id < OoT_BlastMaskTest_ItemIconsCount() && icon != nullptr && icon[0] != '\0',
                     "custom ItemID 0x%02X has no OoT_gItemIcons row (%u entries)", id,
                     (unsigned)OoT_BlastMaskTest_ItemIconsCount());
        }
        const char* icon = OoT_BlastMaskTest_Icon(blast);
        BM_CHECK(icon != nullptr && strcmp(icon, kBmIconPath) == 0 && BmCuratedIntoOoT(icon),
                 "the Blast Mask's icon is '%s', not MM's own icon curated in OoT's half", icon ? icon : "(null)");
        BM_CHECK(BmCuratedIntoOoT("__OTR__item_name_static/gItemNameBlastMaskENGTex") &&
                     BmCuratedIntoOoT("__OTR__objects/object_mask_bakuretu/object_mask_bakuretu_DL_000440"),
                 "the Blast Mask's pause name or its glow pass is not curated in OoT's half");
        BM_CHECK(OoT_BlastMaskTest_AgeReq(blast) == OoT_BlastMaskTest_ChildAge(),
                 "the Blast Mask's age row is %d, not the child's %d", OoT_BlastMaskTest_AgeReq(blast),
                 OoT_BlastMaskTest_ChildAge());
        BM_CHECK(OoT_BlastMaskTest_IsAdvancement() == 1, "RG_BLAST_MASK is not an advancement item");
    }

    // ---- (2) Ownership, gated by the frozen rule --------------------------------------
    {
        OoT_ForeignMaskTest_ArmLive();
        OoT_BlastMask_RegisterRuntime();
        OoT_BlastMaskTest_SetOwned(1);
        ComboContext_Init();
        BM_CHECK(Combo_ForeignMask_Owned(GAME_OOT, COMBO_FOREIGN_MASK_MM_BLAST) == 0,
                 "with nothing frozen OoT owns the Blast Mask");
        BmFreezeWorld(0u, 0u);
        BM_CHECK(Combo_ForeignMask_Owned(GAME_OOT, COMBO_FOREIGN_MASK_MM_BLAST) == 0,
                 "a world frozen with the rule off owns the Blast Mask");
        BmFreezeWorld(kBmBlastOoT, 0u);
        BM_CHECK(Combo_ForeignMask_Owned(GAME_OOT, COMBO_FOREIGN_MASK_MM_BLAST) == 1,
                 "a world frozen with the rule on does not own the Blast Mask its flag says it holds");
        OoT_BlastMaskTest_SetOwned(0);
        BM_CHECK(Combo_ForeignMask_Owned(GAME_OOT, COMBO_FOREIGN_MASK_MM_BLAST) == 0,
                 "the rule alone owns the Blast Mask without its flag");
    }

    // ---- (3) The child trade cycle visits it only while owned under the rule ----------
    {
        const uint8_t first = OoT_TradeCycleTest_FirstItem();
        const int count = OoT_TradeCycleTest_ChildTradeCount();
        const uint8_t truth = (uint8_t)(first + count - 1);
        const uint16_t truthOnly = (uint16_t)(1u << (count - 1));
        OoT_BlastMaskTest_SetOwned(1);
        BmFreezeWorld(kBmBlastOoT, 0u);
        OoT_TradeCycleTest_Set(truth, truthOnly);
        BM_CHECK(OoT_TradeCycleTest_Next() == blast,
                 "after the Mask of Truth the cycle gave 0x%02X, not the Blast Mask",
                 (unsigned)OoT_TradeCycleTest_Next());
        BmFreezeWorld(0u, 0u);
        OoT_TradeCycleTest_Set(truth, truthOnly);
        BM_CHECK(OoT_TradeCycleTest_Next() == truth, "with the rule off the cycle still visited 0x%02X",
                 (unsigned)OoT_TradeCycleTest_Next());
        OoT_BlastMaskTest_SetOwned(0);
    }

    // ---- (4) The shared member's give: the flag and an empty child trade slot --------
    {
        ComboSharedFamilyMember oot;
        memset(&oot, 0, sizeof(oot));
        const bool registered = Combo_SharedFamilyMemberGet(GAME_OOT, kBmSharedBlast, &oot);
        BM_CHECK(registered && (int)oot.nativeId == OoT_BlastMaskTest_PoolId(),
                 "OoT's Blast Mask is not the Shared Blast Mask family's OoT member");
        if (registered && oot.give != nullptr && oot.held != nullptr) {
            OoT_ForeignMaskTest_ArmLive();
            BM_CHECK(!oot.held(oot.nativeId), "a fresh save holds the Blast Mask");
            oot.give(oot.nativeId);
            BM_CHECK(oot.held(oot.nativeId) && OoT_BlastMaskTest_Owned() == 1 &&
                         OoT_BlastMaskTest_TradeSlot() == (int)blast,
                     "the give left owned %d, trade slot 0x%02X", OoT_BlastMaskTest_Owned(),
                     (unsigned)OoT_BlastMaskTest_TradeSlot());
            OoT_ForeignMaskTest_ArmLive();
            OoT_BlastMaskTest_SetTradeSlot(OoT_TradeCycleTest_FirstItem());
            oot.give(oot.nativeId);
            BM_CHECK(OoT_BlastMaskTest_TradeSlot() == (int)OoT_TradeCycleTest_FirstItem(),
                     "the give replaced the child trade slot's own item");
        }
    }

    // ---- (5) THE SHARED PAIR: one bag row, only under the shared rule -----------------
    {
        SharedItem mmBlast;
        memset(&mmBlast, 0, sizeof(mmBlast));
        BM_CHECK(TestNamedItem((uint8_t)GAME_MM, "Blast Mask", &mmBlast), "MM has no item named \"Blast Mask\"");
        uint16_t mmId = 0;
        BM_CHECK(Combo_SharedFamilyMemberId(GAME_MM, kBmSharedBlast, &mmId) && mmId == mmBlast.id,
                 "MM's Blast Mask (%u) is not the Shared Blast Mask family's MM member (%u)", (unsigned)mmBlast.id,
                 (unsigned)mmId);
        ComboLogicSharedPair pairs[RSBS_XITEM_BIT_COUNT];
        BmFreezeWorld(kBmBlastOoT, 0u);
        int n = Combo_SingleBag_SharedPairs(pairs, (int)RSBS_XITEM_BIT_COUNT);
        BM_CHECK(n == 0, "the Blast Mask (OoT) rule alone built %d shared pair(s)", n);
        BmFreezeWorld(kBmBlastOoT | kBmSharedBlast, 0u);
        n = Combo_SingleBag_SharedPairs(pairs, (int)RSBS_XITEM_BIT_COUNT);
        printf("[TEST] (5) shared rule on: %d pair(s)%s", n, n > 0 ? ":" : "");
        for (int i = 0; i < n; i++) {
            printf(" (%u, %u)", (unsigned)pairs[i].ootId, (unsigned)pairs[i].mmId);
        }
        printf(" (expected 1: (%d, %u))\n", OoT_BlastMaskTest_PoolId(), (unsigned)mmBlast.id);
        BM_CHECK(n == 1 && (int)pairs[0].ootId == OoT_BlastMaskTest_PoolId() && pairs[0].mmId == mmBlast.id,
                 "the Shared Blast Mask did not compose exactly one row from OoT's and MM's Blast Masks");
        BM_CHECK((Combo_SharedFamiliesArmed() & kBmSharedBlast) == kBmSharedBlast,
                 "the shared rule did not arm its family");
    }

    // ---- (6) OoT's logic credits it as a child's explosive only under the rule --------
    {
        static const char* const kWhich[] = { "BlastOrSmash", "CanCutShrubs", "CanHitSwitch(close)",
                                              "CanKillEnemy(Beamos)", "CanUse" };
        BmFreezeWorld(0u, 0u);
        for (int w = 0; w < 5; w++) {
            const int got = OoT_BlastMaskTest_Logic(1, 1, w);
            BM_CHECK(got == 0, "rule off: %s answered %d for a child holding the Blast Mask", kWhich[w], got);
        }
        BmFreezeWorld(kBmBlastOoT, 0u);
        for (int w = 0; w < 5; w++) {
            const int child = OoT_BlastMaskTest_Logic(1, 1, w);
            const int adult = OoT_BlastMaskTest_Logic(1, 0, w);
            const int none = OoT_BlastMaskTest_Logic(0, 1, w);
            BM_CHECK(child == 1 && adult == 0 && none == 0,
                     "rule on: %s answered child %d, adult %d, child without it %d (expected 1, 0, 0)", kWhich[w],
                     child, adult, none);
        }
    }

    ComboContext_Init();
    OoT_ForeignMaskTest_ArmLive();
    printf("[TEST] blast-mask-oot-items: %s\n", sBmFailures == 0 ? "PASS" : "FAIL");
    return sBmFailures == 0 ? TEST_PASS : TEST_FAIL;
}
