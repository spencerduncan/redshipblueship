/**
 * @file mm_warp_songs_mm_test.cpp
 * MM's half of the warp-songs-in-MM locks (#1169 lane XW5; the items are
 * 2s2h/Rando/WarpSongsMM.cpp, the rules RSBS_XITEM2_SONG_MINUET_MM .. in
 * src/common/foreign_items.h, the copies rule and the ownership byte
 * src/common/warp_songs_mm.c).
 *
 * THE HELPERS the redship rows in src/common/tests/test_warp_songs_mm.c drive MM's
 * REAL give, conversion, item table, starting-item list, pool, logic macro and
 * freeze through. They only name MM's ids and put the live save in a known state;
 * every decision is the production code's. They live here because only an MM TU
 * has MM's enums, `gSaveContext`, Rando::StaticData and Logic.h in scope.
 *
 * THE RANDO ROW (`mm-warp-songs-pooled-only-when-rule-on`,
 * MM_WarpSongsPooled_RunHeadless): real paired generations (OoT's Generate, then
 * MM's creation-time half through the real OnSaveInit chain, mm-paired-start's
 * shape) with the rules off, on through the hidden-bit test seam
 * (Combo_ComboItemRules2RevealForTest, XW2's; production never calls it) and with
 * the shared rules on too.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include "global.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>

#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/Logic/Logic.h"
#include "2s2h/Rando/StaticData/StaticData.h"
#include "2s2h/Rando/WarpSongsMM.h"
#include "2s2h/ShipUtils.h"

#include "combo_logic.h"         // src/common: the coordinator's placement table
#include "combo_settings_view.h" // src/common: the warp-song keys
#include "context.h"             // src/common: gComboCtx, the frozen states
#include "foreign_items.h"       // src/common: the second rules word and its test seam
#include "warp_songs_mm.h"       // src/common: the ownership byte

extern "C" {
#include "variables.h"
#include "functions.h"
// 3drando/menu.cpp: the headless Generate button (freezes the pairing identity and
// the combo records, stops OoT's fill at its general pass).
int Rando_HeadlessSeedTest(const char* seedStr);
int OoT_ComboLogic_GeneralPassDeferred(void);
// mm_rando_gen_test.cpp: MM's creation-time half over that world.
int MM_Rando_HeadlessPairedHalf(void);
}

namespace {

constexpr RandoItemId kSongs[6] = { RI_SONG_MINUET,  RI_SONG_BOLERO,   RI_SONG_SERENADE,
                                    RI_SONG_REQUIEM, RI_SONG_NOCTURNE, RI_SONG_PRELUDE };

} // namespace

/** MM ids by index: 0..5 the six songs (Minuet .. Prelude), 6 RI_MASK_SPOOKY (the
 *  id before them), 7 RI_MAX_TRAP, 8 RI_MAX, 9 RI_WOODFALL_STRAY_FAIRY,
 *  10 RI_SONG_SOARING, 11 RI_JUNK, 12 RI_MASK_GERUDO; -1 past the table. */
extern "C" int MM_WarpSongsTest_Id(int which) {
    static const int kIds[] = { RI_SONG_MINUET,   RI_SONG_BOLERO,          RI_SONG_SERENADE, RI_SONG_REQUIEM,
                                RI_SONG_NOCTURNE, RI_SONG_PRELUDE,         RI_MASK_SPOOKY,   RI_MAX_TRAP,
                                RI_MAX,           RI_WOODFALL_STRAY_FAIRY, RI_SONG_SOARING,  RI_JUNK,
                                RI_MASK_GERUDO };
    return (which >= 0 && which < (int)(sizeof(kIds) / sizeof(kIds[0]))) ? kIds[which] : -1;
}

/** MM's live save as a cross-game MM session runs it: every slot empty, every
 *  RandoSaveInfo byte zero, human, the 0xFF session fileNum. */
extern "C" void MM_WarpSongsTest_ArmLive(void) {
    std::memset(&gSaveContext, 0, sizeof(SaveContext));
    std::memset(gSaveContext.save.saveInfo.inventory.items, ITEM_NONE,
                sizeof(gSaveContext.save.saveInfo.inventory.items));
    gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.fileNum = 0xFF;
    gSaveContext.flashSaveAvailable = true;
}

/** MM's REAL give (Rando::GiveItem) of item `ri`. */
extern "C" void MM_WarpSongsTest_Give(uint16_t ri) {
    Rando::GiveItem((RandoItemId)ri);
}

/** MM's REAL conversion (Rando::ConvertItem) of item `ri`, no check. */
extern "C" int MM_WarpSongsTest_Convert(uint16_t ri) {
    return (int)Rando::ConvertItem((RandoItemId)ri);
}

/** Does MM's REAL give of `ri` change any byte of MM's save? 1 when it does. The
 *  save is put back afterwards; the give's other effects are not. */
extern "C" int MM_WarpSongsTest_GiveTouchesSave(uint16_t ri) {
    auto before = std::make_unique<SaveContext>();
    std::memcpy(before.get(), &gSaveContext, sizeof(SaveContext));
    Rando::GiveItem((RandoItemId)ri);
    const int touched = std::memcmp(before.get(), &gSaveContext, sizeof(SaveContext)) != 0 ? 1 : 0;
    std::memcpy(&gSaveContext, before.get(), sizeof(SaveContext));
    return touched;
}

/**
 * One fact of `ri`'s item-table row, 1 or 0, by `fact`: 0 a RITYPE_MAJOR row (MM's
 * fill and the bag take it as progression), 1 no MM ItemId (ITEM_NONE: it reaches
 * no ItemId-indexed table), 2 no get-item row (GI_NONE), 3 no draw row (GID_NONE:
 * DrawItem's own case draws it), 4 its icon is MM's own song note
 * (MM_gItemIcons[ITEM_SONG_SOARING]). -1 with no row.
 */
extern "C" int MM_WarpSongsTest_RowFact(uint16_t ri, int fact) {
    const auto it = Rando::StaticData::Items.find((RandoItemId)ri);
    if (it == Rando::StaticData::Items.end()) {
        return -1;
    }
    switch (fact) {
        case 0:
            return it->second.randoItemType == RITYPE_MAJOR ? 1 : 0;
        case 1:
            return it->second.itemId == ITEM_NONE ? 1 : 0;
        case 2:
            return it->second.getItemId == GI_NONE ? 1 : 0;
        case 3:
            return it->second.drawId == GID_NONE ? 1 : 0;
        case 4: {
            const char* icon = Rando::StaticData::GetIconTexturePath((RandoItemId)ri);
            return (icon != nullptr && icon == (const char*)MM_gItemIcons[ITEM_SONG_SOARING]) ? 1 : 0;
        }
        default:
            return -1;
    }
}

/** The Items row's display name, or NULL. */
extern "C" const char* MM_WarpSongsTest_Name(uint16_t ri) {
    const auto it = Rando::StaticData::Items.find((RandoItemId)ri);
    return it == Rando::StaticData::Items.end() ? nullptr : it->second.name;
}

/** The name MM's texts and hints use (Rando::StaticData::GetItemName, with its
 *  article). */
extern "C" const char* MM_WarpSongsTest_TextName(uint16_t ri) {
    static std::string sName;
    sName = Rando::StaticData::GetItemName((RandoItemId)ri, true);
    return sName.c_str();
}

/** Is `ri`'s Items row junk (RITYPE_JUNK)? */
extern "C" int MM_WarpSongsTest_IsJunk(int ri) {
    const auto it = Rando::StaticData::Items.find((RandoItemId)ri);
    return (it != Rando::StaticData::Items.end() && it->second.randoItemType == RITYPE_JUNK) ? 1 : 0;
}

/**
 * MM's REAL logic macro CAN_PLAY_OOT_WARP_SONG (Logic.h) for `ri` on the live save
 * with the Ocarina of Time held when `ocarina`, and the ocarina buttons of
 * `buttons` obtained (bit 0 A, 1 C-up, 2 C-down, 3 C-left, 4 C-right; the
 * RANDO_INF flags Ocarina Button Shuffle sets), the rest not. Whatever rule the
 * caller froze and whatever the ownership byte holds. 1 or 0.
 */
extern "C" int MM_WarpSongsTest_CanPlay(uint16_t ri, int ocarina, uint32_t buttons) {
    INV_CONTENT(ITEM_OCARINA_OF_TIME) = ocarina != 0 ? ITEM_OCARINA_OF_TIME : ITEM_NONE;
    const RandoInf kButtons[5] = { RANDO_INF_OBTAINED_OCARINA_BUTTON_A, RANDO_INF_OBTAINED_OCARINA_BUTTON_C_UP,
                                   RANDO_INF_OBTAINED_OCARINA_BUTTON_C_DOWN, RANDO_INF_OBTAINED_OCARINA_BUTTON_C_LEFT,
                                   RANDO_INF_OBTAINED_OCARINA_BUTTON_C_RIGHT };
    // Written straight into the flag words (z_actor.c's Flags_SetRandoInf layout),
    // so no OnFlagSet hook runs for a probe.
    for (int b = 0; b < 5; b++) {
        const s32 flag = (s32)kButtons[b];
        if ((buttons >> b) & 1u) {
            gSaveContext.save.shipSaveInfo.rando.randoInf[flag >> 4] |= (1 << (flag & 0xF));
        } else {
            gSaveContext.save.shipSaveInfo.rando.randoInf[flag >> 4] &= ~(1 << (flag & 0xF));
        }
    }
    const RandoItemId song = (RandoItemId)ri;
    return CAN_PLAY_OOT_WARP_SONG(song) ? 1 : 0;
}

/**
 * MM's REAL pool (Rando::Logic::GeneratePools) over a fresh save info, every
 * option at its default and Glitchless logic (mm_tree_shuffle_test.cpp's shape),
 * under whatever rules the caller froze: how many copies of `ri` it holds. With
 * `startingItem` nonzero that item is the world's persisted starting list, as a
 * creation persists it. Seeded, so deterministic. -1 when `ri` has no row.
 */
extern "C" int MM_WarpSongsTest_PoolCopies(uint16_t ri, uint16_t startingItem) {
    if (Rando::StaticData::Items.find((RandoItemId)ri) == Rando::StaticData::Items.end()) {
        return -1;
    }
    auto info = std::make_unique<RandoSaveInfo>();
    std::memset(info.get(), 0, sizeof(RandoSaveInfo));
    for (auto& [randoOptionId, row] : Rando::StaticData::Options) {
        info->randoSaveOptions[randoOptionId] = row.defaultValue;
    }
    info->randoSaveOptions[RO_LOGIC] = RO_LOGIC_GLITCHLESS;
    if (startingItem != 0u) {
        std::vector<RandoItemId> start = { (RandoItemId)startingItem };
        Rando::SetStartingItemsInSave(*info, start);
    }
    std::vector<RandoCheckId> checkPool;
    std::vector<RandoItemId> itemPool;
    Ship_Random_Seed(0x1169A5u);
    Rando::Logic::GeneratePools(*info, checkPool, itemPool);
    int copies = 0;
    for (RandoItemId item : itemPool) {
        copies += item == (RandoItemId)ri ? 1 : 0;
    }
    return copies;
}

/**
 * The creation's starting grant for a world whose start list is `ri` alone: the
 * list persisted into the live save's rando info (Rando::SetStartingItemsInSave,
 * as OnFileCreate persists it), then read back and granted item by item exactly as
 * Rando::GrantStartingItems grants it (GiveItem(ConvertItem(item))). 1 when the
 * persisted list read back as `ri` alone.
 */
extern "C" int MM_WarpSongsTest_StartingGrant(uint16_t ri) {
    std::vector<RandoItemId> start = { (RandoItemId)ri };
    Rando::SetStartingItemsInSave(gSaveContext.save.shipSaveInfo.rando, start);
    const std::vector<RandoItemId> persisted = Rando::GetStartingItemsFromSave(gSaveContext.save.shipSaveInfo.rando);
    for (RandoItemId item : persisted) {
        Rando::GiveItem(Rando::ConvertItem(item));
    }
    return (persisted.size() == 1 && persisted[0] == (RandoItemId)ri) ? 1 : 0;
}

/** A departure's freeze of MM's live save and the arrival's restore of it
 *  (Context_FreezeState / Context_RestoreState). 1 when both took. */
extern "C" int MM_WarpSongsTest_FreezeAndRestore(void) {
    Context_FreezeState(GAME_MM, 0, &gSaveContext, sizeof(SaveContext));
    if (!Context_HasFrozenState(GAME_MM)) {
        return 0;
    }
    return Context_RestoreState(GAME_MM, &gSaveContext, sizeof(SaveContext)) != 0 ? 1 : 0;
}

// ============================================================================
// mm-warp-songs-pooled-only-when-rule-on (rando tier)
// ============================================================================

namespace {

int sWsFailures = 0;

void WsCheck(bool ok, const char* what) {
    if (!ok) {
        sWsFailures++;
        printf("[TEST] FAIL: %s\n", what);
    }
}

const ComboSettingId kExtensionKeys[6] = { COMBO_SETTING_SONG_MINUET_MM,   COMBO_SETTING_SONG_BOLERO_MM,
                                           COMBO_SETTING_SONG_SERENADE_MM, COMBO_SETTING_SONG_REQUIEM_MM,
                                           COMBO_SETTING_SONG_NOCTURNE_MM, COMBO_SETTING_SONG_PRELUDE_MM };
const ComboSettingId kSharedKeys[6] = { COMBO_SETTING_SHARED_SONG_MINUET,   COMBO_SETTING_SHARED_SONG_BOLERO,
                                        COMBO_SETTING_SHARED_SONG_SERENADE, COMBO_SETTING_SHARED_SONG_REQUIEM,
                                        COMBO_SETTING_SHARED_SONG_NOCTURNE, COMBO_SETTING_SHARED_SONG_PRELUDE };
// The rule bits, spelled as XW2 allocated them (.redsave format).
constexpr uint32_t kAllExtensions = 0x0000007Eu; // the six MM songs, not songSoaringOot
constexpr uint32_t kSharedMinuet = 0x00000100u;  // sharedSongMinuet

void ClearKeys() {
    for (int i = 0; i < 6; i++) {
        CVarClear(Combo_ComboSettingKey(kExtensionKeys[i]));
        CVarClear(Combo_ComboSettingKey(kSharedKeys[i]));
    }
    Combo_ComboItemRules2RevealForTest(0);
}

/** Every key a leg set is cleared and the seam closed however the row ends. */
struct WsRestore {
    ~WsRestore() {
        ClearKeys();
    }
};

int CountOf(const std::vector<RandoItemId>& items, RandoItemId id) {
    int n = 0;
    for (RandoItemId item : items) {
        n += item == id ? 1 : 0;
    }
    return n;
}

/** Copies of MM's `id` in the finished world: on an MM check, or crossed onto an
 *  OoT host by the single bag (the coordinator's table, still held). */
int PlacedInWorld(RandoItemId id) {
    int placed = 0;
    for (int rc = 0; rc < (int)RC_MAX; ++rc) {
        const RandoSaveCheck& check = RANDO_SAVE_CHECKS[rc];
        placed += (check.shuffled && !check.skipped && check.randoItemId == id) ? 1 : 0;
    }
    ComboLogicPlacement p;
    for (int i = 0; i < Combo_Logic_PlacementCount(GAME_OOT); ++i) {
        if (Combo_Logic_PlacementAt(GAME_OOT, i, &p) && p.item.originGame == (uint8_t)GAME_MM &&
            p.item.id == (uint16_t)id) {
            placed++;
        }
    }
    return placed;
}

/**
 * One leg: a paired creation with the six extension keys on when `extensions`, the
 * Shared Minuet of Forest too when `sharedMinuet`, through the seam when either is
 * on. Checks the frozen word, MM's pool over the half's own persisted input (one
 * copy of each song whose rule is on and whose shared song is off), the world (every
 * pool copy placed) and the ownership byte (nothing owned: the world starts with no
 * warp song, and the bag's simulated rounds gave none to the player). 1 when the
 * creation ran.
 */
int RunLeg(const char* name, const char* seed, bool extensions, bool sharedMinuet) {
    ComboContext_Init();
    ClearKeys();
    if (extensions) {
        for (int i = 0; i < 6; i++) {
            CVarSetInteger(Combo_ComboSettingKey(kExtensionKeys[i]), 1);
        }
    }
    if (sharedMinuet) {
        CVarSetInteger(Combo_ComboSettingKey(kSharedKeys[0]), 1);
    }
    // THE TEST SEAM: the hidden warp-song rules read as offered, as they will once
    // lane XW12 lands, so Generate (and the half's own compares) take the world.
    Combo_ComboItemRules2RevealForTest(extensions || sharedMinuet ? 1 : 0);
    int rc = Rando_HeadlessSeedTest(seed);
    const uint32_t frozen = Combo_ComboCrossItemFlags2();
    printf("[TEST] mm-warp-songs-pooled-only-when-rule-on: %s: generation %d, frozen second word %08X\n", name, rc,
           (unsigned)frozen);
    if (rc != 0 || !Combo_ForeignPairingActive() || !Combo_ComboSettingsFrozen() ||
        !OoT_ComboLogic_GeneralPassDeferred()) {
        printf("[TEST] FAIL: %s: the paired OoT generation did not leave a frozen paired world at its general pass\n",
               name);
        sWsFailures++;
        ClearKeys();
        return 0;
    }
    const uint32_t wantWord = (extensions ? kAllExtensions : 0u) | (sharedMinuet ? kSharedMinuet : 0u);
    WsCheck(frozen == wantWord, "the creation froze a second rules word other than the leg's keys");
    rc = MM_Rando_HeadlessPairedHalf();
    if (rc != 0) {
        printf("[TEST] FAIL: %s: MM's creation-time half did not produce a rando world (rc=%d)\n", name, rc);
        sWsFailures++;
        ClearKeys();
        return 0;
    }

    auto info = std::make_unique<RandoSaveInfo>();
    std::memcpy(info.get(), &gSaveContext.save.shipSaveInfo.rando, sizeof(RandoSaveInfo));
    std::vector<RandoCheckId> checkPool;
    std::vector<RandoItemId> itemPool;
    Rando::Logic::GeneratePools(*info, checkPool, itemPool);
    for (int i = 0; i < 6; i++) {
        const int copies = CountOf(itemPool, kSongs[i]);
        const int placed = PlacedInWorld(kSongs[i]);
        const int want = (!extensions || (i == 0 && sharedMinuet)) ? 0 : 1;
        printf("[TEST]   %s: %s pool %d, placed %d (expected %d)\n", name, Rando::StaticData::Items[kSongs[i]].name,
               copies, placed, want);
        WsCheck(copies == want, extensions ? (i == 0 && sharedMinuet
                                                  ? "under the Shared Minuet the separate MM copy was pooled; it must "
                                                    "be held back for lane XW10's family"
                                                  : "a song whose rule is on is not in MM's pool exactly once")
                                           : "a song whose rule is off reached MM's pool");
        WsCheck(placed == copies, "a pool copy of an MM warp song was not placed in the world");
    }
    const uint8_t owned = Combo_MMWarpSongsOwned();
    printf("[TEST]   %s: ownership byte after the creation %02X (expected 00)\n", name, (unsigned)owned);
    WsCheck(owned == 0u, "the creation left an MM warp song owned: a simulated pickup (the bag's MM engine, MM's own "
                         "pass) leaked out of its save snapshot, or the start granted one");
    ClearKeys();
    return 1;
}

} // namespace

extern "C" int MM_WarpSongsPooled_RunHeadless(void) {
    printf("[TEST] mm-warp-songs-pooled-only-when-rule-on: real paired creations pool no MM warp song with the rules "
           "off, exactly one of each with them on (through the test seam), none of a song whose shared rule is on, "
           "place every copy and leave none owned (#1169 XW5)\n");
    sWsFailures = 0;
    WsRestore restore;
    RunLeg("rules off", "RSBSXW5SONGS0", false, false);
    RunLeg("six rules on", "RSBSXW5SONGS1", true, false);
    RunLeg("six rules on, Shared Minuet of Forest", "RSBSXW5SONGS2", true, true);
    if (sWsFailures != 0) {
        printf("[TEST] FAIL: %d contract(s) broken\n", sWsFailures);
        return 1;
    }
    printf("[TEST] PASS: mm-warp-songs-pooled-only-when-rule-on\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
