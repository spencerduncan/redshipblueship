/**
 * @file netplay_sink.h
 * @brief The seam between the netplay transport and the shared-item machinery.
 *
 * COORDINATION NOTE (#460): the netplay-1a "grant foundation" work is landing
 * the network-safe producer semantics (grant identity, a record path that
 * bypasses content de-dup, the in-place redemption tick, an explicit overflow
 * policy) as its own ADR + code. This header deliberately does NOT hardwire
 * the transport into Combo_RecordSharedItem: all item application goes
 * through ONE registered sink, so when the foundation seam lands, binding to
 * it is a change to netplay_sink.cpp alone. Until then,
 * Netplay_InstallSharedItemSink adapts to the CURRENT Lane-A1 API with its
 * two known gaps documented below.
 *
 * Sink contract:
 *  - Called on the netplay owning thread only, from inside Netplay_Tick.
 *  - Called strictly in Archipelago index order; `grantSeq` is that index.
 *  - Return 0 to ACCEPT: the transport advances the persisted cursor and
 *    will never offer this grant again.
 *  - Return nonzero to REFUSE (e.g. durable storage full): the transport
 *    keeps the cursor, stops applying for this Tick, and re-offers the same
 *    grant on a later Tick — backpressure instead of loss.
 */

#ifndef RSBS_NETPLAY_NETPLAY_SINK_H
#define RSBS_NETPLAY_NETPLAY_SINK_H

#include <stdint.h>

#include "game.h" // GameId

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*NetplayGrantSink)(GameId originGame, uint16_t itemId, uint32_t grantSeq, void* userCtx);

/**
 * Register the grant sink. Passing NULL uninstalls (grants then queue in the
 * bounded inbox and backpressure via the cursor, exactly as if the sink
 * refused). Owning thread only.
 */
void Netplay_SetGrantSink(NetplayGrantSink sink, void* userCtx);

/**
 * Install the interim adapter onto the CURRENT shared-item producer
 * (Combo_RecordSharedItem). Known gaps against the #460 foundation contract,
 * both accepted for increment 1 and both fixed by rebinding this adapter to
 * the foundation seam when it lands:
 *
 *  1. CONTENT DE-DUP MERGE (#460 gap 2): Combo_RecordSharedItem collapses a
 *     second grant of the same (originGame, id) into the existing un-redeemed
 *     slot. Two legitimately distinct grants of the same item therefore
 *     merge into one award until the foundation's unique-record path exists.
 *     (Retransmit-idempotency does NOT rely on this — the AP index cursor
 *     already guarantees each index applies at most once.)
 *
 *  2. NO IN-PLACE REDEMPTION (#460 gap 1): recorded grants are awarded at
 *     the next arrival in the origin game, not immediately. The foundation's
 *     gameplay-gated redemption tick adds the "apply while already in the
 *     game" trigger.
 *
 * The adapter maps "array full" (-1) to REFUSE, which composes with the AP
 * cursor into lossless backpressure (#460 gap 4: reject-and-signal, never
 * silent drop).
 */
void Netplay_InstallSharedItemSink(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_NETPLAY_NETPLAY_SINK_H
