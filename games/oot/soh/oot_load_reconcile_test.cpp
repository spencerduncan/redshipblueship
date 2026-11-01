/**
 * @file oot_load_reconcile_test.cpp
 * ROM-free, display-free lock for #1163 ("I went from 99 to 93 rupees going from
 * OoT to MM, and it stays at 93 when I go back") and the load leg of #1159
 * (heart pieces). CTest label "redship", row SharedPoolLoadReconcile in
 * CMake/SingleExecutable.cmake, dispatch "shared-pool-load-reconcile" in
 * src/common/test_runner.cpp, beside the shared-wallet row.
 *
 * WHAT WAS BROKEN. Nothing reconciled OoT's live save with the shared pool when
 * a file was loaded. The load drops every RAM watermark (save.cpp, context.cpp)
 * and OoT applied the pool only on a cross-game arrival (z_play.c), never on a
 * .redsave load. The first OoT harvest after the load found no watermark and an
 * occupied slot, so it seeded the watermark at OoT's live count with a zero
 * delta (shared_resources.c, the first-harvest seed):
 *   - form 1, the report: pool 93 and OoT 93 at load; OoT earns to 99; the
 *     crossing's harvest seeds at 99, delta 0, the pool stays 93; MM assigns 93
 *     and OoT's next arrival assigns 93;
 *   - form 2, a stale half: a save from Termina stores OoT's frozen half (99)
 *     beside a pool MM has spent from (93). The load lays that half over the
 *     .sav, so OoT shows 99 while the pool holds 93 until the first crossing,
 *     and any gain before OoT's first harvest is dropped the same way.
 * Every consumable kind was hit (rupees, current health, current magic, ammo)
 * and every monotonic tier stayed stale until a crossing: a heart piece found in
 * Termina left OoT's piece nibble behind, so OoT needed four more pieces of its
 * own for the next heart (#1159).
 *
 * THE SEAM UNDER TEST. OoT_Combo_OnLoadFileSeam (games/oot/soh/SaveManager.cpp),
 * the body of OoT's OnLoadFile hook, driven exactly as LoadFile drives it: the
 * .sav's base section in memory, the .sav's mirrored generation passed in, a
 * real .redsave on disk (the oot-check-state-load row's recipe). OoT's REAL
 * OoT_HarvestSharedResources reads the pool back; OoT's REAL Item_Give gives the
 * heart pieces.
 *
 * THE LEGS (each RED without the load reconcile unless marked GREEN):
 *   (1) the report: pool 93, OoT 93; after the load OoT earns 6 and harvests:
 *       the pool holds 99, not 93;
 *   (2) the stale half: the .redsave's newer OoT half at 99 beside a pool of 93
 *       (MM spent 6). After the load OoT shows 93, not 99; it earns 6 and the
 *       harvest leaves the pool at 99;
 *   (3) hearts (#1159): pool 52 quarters (3 hearts + 1 piece, the piece found in
 *       Termina), OoT's half 48 with no piece. After the load OoT's piece
 *       nibble is 1; three OoT pieces later the harvest leaves the pool at 64
 *       quarters (4 hearts), not 60;
 *   (4) the load floor: a file saved at one heart. The reconcile leaves OoT's
 *       current health at the pool's one heart, below the floor
 *       OoT_Sram_OpenSave applies after the seam (GREEN either way: the floor
 *       still sees its input and lifts it to 3 hearts); the floor is then an
 *       ordinary gain, so the next harvest leaves the pool at 3 hearts (RED
 *       without the reconcile: the harvest seeds at 3 hearts and the pool stays
 *       at one, which MM would hand back);
 *   (5) magic: the pool's current magic (half a meter after an MM spend) lands
 *       in OoT's `magic` field, not parked: the file select parks `magic` into
 *       magicFillTarget itself after the load (FileChoose_LoadGame), so an
 *       apply that parked at the seam would be overwritten with 0 and the next
 *       harvest would drain the pool. After that idiom the harvest leaves the
 *       pool at half a meter.
 *
 * WHAT THE ROW DOES NOT RUN. OoT_Sram_OpenSave and FileChoose_LoadGame need
 * OTRGlobals (Save_LoadFile resets its randomizer context), which no ROM-free
 * row has; legs (4) and (5) run their few lines after the seam as those
 * functions do, cited where they are written. Nor the receive hook's
 * fourth-piece wrap (hook_handlers.cpp, RandomizerOnItemReceiveHandler), which
 * needs a queued check: four pieces at a capacity and one heart more with no
 * pieces are the same quantity to the harvest (Combo_MakeHealthQuarters).
 *
 * The files are vanilla paired files (no randomizer section): the seam does not
 * depend on the quest, and the randomizer-only wallet rule is the shared-wallet
 * row's subject.
 */

#include <z64.h>

#include "context.h"
#include "game.h"
#include "save.h"
#include "shared_resources.h"

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/item-tables/ItemTableManager.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>
#include "test_scratch.h" // #824, #925: a scratch directory per process

extern "C" SaveContext gSaveContext;
// The seam (games/oot/soh/SaveManager.cpp).
extern "C" void OoT_Combo_OnLoadFileSeam(int32_t fileNum, uint32_t savGeneration,
                                         const RsbsOoTSavIdentity* savIdentity);
// OoT's real harvest (games/oot/soh/GameExports_SingleExe.cpp).
extern "C" void OoT_HarvestSharedResources(void);
// OoT's real give (z_parameter.c; safe with a NULL play).
extern "C" u8 OoT_Item_Give(PlayState* play, u8 item);

namespace {

inline const char* kSaveDir() {
    return rsbs::test::ScratchDirCStr("rsbs_test_shared_pool_load_reconcile");
}
constexpr int kSlot = 0;
constexpr uint32_t kHeartPieceShift = 28u; // OoT's piece nibble, questItems bits 28..31
constexpr uint32_t kHeartPieceMask = 0xF0000000u;

// File-static: SoH's runtime SaveContext is over 100 KB.
SaveContext sSav;
SaveContext sHalf;

int sFailures = 0;

#define SPLR_CHECK(cond, ...)                                      \
    do {                                                           \
        if (!(cond)) {                                             \
            printf("[TEST] FAIL (shared-pool-load-reconcile): ");  \
            printf(__VA_ARGS__);                                   \
            printf("  [%s] (%s:%d)\n", #cond, __FILE__, __LINE__); \
            sFailures++;                                           \
        }                                                          \
    } while (0)

/** A hook type's registry set aside for a scope. OoT_Item_Give dispatches
 *  OnItemReceive and OnTimestamp; inside AllTests an earlier row can leave
 *  gameplay hooks registered that read a play state this row does not have. */
template <typename H> struct HooksAside {
    decltype(GameInteractor::RegisteredGameHooks<H>::functions) functions;
    decltype(GameInteractor::RegisteredGameHooks<H>::functionsForFilter) filters;
    HooksAside() {
        functions.swap(GameInteractor::RegisteredGameHooks<H>::functions);
        filters.swap(GameInteractor::RegisteredGameHooks<H>::functionsForFilter);
    }
    ~HooksAside() {
        functions.swap(GameInteractor::RegisteredGameHooks<H>::functions);
        filters.swap(GameInteractor::RegisteredGameHooks<H>::functionsForFilter);
    }
};

/** One heart piece through OoT's own give, with no other hook listening. */
void GiveHeartPiece(void) {
    HooksAside<GameInteractor::OnItemReceive> receive;
    HooksAside<GameInteractor::OnTimestamp> timestamp;
    OoT_Item_Give(nullptr, ITEM_HEART_PIECE_2);
}

/** The pool's value for @p kind, or -1 when it has never been shared. */
int Pool(uint8_t kind) {
    uint16_t value = 0;
    if (!Combo_GetSharedResource(kind, &value)) {
        return -1;
    }
    return (int)value;
}

int Nibble(const SaveContext* s) {
    return (int)((s->inventory.questItems & kHeartPieceMask) >> kHeartPieceShift);
}

/** A vanilla OoT file in play: child Link, 3 hearts, nothing else. GAMEMODE_NORMAL
 *  because the file select sets it before OoT_Sram_OpenSave loads the file, and
 *  a frozen half is always captured in play. */
void BlankHalf(SaveContext* s) {
    memset(s, 0, sizeof(SaveContext));
    s->ship.quest.id = QUEST_NORMAL;
    s->fileNum = kSlot;
    s->gameMode = GAMEMODE_NORMAL;
    s->linkAge = LINK_AGE_CHILD;
    s->healthCapacity = 0x30;
    s->health = 0x30;
    s->magicState = MAGIC_STATE_IDLE;
}

using PoolAuthor = void (*)(void);

/**
 * The slot's record on disk. Commit 1 is OoT's own save of @p sav, the commit
 * the .sav mirrors, beside the pool @p atSav authors. Commit 2, when @p half is
 * given, is a save from Termina: OoT's frozen half @p half beside the pool
 * @p atHalf moves on from there. Returns the .sav's mirrored generation, or 0.
 */
uint32_t WriteRecord(const SaveContext& sav, PoolAuthor atSav, const SaveContext* half, PoolAuthor atHalf) {
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kSaveDir());
    Context_InitFrozenStates();
    Context_ClearAllFrozenStates();
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    mgr.DeleteSave(kSlot);
    mgr.ResetSlotSessionState();
    mgr.ArmSlotOnCreate(kSlot);
    mgr.SetActiveSlot(kSlot);

    // A vanilla paired file: no pairing identity and a blank Majora's Mask half
    // (the oot-check-state-load row's vanilla leg).
    static std::vector<uint8_t> sMMHalf(MM_SAVE_CONTEXT_SIZE, 0);
    std::fill(sMMHalf.begin(), sMMHalf.end(), (uint8_t)0);
    Context_UpdateShadowCopy(GAME_MM, sMMHalf.data(), sMMHalf.size());

    if (atSav != nullptr) {
        atSav();
    }
    Context_UpdateShadowCopy(GAME_OOT, &sav, sizeof(SaveContext));
    gComboCtx.sourceGame = GAME_OOT;
    if (RsbsSave_Save(kSlot) != 1) {
        return 0;
    }
    const uint32_t savGeneration = gComboCtx.commitGeneration;
    if (half != nullptr) {
        if (atHalf != nullptr) {
            atHalf();
        }
        Context_UpdateShadowCopy(GAME_OOT, half, sizeof(SaveContext));
        gComboCtx.sourceGame = GAME_MM;
        if (RsbsSave_Save(kSlot) != 1 || gComboCtx.commitGeneration != savGeneration + 1) {
            return 0;
        }
    }
    // The process ends; nothing cross-game survives in memory.
    ComboContext_Init();
    Context_ClearAllFrozenStates();
    Combo_ResetSharedResourceWatermarks();
    mgr.ResetSlotSessionState();
    mgr.SetActiveSlot(-1);
    return savGeneration;
}

/** LoadFile as OoT's file select runs it: the .sav's base section in memory, then
 *  the OnLoadFile seam. A new process holds no watermark. */
void LoadAsFileSelect(const SaveContext& sav, uint32_t savGeneration) {
    memcpy(&gSaveContext, &sav, sizeof(SaveContext));
    Combo_ResetSharedResourceWatermarks();
    OoT_Combo_OnLoadFileSeam(kSlot, savGeneration, nullptr);
}

// ---- pool authors: each is what a game's own harvest offers ----------------
void PoolRupees93(void) {
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, 93);
}
void PoolRupees99(void) {
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, 99);
}
void PoolMMSpends6Rupees(void) {
    // MM arrives holding the pool's 99 (its first harvest seeds there) and spends 6.
    Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_RUPEES, 99);
    Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_RUPEES, 93);
}
void PoolThreeHearts(void) {
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_HEALTH_QUARTERS, 48);
}
void PoolMMFindsAPiece(void) {
    Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_HEALTH_QUARTERS, 52);
}
void PoolOneHeartLeft(void) {
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_HEALTH_QUARTERS, 48);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_HEALTH_CURRENT, 0x10);
}
void PoolFullMeter(void) {
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_MAGIC_LEVEL, 1);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_MAGIC_CURRENT, 0x30);
}
void PoolMMSpendsHalfAMeter(void) {
    Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_MAGIC_CURRENT, 0x30);
    Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_MAGIC_CURRENT, 0x18);
}

int RunLegs(void) {
    // ======================================================================
    // (1) #1163 as reported: pool 93, OoT 93; OoT earns to 99 before its first
    // harvest after the load.
    // ======================================================================
    {
        BlankHalf(&sSav);
        sSav.rupees = 93;
        const uint32_t gen = WriteRecord(sSav, PoolRupees93, nullptr, nullptr);
        SPLR_CHECK(gen != 0, "leg 1 setup: the record is written");
        if (gen == 0) {
            return 1;
        }
        LoadAsFileSelect(sSav, gen);
        const int loaded = (int)gSaveContext.rupees;
        gSaveContext.rupees = (s16)(gSaveContext.rupees + 6); // the player earns 6
        OoT_HarvestSharedResources();                         // the crossing's harvest (or any save)
        printf("[TEST] shared-pool-load-reconcile: leg 1 (#1163): pool 93, OoT %d after the load; OoT earns 6 to %d; "
               "the harvest leaves the pool at %d\n",
               loaded, (int)gSaveContext.rupees, Pool(RSBS_SHARED_RES_RUPEES));
        SPLR_CHECK(loaded == 93, "leg 1: OoT shows %d after the load (want 93)", loaded);
        SPLR_CHECK(Pool(RSBS_SHARED_RES_RUPEES) == 99,
                   "#1163: rupees OoT earned after a load and before its first harvest are dropped: the pool holds %d "
                   "(want 99), so MM and OoT's next arrival hand back 93",
                   Pool(RSBS_SHARED_RES_RUPEES));
    }

    // ======================================================================
    // (2) #1163, the stale half: a save from Termina stored OoT's frozen half
    // (99) beside a pool MM spent from (93).
    // ======================================================================
    {
        BlankHalf(&sSav);
        sSav.rupees = 99;
        memcpy(&sHalf, &sSav, sizeof(SaveContext));
        const uint32_t gen = WriteRecord(sSav, PoolRupees99, &sHalf, PoolMMSpends6Rupees);
        SPLR_CHECK(gen != 0, "leg 2 setup: the two commits are written");
        if (gen == 0) {
            return 1;
        }
        LoadAsFileSelect(sSav, gen);
        const int loaded = (int)gSaveContext.rupees;
        gSaveContext.rupees = (s16)(gSaveContext.rupees + 6);
        OoT_HarvestSharedResources();
        printf("[TEST] shared-pool-load-reconcile: leg 2 (stale half): the record's OoT half 99, pool 93; OoT shows "
               "%d after the load; OoT earns 6; the harvest leaves the pool at %d\n",
               loaded, Pool(RSBS_SHARED_RES_RUPEES));
        SPLR_CHECK(loaded == 93,
                   "#1163: a file saved from Termina shows OoT's frozen half (%d rupees) instead of the pool's 93",
                   loaded);
        SPLR_CHECK(Pool(RSBS_SHARED_RES_RUPEES) == 99,
                   "#1163: after a stale-half load, 6 rupees earned before OoT's first harvest leave the pool at %d "
                   "(want 99)",
                   Pool(RSBS_SHARED_RES_RUPEES));
    }

    // ======================================================================
    // (3) #1159: a heart piece found in Termina, then three found in Hyrule.
    // ======================================================================
    {
        BlankHalf(&sSav); // 3 hearts, piece nibble 0
        memcpy(&sHalf, &sSav, sizeof(SaveContext));
        const uint32_t gen = WriteRecord(sSav, PoolThreeHearts, &sHalf, PoolMMFindsAPiece);
        SPLR_CHECK(gen != 0, "leg 3 setup: the two commits are written");
        if (gen == 0) {
            return 1;
        }
        LoadAsFileSelect(sSav, gen);
        const int nibble = Nibble(&gSaveContext);
        const int capacity = (int)gSaveContext.healthCapacity;
        for (int i = 0; i < 3; i++) {
            GiveHeartPiece();
        }
        const int quartersHeld =
            (int)Combo_MakeHealthQuarters((uint16_t)gSaveContext.healthCapacity, (uint16_t)Nibble(&gSaveContext));
        OoT_HarvestSharedResources();
        printf("[TEST] shared-pool-load-reconcile: leg 3 (#1159): pool 52 quarters, OoT's half 48 with no piece; after "
               "the load OoT holds capacity %d with %d piece(s); three OoT pieces later OoT holds %d quarters and the "
               "harvest leaves the pool at %d\n",
               capacity, nibble, quartersHeld, Pool(RSBS_SHARED_RES_HEALTH_QUARTERS));
        SPLR_CHECK(capacity == 0x30 && nibble == 1,
                   "#1159: the piece found in Termina is not in OoT after the load (capacity %d, nibble %d; want 48 "
                   "and 1)",
                   capacity, nibble);
        SPLR_CHECK(Pool(RSBS_SHARED_RES_HEALTH_QUARTERS) == 64,
                   "#1159: one Termina piece and three Hyrule pieces leave the pool at %d quarters (want 64, a whole "
                   "fourth heart)",
                   Pool(RSBS_SHARED_RES_HEALTH_QUARTERS));
    }

    // ======================================================================
    // (4) the load floor: a file saved at one heart.
    // ======================================================================
    {
        BlankHalf(&sSav);
        sSav.health = 0x10;
        const uint32_t gen = WriteRecord(sSav, PoolOneHeartLeft, nullptr, nullptr);
        SPLR_CHECK(gen != 0, "leg 4 setup: the record is written");
        if (gen == 0) {
            return 1;
        }
        LoadAsFileSelect(sSav, gen);
        const int atSeam = (int)gSaveContext.health;
        // OoT_Sram_OpenSave runs on after the seam (z_sram.c, "If health is less
        // than 3 hearts, give 3 hearts"; FullHealthSpawn off). The row cannot
        // call it: its Save_LoadFile needs OTRGlobals.
        if (gSaveContext.health < STARTING_HEALTH) {
            gSaveContext.health = STARTING_HEALTH;
        }
        const int afterFloor = (int)gSaveContext.health;
        OoT_HarvestSharedResources();
        printf("[TEST] shared-pool-load-reconcile: leg 4 (load floor): pool and file at one heart; OoT holds %d after "
               "the seam and %d after the load floor; the harvest leaves the pool's current health at %d\n",
               atSeam, afterFloor, Pool(RSBS_SHARED_RES_HEALTH_CURRENT));
        SPLR_CHECK(atSeam < STARTING_HEALTH && afterFloor == STARTING_HEALTH,
                   "the load floor no longer sees its input or no longer lifts it: %d at the seam, %d after the floor "
                   "(want below %d, then %d): a reconcile after OoT_Sram_OpenSave would undo the floor",
                   atSeam, afterFloor, STARTING_HEALTH, STARTING_HEALTH);
        SPLR_CHECK(Pool(RSBS_SHARED_RES_HEALTH_CURRENT) == STARTING_HEALTH,
                   "#1163 (current health): the load floor's 3 hearts never reach the pool (%d, want %d), so MM hands "
                   "back the one heart the file was saved with",
                   Pool(RSBS_SHARED_RES_HEALTH_CURRENT), STARTING_HEALTH);
    }

    // ======================================================================
    // (5) magic: half a meter left after an MM spend; the stale half holds a
    // full meter.
    // ======================================================================
    {
        BlankHalf(&sSav);
        sSav.isMagicAcquired = 1;
        sSav.magicLevel = 1;
        sSav.magicCapacity = 0x30;
        sSav.magic = 0x30;
        memcpy(&sHalf, &sSav, sizeof(SaveContext));
        const uint32_t gen = WriteRecord(sSav, PoolFullMeter, &sHalf, PoolMMSpendsHalfAMeter);
        SPLR_CHECK(gen != 0, "leg 5 setup: the two commits are written");
        if (gen == 0) {
            return 1;
        }
        LoadAsFileSelect(sSav, gen);
        const int atSeam = (int)gSaveContext.magic;
        // FileChoose_LoadGame runs after OoT_Sram_OpenSave (z_file_choose.c, the
        // same lines in the integration tier's file-select drive): it parks
        // `magic` for the regrow. The row cannot call it (OTRGlobals again).
        gSaveContext.magicCapacity = 0;
        gSaveContext.magicFillTarget = gSaveContext.magic;
        gSaveContext.magic = 0;
        gSaveContext.magicLevel = gSaveContext.magic;
        gSaveContext.magicState = MAGIC_STATE_IDLE;
        gSaveContext.prevMagicState = MAGIC_STATE_IDLE;
        OoT_HarvestSharedResources();
        printf("[TEST] shared-pool-load-reconcile: leg 5 (magic): the record's OoT half a full meter, pool half a "
               "meter; OoT's magic is %d after the seam (fill target %d after the file select parks it); the harvest "
               "leaves the pool's current magic at %d\n",
               atSeam, (int)gSaveContext.magicFillTarget, Pool(RSBS_SHARED_RES_MAGIC_CURRENT));
        SPLR_CHECK(atSeam == 0x18,
                   "OoT's magic after the load is %d (want the pool's 24 in `magic`, where the file select parks it "
                   "from: 48 is the stale half, 0 an apply that parked it at the seam)",
                   atSeam);
        SPLR_CHECK(Pool(RSBS_SHARED_RES_MAGIC_CURRENT) == 0x18,
                   "the first harvest after a load moved the pool's current magic to %d (want 24)",
                   Pool(RSBS_SHARED_RES_MAGIC_CURRENT));
    }
    return 0;
}

} // namespace

extern "C" int OoT_LoadReconcile_RunHeadless(void) {
    printf("[TEST] shared-pool-load-reconcile: loading a paired file reconciles OoT's live save with the shared pool "
           "before anything is earned or spent (#1163, #1159)\n");
    sFailures = 0;

    // OoT_Item_Give runs the OnItemReceive and OnTimestamp hooks, and its
    // vanilla item-table lookup needs an ItemTableManager. A ROM-free process
    // has neither (OTRGlobals creates the table manager); an empty MOD_NONE
    // table stands in (the oot-windows-gate row's recipe).
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    ItemTableManager* const savedItemTables = ItemTableManager::Instance;
    std::unique_ptr<ItemTableManager> harnessItemTables;
    if (ItemTableManager::Instance == nullptr) {
        harnessItemTables = std::make_unique<ItemTableManager>();
        harnessItemTables->AddItemTable(MOD_NONE);
        ItemTableManager::Instance = harnessItemTables.get();
    }

    const int setup = RunLegs();

    // Leave process state clean for whatever runs next, on pass or fail.
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.DeleteSave(kSlot);
    mgr.ResetSlotSessionState();
    mgr.SetActiveSlot(-1);
    mgr.SetSaveDirectory("Save");
    ComboContext_Init();
    Context_ClearAllFrozenStates();
    Combo_ResetSharedResourceWatermarks();
    memset(&gSaveContext, 0, sizeof(SaveContext));
    ItemTableManager::Instance = savedItemTables;

    if (setup != 0 || sFailures != 0) {
        printf("[TEST] FAIL (shared-pool-load-reconcile): %d failure(s)\n", sFailures);
        return 1;
    }
    printf("[TEST] PASS: loading a paired file reconciles OoT's live save with the shared pool\n");
    return 0;
}

/**
 * ROM-free lock for NULL-play in-place C/D-pad button upgrades inside
 * OoT_Item_Give (called by OoT_ForeignItem_Give with play == NULL during
 * cross-game arrival into OoT).
 *
 * Regression: ITEM_OCARINA_TIME upgraded gSaveContext.equips.buttonItems[i]
 * from ITEM_OCARINA_FAIRY to ITEM_OCARINA_TIME and called
 * Interface_LoadItemIcon1(play, i) without `if (play != NULL)` (z_parameter.c),
 * crashing with 0xc0000005 on arrival whenever the Fairy Ocarina was equipped.
 */
extern "C" int OoT_ForeignItem_TestNullPlayUpgradeGives(void) {
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    ItemTableManager* const savedItemTables = ItemTableManager::Instance;
    std::unique_ptr<ItemTableManager> harnessItemTables;
    if (ItemTableManager::Instance == nullptr) {
        harnessItemTables = std::make_unique<ItemTableManager>();
        harnessItemTables->AddItemTable(MOD_NONE);
        ItemTableManager::Instance = harnessItemTables.get();
    }

    HooksAside<GameInteractor::OnItemReceive> receive;
    HooksAside<GameInteractor::OnTimestamp> timestamp;

    int failures = 0;

    // 1. Child Link on arrival: Fairy Ocarina and Hookshot on live C-buttons
    //    and archived adultEquips C-buttons.
    memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.fileNum = 0;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.ship.quest.id = QUEST_RANDOMIZER;
    gSaveContext.linkAge = LINK_AGE_CHILD;
    gSaveContext.inventory.items[SLOT_OCARINA] = ITEM_OCARINA_FAIRY;
    gSaveContext.inventory.items[SLOT_HOOKSHOT] = ITEM_HOOKSHOT;
    gSaveContext.equips.buttonItems[1] = ITEM_OCARINA_FAIRY;
    gSaveContext.equips.buttonItems[2] = ITEM_HOOKSHOT;
    gSaveContext.adultEquips.buttonItems[3] = ITEM_OCARINA_FAIRY;
    gSaveContext.adultEquips.buttonItems[1] = ITEM_HOOKSHOT;

    OoT_Item_Give(nullptr, ITEM_OCARINA_TIME);
    OoT_Item_Give(nullptr, ITEM_LONGSHOT);

    if (gSaveContext.inventory.items[SLOT_OCARINA] != ITEM_OCARINA_TIME ||
        gSaveContext.equips.buttonItems[1] != ITEM_OCARINA_TIME ||
        gSaveContext.adultEquips.buttonItems[3] != ITEM_OCARINA_TIME) {
        printf("[TEST] FAIL: NULL-play ITEM_OCARINA_TIME give as child did not upgrade inventory/equips/adultEquips\n");
        failures++;
    }
    if (gSaveContext.inventory.items[SLOT_HOOKSHOT] != ITEM_LONGSHOT ||
        gSaveContext.equips.buttonItems[2] != ITEM_LONGSHOT ||
        gSaveContext.adultEquips.buttonItems[1] != ITEM_LONGSHOT) {
        printf("[TEST] FAIL: NULL-play ITEM_LONGSHOT give as child did not upgrade inventory/equips/adultEquips\n");
        failures++;
    }

    // 2. Adult Link: Fairy Ocarina and Hookshot on live C-buttons and archived
    //    childEquips C-buttons.
    memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.fileNum = 0;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.ship.quest.id = QUEST_RANDOMIZER;
    gSaveContext.linkAge = LINK_AGE_ADULT;
    gSaveContext.inventory.items[SLOT_OCARINA] = ITEM_OCARINA_FAIRY;
    gSaveContext.inventory.items[SLOT_HOOKSHOT] = ITEM_HOOKSHOT;
    gSaveContext.equips.buttonItems[2] = ITEM_OCARINA_FAIRY;
    gSaveContext.equips.buttonItems[3] = ITEM_HOOKSHOT;
    gSaveContext.childEquips.buttonItems[1] = ITEM_OCARINA_FAIRY;
    gSaveContext.childEquips.buttonItems[2] = ITEM_HOOKSHOT;

    OoT_Item_Give(nullptr, ITEM_OCARINA_TIME);
    OoT_Item_Give(nullptr, ITEM_LONGSHOT);

    if (gSaveContext.inventory.items[SLOT_OCARINA] != ITEM_OCARINA_TIME ||
        gSaveContext.equips.buttonItems[2] != ITEM_OCARINA_TIME ||
        gSaveContext.childEquips.buttonItems[1] != ITEM_OCARINA_TIME) {
        printf("[TEST] FAIL: NULL-play ITEM_OCARINA_TIME give as adult did not upgrade inventory/equips/childEquips\n");
        failures++;
    }
    if (gSaveContext.inventory.items[SLOT_HOOKSHOT] != ITEM_LONGSHOT ||
        gSaveContext.equips.buttonItems[3] != ITEM_LONGSHOT ||
        gSaveContext.childEquips.buttonItems[2] != ITEM_LONGSHOT) {
        printf("[TEST] FAIL: NULL-play ITEM_LONGSHOT give as adult did not upgrade inventory/equips/childEquips\n");
        failures++;
    }

    memset(&gSaveContext, 0, sizeof(SaveContext));
    ItemTableManager::Instance = savedItemTables;
    return failures == 0 ? 1 : 0;
}
