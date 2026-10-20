/**
 * OoT item-action table bounds lock (#984).
 *
 * OoT_Player_ItemToItemAction (z_player.c) and func_80846720 index the static
 * sItemActions[] table with an ItemID. The vanilla table stops at
 * ITEM_SWORD_BIGGORON (0x3D, 62 entries), but a u8 ItemID can name anything up
 * to ITEM_NONE (0xFF), and ITEM_ROCS_FEATHER (0x9C) is a custom ItemID appended
 * after the vanilla ids. The Roc's Feather lookups on a C button (mask check,
 * OoT_Player_ItemIsInUse, the held-button check) read 94 bytes past the end, so
 * the item action Link compared against was one byte of a code address.
 *
 * What it locks, through two things only z_player.c can see (the table is
 * static): the entry count it exports as gItemActionsCount and the raw entry
 * OoT_Player_ItemActionTableEntry(item) returns.
 *
 *   1. The count covers ITEM_ROCS_FEATHER.
 *   2. The count covers every value a u8 can hold, so a custom ItemID appended
 *      after Roc's Feather later is in range too.
 *   3. Every ItemID the vanilla table does not name (0x3E through 0xFF) resolves
 *      to PLAYER_IA_NONE (0), the action AssignableTunicsAndBoots already
 *      returns for those items, never to a neighbour's byte.
 *   4. The vanilla entries are untouched: ITEM_DEKU_STICK (0x00) is
 *      PLAYER_IA_DEKU_STICK (non-zero) and ITEM_SWORD_BIGGORON (0x3D) is
 *      PLAYER_IA_SWORD_BIGGORON (non-zero), so sizing the table did not
 *      zero-fill it.
 *
 * Red half: restore the unsized `static s8 sItemActions[] = {` definition and
 * the row fails at check 1 with a count of 62. Size it to ITEM_ROCS_FEATHER + 1
 * and it passes check 1 and fails check 2.
 *
 * ITEM_ROCS_FEATHER and friends are spelled as literals rather than included:
 * restating the contract independently is the point of a regression lock (as
 * in test_item_age_reqs_bounds.c). ROM-free and display-free.
 *
 * Included at FILE SCOPE by test_runner.cpp (compiled as C++); the declarations
 * carry C linkage because the definitions live in a C translation unit.
 */

#include <cstdint>
#include <cstdio>

extern "C" {
extern const uint32_t gItemActionsCount;
int32_t OoT_Player_ItemActionTableEntry(uint32_t item);
}

#define ITEMACT_ITEM_ROCS_FEATHER 0x9C
#define ITEMACT_FIRST_UNNAMED 0x3E
#define ITEMACT_U8_DOMAIN 0x100
#define ITEMACT_IA_NONE 0

TestResult Test_ItemActionsBounds(void) {
    printf("[TEST] item-actions-bounds: sItemActions covers every u8 ItemID (#984)\n");
    printf("[TEST] sItemActions has %u entries\n", (unsigned)gItemActionsCount);

    if (gItemActionsCount <= ITEMACT_ITEM_ROCS_FEATHER) {
        printf("[TEST] FAIL: sItemActions has %u entries, so OoT_Player_ItemToItemAction(ITEM_ROCS_FEATHER = 0x%X) "
               "reads past the end\n",
               (unsigned)gItemActionsCount, ITEMACT_ITEM_ROCS_FEATHER);
        return TEST_FAIL;
    }
    if (gItemActionsCount < ITEMACT_U8_DOMAIN) {
        printf("[TEST] FAIL: sItemActions has %u entries, fewer than the %u values a u8 ItemID can hold, so a lookup "
               "of a later custom ItemID can index past the end\n",
               (unsigned)gItemActionsCount, (unsigned)ITEMACT_U8_DOMAIN);
        return TEST_FAIL;
    }

    for (uint32_t item = ITEMACT_FIRST_UNNAMED; item < ITEMACT_U8_DOMAIN; item++) {
        int32_t action = OoT_Player_ItemActionTableEntry(item);
        if (action != ITEMACT_IA_NONE) {
            printf("[TEST] FAIL: sItemActions[0x%X] is %d, not PLAYER_IA_NONE, for an ItemID the vanilla table does "
                   "not name\n",
                   (unsigned)item, (int)action);
            return TEST_FAIL;
        }
    }

    if (OoT_Player_ItemActionTableEntry(0x00) == ITEMACT_IA_NONE ||
        OoT_Player_ItemActionTableEntry(0x3D) == ITEMACT_IA_NONE) {
        printf("[TEST] FAIL: a vanilla entry (ITEM_DEKU_STICK / ITEM_SWORD_BIGGORON) reads PLAYER_IA_NONE, so the "
               "table was not initialised\n");
        return TEST_FAIL;
    }

    printf("[TEST] PASS: sItemActions covers ITEM_ROCS_FEATHER and every u8 ItemID, unnamed ids are PLAYER_IA_NONE\n");
    return TEST_PASS;
}

#undef ITEMACT_ITEM_ROCS_FEATHER
#undef ITEMACT_FIRST_UNNAMED
#undef ITEMACT_U8_DOMAIN
#undef ITEMACT_IA_NONE
