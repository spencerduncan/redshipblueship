/**
 * @file test_named_items.h
 * @brief Real, describer-named items for the ROM-free locks that need "an item
 *        of game X with a name" (ADR 0010 increment 3).
 *
 * Until increment 3 those locks borrowed a row of a pinned foreign-item pool
 * (kForeignPoolV1 / kForeignPoolMMV1). The pools retired with the overlay passes
 * (D3), and the names now come from each game's registered describer, so the
 * locks look their items up the way the spoiler-LOAD inverse does: by
 * (origin, display name), through Combo_GetForeignItemByNameFor. src/common
 * still never names an RG_* or RI_* value.
 *
 * OoT's item table is filled at OTR bring-up; a ROM-free process that never ran
 * it gets it from OoT_ComboLogic_TestEnsureItemTableTransient (idempotent), which
 * leaves no Rando::Context behind: other rows in the same `--test all` process
 * assert that none exists yet.
 */
#ifndef RSBS_TEST_NAMED_ITEMS_H
#define RSBS_TEST_NAMED_ITEMS_H

#include "../foreign_items.h"

extern "C" int OoT_ComboLogic_TestEnsureItemTableTransient(void);

/** The tagged item of @p origin whose describer name is @p name. False when the
 *  origin's table has no such row (or, for OoT, could not be built). */
static inline bool TestNamedItem(uint8_t origin, const char* name, SharedItem* out) {
    if (origin == (uint8_t)GAME_OOT && OoT_ComboLogic_TestEnsureItemTableTransient() != 0) {
        return false;
    }
    return Combo_GetForeignItemByNameFor(origin, name, out);
}

#endif // RSBS_TEST_NAMED_ITEMS_H
