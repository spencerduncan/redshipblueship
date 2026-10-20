/**
 * @file oot_shared_wallet_test.cpp
 * OoT's half of the shared-wallet lock (#991, #992; operator ruling R2,
 * 2026-10-19: OoT has one wallet progression, and a wallet found in Termina IS
 * the pair's wallet).
 *
 * THE SHIM HELPERS the redship row `shared-wallet`
 * (src/common/tests/test_shared_wallet.c) drives OoT's REAL
 * OoT_HarvestSharedResources / OoT_ApplySharedResources through, plus the two
 * consumers whose disagreement was the defect: logic's GetWalletCapacity (read
 * through a real Rando::Logic over OoT's live save) and the per-frame rupee pin
 * (the REAL RandomizerOnGameFrameUpdateHandler, which reads only OoT's live
 * save). The helpers only put the live save in a known state and read it back;
 * every merge decision is the production shims'.
 *
 * "Shuffle Child's Wallet on, child wallet not found" is the state a new file
 * is created in: the setting is read once, at creation, and leaves
 * RAND_INF_HAS_WALLET clear (savefile.cpp). The live save the row builds carries
 * exactly that, so no Randomizer context is needed.
 */

#include <cstring>
#include <memory>

// At file scope, not inside an extern "C" block (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
#include "variables.h"
#include "macros.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/location_access.h"

extern "C" void OoT_Inventory_ChangeUpgrade(s16 upgrade, s16 value); // GameExports_SingleExe.cpp's own use
extern "C" s32 Flags_GetRandomizerInf(RandomizerInf flag);
extern "C" void Flags_SetRandomizerInf(RandomizerInf flag);

// hook_handlers.cpp: the per-frame handler that pins rupees at 0 while the
// wallet flag is clear. Reads and writes only gSaveContext.
void RandomizerOnGameFrameUpdateHandler();

/** OoT's live save as a real loaded RANDOMIZER file: GAMEMODE_NORMAL is what
 *  Combo_SaveIsLiveFile requires before OoT_HarvestSharedResources does anything.
 *  Wallet tier @p tier, @p rupees in the wallet, and the possession flag @p
 *  hasWallet (0 = Shuffle Child's Wallet on and the child wallet not found).
 *  Everything else zero, so the harvest offers no other kind a nonzero value. */
extern "C" void OoT_SharedWalletTest_ArmLive(int tier, int rupees, int hasWallet) {
    memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.fileNum = 0;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.ship.quest.id = QUEST_RANDOMIZER;
    OoT_Inventory_ChangeUpgrade(UPG_WALLET, (s16)tier);
    gSaveContext.rupees = (s16)rupees;
    if (hasWallet != 0) {
        Flags_SetRandomizerInf(RAND_INF_HAS_WALLET);
    }
}

/** OoT's own child-wallet give (Item_Give for the first progressive wallet when
 *  the flag is clear): sets the possession flag, leaves the tier at 0. */
extern "C" void OoT_SharedWalletTest_GiveChildWallet(void) {
    Flags_SetRandomizerInf(RAND_INF_HAS_WALLET);
}

/** Set OoT's live rupee count (what the player holds mid-stint). */
extern "C" void OoT_SharedWalletTest_SetRupees(int rupees) {
    gSaveContext.rupees = (s16)rupees;
}

/** 1 when RAND_INF_HAS_WALLET is set in OoT's live save. */
extern "C" int OoT_SharedWalletTest_HasWalletFlag(void) {
    return Flags_GetRandomizerInf(RAND_INF_HAS_WALLET) ? 1 : 0;
}

/** OoT's live wallet tier (UPG_WALLET). */
extern "C" int OoT_SharedWalletTest_Tier(void) {
    return (int)CUR_UPG_VALUE(UPG_WALLET);
}

/** OoT's live rupee count. */
extern "C" int OoT_SharedWalletTest_Rupees(void) {
    return (int)gSaveContext.rupees;
}

/** One frame of the randomizer's per-frame handler (pins rupees at 0 while the
 *  wallet flag is clear). */
extern "C" void OoT_SharedWalletTest_Frame(void) {
    RandomizerOnGameFrameUpdateHandler();
}

/** The capacity logic reads for the live save: GetWalletCapacity() over a real
 *  Rando::Logic whose save context is OoT's live one. @p childRow, when
 *  nonnull, receives logic's possession answer for the child wallet row. */
extern "C" int OoT_SharedWalletTest_LogicCapacity(int* childRow) {
    auto saved = ::logic;
    auto probe = std::make_shared<Rando::Logic>();
    probe->SetSaveContext(&gSaveContext);
    ::logic = probe;
    const int capacity = (int)GetWalletCapacity();
    if (childRow != nullptr) {
        *childRow = probe->HasItem(RG_CHILD_WALLET) ? 1 : 0;
    }
    ::logic = saved;
    return capacity;
}
