/**
 * @file relay_client.c
 * @brief Grant-relay client implementation (ADR 0007).
 *
 * The whole file runs on the game thread. There is no thread here to get
 * wrong — that is salvage item 1 (ADR 0006 §5) discharged by construction.
 */

#include "relay_client.h"

#include <stdio.h>
#include <string.h>

#include "../shared_items.h" // Combo_SubmitSourcedGrant, ComboGrantResult

// ============================================================================
// Room fingerprint
// ============================================================================

uint32_t Relay_SourceKeyForRoom(const uint8_t roomUuid[RSBS_RELAY_UUID_LEN]) {
    // FNV-1a 32. Forced nonzero because ADR 0005 reserves 0 for "empty slot".
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < RSBS_RELAY_UUID_LEN; ++i) {
        h ^= (uint32_t)roomUuid[i];
        h *= 16777619u;
    }
    return h == 0u ? 1u : h;
}

// ============================================================================
// Lifecycle
// ============================================================================

void Relay_Init(RelayClient* c, const uint8_t roomUuid[RSBS_RELAY_UUID_LEN], uint8_t localSlot, uint32_t settingsHash) {
    if (c == NULL) {
        return;
    }
    memset(c, 0, sizeof(*c));
    if (roomUuid != NULL) {
        memcpy(c->roomUuid, roomUuid, RSBS_RELAY_UUID_LEN);
    }
    c->sourceKey = Relay_SourceKeyForRoom(c->roomUuid);
    c->localSlot = localSlot;
    c->settingsHash = settingsHash;
    c->state = RSBS_RELAY_STATE_IDLE;
    c->nextSeq = 1u; // ADR 0005: a new source must start at seq 1
    c->sendSeq = 0u;
}

/**
 * Send from `data + *offset` onward, advancing *offset by whatever the channel
 * accepted. Returns true only when the whole buffer is out.
 *
 * The caller-owned offset is the point: a real socket can accept a PREFIX and
 * then block. Restarting from zero on the next attempt would put those bytes
 * on the wire twice and desynchronise the server's length-prefixed parser —
 * a corruption bug that a never-blocking loopback mock would never surface.
 */
static bool ChannelSendFrom(RelayClient* c, const uint8_t* data, uint32_t len, uint32_t* offset) {
    while (*offset < len) {
        const int n = c->channel.send(c->channel.userData, data + *offset, len - *offset);
        if (n < 0) {
            c->state = RSBS_RELAY_STATE_FAILED;
            return false;
        }
        if (n == 0) {
            return false; // would block; resume from *offset next tick
        }
        *offset += (uint32_t)n;
    }
    return true;
}

bool Relay_Connect(RelayClient* c, const RelayChannel* channel) {
    if (c == NULL || channel == NULL || channel->send == NULL || channel->recv == NULL) {
        return false;
    }
    c->channel = *channel;
    c->channelSet = true;

    uint8_t hello[RSBS_RELAY_HELLO_LEN];
    RelayProto_EncodeHello(hello);
    uint32_t off = 0;
    if (!ChannelSendFrom(c, hello, RSBS_RELAY_HELLO_LEN, &off)) {
        return false;
    }
    c->state = RSBS_RELAY_STATE_HELLO;
    return true;
}

// ============================================================================
// Inbox (bounded, recoverable drop — salvage item 2)
// ============================================================================

static void InboxPush(RelayClient* c, const RsbsGrantPayload* g) {
    if (c->inboxCount >= RSBS_RELAY_INBOX_CAP) {
        // Drop WITHOUT advancing the grant cursor and flag a replay. Bounded
        // memory costs latency, never items: the dropped grant is re-derived
        // by the next replay-from-zero (ADR 0007 §3.3).
        c->statInboxDropped++;
        c->replayNeeded = true;
        return;
    }
    const uint32_t slot = (c->inboxHead + c->inboxCount) % RSBS_RELAY_INBOX_CAP;
    c->inbox[slot] = *g;
    c->inboxCount++;
}

static bool InboxPeek(const RelayClient* c, RsbsGrantPayload* out) {
    if (c->inboxCount == 0) {
        return false;
    }
    *out = c->inbox[c->inboxHead];
    return true;
}

static void InboxPop(RelayClient* c) {
    if (c->inboxCount == 0) {
        return;
    }
    c->inboxHead = (c->inboxHead + 1u) % RSBS_RELAY_INBOX_CAP;
    c->inboxCount--;
}

// ============================================================================
// Frame handling
// ============================================================================

static void HandleEntry(RelayClient* c, const RelayLedgerEntry* entry) {
    c->statEntriesSeen++;

    RsbsGrantPayload g;
    if (!RelayProto_DecodeGrant(entry->payload, entry->payloadLen, &g)) {
        // Foreign or malformed: skip and count, never fatal (ADR 0007 §4.3).
        // The server's own length prefix already kept us framed.
        c->statForeignSkipped++;
        return;
    }

    // Self-echo and other players' grants. The real server fans OP_TRANSFER out
    // to EVERY client on the ledger INCLUDING the sender (client.c
    // multiClientCmdTransfer does not skip client->id — unlike the chat path,
    // which does), so our own gifts come straight back to us and MUST be
    // filtered here.
    if (g.targetSlot != c->localSlot) {
        c->statNotForUs++;
        return;
    }

    // Advisory only — the server does not read settingsHash and a peer that
    // wants to lie about it can (ADR 0007 §5.3). Warn once; never enforce.
    if (!c->settingsMismatchSeen && c->settingsHash != 0u && g.settingsHash != 0u &&
        g.settingsHash != c->settingsHash) {
        c->settingsMismatchSeen = true;
        fprintf(stderr, "[relay] warning: peer slot %u reports a different settings hash "
                        "(%08x vs %08x) — you may be on different seeds\n",
                (unsigned)g.senderSlot, (unsigned)g.settingsHash, (unsigned)c->settingsHash);
    }

    InboxPush(c, &g);
}

static bool PumpReceive(RelayClient* c) {
    // Drain whatever the channel has, into the accumulation buffer.
    for (;;) {
        if (c->rxLen >= RSBS_RELAY_RX_CAP) {
            break; // full; decode below will drain it
        }
        const int n = c->channel.recv(c->channel.userData, c->rx + c->rxLen, RSBS_RELAY_RX_CAP - c->rxLen);
        if (n < 0) {
            c->state = RSBS_RELAY_STATE_FAILED;
            return false;
        }
        if (n == 0) {
            break;
        }
        c->rxLen += (uint32_t)n;
    }
    return true;
}

static void ConsumeRx(RelayClient* c, uint32_t n) {
    if (n == 0) {
        return;
    }
    if (n >= c->rxLen) {
        c->rxLen = 0;
        return;
    }
    memmove(c->rx, c->rx + n, c->rxLen - n);
    c->rxLen -= n;
}

static void ProcessHandshake(RelayClient* c) {
    if (c->state == RSBS_RELAY_STATE_HELLO) {
        if (c->rxLen < RSBS_RELAY_SVHELLO_LEN) {
            return;
        }
        if (memcmp(c->rx, RSBS_RELAY_MAGIC, RSBS_RELAY_MAGIC_LEN) != 0) {
            fprintf(stderr, "[relay] server hello has bad magic; refusing\n");
            c->state = RSBS_RELAY_STATE_FAILED;
            return;
        }
        uint32_t serverVersion = 0;
        for (int i = 3; i >= 0; --i) {
            serverVersion = (serverVersion << 8) | (uint32_t)c->rx[RSBS_RELAY_MAGIC_LEN + i];
        }
        // Refuse rather than proceed hopefully into a misparse (ADR 0007 §3.4).
        if (RSBS_RELAY_VERSION_MAJOR(serverVersion) != RSBS_RELAY_VERSION_MAJOR(RSBS_RELAY_PROTOCOL_VERSION)) {
            fprintf(stderr, "[relay] server protocol %08x is incompatible with ours %08x; refusing\n",
                    (unsigned)serverVersion, (unsigned)RSBS_RELAY_PROTOCOL_VERSION);
            c->state = RSBS_RELAY_STATE_FAILED;
            return;
        }
        c->serverClientId = (uint16_t)((uint16_t)c->rx[9] | ((uint16_t)c->rx[10] << 8));
        ConsumeRx(c, RSBS_RELAY_SVHELLO_LEN);
        // Move to JOINING rather than sending the join inline. If the send
        // blocks we must be able to resume it on a later tick — and by then the
        // server hello is already consumed, so a state that means "hello done,
        // join still owed" is the only thing that keeps the join from being
        // dropped on the floor.
        c->state = RSBS_RELAY_STATE_JOINING;
        c->joinSent = 0;
    }

    if (c->state == RSBS_RELAY_STATE_JOINING) {
        // Join at base 0, always (ADR 0007 §3.3): the grant cursor de-dups the
        // already-delivered prefix, so we never persist a second cursor.
        uint8_t join[RSBS_RELAY_JOIN_LEN];
        RelayProto_EncodeJoin(c->roomUuid, 0u, join);
        if (!ChannelSendFrom(c, join, RSBS_RELAY_JOIN_LEN, &c->joinSent)) {
            return; // partial or blocked; resumes from c->joinSent next tick
        }
        // The server sends no join acknowledgement — it simply begins streaming
        // ledger entries — so there is nothing further to await.
        c->state = RSBS_RELAY_STATE_READY;
    }
}

static void ProcessFrames(RelayClient* c) {
    for (;;) {
        uint32_t consumed = 0;
        RelayLedgerEntry entry;
        const RelayDecodeStatus st = RelayProto_DecodeFrame(c->rx, c->rxLen, &consumed, &entry);
        if (st == RSBS_RELAY_DECODE_NEED_MORE) {
            return;
        }
        if (st == RSBS_RELAY_DECODE_ERROR) {
            fprintf(stderr, "[relay] undecodable frame; closing\n");
            c->state = RSBS_RELAY_STATE_FAILED;
            return;
        }
        if (st == RSBS_RELAY_DECODE_ENTRY) {
            HandleEntry(c, &entry);
        }
        // NOP and MSG are consumed and ignored.
        ConsumeRx(c, consumed);
    }
}

static void PumpSend(RelayClient* c) {
    while (c->outboxCount > 0) {
        const uint32_t slot = c->outboxHead;
        if (!ChannelSendFrom(c, c->outbox[slot], c->outboxLen[slot], &c->outboxSent)) {
            return; // partial or blocked; resumes from c->outboxSent next tick
        }
        c->outboxHead = (c->outboxHead + 1u) % RSBS_RELAY_OUTBOX_CAP;
        c->outboxCount--;
        c->outboxSent = 0; // next frame starts from its own beginning
    }
}

// ============================================================================
// Applying to the ADR 0005 seam
// ============================================================================

static int ApplyInbox(RelayClient* c) {
    int accepted = 0;

    while (c->inboxCount > 0) {
        RsbsGrantPayload g;
        if (!InboxPeek(c, &g)) {
            break;
        }

        const ComboGrantResult r =
            Combo_SubmitSourcedGrant(c->sourceKey, c->nextSeq, (GameId)g.originGame, g.itemId);

        switch (r) {
            case RSBS_GRANT_ACCEPTED:
                c->statAccepted++;
                c->nextSeq++;
                accepted++;
                InboxPop(c);
                break;

            case RSBS_GRANT_DUPLICATE:
                // Already delivered in an earlier session/run — this is the
                // designed outcome of the replay-from-zero catch-up, not an
                // error. Advance past it.
                c->statDuplicate++;
                c->nextSeq++;
                InboxPop(c);
                break;

            case RSBS_GRANT_RETRY_FULL:
                // Backpressure, NOT loss (ADR 0005 §3): the cursor did not
                // advance, so the grant is still owed. Leave it at the head of
                // the inbox and stop — re-offering the same seq later is
                // accepted rather than deduped. Advancing past this is the one
                // way to actually lose an item.
                c->statRetryFull++;
                c->replayNeeded = true;
                return accepted;

            case RSBS_GRANT_GAP:
                // Should be unreachable: we assign nextSeq densely ourselves.
                // If it happens, the local model and our counter disagree, and
                // guessing would corrupt ordering. Stop and force a replay.
                c->statGap++;
                c->replayNeeded = true;
                return accepted;

            case RSBS_GRANT_NO_SOURCE_SLOT:
                fprintf(stderr, "[relay] all %u grant-source slots are taken; cannot bind room\n",
                        (unsigned)RSBS_GRANT_SOURCE_CAP);
                c->state = RSBS_RELAY_STATE_FAILED;
                return accepted;

            case RSBS_GRANT_REJECTED:
            default:
                // Malformed at the seam though it passed our decode. Drop it
                // rather than wedging the stream; it is one peer's bad grant.
                fprintf(stderr, "[relay] seam rejected grant (game %u item %u); dropping\n",
                        (unsigned)g.originGame, (unsigned)g.itemId);
                InboxPop(c);
                break;
        }
    }

    return accepted;
}

// ============================================================================
// Public pump
// ============================================================================

int Relay_Tick(RelayClient* c) {
    if (c == NULL || !c->channelSet) {
        return 0;
    }
    if (c->state == RSBS_RELAY_STATE_IDLE || c->state == RSBS_RELAY_STATE_FAILED) {
        return 0;
    }

    if (!PumpReceive(c)) {
        return 0;
    }

    ProcessHandshake(c);

    if (c->state == RSBS_RELAY_STATE_READY) {
        ProcessFrames(c);
        PumpSend(c);
    }

    // The latch stops APPLYING only. Receiving, decoding and filing continue,
    // so a suspended game does not stall the peer or lose ledger position
    // (salvage item 3).
    if (c->suspended) {
        return 0;
    }

    return ApplyInbox(c);
}

bool Relay_SendGrant(RelayClient* c, uint8_t targetSlot, GameId originGame, uint16_t itemId) {
    if (c == NULL || c->outboxCount >= RSBS_RELAY_OUTBOX_CAP) {
        return false;
    }
    if (targetSlot == 0 || c->localSlot == 0) {
        return false;
    }

    if (c->sendSeq == 0xFFFFu) {
        fprintf(stderr, "[relay] outbound sequence exhausted; refusing to wrap\n");
        return false;
    }

    RsbsGrantPayload g;
    g.version = RSBS_GRANT_PAYLOAD_VERSION;
    g.senderSlot = c->localSlot;
    g.targetSlot = targetSlot;
    g.originGame = (uint8_t)originGame;
    g.itemId = itemId;
    g.senderSeq = (uint16_t)(c->sendSeq + 1u);
    g.settingsHash = c->settingsHash;

    uint8_t payload[RSBS_GRANT_PAYLOAD_LEN];
    if (!RelayProto_EncodeGrant(&g, payload)) {
        return false;
    }

    const uint32_t slot = (c->outboxHead + c->outboxCount) % RSBS_RELAY_OUTBOX_CAP;
    const uint32_t n =
        RelayProto_EncodeTransfer(RelayProto_GrantKey(&g), payload, RSBS_GRANT_PAYLOAD_LEN, c->outbox[slot]);
    if (n == 0) {
        return false;
    }
    c->outboxLen[slot] = n;
    c->outboxCount++;
    c->sendSeq = g.senderSeq;
    return true;
}

void Relay_OnSuspend(RelayClient* c) {
    if (c != NULL) {
        c->suspended = true;
    }
}

void Relay_OnResume(RelayClient* c) {
    if (c != NULL) {
        c->suspended = false;
    }
}

void Relay_ClearSessionState(RelayClient* c) {
    if (c == NULL) {
        return;
    }
    // Undelivered grants belong to the session that received them. Retiring
    // them here — called FROM Context_InvalidateSessionState — keeps one list
    // of "things a dead session owns" (#440 guidance on #460).
    c->inboxHead = 0;
    c->inboxCount = 0;
    c->outboxHead = 0;
    c->outboxCount = 0;
    c->outboxSent = 0;
    c->rxLen = 0;
    // The cursor died with gComboCtx, so our dense counter restarts too;
    // otherwise we would submit seq N+1 against a cursor of 0 and wedge on GAP.
    c->nextSeq = 1u;
    c->sendSeq = 0u;
    c->replayNeeded = true;
}

bool Relay_ReplayNeeded(const RelayClient* c) {
    return c != NULL && c->replayNeeded;
}
