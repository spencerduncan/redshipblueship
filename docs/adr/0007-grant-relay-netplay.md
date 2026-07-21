# ADR 0007: Netplay is a grant-only relay speaking OoTMM's ledger protocol — reuse the server, write no server

- Status: **Proposed** (2026-07-21)
- For: #460 (netplay increment 1b), executing the maintainer decision recorded
  on that issue: *"archipelago is a nongoal. if we wanted archipelago we could
  just use ship and 2ship."*
- Depends on: **ADR 0005** (per-source grant cursors — the producer seam;
  PR #473) and **ADR 0002** (origin-tagged `SharedItem`, the ComboContext
  growth contract)
- Composes with: **#440 / PR #464** (session invalidation retires a dead
  session's crossings)
- Supersedes the open branch of: **ADR 0006** §4 — its question was answered
  *no*, so this ADR takes 0006's "no" path and carries its §5 salvage list

## Decision

Netplay is a **grant-only relay**: peers exchange `SharedItem` grants and
nothing else. It rides ADR 0005's `Combo_SubmitSourcedGrant` seam and opens no
second path into `Combo_RecordSharedItem`.

Three sub-decisions, each argued below:

1. **Reuse [OoTMM's `multi-server`][ootmm] protocol and binary; write no
   server in this repository.** Its ledger is *content-agnostic*, which is
   precisely the property Archipelago lacked, so the wall ADR 0006 measured
   does not exist here.
2. **Add no transport library and no submodule.** The protocol is
   length-prefixed binary over plain TCP; a small in-tree socket shim is
   smaller than the CI cost of a dependency (ADR 0006 §7).
3. **Trusted peers only, no validation** — stated in full in §6, because an
   unstated trust model is how a fun feature becomes a griefing vector.

Everything is behind `RSBS_NETPLAY`, default **OFF**, with the default build
byte-unaffected (§7).

## 1. Server: reuse vs write

### 1.1 Licence, verified from the file

ADR 0006 §6 was burned by GitHub's classifier, so this was checked by reading
the bytes:

```
$ gh api repos/OoTMM/multi-server/contents/LICENSE --jq '.content' | base64 -d
MIT License

Copyright (c) 2020-2022 OoTMM Team
```

| Project | Ref | Licence | How verified |
|---|---|---|---|
| `OoTMM/multi-server` | `master`, pushed 2025-03-27 | **MIT** | full LICENSE text read (1072 bytes, standard MIT body) |

Note the default branch is **`master`**, not `main` — `raw.githubusercontent.com/.../main/LICENSE` 404s, which is exactly the sort of thing that produces an unverified licence claim.

Two further facts that matter more than the licence: we **vendor no code from
it at all** (we implement a client against its wire format, which is a
specification, not a copyrightable body of code we are copying), and the
nearest GPL-3.0 prior art ([MMRecompRando][mmrr]) was **not** read, adapted, or
consulted — consistent with ADR 0006.

### 1.2 The property that makes reuse correct: the server is content-agnostic

This is the whole argument. Read from `src/MultiServer/multi.h` and
`ledger.c`, the server's unit of work is:

```c
typedef struct PACKED { uint64_t key; uint8_t size; } LedgerEntryHeader;
/* followed by `size` opaque payload bytes */
```

`multiLedgerWrite` de-dups on `key`, appends to a disk-backed log, and
broadcasts. **It never inspects the payload.** There is no game identity, no
slot table, no generated-world registry, and therefore **no admissibility
check to fail**.

Contrast ADR 0006 §1.1: Archipelago refuses `Connect` with `InvalidSlot` /
`InvalidGame` because a room is generated ahead of time from apworlds. That
refusal is what made an AP loopback harness a fiction — the mock accepted what
a real server rejects. A `multi-server` ledger accepts an RSBS grant payload
today, unmodified, because it cannot tell an RSBS grant from an OoTMM one.

**This inverts ADR 0006's conclusion for this transport, and it is the reason
this ADR builds where 0006 declined to.**

### 1.3 The protocol already implements what ADR 0005 needs

Read from `client.c`. The handshake is four steps:

| Step | Direction | Bytes |
|---|---|---|
| 1 | C→S | `"OOMM2"` + `u32 version` (9) |
| 2 | S→C | `"OOMM2"` + `u32 VERSION` + `u16 clientId` (11) |
| 3 | C→S | `u8 roomUuid[16]` + `u32 ledgerBase` (20) |
| 4 | S→C | every ledger entry from `ledgerBase` onward, then live tail |

Then a command loop: `OP_TRANSFER` (1) submits one entry, `OP_MSG` (2) is
chat. The server rejects `header.size > 128` and disconnects; it also
disconnects if `ledgerBase > ledger.count`.

Three properties fall out, and each is something we would otherwise have to
build:

- **`ledgerBase` is a join-time catch-up cursor.** A late joiner sends a base
  and receives the backlog before any live traffic. Catch-up is *in the
  protocol*, not bolted on.
- **`key` is server-side idempotency**, disk-persisted via the ledger's
  `keysSet`, so a client retransmit is absorbed before it ever fans out.
- **The ledger is a persistent total order.** Entry order is stable across
  reconnects and server restarts (it is replayed from the `data` file at
  ledger open), which is what makes ADR 0005's strict in-order acceptance
  implementable at all.

That maps onto ADR 0005 with no impedance mismatch, and §3 below states the
mapping precisely.

### 1.4 The comparison

| | Reuse `multi-server` | Write a minimal own relay |
|---|---|---|
| Licence | MIT, verified §1.1 | n/a |
| Admissibility wall | **none** (§1.2) | none |
| Ledger persistence, dedup, catch-up | **already implemented, in production for the sibling project** | ~600–900 lines of C plus a durability story |
| Self-hosting | `Dockerfile.prod` + `docker-start.sh` in-repo; single static C binary, no runtime deps | ours to build, package, and document |
| Server code we maintain | **zero** | a network-facing service, forever |
| CI cost | zero (no server in our tree) | a second build target and its tests |
| Portability | Linux-only (`sys/epoll.h`) | our choice |
| Control over protocol evolution | none — `VERSION 0x00000200` is theirs | total |
| TLS / auth | none | none (we would not add it either — §6) |

**Decision: reuse.** The two columns differ in exactly one place that favours
writing — protocol control — and that cost is bounded because the wire format
is 60 lines of struct definitions we have already read and pinned (§3.4). The
Linux-only server is irrelevant: it is a *server*, and it ships a Dockerfile.
Everything else favours reuse decisively, and "zero server code we maintain"
is the difference between a feature and a service commitment.

**We are not, however, locked in.** Because we speak the protocol rather than
link the server, a future minimal own relay is a drop-in: it would have to
implement the four-step handshake, a keyed append-only log, and base-cursor
replay — all of which are specified here. This ADR records the protocol so
that remains true even if upstream disappears.

## 2. What rides the wire

The payload is ours; the server never reads it. One grant, fixed 16 bytes:

```c
typedef struct PACKED {   // RSBS relay grant payload v1 (16 bytes)
    uint8_t  magic[4];    // 'R','S','B','S'
    uint8_t  version;     // 1
    uint8_t  senderSlot;  // 1..255, the sender's slot in this room
    uint8_t  targetSlot;  // 1..255, the intended recipient
    uint8_t  originGame;  // GameId: the id-space owner (ADR 0002)
    uint16_t itemId;      // RG_* or RI_*, per originGame
    uint16_t senderSeq;   // sender's own dense 1-based counter
    uint32_t settingsHash;// advisory only — see §5.3
    uint16_t reserved;    // zero == unset (ADR 0002 growth contract)
} RsbsGrantPayload;
```

Well under the server's 128-byte cap, leaving headroom for a v2 without
renegotiating anything.

The ledger `key` is `((u64)senderSlot << 48) | ((u64)senderSeq << 16) | itemId`
— unique per (sender, sequence) so the server's `keysSet` absorbs a sender's
own retransmit, while two *different* senders gifting the same item produce
different keys and both survive. This matters: server-side dedup collapsing
two legitimate gifts would reintroduce, one layer lower, exactly the bug
ADR 0005 §1 exists to fix.

`magic` + `version` exist because the ledger is content-agnostic in both
directions: an OoTMM client pointed at the same room writes OoTMM payloads
into it. A malformed or foreign entry is **skipped, not fatal** (§4.3).

## 3. Mapping onto ADR 0005's cursors

### 3.1 A room is one source, not one source per peer

`sourceKey = fnv1a32(roomUuid[16])`, forced nonzero. **One** `RSBS_GRANT_SOURCE_CAP`
slot per room, regardless of how many peers are in it.

This is right rather than merely economical: the thing that delivers grants in
order is the *ledger*, not any individual peer. Peers do not have independent
delivery streams — they have entries in a shared log. Modelling each peer as
an ADR 0005 source would create N cursors over one stream, which is precisely
the "two cursors for one stream is a divergence bug waiting to happen"
failure the #473 author warned about on #460.

Two peers gifting you the same item are still two items, as ADR 0005 requires
— they are two ledger entries with two indices, hence two `seq` values, hence
two accepts. Content de-dup never sees them (sourced entries carry
`RSBS_SHARED_ITEM_SOURCED`).

### 3.2 `seq` is a dense renumbering of entries addressed to us

The ledger index cannot be `seq` directly. We skip entries addressed to other
players, and skipping would leave holes — every hole is an `RSBS_GRANT_GAP`,
and ADR 0005's strict in-order acceptance would wedge permanently.

So: **`seq` = the ordinal of the entry among ledger entries addressed to
*this* slot**, 1-based and dense. The Nth grant in the ledger targeting me is
my `seq` N.

This is deterministic and stable — the ledger is append-only and
server-deduplicated, so the subsequence addressed to any given slot is fixed
for all time. Two clients replaying the same ledger derive identical `seq` for
identical entries, and so does the same client after a reconnect.

### 3.3 Catch-up: always join at `ledgerBase = 0`; the cursor de-dups

We do **not** persist a ledger position. We send `ledgerBase = 0` on every
join and replay the entire ledger; the already-delivered prefix returns
`RSBS_GRANT_DUPLICATE` from the grant cursor and the novel tail records.

This is deliberate, and it is the path ADR 0005 §1 explicitly designed for
("the spike's resync trap … becomes the *designed* path"). The alternative —
persisting a ledger index alongside the grant cursor — is the second cursor
over one stream that §3.1 rejects: the two disagree after any partial accept,
and `RSBS_GRANT_RETRY_FULL` makes partial accepts a *designed* occurrence, not
an edge case.

The cost is bounded and small: a full replay is `entries × 25` bytes on the
wire (9-byte header + 16-byte payload). A thousand-grant room is 25 KB per
join. That is cheaper than the class of bug the second cursor invites.

It also yields a property worth having explicitly: **crash recovery is free
and self-consistent.** Grants and cursors live in the same Tier-1 record and
are retired atomically (ADR 0005 §6), so a crash before the next `.redsave`
save loses the accepted grant *and* the cursor advance together. The next join
replays from 0 and re-delivers it. Losing half of that pair would be a real
bug; the atomicity ADR 0005 chose is what makes replay a repair rather than a
duplication.

### 3.4 Version pin

We pin against `VERSION 0x00000200` and send `"OOMM2"`. The handshake reply
carries the server's version; **a mismatch in the high 16 bits refuses the
connection with an operator-visible message** rather than proceeding
hopefully. The legacy `"OoTMM"` 5-byte header is not implemented.

## 4. Client structure — ADR 0006 §5's salvage list, carried over intact

Each item below closes a *verified* hazard in the vendored Anchor client
(spike §6). They are carried structurally, not as guidance:

### 4.1 No receive thread

The socket is serviced only from `Netplay_Tick()`, called on the game thread.
Non-blocking sockets, `poll()`/`select()` with a zero timeout, decode into the
inbox, submit to the seam — all on the owning thread. **Anchor hazards #1 and
#2 cannot exist by construction**, and ADR 0005's "game thread only" seam
contract is satisfied without a marshalling layer to get wrong.

### 4.2 Bounded inbox with recoverable drop

A fixed ring of decoded grants. On overflow we **drop the tail without
advancing the grant cursor** and flag a replay. Bounded memory, zero item
loss, and it composes exactly with `RSBS_GRANT_RETRY_FULL`: both are "the
source still owes this", and both are healed by the §3.3 replay. Closes
Anchor hazard #3 (unbounded queue growth across a switch).

### 4.3 Malformed and foreign entries are skipped, never fatal

A ledger entry failing the `magic`/`version`/size check is counted and
skipped. It does not desynchronise the stream (entries are length-prefixed by
the server's own header) and does not disconnect. Rationale in §2: the ledger
is shared-format by design.

### 4.4 Explicit suspend latch

`Netplay_OnSuspend()` stops *applying* grants; polling and decoding continue
into the bounded inbox. `Netplay_OnResume()` drains in received order. This is
the resume-contract lesson from `single-exe-resume-contract`: the lifecycle
has no implicit inverse, so the latch is explicit and its release is a
separate, named action.

### 4.5 Room binding is refuse-on-mismatch, and it is *free*

ADR 0006 §5 wanted room-fingerprint binding. We get it without new state:
`sourceKey` **is** the room fingerprint (§3.1). Joining a different room
derives a different `sourceKey`, which is a different ADR 0005 source slot,
which starts at `seq` 1. A cursor can never be applied against a room that did
not produce it, because the key *is* the room. Rebinding is an operator action
(entering a different room id), never automatic.

## 5. Combo-specific semantics

### 5.1 Across a game switch

`gComboCtx.sharedItemsTagged` is process-global and crosses the switch by
being process-global (shared_items.h). So a grant received while you are in MM
but tagged `GAME_OOT` simply waits, un-redeemed, and is awarded at the next
OoT safe point. **The relay needs no switch awareness whatsoever** — this is
the payoff of building on ADR 0005 rather than beside it.

Two consequences to be explicit about:

- **A peer sees nothing about which game you are in.** The relay carries no
  presence, no location, no status — only grants. A sender cannot know, and
  does not need to know, whether you are in OoT or MM. Grants are addressed to
  a *player*, never to a game.
- **The client object is owned by `src/common/`,** not by either game, and is
  driven by the lifecycle hooks (§4.4). It is not torn down by a switch.

The redemption tick that ADR 0005 §4 *defined but deliberately did not wire*
gets wired here — this is the increment that supplies "the first producer that
can target the active game mid-session", which is the precise condition
ADR 0005 named for wiring it.

### 5.2 A room is per combo session

Yes, and #440's fix (PR #464) is what makes it true rather than aspirational.
`Context_InvalidateSessionState` retires items and cursors in one
`ComboContext_Init`, so:

- reset → new file: the room's cursor dies with the crossings it authored. A
  reconnect to the same room replays from 0 into a fresh world and every entry
  is accepted anew — correct, because it *is* a different world.
- reset → same slot: both are reloaded from that slot's `.redsave`, still
  paired.
- A dead room's retransmit into a live different session is a `GAP` (new
  sources start at 1), never a silent acceptance.

Per the #440 guidance on #460: **the relay's own RAM inbox is session-scoped
state and is cleared from `Context_InvalidateSessionState`**, alongside
`Combo_ClearSharedItemOutbox()` — not at our own call sites — so there stays
one list of "things a dead session owns".

We carve **nothing** from `reserved[]`. Room identity is `sourceKey`, which is
derived, not stored; our slot number and room id are configuration, not
session state. This is a deliberate outcome: ADR 0006 §3 recorded a prior
carve colliding byte-for-byte with ADR 0005's, and the cleanest way not to
repeat that is to need no bytes. `reserved[264]` at offset 740 is left wholly
to ADR 0005's successors.

### 5.3 State peers cannot see

`foreignPlacements` and `sharedRandoSettingsHash` are **not** exchanged and
**not** validated. The relay does not know, and cannot check, whether a grant
is consistent with the recipient's seed.

The payload carries `settingsHash` for an **advisory** mismatch warning only —
the client surfaces "this peer's settings hash differs from yours" once per
room and continues. It is not enforcement, and calling it enforcement would be
the lie: the server does not read it, and a peer that wants to lie about it
can. It exists to catch the honest mistake (two friends on different seeds),
not the dishonest one.

The deeper point is the one ADR 0006 §1.3 identified from the other side:
**RSBS keeps placement authority.** A grant is an item handed to a player, not
a location check resolved by a generator. There is no shared world model to
disagree about, which is why a relay is tractable where an apworld was not.

### 5.4 What `REDEEMED` does and does not guarantee

Stated plainly, because it is the field most likely to be over-read:

**It guarantees** that the award callback ran exactly once for that entry, and
that redemption is idempotent under any interleaving of safe points
(ADR 0005 §4).

**It does not guarantee:**

- **that the player actually holds the item.** The award routes through each
  game's live give path, which may cap, ignore, or transform (a full wallet, a
  progressive resolving to something else). `REDEEMED` means *awarded*, not
  *received*.
- **that the sender knows.** There is **no acknowledgement in this protocol.**
  The ledger has no delivery receipt and we add none. A sender knows its entry
  reached the *server*; it never learns that a peer redeemed it. Any UI must
  not claim otherwise.
- **anything across an unsaved crash.** `REDEEMED` lives in `gComboCtx` and is
  durable only at `.redsave` save. A crash after redemption but before save
  loses the flag *and* the cursor together — §3.3's replay then re-delivers,
  which is correct-by-atomicity but is a re-award, not a no-op.
- **ordering against local pickups.** Received order is a contract *among
  sourced grants* (ADR 0005 §4). A grant and a local pickup racing in the same
  frame have no defined relative order, and progressives resolve against
  whichever lands first.

## 6. Trust model — trusted peers only, no validation

Stated in full and without softening, per the brief.

**There is no authentication, no authorization, and no validation of grant
content.** The protocol has no TLS, no credentials, and no identity beyond a
self-asserted `senderSlot`. The 16-byte room UUID is a **bearer capability**:
anyone who knows it can read the room's entire history and append to it. Treat
it as a secret and use a fresh one per session.

A malicious or compromised peer **in your room** can:

- grant you arbitrary items, including progression-breaking or seed-invalidating ones;
- impersonate another peer by asserting their `senderSlot`;
- read every grant exchanged in the room, including who received what;
- push your 64-slot array to capacity — though this is the least effective
  attack available, because overflow is loud (`sharedItemOverflowCount`) and
  sourced grants are backpressured rather than lost (ADR 0005 §3).

A malicious **server operator** can do all of the above, plus withhold or
reorder entries — though reordering surfaces as `RSBS_GRANT_GAP` rather than
silent corruption, and withholding is indistinguishable from a peer not
sending.

**What is structurally not possible**, and these are the boundaries worth
holding:

- **No code execution from payload.** Fixed-size struct, bounds-checked
  decode, no pointers, no length fields we trust, no allocation driven by
  remote input.
- **No direct write to another player's save.** Every grant routes through
  `Combo_SubmitSourcedGrant` and then the game's own give path. There is no
  code path from the wire to a `SaveContext` field.
- **No crossing into an unrelated session.** #440's invalidation retires
  cursors with items, so a dead room's stream cannot be accepted by a live
  different world (§5.2).
- **No outbound connection you did not ask for.** §7.

**The posture, stated for players:** this is co-op with people you trust, on
the same footing as handing someone your controller. It is not hardened
against an adversary in the room, and it is not intended to be. If you want a
stronger guarantee, self-host and share the room id narrowly.

Adding authentication is a coherent future increment (per-peer keys signing
the payload's `senderSlot`), but it would be *our* layer inside the opaque
payload, not the server's — and it is out of scope here.

## 7. Build shape: `RSBS_NETPLAY`, default OFF, default build byte-unaffected

```cmake
option(RSBS_NETPLAY "Build the netplay grant relay client (experimental)" OFF)
```

With `RSBS_NETPLAY=OFF` — the default, and what every CI job and every release
build uses today:

- **no netplay source file is added to any target**, so no new symbols link;
- **no CVar is registered**, no menu entry appears, no port is opened;
- **no submodule is added**, so no CI checkout grows (ADR 0006 §7's argument,
  applied to ourselves);
- the header exposes no-op inline stubs behind `#ifdef`, so lifecycle call
  sites compile identically.

The claim is verifiable, not asserted: a CI step compares the default build's
symbol table against `main`'s and fails on any new symbol matching
`Netplay_*`/`Rsbs_Relay_*`. That is the honest form of "byte-unaffected" —
byte-identical binaries are not reproducible across toolchain nondeterminism,
but *no new linked symbols* is checkable and is the property that matters.

**No auto-connect, ever.** Even with `RSBS_NETPLAY=ON`, the client is inert
until an operator supplies a room id and explicitly connects. There is no
discovery, no default server, no telemetry, and no outbound connection in any
default path.

### Why no transport library

The wire format is a 9-byte handshake, a 20-byte join, and length-prefixed
binary frames over plain TCP. No HTTP, no WebSocket, no TLS, no framing
ambiguity. Against that, `asio`/`websocketpp` (ADR 0006 §6) would add
submodules to every checkout to supply framing we implement in a few hundred
lines of `send`/`recv` over a non-blocking socket, plus a Winsock/BSD shim
that the tree needs anyway for a Windows target. ADR 0006 argued against
carrying dependencies for inert code; that argument does not weaken when the
code is ours.

## 8. What the CI lock proves — and what it does not

The loopback harness runs two in-process client objects against an in-process
ledger implementing the same semantics as §1.3: append-only, `key`-deduped,
`ledgerBase` replay, broadcast-to-all-**including the sender**.

That last clause is load-bearing. The real server fans a new entry out to
every client on the ledger *including the one that wrote it* (`client.c`,
`multiClientCmdTransfer`), so self-echo is a real condition the client must
filter by `targetSlot`. A mock that conveniently skipped the sender would hide
a live bug — exactly the shortcut ADR 0006 §2(b) caught in the AP mock, and
the reason it is called out here rather than left to implementation taste.

**Proves:**

- a retransmitted grant yields **exactly one** item (cursor `DUPLICATE`);
- two distinct peers gifting the same item yield **two** items (the ADR 0005 §1 regression);
- received-order redemption (progressives resolve against the live save, so order changes *what you get*);
- grants recorded in one game are awarded after a **cross-game switch**;
- **late-join catch-up**: a client joining after N grants receives all N;
- **self-echo is filtered** — a sender does not award itself its own gift;
- overflow is **loud** (`sharedItemOverflowCount`) and sourced grants are backpressured, not lost;
- a malformed/foreign ledger entry is skipped without desynchronising;
- the default (`RSBS_NETPLAY=OFF`) build links no netplay symbols.

**Does not prove:**

- **that our byte framing matches the real server's.** The mock is a
  re-derivation from reading `client.c`/`ledger.c`, not the real binary. Only
  an integration run against `multi-server` proves the wire. This is the same
  *shape* of gap ADR 0006 identified — and it is **materially smaller in
  kind**, which is the honest distinction: a framing mismatch is a bug we can
  fix unilaterally, because there is no admissibility check we can fail. AP's
  gap was a wall (no client of ours could ever be admitted); this one is a
  defect class. Framing is pinned against golden byte vectors transcribed from
  the server source, and §3.4's version check fails loudly rather than
  silently misparsing.
- **real socket behaviour**: partial reads, `EAGAIN`, mid-frame disconnects,
  the server's edge-triggered epoll, TCP segmentation across frame boundaries.
  The harness exercises the codec and the state machine, not the syscalls.
- **concurrency under real scheduling.** §4.1 makes threading a non-issue by
  construction, which is the point — but "by construction" is an argument, not
  a test.
- **that the live server still behaves as read.** Pinned to `master` @
  2025-03-27, `VERSION 0x00000200`. Upstream can change; §3.4 turns that into
  a refusal rather than corruption.

## 9. Consequences

- **No server code enters this repository.** Self-hosting is documented by
  pointing at upstream's `Dockerfile.prod`.
- **No submodule, no new dependency, no `reserved[]` carve.** All three of the
  collision/CI-cost hazards that bit prior attempts are avoided by needing
  nothing.
- **ADR 0005's redemption tick gets wired**, satisfying the exact condition
  its §4 named for doing so.
- **ADR 0006 stays correct and is not superseded** — its measurement of
  Archipelago stands; this ADR takes the "no" branch it defined and carries
  its §5 list forward.
- **Blocked on PR #473.** The seam must land first; nothing here is buildable
  against `main` until it does.
- **Archipelago remains permanently out of scope** (#460 maintainer decision).
  Nothing in this design moves toward or away from it; a relay grant and an AP
  item are different objects with different authorities.

[ootmm]: https://github.com/OoTMM/multi-server
[mmrr]: https://github.com/RecompRando/MMRecompRando
