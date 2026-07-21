# ADR 0006: Archipelago as netplay transport costs an apworld, not a client — 1b stays deferred

- Status: **Proposed** (2026-07-21)
- For: #460 (netplay increment 1), the 1b transport decision left open by
  `docs/netplay-increment-1-spike.md` §7 and §9
- Composes with: ADR 0005 (per-source grant cursors, #463) — the local seam a
  transport would write against
- Measured against `origin/main@d075aeaf` by building the client far enough to
  find the wall, then reading the Archipelago protocol and world specs to
  measure it. Every claim below cites a primary source; none is quoted from
  the spike.

## Decision

**Do not land an Archipelago transport now.** The spike's §9 recommendation to
defer 1b was correct, and this document replaces its *reasoning* with a
measurement: the blocker is not client effort, it is that **Archipelago cannot
route a single item to us until an RSBS `apworld` exists**, and an apworld is a
full randomizer-logic implementation for both games in Python — a strictly
larger problem than the one RedShipBlueShip has already solved in C.

A working C++ client was built to the point of proving this. It is preserved
unmerged on `claude/netplay-transport` and **deliberately not merged**; §5
records what is worth salvaging when 1b is revisited.

This ADR decides *not to build*. It does not reopen the transport comparison —
if the maintainer answers §4's question in the affirmative, Archipelago is still
the best-licensed option, and §5's design still applies.

## 1. Why the client is not the work

### 1.1 The wall: `Connect` is validated against a generated multiworld

Archipelago's own protocol spec, [`docs/network protocol.md`][proto] lines
123–124, defines the two refusal modes:

> InvalidSlot indicates that the sent 'name' field did not match any auth entry
> on the server.
> InvalidGame indicates that a correctly named slot was found, but the game for
> it mismatched.

Both are properties of the **room the server generated**, not of the client. An
AP server does not accept arbitrary clients announcing arbitrary games; it
accepts clients matching slots in a multiworld that was generated ahead of time
from apworlds. A client announcing game `"RedShipBlueShip"` is refused by every
Archipelago server that exists today, and will be until an RSBS apworld is
written, packaged, and used to generate the room.

This is the whole finding. The transport is not gated on transport work.

### 1.2 What an apworld actually requires

From [`docs/world api.md`][worldapi], a world must supply — in Python, as a
subclass of `AutoWorld.World`:

| Requirement | §  | Cost for RSBS |
|---|---|---|
| Item table, unique non-numeric names → stable ids | Items | Every `RG_*` **and** `RI_*`, re-expressed |
| Location table, unique non-numeric names → stable ids | Locations | Every check in **both** games |
| Region graph with a `Menu` origin region | Regions | Both overworlds, both dungeon sets |
| Entrances connecting all regions | Entrances | Both games, plus the cross-game link |
| Access rules (`set_rules`) | Access Rules | **The combined logic — the OoTMM-scale problem** |
| Item rules, events, `create_regions` / `create_items` | Generation | Full generation integration |
| apworld packaging | [apworld specification.md][apspec] | Release channel, versioning, data-package checksums |

The access-rules row is the one that matters. Writing it means expressing
RedShipBlueShip's cross-game logic **a second time, in a second language, in a
second repository, under a second project's release cadence** — and keeping the
two in agreement forever, because a divergence between the apworld's logic and
the game's own is an unwinnable seed.

### 1.3 The placement-authority conflict

RSBS already owns placement: `ComboContext.foreignPlacements` and
`sharedRandoSettingsHash` (`src/common/context.h`) exist precisely because this
tree generates its own cross-game item distribution.

Archipelago generation *also* owns placement — that is what an apworld is for.
These are not composable; one has to be authoritative. Deciding which is a
product decision about what RedShipBlueShip *is* (a self-contained combo
randomizer, or an AP world), and it has never been asked, let alone answered.
No amount of client code resolves it.

## 2. What the built client proved, and where it stops

The preserved branch has a complete, poll-driven `apclientpp` client:
handshake, room-fingerprint binding, index-ordered application, bounded inbox,
gap→`Sync` recovery, suspend/resume gating. It is good code and it is inert —
it is wired into no build file (verified: no `CMakeLists.txt` or `.cmake` in the
tree references `src/netplay` or `RSBS_NETPLAY`).

Two defects found while auditing it are worth recording, because both are the
kind that a loopback harness would have hidden:

**(a) It is receive-only.** The client never sends `LocationChecks`. It can
consume a multiworld item stream but can never contribute one. A participant
that only receives is not a multiworld participant; it is a gift recipient.
Nothing in the mock harness would have caught this, because the mock never
expects a check.

**(b) The mock validates nothing that matters.** `mock_ap_server.hpp` replies
`Connected` to any `Connect` without inspecting the announced game or slot —
exactly the field a real server refuses on (§1.1). The loopback suite would
therefore have gone green while the real integration remained impossible. This
is the specific failure mode the spike's §8 optimism about CI-lockability did
not anticipate: the harness can lock our *semantics* (and ADR 0005 does that
well, transport-free) but it cannot lock our *admissibility* to a real room.

A third item is a reasoning error rather than a defect: `netplay_items.h`
justifies its `0x52530000` id base as being "far outside every id range shipped
by existing Archipelago worlds, so a real AP room's foreign item ids can never
alias ours." Per [`network protocol.md`][proto] line 728, ids are namespaced
per game — *"Any names and IDs are only unique in its own world data package,
but different games may reuse these names or IDs"* — so aliasing was never the
risk the comment defends against, and the real constraint (our ids must match
our apworld's data package byte for byte) is the one left unaddressed. Any
revived mapping should be derived from the apworld, not chosen by the client.

## 3. Dependency state (independent of §1)

Both hard dependencies are unmerged, so even a client worth landing could not
land today:

| Dependency | State (2026-07-21) |
|---|---|
| ADR 0005 seam, #463 — `Combo_SubmitSourcedGrant` / `Combo_GetGrantCursor` | **Open, BLOCKED.** Verified absent from `origin/main`; exists only on `claude/netplay-grant-foundation`. |
| #440 stale session state, PR #464 | **Open, BLOCKED.** The spike §5 calls this a hard dependency: a stale cursor lets a dead room's grants leak into a fresh seed. |

The WIP branch's own `src/common/context.h` carve (`netplayApAppliedCount`,
`netplayApRoomFingerprint`, `reserved[]` 332 → 324) **collides byte-for-byte**
with ADR 0005's carve (`grantCursors[8]` at offset 672,
`sharedItemOverflowCount` at 736, `reserved[]` 332 → 264). Both take the front
of `reserved[]` immediately after `foreignPlacements`.

The collision resolves in ADR 0005's favour and the WIP carve is **discarded**,
because 0005's per-source cursor was designed to host exactly this: index `i`
maps to seq `i+1`, and `Combo_GetGrantCursor` is the `Sync` resume point. A
separate AP-specific cursor would have been a second, redundant answer to a
question already answered — and a second path into `Combo_RecordSharedItem`,
which #460 explicitly forbids. **No carve from `reserved[]` is made by this
ADR.**

## 4. The question this hands back

The spike (§7) listed two open questions and correctly called them *"a
maintainer call, not an engineering one."* Measurement collapses them into one:

> **Is RedShipBlueShip willing to be an Archipelago world** — to express its
> cross-game logic a second time in Python, cede placement authority to AP
> generation, and track the Archipelago project's release cadence?

If **no**, Archipelago is off the table permanently, and 1b should be re-scoped
against a transport that does not require a world definition — a direct
peer-to-peer grant exchange, or a relay in the shape of [OoTMM's
multi-server][ootmm] (C, MIT), which the closest sibling project chose over
Archipelago for plausibly these reasons.

If **yes**, the apworld is the project and the client is its epilogue; sequence
the Python work first and revive §5 afterwards.

## 5. Salvage list

When 1b is revisited, these decisions from the preserved branch were audited
and are worth keeping — they are transport-shaped, not AP-shaped, and each
closes a verified Anchor hazard from the spike §6:

- **No receive thread.** `apclientpp` is poll-driven; the socket is serviced
  only inside a `Tick` called from the game thread, so every callback lands on
  the owning thread. Anchor hazards #1 and #2 cannot exist by construction
  rather than by discipline. Keep this property under any transport.
- **Bounded inbox with recoverable drop.** Overflow drops the tail *without*
  advancing the cursor and schedules a replay, so bounded memory costs no
  items. This is the correct shape of Anchor hazard #3's fix, and it composes
  with ADR 0005's backpressure (refused grant leaves the cursor unmoved).
- **Explicit suspend latch.** `OnGameSuspend` stops *applying* while polling
  continues; `OnGameResume` drains in received order. Matches the
  resume-contract lesson that the lifecycle has no implicit inverse.
- **Room-fingerprint binding with refuse-on-mismatch.** A cursor is meaningless
  against a room that did not produce it; binding is explicit and rebinding is
  an operator action, never automatic. This is the right instinct regardless of
  transport, and it partially overlaps #440 — but does not substitute for it.

## 6. License verification

Checked by reading each project's actual `LICENSE`/`COPYING`, not GitHub's
classifier — which reports `NOASSERTION` for all five, including Archipelago
itself, so the spike's "both MIT" claim was not verifiable the way it was made.

| Project | Pin | Actual license |
|---|---|---|
| `black-sliver/apclientpp` | `65638b74` | **MIT** |
| `black-sliver/wswrap` | `47438193` | **MIT** |
| `zaphoyd/websocketpp` | `56123c87` (0.8.2) | **BSD-3-Clause** + bundled zlib/MIT/Aladdin components |
| `chriskohlhoff/asio` | `12e0ce9e` (1.30.2) | **Boost Software License 1.0** |

**All four are permissive and compatible**, so nothing here blocks adoption —
but only two of the four are MIT. Any future statement of this dependency set
should say "MIT, BSD-3-Clause and BSL-1.0", not "all MIT".

Unchanged and still decisive against the alternative: the Anchor **server**
carries no license file at all (all-rights-reserved by default), which is why
lighting up the vendored Anchor client remains a no-go independent of this ADR.

The nearest MM-side prior art, [MMRecompRando][mmrr], is **GPL-3.0** and was
**not** read, copied, adapted, or vendored during this work.

## 7. Consequences

- **No submodules are added.** The four dependencies stay out of the tree until
  something in-tree can use them. Adding them now would put four extra
  checkouts on every CI job to support code that cannot complete a transaction.
- **No build option is declared.** `RSBS_NETPLAY` is not introduced;
  `BUILD_REMOTE_CONTROL` remains undeclared. The default build is unaffected in
  the strongest available sense — no new sources, no new symbols, no new
  submodules, docs only.
- **No `.redsave` format change.** §3's carve is discarded; ADR 0005's stands
  alone.
- **#460 stays open** for 1b, now blocked on a stated question (§4) rather than
  on unstated ones.
- **ADR 0005 is unaffected** and remains the right thing to land: it is
  transport-agnostic, and every property it locks is needed under any answer to
  §4.

[proto]: https://github.com/ArchipelagoMW/Archipelago/blob/main/docs/network%20protocol.md
[worldapi]: https://github.com/ArchipelagoMW/Archipelago/blob/main/docs/world%20api.md
[apspec]: https://github.com/ArchipelagoMW/Archipelago/blob/main/docs/apworld%20specification.md
[ootmm]: https://github.com/OoTMM/multi-server
[mmrr]: https://github.com/RecompRando/MMRecompRando
