/**
 * @file test_shared_items_class.c
 * @brief Locks for the single-owner item classification table (ADR 0010 answer
 *        O8; #645 increment 3, lane K5).
 *
 * The owner lives in src/common/shared_items.{h,c}; the two SOURCES are the
 * classify functions registered from the two combo-logic engine TUs
 * (ComboLogicEngineOoT.cpp, ComboLogicEngineSingleExe.cpp). This row drives the
 * REAL registered sources over the REAL item tables of both games — OoT's
 * `itemTable` (brought up display- and ROM-free by a bridge) and MM's
 * `Rando::StaticData::Items` (a static map) — so it runs in the display-free
 * `redship` tier with no generation.
 *
 * The claims, each with the defect it catches:
 *
 *  S1 REGISTRATION. Both sources are registered, and each is the engine TU's own
 *     classify function (pointer identity). Catches the registrar-elision class
 *     (#516/#678): a source whose registrar never ran leaves its game unclassified.
 *  S2 COVERAGE. Every id either source calls a fill item carries exactly one
 *     REAL class in the owner table; no fill item is unclassified; every class
 *     occurs in both games; and every real item-table row that is NOT a fill item
 *     is one of an exact, adjudicated handful (OoT: RG_TRIFORCE and RG_HINT —
 *     RG_NONE is not a real row to OoT's identity test; MM: RI_UNKNOWN, RI_NONE,
 *     RI_TRIFORCE_PIECE_PREVIOUS). MM's row oracle is a DIFFERENT TU's bridge
 *     (ForeignItemsSingleExe.cpp), so a source that silently dropped rows is
 *     caught by an observer that is not itself.
 *  S3 TRAPS ARE NEVER PROGRESSION, and the trap-first rule is load-bearing:
 *     every criterion-5 (reward, not punishment) exclusion of either foreign pool
 *     is TRAP in the owner (an independent, hand-adjudicated oracle in another
 *     TU); at least one trap IS advancement to its own fill (MM's RI_TRAP is
 *     RITYPE_LESSER), so deleting the trap-first rule turns this red; and no trap
 *     may cross under any armed set. The "PROGRESSION exactly where the fill says
 *     advancement" half is a PRECEDENCE check only: its oracle (the
 *     *_TestFillAdvancement bridges) evaluates the same predicate expression the
 *     classifier does, in the same TU, so it proves the classes are ordered
 *     trap > progression > filler over that predicate, NOT that the predicate is
 *     the right one. Choosing the predicate is argued in shared_items.h.
 *  S4 THE OWNER STORES AND RE-CHECKS WHAT ITS SOURCE SAID. For every id the
 *     public owner answer equals a direct call of the source, and
 *     Combo_ItemClassVerify reports zero divergences. Both sides are pure
 *     functions of static tables, so over the REAL sources this is a STORAGE
 *     check (the owner kept every row it was given), not two independent
 *     authorities agreeing. The disagreement it exists to catch is a source that
 *     is NOT pure; its RED HALVES are (a) in-row, a synthetic source whose answer
 *     changes between the build and the verify, and (b) in S6, the real sources
 *     re-verified after the frozen caps are published (red if a source reads live
 *     caps; mutation M7).
 *  S5 A SECOND REGISTRATION IS REFUSED. A synthetic source for an origin that
 *     already has one is refused and counted, the registered source is unchanged,
 *     and so is the table; the same holds for the SAME source registering twice,
 *     for GAME_NONE, for a malformed source on an empty origin, and for NULL —
 *     NULL is not an un-registration. The only way to remove a source is the
 *     test-only Combo_TestUnregisterItemClassSource, which is counted.
 *  S6 THE SETTINGS-CONDITIONAL PREDICATE. Each OoT key family carries exactly the
 *     ONE setting that confines it (dungeon small keys: keysanity; fortress keys:
 *     gerudo keys; dungeon boss keys: boss keysanity; Ganon's boss key: its own
 *     setting), with exact row counts, and none crosses under every OTHER bit
 *     armed; MM's rows carry no confinement family at all (MM has no restricted
 *     pass); MM's souls cross only once the frozen profile's give caps arm them,
 *     read through the published-caps surface; goal pieces never cross; and both
 *     real sources still Verify clean with the caps published — the condition is
 *     a predicate over the frozen record, not an input to the source.
 *  S7 THE FOREIGN POOLS AGREE. Every row of both registered foreign pools (the
 *     hand-adjudicated tables that already carry an itemClass) is PROGRESSION in
 *     the owner: a pool row is an item ADR 0011 lets cross, and the owner's
 *     predicate lets only progression cross, so a renewable, junk, trap or
 *     unclassified pool row is a disagreement.
 *
 * PROCESS STATE. AllTests runs every row in one process, so this row leaves what
 * it can as it found it: a scope guard restores the published give caps of both
 * origins and the real MM source on EVERY exit, failure paths included. One thing
 * is deliberately not undone: when this process never ran the OTR bring-up, the
 * OoT bridge creates a Rando::Context and fills OoT's static item table (the
 * same one-way bring-up the headless seed rows perform; the Context's Logic
 * back-edge keeps it alive). Both are idempotent for any later row: a row that
 * needs a Context finds one, and InitItemTable is what OTR bring-up would have
 * run anyway.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE and therefore
 * compiled as C++, like every other file in this directory.
 */

#include "../foreign_items.h"
#include "../game.h"
#include "../shared_items.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>

extern "C" {
// The two sources (engine TUs) and their test bridges.
int OoT_ComboLogic_ClassifyItem(uint16_t id, ComboItemClassRow* out);
int MM_ComboLogic_ClassifyItem(uint16_t id, ComboItemClassRow* out);
int OoT_ComboLogic_TestEnsureItemTable(void);
int OoT_ComboLogic_TestFillAdvancement(uint16_t id);
int MM_ComboLogic_TestFillAdvancement(uint16_t id);
// The foreign-pool TUs' adjudications (ForeignItemsSingleExe.cpp, both games).
int OoT_ForeignItem_TestExclusionAt(int index, uint16_t* outId, uint8_t* outCriterion);
int MM_ForeignItem_TestExclusionAt(int index, uint16_t* outId, uint8_t* outCriterion);
int MM_ForeignItem_TestIsJunkClassId(uint16_t riId);
}

#define SIC_ASSERT(cond, msg)                                                   \
    do {                                                                        \
        if (!(cond)) {                                                          \
            printf("[TEST] FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__);      \
            return TEST_FAIL;                                                   \
        }                                                                       \
    } while (0)

namespace {

SharedItem SicItem(uint8_t origin, uint16_t id) {
    SharedItem item;
    item.originGame = origin;
    item.flags = 0;
    item.id = id;
    return item;
}

const char* SicGameName(uint8_t origin) {
    return origin == (uint8_t)GAME_OOT ? "OoT" : "MM";
}

ComboItemClassifyFn SicRealClassify(uint8_t origin) {
    return origin == (uint8_t)GAME_OOT ? OoT_ComboLogic_ClassifyItem : MM_ComboLogic_ClassifyItem;
}

int SicFillAdvancement(uint8_t origin, uint16_t id) {
    return origin == (uint8_t)GAME_OOT ? OoT_ComboLogic_TestFillAdvancement(id) : MM_ComboLogic_TestFillAdvancement(id);
}

// An FNV digest over the whole stored table of one origin (class and arming word
// of every id), so "nothing moved" is an equality rather than an argument.
uint32_t SicTableDigest(uint8_t origin) {
    const ComboItemClassSource* src = Combo_GetItemClassSource(origin);
    uint32_t hash = 2166136261u;
    if (src == nullptr) {
        return 0;
    }
    for (uint32_t id = 0; id < src->idSpace; id++) {
        const SharedItem item = SicItem(origin, (uint16_t)id);
        const uint32_t word = ((uint32_t)Combo_ItemClassOf(item) << 24) ^ Combo_ItemClassArmedBy(item);
        for (int b = 0; b < 4; b++) {
            hash ^= (word >> (8 * b)) & 0xFFu;
            hash *= 16777619u;
        }
    }
    return hash;
}

// ---------------------------------------------------------------------------
// Synthetic sources for the red halves. A tiny id space; the flip-flop answers
// differently once `sSicFlip` is set, which is what a source reading live state
// would do between the owner's build and a later fill.
// ---------------------------------------------------------------------------
bool sSicFlip = false;

int SicFlipFlopClassify(uint16_t id, ComboItemClassRow* out) {
    ComboItemClassRow row = { RSBS_FILL_CLASS_NONE, 0u };
    if (id == 0u) {
        *out = row;
        return 0;
    }
    row.fillClass = (id == 3u && sSicFlip) ? RSBS_FILL_CLASS_JUNK : RSBS_FILL_CLASS_PROGRESSION;
    *out = row;
    return 1;
}

int SicTrapEverythingClassify(uint16_t id, ComboItemClassRow* out) {
    (void)id;
    out->fillClass = RSBS_FILL_CLASS_TRAP;
    out->armedBy = 0u;
    return 1;
}

const ComboItemClassSource kSicFlipFlopSource = { RSBS_ITEM_CLASS_SOURCE_ABI, 8u, SicFlipFlopClassify };
const ComboItemClassSource kSicIntruderSource = { RSBS_ITEM_CLASS_SOURCE_ABI, 8u, SicTrapEverythingClassify };
const ComboItemClassSource kSicBadAbiSource = { RSBS_ITEM_CLASS_SOURCE_ABI + 1u, 8u, SicFlipFlopClassify };
const ComboItemClassSource kSicTooWideSource = { RSBS_ITEM_CLASS_SOURCE_ABI, (uint16_t)(RSBS_ITEM_CLASS_ID_CAP + 1u),
                                                 SicFlipFlopClassify };

// ---------------------------------------------------------------------------
// S2 + S3 for one origin.
// ---------------------------------------------------------------------------
TestResult SicCoverageAndTraps(uint8_t origin, int expectedRealNonFillRows) {
    const ComboItemClassSource* src = Combo_GetItemClassSource(origin);
    SIC_ASSERT(src != nullptr, "source registered");
    const int fillItems = Combo_ItemClassBuild(origin);
    printf("[TEST] %s: id space %u, %d fill items\n", SicGameName(origin), (unsigned)src->idSpace, fillItems);
    // Anti-vacuity: both real tables hold a couple of hundred items; a source that
    // answered "not ready" or "no fill item" everywhere must not pass as 0 == 0.
    SIC_ASSERT(fillItems >= 200, "the owner table holds the whole item table, not a handful");

    int perClass[RSBS_FILL_CLASS_COUNT] = { 0 };
    int realNonFill = 0;
    int trapsTheFillCallsAdvancement = 0;
    for (uint32_t id = 0; id < src->idSpace; id++) {
        const SharedItem item = SicItem(origin, (uint16_t)id);
        ComboItemClassRow direct = { 0xFFu, 0xFFFFFFFFu };
        const int rv = src->classify((uint16_t)id, &direct);
        SIC_ASSERT(rv == 0 || rv == 1, "the source is ready and answers every id of its space");
        const uint8_t owned = Combo_ItemClassOf(item);
        SIC_ASSERT(owned < RSBS_FILL_CLASS_COUNT, "every stored class is a real enumerator");
        perClass[owned]++;

        // S2: a fill item carries exactly one REAL class; a non-fill id carries none.
        if (rv == 1) {
            if (owned == RSBS_FILL_CLASS_NONE) {
                printf("[TEST] %s id %u is a fill item with NO class\n", SicGameName(origin), (unsigned)id);
            }
            SIC_ASSERT(owned != RSBS_FILL_CLASS_NONE, "no fill item is unclassified");
        } else {
            SIC_ASSERT(owned == RSBS_FILL_CLASS_NONE, "a non-fill id carries no class");
        }

        // S2: the real-row oracle. For MM it is ForeignItemsSingleExe.cpp's bridge
        // (another TU); for OoT the fill-advancement bridge answers -1 exactly for
        // a non-row.
        const int realRow = origin == (uint8_t)GAME_MM ? (MM_ForeignItem_TestIsJunkClassId((uint16_t)id) != -1)
                                                       : (OoT_ComboLogic_TestFillAdvancement((uint16_t)id) != -1);
        if (realRow && rv == 0) {
            printf("[TEST] %s id %u: a real item row the source calls NOT a fill item\n", SicGameName(origin),
                   (unsigned)id);
            realNonFill++;
        }
        SIC_ASSERT(realRow || rv == 0, "the source classifies no id that has no item row");

        // S3: PROGRESSION exactly where the game's own fill says advancement and
        // the item is not a trap. A PRECEDENCE check: the oracle evaluates the same
        // predicate as the classifier (see the file header).
        if (rv == 1) {
            const int adv = SicFillAdvancement(origin, (uint16_t)id);
            SIC_ASSERT(adv == 0 || adv == 1, "a fill item has a fill-predicate answer");
            if (owned == RSBS_FILL_CLASS_PROGRESSION) {
                SIC_ASSERT(adv == 1, "PROGRESSION only where the game's own fill says advancement");
            }
            if (adv == 1 && owned != RSBS_FILL_CLASS_PROGRESSION) {
                SIC_ASSERT(owned == RSBS_FILL_CLASS_TRAP, "advancement that is not PROGRESSION is only ever a TRAP");
                printf("[TEST] %s id %u: advancement to its own fill, classed TRAP (trap-first rule)\n",
                       SicGameName(origin), (unsigned)id);
                trapsTheFillCallsAdvancement++;
            }
            if (owned == RSBS_FILL_CLASS_TRAP) {
                SIC_ASSERT(!Combo_ItemClassMayCrossUnder(item, 0xFFFFFFFFu), "a trap never crosses, armed or not");
            }
        }
    }
    printf("[TEST] %s classes: progression=%d junk=%d renewable=%d trap=%d none=%d; real rows that are not fill "
           "items=%d\n",
           SicGameName(origin), perClass[RSBS_FILL_CLASS_PROGRESSION], perClass[RSBS_FILL_CLASS_JUNK],
           perClass[RSBS_FILL_CLASS_RENEWABLE], perClass[RSBS_FILL_CLASS_TRAP], perClass[RSBS_FILL_CLASS_NONE],
           realNonFill);
    SIC_ASSERT(perClass[RSBS_FILL_CLASS_PROGRESSION] > 0 && perClass[RSBS_FILL_CLASS_JUNK] > 0 &&
                   perClass[RSBS_FILL_CLASS_RENEWABLE] > 0 && perClass[RSBS_FILL_CLASS_TRAP] > 0,
               "all four classes occur in each game (anti-vacuity)");
    SIC_ASSERT(Combo_ItemClassCount(origin, RSBS_FILL_CLASS_TRAP) == perClass[RSBS_FILL_CLASS_TRAP],
               "Combo_ItemClassCount agrees with a direct walk");
    // An item-table row a fill never draws must be ADJUDICATED, not dropped: a new
    // event or draw-only row upstream turns this red until someone decides it.
    SIC_ASSERT(realNonFill == expectedRealNonFillRows,
               "the real rows that are not fill items are exactly the adjudicated sentinel / event / draw-only rows");
    if (origin == (uint8_t)GAME_MM) {
        SIC_ASSERT(trapsTheFillCallsAdvancement >= 1,
                   "MM's RI_TRAP is non-junk to MM's own fill, so the trap-first rule decides at least one row");
    }

    // S3: every criterion-5 (REWARD) exclusion of this game's foreign pool is a
    // TRAP here — the pool TU's hand adjudication and this table name the same
    // punishments.
    int rewardExclusions = 0;
    for (int index = 0;; index++) {
        uint16_t excludedId = 0;
        uint8_t criterion = 0;
        const int more = origin == (uint8_t)GAME_OOT ? OoT_ForeignItem_TestExclusionAt(index, &excludedId, &criterion)
                                                     : MM_ForeignItem_TestExclusionAt(index, &excludedId, &criterion);
        if (!more) {
            break;
        }
        if (criterion == RSBS_FOREIGN_CRIT_REWARD) {
            rewardExclusions++;
            SIC_ASSERT(Combo_ItemClassOf(SicItem(origin, excludedId)) == RSBS_FILL_CLASS_TRAP,
                       "a criterion-5 exclusion is a TRAP in the owner table");
        }
    }
    SIC_ASSERT(rewardExclusions >= 1, "each foreign pool adjudicates at least one trap (anti-vacuity)");
    return TEST_PASS;
}

// S7 for one origin.
TestResult SicForeignPoolAgrees(uint8_t origin) {
    const ComboForeignItemDef* pool = nullptr;
    const int count = Combo_GetForeignItemPoolFor(origin, &pool);
    SIC_ASSERT(count > 0 && pool != nullptr, "the foreign pool is registered");
    int perClass[RSBS_FILL_CLASS_COUNT] = { 0 };
    for (int i = 0; i < count; i++) {
        const uint8_t cls = Combo_ItemClassOf(pool[i].item);
        SIC_ASSERT(cls < RSBS_FILL_CLASS_COUNT, "a pool row's class is a real enumerator");
        perClass[cls]++;
        if (cls != RSBS_FILL_CLASS_PROGRESSION) {
            printf("[TEST] %s pool row '%s' (id %u) is %s in the owner table\n", SicGameName(origin), pool[i].name,
                   (unsigned)pool[i].item.id, Combo_ItemClassName(cls));
        }
        // A pool row is an item ADR 0011 lets cross; the owner lets only
        // PROGRESSION cross (Combo_ItemClassMayCrossUnder). Anything else —
        // renewable included — is the two tables disagreeing about one item.
        SIC_ASSERT(cls == RSBS_FILL_CLASS_PROGRESSION, "every foreign-pool row is PROGRESSION in the owner table");
    }
    printf("[TEST] %s foreign pool (%d rows): progression=%d (every row)\n", SicGameName(origin), count,
           perClass[RSBS_FILL_CLASS_PROGRESSION]);
    return TEST_PASS;
}

// Restores, on EVERY exit of the row (an early SIC_ASSERT return included), the
// process state the row touches: the published give caps of both origins and the
// real MM source. See the file header's PROCESS STATE paragraph.
struct SicStateGuard {
    const ComboItemClassSource* realMM;
    bool capsPublished[2];
    uint32_t caps[2];

    SicStateGuard() : realMM(Combo_GetItemClassSource((uint8_t)GAME_MM)) {
        for (int i = 0; i < 2; i++) {
            const uint8_t origin = i == 0 ? (uint8_t)GAME_OOT : (uint8_t)GAME_MM;
            capsPublished[i] = Combo_ForeignGiveCapsPublished(origin);
            caps[i] = Combo_ForeignGiveCaps(origin);
        }
    }
    ~SicStateGuard() {
        if (realMM != nullptr && Combo_GetItemClassSource((uint8_t)GAME_MM) != realMM) {
            Combo_TestUnregisterItemClassSource((uint8_t)GAME_MM);
            if (Combo_RegisterItemClassSource((uint8_t)GAME_MM, realMM)) {
                printf("[TEST] scope guard: restored MM's real classification source on exit\n");
            } else {
                printf("[TEST] WARNING: could not restore MM's real classification source\n");
            }
        }
        Combo_ClearForeignGiveCaps();
        for (int i = 0; i < 2; i++) {
            if (capsPublished[i]) {
                Combo_PublishForeignGiveCaps(i == 0 ? (uint8_t)GAME_OOT : (uint8_t)GAME_MM, caps[i]);
            }
        }
    }
};

// One OoT confinement family for S6: rows whose arming word is exactly `bit`.
struct SicKeyFamily {
    uint32_t bit;
    int expectedProgressionRows;
    const char* name;
};

} // namespace

TestResult Test_SharedItemClass(void) {
    printf("[TEST] shared-item-class: the single-owner item classification table (ADR 0010 O8)\n");
    const SicStateGuard guard;

    // ---- S1: registration -------------------------------------------------
    const ComboItemClassSource* oot = Combo_GetItemClassSource((uint8_t)GAME_OOT);
    const ComboItemClassSource* mm = Combo_GetItemClassSource((uint8_t)GAME_MM);
    SIC_ASSERT(oot != nullptr, "OoT's source registered (ComboLogicEngineOoT.cpp registrar ran)");
    SIC_ASSERT(mm != nullptr, "MM's source registered (ComboLogicEngineSingleExe.cpp registrar ran)");
    SIC_ASSERT(oot->classify == OoT_ComboLogic_ClassifyItem, "OoT's source is the engine TU's classify");
    SIC_ASSERT(mm->classify == MM_ComboLogic_ClassifyItem, "MM's source is the engine TU's classify");
    SIC_ASSERT(Combo_GetItemClassSource((uint8_t)GAME_NONE) == nullptr, "GAME_NONE has no source");

    // OoT's item table is filled at OTR bring-up, which this tier never runs. Before
    // the bridge brings it up the source must say NOT READY and the owner must cache
    // nothing — the premature-query path.
    if (OoT_ComboLogic_ClassifyItem(1, nullptr) < 0) {
        SIC_ASSERT(Combo_ItemClassBuild((uint8_t)GAME_OOT) == -1, "a not-ready source builds nothing");
        SIC_ASSERT(Combo_ItemClassOf(SicItem((uint8_t)GAME_OOT, 1)) == RSBS_FILL_CLASS_NONE,
                   "a premature query answers NONE");
        printf("[TEST] OoT source reported not-ready before the item table existed; nothing was cached\n");
    }
    SIC_ASSERT(OoT_ComboLogic_TestEnsureItemTable() == 0, "OoT's item table is up");

    // ---- S2 + S3 ------------------------------------------------------------
    // OoT: RG_TRIFORCE and RG_HINT (RG_NONE is not a real row to the identity
    // test). MM: RI_UNKNOWN, RI_NONE, RI_TRIFORCE_PIECE_PREVIOUS.
    if (SicCoverageAndTraps((uint8_t)GAME_OOT, 2) != TEST_PASS) {
        return TEST_FAIL;
    }
    if (SicCoverageAndTraps((uint8_t)GAME_MM, 3) != TEST_PASS) {
        return TEST_FAIL;
    }

    // ---- S4: sources agree with the owner ----------------------------------
    for (uint8_t origin = (uint8_t)GAME_OOT; origin <= (uint8_t)GAME_MM; origin++) {
        const ComboItemClassSource* src = Combo_GetItemClassSource(origin);
        for (uint32_t id = 0; id < src->idSpace; id++) {
            ComboItemClassRow direct = { RSBS_FILL_CLASS_NONE, 0u };
            SicRealClassify(origin)((uint16_t)id, &direct);
            const SharedItem item = SicItem(origin, (uint16_t)id);
            SIC_ASSERT(Combo_ItemClassOf(item) == direct.fillClass && Combo_ItemClassArmedBy(item) == direct.armedBy,
                       "the owner's answer equals the source's for every id");
        }
        const int diverging = Combo_ItemClassVerify(origin);
        printf("[TEST] %s: Combo_ItemClassVerify = %d\n", SicGameName(origin), diverging);
        SIC_ASSERT(diverging == 0, "the real source and the owner table agree");
    }
    // Red half: a source that answers differently after the build IS reported.
    const uint32_t mmDigest = SicTableDigest((uint8_t)GAME_MM);
    uint32_t refusedBefore = Combo_ItemClassRefusedRegistrations();
    SIC_ASSERT(!Combo_RegisterItemClassSource((uint8_t)GAME_MM, nullptr),
               "NULL is refused, not an un-registration (no silent replace-in-two-steps)");
    SIC_ASSERT(Combo_ItemClassRefusedRegistrations() == refusedBefore + 1u, "the NULL refusal is counted");
    SIC_ASSERT(Combo_GetItemClassSource((uint8_t)GAME_MM) == mm, "a refused NULL leaves the source in place");
    const uint32_t unregBefore = Combo_ItemClassUnregistrations();
    SIC_ASSERT(Combo_TestUnregisterItemClassSource((uint8_t)GAME_MM), "un-register MM through the test-only door");
    SIC_ASSERT(Combo_ItemClassUnregistrations() == unregBefore + 1u, "the un-registration is counted");
    SIC_ASSERT(Combo_GetItemClassSource((uint8_t)GAME_MM) == nullptr, "un-registered");
    SIC_ASSERT(Combo_RegisterItemClassSource((uint8_t)GAME_MM, &kSicFlipFlopSource), "synthetic source accepted");
    sSicFlip = false;
    SIC_ASSERT(Combo_ItemClassBuild((uint8_t)GAME_MM) == 7, "synthetic table built (ids 1..7)");
    SIC_ASSERT(Combo_ItemClassVerify((uint8_t)GAME_MM) == 0, "an unchanged synthetic source agrees");
    sSicFlip = true;
    const int flipDivergence = Combo_ItemClassVerify((uint8_t)GAME_MM);
    printf("[TEST] red half: a source that changed its answer after the build -> Verify = %d\n", flipDivergence);
    SIC_ASSERT(flipDivergence == 1, "a diverging source is reported (red half observed)");
    SIC_ASSERT(Combo_ItemClassOf(SicItem((uint8_t)GAME_MM, 3)) == RSBS_FILL_CLASS_PROGRESSION,
               "the owner keeps ITS answer; the source's new one is only reported");
    sSicFlip = false;

    // ---- S5: a second registration is refused ------------------------------
    uint32_t refused = Combo_ItemClassRefusedRegistrations();
    SIC_ASSERT(!Combo_RegisterItemClassSource((uint8_t)GAME_MM, &kSicIntruderSource),
               "a second source for a held origin is refused (synthetic origin state)");
    SIC_ASSERT(Combo_ItemClassRefusedRegistrations() == refused + 1u, "the refusal is counted");
    SIC_ASSERT(Combo_GetItemClassSource((uint8_t)GAME_MM) == &kSicFlipFlopSource, "the held source is unchanged");
    SIC_ASSERT(Combo_TestUnregisterItemClassSource((uint8_t)GAME_MM), "un-register the synthetic source");
    SIC_ASSERT(!Combo_RegisterItemClassSource((uint8_t)GAME_MM, &kSicBadAbiSource), "a wrong ABI is refused");
    SIC_ASSERT(!Combo_RegisterItemClassSource((uint8_t)GAME_MM, &kSicTooWideSource),
               "an id space wider than the owner's storage is refused, not truncated");
    SIC_ASSERT(Combo_GetItemClassSource((uint8_t)GAME_MM) == nullptr, "a refused source is not installed");
    // Restore the REAL source and prove the table comes back identical.
    SIC_ASSERT(Combo_RegisterItemClassSource((uint8_t)GAME_MM, mm), "the real MM source re-registers");
    SIC_ASSERT(Combo_ItemClassVerify((uint8_t)GAME_MM) == 0, "restored MM source agrees");
    SIC_ASSERT(SicTableDigest((uint8_t)GAME_MM) == mmDigest, "the restored MM table is the one built before");
    SIC_ASSERT(Combo_ItemClassUnregistrations() == unregBefore + 2u, "both test un-registrations were counted");

    // Now against the REAL registered sources.
    const uint32_t ootDigest = SicTableDigest((uint8_t)GAME_OOT);
    refused = Combo_ItemClassRefusedRegistrations();
    SIC_ASSERT(!Combo_RegisterItemClassSource((uint8_t)GAME_OOT, &kSicIntruderSource),
               "an intruding source for OoT is refused");
    SIC_ASSERT(!Combo_RegisterItemClassSource((uint8_t)GAME_OOT, oot), "the SAME source registering twice is refused");
    SIC_ASSERT(!Combo_RegisterItemClassSource((uint8_t)GAME_NONE, &kSicIntruderSource), "GAME_NONE is refused");
    SIC_ASSERT(Combo_ItemClassRefusedRegistrations() == refused + 3u, "every refusal is counted");
    SIC_ASSERT(Combo_GetItemClassSource((uint8_t)GAME_OOT) == oot, "OoT's registered source is unchanged");
    SIC_ASSERT(SicTableDigest((uint8_t)GAME_OOT) == ootDigest, "OoT's table is unchanged by the refused intruder");
    SIC_ASSERT(Combo_ItemClassCount((uint8_t)GAME_OOT, RSBS_FILL_CLASS_TRAP) == 1,
               "OoT still has exactly its one trap (the intruder classes everything TRAP)");

    // ---- S6: the settings-conditional predicate ----------------------------
    // Each OoT key family carries exactly the ONE setting that confines it
    // (shared_items.h, "ONE SETTING PER CONFINEMENT BIT"). Exact counts: 8
    // dungeons with small keys x (key + ring); the fortress key and ring; the five
    // dungeon boss keys; Ganon's. The two treasure-game keys carry NO confinement
    // (RSK_SHUFFLE_CHEST_MINIGAME only decides presence), so folding them — or the
    // fortress keys — back into the keysanity family moves these counts.
    const SicKeyFamily ootKeyFamilies[] = {
        { RSBS_FILL_ARM_SMALL_KEYS_ROAM, 16, "dungeon small keys + key rings (RSK_KEYSANITY)" },
        { RSBS_FILL_ARM_GERUDO_KEYS_ROAM, 2, "fortress key + key ring (RSK_GERUDO_KEYS)" },
        { RSBS_FILL_ARM_BOSS_KEYS_ROAM, 5, "dungeon boss keys (RSK_BOSS_KEYSANITY)" },
        { RSBS_FILL_ARM_GANON_BOSS_KEY_ROAM, 1, "Ganon's boss key (RSK_GANONS_BOSS_KEY)" },
    };
    for (const SicKeyFamily& family : ootKeyFamilies) {
        int rows = 0;
        for (uint32_t id = 0; id < oot->idSpace; id++) {
            const SharedItem item = SicItem((uint8_t)GAME_OOT, (uint16_t)id);
            if (Combo_ItemClassOf(item) != RSBS_FILL_CLASS_PROGRESSION || Combo_ItemClassArmedBy(item) != family.bit) {
                continue;
            }
            rows++;
            SIC_ASSERT(!Combo_ItemClassMayCrossUnder(item, 0u), "a confined OoT key does not cross unarmed");
            SIC_ASSERT(Combo_ItemClassMayCrossUnder(item, family.bit), "an OoT key crosses once ITS setting roams");
            SIC_ASSERT(!Combo_ItemClassMayCrossUnder(item, 0xFFFFFFFFu & ~family.bit),
                       "no OTHER armed setting lets a confined key cross (e.g. keysanity=anywhere with gerudo keys "
                       "confined)");
        }
        printf("[TEST] OoT progression rows confined by %s: %d\n", family.name, rows);
        SIC_ASSERT(rows == family.expectedProgressionRows,
                   "each OoT key family is tagged with exactly its own setting (exact row count)");
    }

    Combo_ClearForeignGiveCaps();
    SIC_ASSERT(Combo_ItemClassArmedFromFrozen((uint8_t)GAME_MM) == 0u, "nothing published arms nothing");
    int mmSouls = 0;
    int mmUnconditional = 0;
    int goalPieces = 0;
    Combo_PublishForeignGiveCaps((uint8_t)GAME_OOT, RSBS_GIVECAP_ALL_V1);
    Combo_PublishForeignGiveCaps((uint8_t)GAME_MM, RSBS_GIVECAP_ALL_V1);
    const uint32_t mmArmed = Combo_ItemClassArmedFromFrozen((uint8_t)GAME_MM);
    SIC_ASSERT(mmArmed == RSBS_GIVECAP_ALL_V1, "published give caps arm exactly their families");
    SIC_ASSERT((mmArmed & (RSBS_FILL_ARM_WORLD_EVENT | RSBS_FILL_ARM_SHOP_STOCK)) == 0u,
               "no frozen record arms a goal quantity or a shop's stock");
    for (uint8_t origin = (uint8_t)GAME_OOT; origin <= (uint8_t)GAME_MM; origin++) {
        const ComboItemClassSource* src = Combo_GetItemClassSource(origin);
        for (uint32_t id = 0; id < src->idSpace; id++) {
            const SharedItem item = SicItem(origin, (uint16_t)id);
            const uint8_t cls = Combo_ItemClassOf(item);
            const uint32_t needs = Combo_ItemClassArmedBy(item);
            if (cls == RSBS_FILL_CLASS_JUNK || cls == RSBS_FILL_CLASS_RENEWABLE) {
                SIC_ASSERT(!Combo_ItemClassMayCrossUnder(item, 0xFFFFFFFFu), "filler never crosses");
            }
            if ((needs & RSBS_FILL_ARM_WORLD_EVENT) != 0u) {
                goalPieces++;
                SIC_ASSERT(!Combo_ItemClassMayCrossUnder(item, Combo_ItemClassArmedFromFrozen(origin)),
                           "a goal quantity never crosses under anything a frozen record publishes");
            }
            if (origin != (uint8_t)GAME_MM || cls != RSBS_FILL_CLASS_PROGRESSION) {
                continue;
            }
            SIC_ASSERT((needs & ~(RSBS_FILL_ARM_GIVECAPS_MASK | RSBS_FILL_ARM_WORLD_EVENT)) == 0u,
                       "MM rows carry no confinement family (MM's fill has no restricted pass)");
            if (needs == 0u) {
                mmUnconditional++;
                SIC_ASSERT(Combo_ItemClassMayCrossUnder(item, 0u), "an unconditional MM progression item crosses");
            }
            if (needs == RSBS_FILL_ARM_SOULS) {
                mmSouls++;
                SIC_ASSERT(!Combo_ItemClassMayCrossUnder(item, 0u), "an MM soul does not cross unarmed");
                SIC_ASSERT(Combo_ItemClassMayCrossUnder(item, mmArmed), "an MM soul crosses under published caps");
            }
        }
    }
    printf("[TEST] MM: %d unconditional progression rows, %d soul rows; goal-piece rows (both games): %d\n",
           mmUnconditional, mmSouls, goalPieces);
    SIC_ASSERT(mmSouls >= 40 && mmUnconditional >= 100 && goalPieces == 2, "the families are populated");
    // The stored table cannot move here (the owner builds once), so the check is
    // on the SOURCES: re-walked with the frozen caps published, each must give the
    // answer it gave when the table was built. A source that read the live caps
    // (baking the condition into the class) diverges here (mutation M7).
    for (uint8_t origin = (uint8_t)GAME_OOT; origin <= (uint8_t)GAME_MM; origin++) {
        const int diverging = Combo_ItemClassVerify(origin);
        printf("[TEST] %s: Combo_ItemClassVerify with the frozen caps published = %d\n", SicGameName(origin),
               diverging);
        SIC_ASSERT(diverging == 0,
                   "publishing the frozen caps changes no source answer: the condition is a predicate, not an input");
    }
    Combo_ClearForeignGiveCaps();

    // ---- S7: the foreign pools agree ---------------------------------------
    if (SicForeignPoolAgrees((uint8_t)GAME_OOT) != TEST_PASS || SicForeignPoolAgrees((uint8_t)GAME_MM) != TEST_PASS) {
        return TEST_FAIL;
    }

    printf("[TEST] shared-item-class: PASS\n");
    return TEST_PASS;
}
