/**
 * OoT pause-menu item age table bounds lock (#983).
 *
 * OoT_Play_Init (games/oot/src/code/z_play.c:835) writes
 * gItemAgeReqs[ITEM_ROCS_FEATHER] on every scene load, and the pause menu reads
 * gItemAgeReqs[item] through CHECK_AGE_REQ_ITEM for the item grid and the item
 * cycle (ovl_kaleido_scope/z_kaleido_item.c). ITEM_ROCS_FEATHER is a custom
 * ItemID, 0x9C, appended after the vanilla ids, but the table held only the 86
 * vanilla entries (0x00-0x55). The write landed 70 bytes past it, in
 * D_8082AAEC on the Windows link, and the reads followed it there.
 *
 * What it locks: the table's entry count, which only the defining TU
 * (z_kaleido_scope_PAL.c) can see because both declarations are unsized, so it
 * exports gItemAgeReqsCount for this row.
 *
 *   1. The count covers ITEM_ROCS_FEATHER, the index z_play.c writes.
 *   2. The count covers every value a u8 can hold. The readers index with u8
 *      values (inventory.items[], equips.buttonItems[], the cycle's left and
 *      right items), so this is the bound that keeps every read in range,
 *      including custom ItemIDs appended after Roc's Feather later.
 *
 * Red half: restore the unsized `u8 gItemAgeReqs[] = {` definition and the row
 * fails at check 1 with a count of 86. Size it to ITEM_ROCS_FEATHER + 1 and it
 * passes check 1 and fails check 2.
 *
 * ITEM_ROCS_FEATHER is spelled as a literal rather than included: restating the
 * contract independently is the point of a regression lock (as in
 * test_seq_map_bounds.c). ROM-free and display-free.
 *
 * Included at FILE SCOPE by test_runner.cpp (compiled as C++); the declaration
 * carries C linkage because the definition lives in a C translation unit.
 */

#include <cstdint>
#include <cstdio>

extern "C" {
extern const uint32_t gItemAgeReqsCount;
}

#define AGEREQ_ITEM_ROCS_FEATHER 0x9C
#define AGEREQ_U8_DOMAIN 0x100

TestResult Test_ItemAgeReqsBounds(void) {
    printf("[TEST] item-age-reqs-bounds: gItemAgeReqs covers every u8 ItemID (#983)\n");
    printf("[TEST] gItemAgeReqs has %u entries\n", (unsigned)gItemAgeReqsCount);

    if (gItemAgeReqsCount <= AGEREQ_ITEM_ROCS_FEATHER) {
        printf("[TEST] FAIL: gItemAgeReqs has %u entries, so z_play.c's write to [ITEM_ROCS_FEATHER = 0x%X] lands "
               "past the end\n",
               (unsigned)gItemAgeReqsCount, AGEREQ_ITEM_ROCS_FEATHER);
        return TEST_FAIL;
    }
    if (gItemAgeReqsCount < AGEREQ_U8_DOMAIN) {
        printf("[TEST] FAIL: gItemAgeReqs has %u entries, fewer than the %u values a u8 ItemID can hold, so a "
               "CHECK_AGE_REQ_ITEM read of a later custom ItemID can index past the end\n",
               (unsigned)gItemAgeReqsCount, (unsigned)AGEREQ_U8_DOMAIN);
        return TEST_FAIL;
    }

    printf("[TEST] PASS: gItemAgeReqs covers ITEM_ROCS_FEATHER and every u8 ItemID\n");
    return TEST_PASS;
}

#undef AGEREQ_ITEM_ROCS_FEATHER
#undef AGEREQ_U8_DOMAIN
