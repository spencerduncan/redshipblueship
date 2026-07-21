/**
 * @file netplay_sink.c
 * @brief Interim shared-item sink adapter (see netplay_sink.h).
 *
 * THE ONE PLACE that binds the transport to the shared-item machinery. When
 * the #460 grant-foundation seam lands (unique-record path + redemption
 * tick), this adapter rebinds to it and nothing else in src/netplay changes.
 */

#include "netplay_sink.h"

#include "shared_items.h" // Combo_RecordSharedItem (current Lane-A1 producer)

#include <stdio.h>

static int SharedItemSink(GameId originGame, uint16_t itemId, uint32_t grantSeq, void* userCtx) {
    (void)userCtx;
    // NOTE (#460 gap 2): Combo_RecordSharedItem content-de-dups on
    // (originGame, id) among un-redeemed entries, so a second grant of the
    // same item before redemption MERGES into the existing slot; its return
    // value cannot distinguish merged from fresh. Accepted for increment 1 —
    // the AP cursor still guarantees at-most-once per index — and fixed by
    // rebinding to the foundation's unique-record path.
    int slot = Combo_RecordSharedItem(originGame, itemId);
    if (slot < 0) {
        // Durable array full (#460 gap 4): REFUSE so the transport holds the
        // cursor and retries later — reject-and-signal, never silent drop.
        fprintf(stderr, "[Netplay] grant %u refused: shared-item array full\n", grantSeq);
        return -1;
    }
    return 0;
}

void Netplay_InstallSharedItemSink(void) {
    Netplay_SetGrantSink(SharedItemSink, NULL);
}
