# Netplay increment 1: multiworld-lite

> **Status: Proposed.** Design spike, dated 2026-07-21, verified against
> `origin/main@b33089b4`. Nothing here is committed work. This document exists to
> make one decision answerable: *is cross-instance `SharedItem` delivery the right
> first netplay increment, and if so on what transport?*
>
> Companion: `docs/unified-surface-findings.md` §5, which established that the
> upstream Anchor client is already vendored and inert. This spike goes a layer
> deeper and, on the transport question, **reaches a different conclusion than §5
> implied** — see §7.

## TL;DR

**Go — with the scope re-cut.** The valuable, low-risk, CI-lockable part of
increment 1 is not the wire at all. It is the four *local* semantic gaps that any
remote grant hits the moment it arrives, all of which are invisible today because
the only producer is an in-process cross-game hand-off:

1. **Redemption never fires without a game switch.** `Combo_RedeemSharedItemsForGame`
   is called only from each game's presence-gated arrival point. A remote grant for
   the game you are *already in* would sit un-redeemed until you switched away and
   back.
2. **The producer de-dups on `(originGame, id)`,** which is correct for a re-fired
   in-process producer and **wrong for a network feed** — two peers sending you the
   same item before you redeem collapse into one item.
3. **`SharedItem` is 4 bytes with no sender or sequence field,** so "origin-tagged"
   in the network sense (which *peer* sent it) is not representable without a
   `.redsave` format bump.
4. **The array is 64 slots and drops silently when full** (`stderr` log, `-1`
   return), with no backpressure path back to a sender.

None of the four needs a socket to fix, and all four are lockable ROM-free in the
existing `--test` harness. That is phase **1a**, and it is worth doing on its own
merits: it is also the prerequisite for *any* transport, so it cannot be wasted work.

On transport (phase **1b**), the licensing picture inverts the assumption in §5:
**Archipelago is the cleaner path than Anchor.** Archipelago is MIT, `apclientpp`
is MIT, and HarbourMasters already ship a native in-process AP client for SoH built
on that exact library — whereas the Anchor **server carries no license at all**
(all-rights-reserved by default). See §7.

**Rough size:** 1a ≈ 3–5 days, fully CI-lockable. 1b ≈ 1.5–3 weeks depending on
transport, mostly operator-gated. Recommend committing to 1a now and re-deciding 1b
against a working loopback harness.

---

## 1. Where a remote grant plugs in

The consumer side needs no new concept. `src/common/shared_items.h` already defines
the exact shape a remote grant wants:

```
producer  → Combo_RecordSharedItem(GameId originGame, uint16_t id)
consumer  → Combo_RedeemSharedItemsForGame(GameId, ComboSharedItemAward, void*)
award     → OoT_ForeignItem_Give(id)  /  MM's equivalent
durability→ gComboCtx.sharedItemsTagged[64], serialized into every .redsave
single-use→ RSBS_SHARED_ITEM_REDEEMED, set after the award callback returns
```

A remote grant is **one more producer call site** feeding `Combo_RecordSharedItem`.
Today there are two producers:

| Producer | Site | Path |
|---|---|---|
| MM foreign pickup | `games/mm/2s2h/Rando/Foreign.cpp:193` | direct record |
| Staged / switch-boundary | `Combo_CommitStagedSharedItems` at both `Game_Suspend`s | outbox flush |

A network producer is a third, structurally identical to the first: it calls
`Combo_RecordSharedItem` from the game thread with an `(originGame, id)` pair
decoded off the wire. Dedup, persistence and arrival-time redemption come free —
**subject to the four caveats in §3**, which is where the real work is.

### The consumer gap

This is the load-bearing finding of §1. The redemption points are:

- `OoT_ConsumeSharedItems` → `games/oot/soh/GameExports_SingleExe.cpp:1055`, called
  from `OoT_Play_Init`'s presence-gated startup-entrance consumption
  (`games/oot/src/code/z_play.c`)
- `MM_ConsumeSharedItems` → `games/mm/2s2h/GameExports_SingleExe.cpp:1718`

Both run **only on a cross-game arrival**, once per arrival. `shared_items.h`
documents this deliberately as *"applies on next switch only"*, and for in-process
cross-game hand-offs it is exactly right: the item was picked up in the other game,
so by construction you are arriving.

A remote grant breaks that assumption. The sender is another *process*, not the other
game, so the grant's target game is very often the one you are already playing. It
would be recorded, persisted, and then sit there — correct but undelivered — until
the player happened to switch to the other game and back.

**Increment 1 therefore needs a third redemption trigger**: a periodic, gameplay-gated
redemption tick that calls the *same* `Combo_RedeemSharedItemsForGame` with the *same*
award callback. The natural home is an `OnGameFrameUpdate`-class hook gated on "a save
is loaded and we are in normal gameplay" — the same gate `OoT_ForeignItem_Give`'s
prerequisites already assume. No new give path, no new state; only a new *when*.

This is the single largest correctness item in 1a and it is entirely testable without
a socket.

---

## 2. Transport options

Three candidates. The evaluation changed materially once licensing was checked.

### (a) Vendored Anchor `Network` base class

`games/oot/soh/Network/Network.h` — a ~120-line TCP client: SDL2_net, one receive
thread, null-delimited JSON accumulation, virtual `OnIncomingJson`. Already in tree,
already compiled, SDL2_net already staged in vcpkg/apt/Docker.

**Against:** the Anchor *server* — [garrettjoecox/anchor](https://github.com/garrettjoecox/anchor)
— **carries no license file** (GitHub API reports `"license": null`; no `LICENSE` at
the repo root). Unlicensed means all-rights-reserved by default. Self-hosting is
community-normal, but shipping or depending on it is legally gray, and the vendored
client code currently rests on no explicit grant either. Also inherits the hazards in
§6 wholesale.

### (b) Clean minimal reimplementation

The protocol is trivial: null-delimited JSON objects over TCP, each with a `type`
key. Reimplementing a grant-only client is maybe 200 lines against `nlohmann::json`
(already a dependency) and either SDL2_net or a raw socket. Owes nothing to the
unlicensed server because the wire format is not the copyrightable part — but it also
gets us a bespoke protocol nobody else speaks, and a server we would have to write
and host.

### (c) Archipelago via `apclientpp`

- Archipelago: **MIT** ([LICENSE](https://github.com/ArchipelagoMW/Archipelago/blob/main/LICENSE))
- `apclientpp`: **MIT** ([black-sliver/apclientpp](https://github.com/black-sliver/apclientpp))
- Protocol: WebSocket + JSON, documented at
  [network protocol.md](https://github.com/ArchipelagoMW/Archipelago/blob/main/docs/network%20protocol.md)

Item delivery is `ReceivedItems`, carrying a server-authoritative monotonic `index`
plus a `NetworkItem[]`. `Connect`/`Connected`/`RoomInfo` handshake; `LocationChecks`
for our side of the ledger; mandatory `Sync` + `LocationChecks` resync when `index`
does not match what the client expected.

Deps are heavier than (a): nlohmann/json (have it), wswrap, websocketpp + asio,
OpenSSL for `wss`, zlib. Real integration cost, but a well-trodden one.

**The decisive precedent:** HarbourMasters already ship a native, in-process AP
client for SoH — [HarbourMasters/Archipelago-SoH](https://github.com/HarbourMasters/Archipelago-SoH)
(`oot-soh` branch, `worlds/oot_soh`), latest release **1.4.2 dated 2026-07-03**. It
originated as [aMannus/Shipwright#76](https://github.com/aMannus/Shipwright/pull/76)
(208 commits), and that branch's `.gitmodules` declares `subprojects/apclientpp`
alongside libultraship / ZAPDTR / OTRExporter — **the same submodule shape as our
tree**. Release notes state the intent to fold AP support into mainline SoH.

Note this is *not* the mainline Archipelago OoT world, which is emulator-based
(BizHawk 2.10+ and `connector_oot.lua`, per the
[setup guide](https://archipelago.gg/tutorial/Ocarina%20of%20Time/setup_en)) and
therefore useless to us. The SoH fork is the relevant art.

---

## 3. Message shape, idempotency, ordering

### Wire shape

Minimal honest grant message, transport-independent:

```json
{
  "type": "SHARED_ITEM_GRANT",
  "grantId": "<sender-scoped unique id>",
  "senderId": 1234,
  "seq": 87,
  "originGame": "OOT",
  "id": 42
}
```

`originGame` + `id` are exactly `Combo_RecordSharedItem`'s parameters — `GAME_OOT` ⇒
`id` is an OoT `RandomizerGet` (`RG_*`), `GAME_MM` ⇒ an MM `RandoItemId` (`RI_*`).
`grantId`/`senderId`/`seq` are the parts we do **not** currently have anywhere to put.

### What the REDEEMED bit gives you — and what it does not

**Gives you, for free:**
- Single-use *per slot*. Once awarded, the bit is set and the entry is never cleared,
  so no slot can award twice — verified by `test_foreign_items.c:160,169` (second
  redeem returns 0) and `test_shared_state_roundtrip.c:241,262`.
- Durability across save/load and across a game switch: the array is process-global
  and serialized in every `.redsave`.

**Does not give you:**

1. **Duplicate-delivery protection before redemption.** This is the important one, and
   it is worse than "not provided" — the existing de-dup is actively wrong for network
   use. `src/common/shared_items.c:56` returns an existing slot unchanged when it finds
   a match on `(originGame, id)` that is **not yet redeemed**:

   ```c
   if (slot->originGame == (uint8_t)originGame && slot->id == id &&
       (slot->flags & RSBS_SHARED_ITEM_REDEEMED) == 0) {
       return i; // already pending — leave it exactly as-is
   }
   ```

   For an in-process re-fired producer that is precisely the desired behaviour and the
   header says so. For a network feed it silently **merges two legitimately distinct
   grants**: two peers each sending you a Deku Nut before you next redeem yields one
   Deku Nut. A retransmit and a genuine second gift are indistinguishable at this API.

   Increment 1 must therefore dedup on `grantId` *above* `Combo_RecordSharedItem`, and
   must reach the array through a path that does **not** apply content de-dup. Options:
   a `Combo_RecordSharedItemUnique()` variant, or an explicit "already de-duped by
   caller" flag. Do not paper over this by making network grants unique-by-id.

2. **A place to store `grantId`.** `SharedItem` is 4 bytes — `originGame` (1),
   `flags` (1), `id` (2) — and `src/common/context.h:182-186` static-asserts both the
   size and every member offset as `.redsave` **format**. There is no spare field.
   Options, in increasing cost:
   - keep the seen-`grantId` set in a *separate* `gComboCtx` region (still needs a
     format bump to persist, per the `RSBS_COMBO_CONTEXT_RECORD_SIZE` /
     `RSBS_SAVE_VERSION` coupling documented at `context.h:124`);
   - keep it RAM-only and accept that a save/reload re-opens a duplicate window;
   - let the transport own the cursor (Archipelago's `index` does exactly this — see
     below).

3. **Ordering.** Nothing in the array preserves send order; redemption is slot order,
   which is first-free order. For fungible items this is invisible. For progressive
   items resolved against the live save (`OoT_ForeignItem_Give` → `GetGIEntry`
   resolution) order changes *what you get*. Increment 1 should redeem in received
   order and say so.

4. **Capacity / backpressure.** `RSBS_SHARED_ITEM_CAP` is 64
   (`context.h:138`). On overflow `Combo_RecordSharedItem` logs to `stderr` and returns
   `-1`; `Combo_CommitStagedSharedItems` then *drops* the staged entry rather than
   leaking it forward. For two games hand-shaking a few items that is fine. For a
   multiworld feed, 64 un-redeemed items is reachable — and a dropped grant is a lost
   item with no sender-side signal. Increment 1 needs either a bounded receive queue
   that stops acking, or a raised cap, or both.

### The Archipelago wrinkle

Archipelago's `ReceivedItems` carries a server-authoritative monotonic `index`, and
the client must `Sync` when it diverges. That composes with our model but **inverts
ownership**: AP owns the cursor, and our `REDEEMED` bit degrades to a local
idempotency guard beneath it. Concretely, the `.redsave` would have to persist AP's
index too, or every load triggers a full resync and re-delivers the whole item stream
into an array whose content de-dup (caveat 1) would then quietly eat the duplicates.
That interaction is a genuine trap and is worth an explicit test.

---

## 4. Across a game switch

The array itself crosses cleanly — it is process-global and the freeze/restore
machinery does not touch it (`shared_items.h` header, and
`Context_FreezeState` moves only a SaveContext blob). A grant recorded while in MM
for `GAME_OOT` is still there on arrival in OoT.

What does *not* survive the switch is anything transport-side. Every hazard in §6
lands here: with the current Anchor structure the receive thread keeps running while
OoT is suspended, its queue keeps growing with nothing draining it, and
`PLAYER_UPDATE` keeps mutating `clients` against frozen structures. Increment 1 must
gate the network client on `Game_Suspend` / `Game_Resume` explicitly — the lifecycle
has no implicit inverse, which is the same lesson recorded in the single-exe
resume-contract work.

Minimum contract for 1b:
- `Game_Suspend`: stop *applying* grants; keep the socket open but buffer bounded,
  or disconnect cleanly. Do not leave an unbounded producer running against a stopped
  consumer.
- `Game_Resume`: drain the buffer through the redemption tick, in received order.

---

## 5. Across a soft reset — blocked on #440

**This is a hard dependency, not a footnote.**
[#440](https://github.com/spencerduncan/redshipblueship/issues/440) records that
cross-game session state is process-global and **nothing invalidates it** on soft
reset or new game: frozen blobs, both shadows, and `gComboCtx` session fields
*including `sharedItemsTagged`* all survive. The operator-observed symptom was a new
seed inheriting the previous session's MM clock and stray-fairy flags; #440 correctly
files it as cross-seed contamination.

For netplay this gets worse in a specific way. Today the contaminating writes come
only from the player's own prior session. With a network producer, a stale
`sharedItemsTagged` can carry **another player's** grants from a dead room into a
fresh, unrelated seed — items that were never findable in that world. And because
the entries are un-redeemed, the new session's first arrival will happily award them.

Increment 1 should not ship its network producer before #440's invalidation contract
lands, and 1a should extend #440's headless test to assert that a *remotely sourced*
grant is invalidated by the same reset path. Sequencing 1a after (or alongside) #440
is the cheap ordering.

---

## 6. Verified hazards in the vendored Anchor client

Measured against `origin/main@b33089b4` while making the `clientVersion` fix.
**Documented, deliberately not fixed** — all four are inert while
`BUILD_REMOTE_CONTROL` is undeclared, and fixing them only matters once someone
intends to flip it. Any of them is a blocker for option (a) in §2.

| # | Hazard | Evidence | Why it bites the combo |
|---|---|---|---|
| 1 | **Receive thread survives `Game_Suspend`** | `Network::Enable/Disable` (`Network.cpp:7,28`) are the only lifecycle controls; the loop is `while (isEnabled)`. `Anchor::Enable/Disable` are called *only* from `OTRGlobals.cpp:1830` (OoT init) and `:1919` (OoT shutdown), both gated on `CVAR_REMOTE_ANCHOR("Enabled")`. Nothing in the switch path touches either. | The thread outlives the game it belongs to. It keeps receiving, parsing and dispatching while OoT is frozen. |
| 2 | **`PLAYER_UPDATE` handled on the network thread** | `Anchor.cpp:106-109` — `if (packetType == PLAYER_UPDATE) { HandlePacket_PlayerUpdate(payload); return; }`, i.e. *before* the queue. Every other type is queued for the game thread. The handler (`Packets/PlayerUpdate.cpp:80-117`) mutates `clients[clientId]` with **no mutex**. | An unsynchronised `std::map` write racing game-thread readers (`RefreshClientActors`, `AnchorRoomWindow::DrawElement`, the DummyPlayer actor callbacks) — a data race even single-game, and it writes against frozen OoT structures across a switch. `AnchorClient::player` is a raw `Player*` into OoT actor memory. |
| 3 | **Unbounded incoming queue growth across a switch** | `incomingPacketQueue` (`Anchor.h:77`, plain `std::queue`) is drained only by `ProcessIncomingPacketQueue`, bound at `HookHandlers.cpp:98` to `COND_HOOK(OnGameFrameUpdate, isConnected, ...)`. OoT frame hooks stop firing when OoT is suspended; hazard 1 keeps the producer running. | Producer runs, consumer stops, queue has no bound. Switch away for a few minutes on a busy room and come back to a flood — memory growth plus a burst of stale state applied at once. |
| 4 | **`UPDATE_TEAM_STATE` serializes only `gSaveContext`** | `Packets/UpdateTeamState.cpp` — `payload["state"] = gSaveContext;` plus scene flags and rando `itemLocations`. No `gComboCtx`. | `sharedItemsTagged`, `foreignPlacements` and all MM state are invisible to peers. Two combo clients "syncing" would agree on OoT and silently diverge on everything that makes this a combo. The protocol needs a `gComboCtx` slot before dual-game sync means anything. |

Hazard 4 is the one that most constrains reuse: Anchor's team-state model is
whole-save replacement, which is a fundamentally different (and much more invasive)
sync philosophy than the item-grant queue increment 1 wants. That is an argument for
not building increment 1 on Anchor's semantics even if we reuse its socket.

A fifth hazard — the stock-identical `clientVersion` — is **fixed** by the change that
accompanies this doc. It is worth restating why it was load-bearing rather than
cosmetic: version equality is a **hard bidirectional packet filter** (`Anchor.cpp:101`
drops every packet from a mismatched peer except the two types that carry the version
itself), so advertising a byte-identical stock SoH build string was not a mislabel —
it was an open door into public stock rooms.

---

## 7. Archipelago vs Anchor as the first move

The survey changed the answer here, so the reasoning is worth stating plainly.

**Licensing runs the opposite way to the assumption.** Anchor looked like the cheap
option because it is already vendored. But its server is unlicensed
(all-rights-reserved by default), while Archipelago and `apclientpp` are both MIT.
The option that appeared legally gray in §5 is the *only* one that is legally gray.

**The OoT half is substantially solved by someone else.** Archipelago-SoH is a native
in-process client, actively released (1.4.2, 2026-07-03), built on the same submodule
shape we use, with stated intent to merge into mainline SoH. Tracking it is plausible
in a way that maintaining a bespoke protocol is not.

**The MM half is greenfield either way.** No 2S2H Archipelago client exists — the
only public signal is [2S2H discussion #1533](https://github.com/HarbourMasters/2ship2harkinian/discussions/1533)
(opened 2026-02-01, zero comments). The nearest MM art is
[MMRecompRando](https://github.com/RecompRando/MMRecompRando), which is **GPL-3.0**
and targets Zelda 64: Recompiled — readable for design, not copyable into this tree.
And the MM Anchor client is gated on the `2ship_enh` migration regardless. So the MM
side offers no reason to prefer Anchor.

**For reference, OoTMM** — the project that inspired this one — rolled its own relay
([OoTMM/multi-server](https://github.com/OoTMM/multi-server), C, MIT) rather than
using Archipelago, and supports async multiworld through it. Worth knowing that the
closest sibling project chose neither of our options.

**Two open questions** before 1b commits to Archipelago:
1. Does `oot_soh`'s location/item ID space collide with our `SharedItem` origin
   tagging? Our `id` is a per-game `RG_*`/`RI_*`; AP's is a flat global namespace.
   This needs a concrete mapping decision, not a hand-wave.
2. Would we track HarbourMasters' AP fork or diverge? Tracking inherits their world
   definition and their upgrade cadence; diverging is more freedom and more
   maintenance. This is a maintainer call, not an engineering one.

**Recommendation:** do not decide 1b yet. Build 1a — which is transport-agnostic and
needed under every option — and let the loopback harness be the thing that makes the
1b decision concrete rather than speculative.

---

## 8. What a loopback harness can lock ROM-free

The existing runner is a `gTests[]` table in `src/common/test_runner.cpp`
(`{name, description, fn}`, dispatched via `redship --test <name>`), registered as
CTest through `redship_add_test()` in `CMake/SingleExecutable.cmake` — one appended
line per test, enforced by `CMake/CheckTestRegistration.cmake`. `test_foreign_items.c`
and `test_shared_state_roundtrip.c` already drive the exact machinery below with no
ROM and no display.

**Lockable in CI, no ROM, no operator (all of phase 1a):**

| Property | Test shape |
|---|---|
| Grant decode → `Combo_RecordSharedItem` round-trips `(originGame, id)` | feed a synthetic message through the decoder, assert slot contents |
| **Duplicate `grantId` delivers once** | same `grantId` twice → one slot, one award |
| **Distinct grants of the same item both deliver** | the §3 caveat-1 regression; two `grantId`s, same `(originGame,id)`, un-redeemed → **two** awards |
| Redemption without a game switch | drive the new redemption tick directly, assert award fires with no switch |
| Received-order redemption | interleave grants, assert award order |
| Overflow behaviour is explicit | push past `RSBS_SHARED_ITEM_CAP`, assert the defined policy (reject-and-signal, not silent drop) |
| Persistence | record → `.redsave` save/load → assert un-redeemed grants survive and redeemed stay redeemed (extends `test_save_roundtrip.c`) |
| Switch crossing | record for OoT while in MM → switch → assert award (extends `test_shared_state_roundtrip.c`) |
| **Reset invalidation** | #440's contract, extended to a remotely-sourced grant |
| Loopback pair | two in-process client objects over a socketpair / in-memory channel; assert a grant sent by A is awarded by B |

That last row is what makes this "multiworld-lite" rather than "a queue refactor": a
genuine two-instance exchange, fully deterministic, no server, no ROM, no display.

**Operator-only (cannot be locked in CI):**
- Anything against a real server (Anchor room or AP room) — network, accounts, a
  second machine.
- The actual item *give* landing correctly in-game — `OoT_ForeignItem_Give` resolves
  progressives against the live save and dispatches into `OoT_Item_Give` /
  `Randomizer_Item_Give`, which needs a booted, ROM-backed game.
- Anything involving MM's give path while `2ship_enh` is unmigrated.
- Switch-while-connected under real traffic (hazards 1–3 in combination).
- Long-session behaviour: reconnect, server restart, queue drain after a long suspend.

The split is favourable: every *semantic* question is CI-lockable, and the
operator-only residue is all "does the real socket and the real give work", which is
exactly the part a loopback harness is not supposed to answer.

---

## 9. Sizing and recommendation

| Phase | Scope | Size | Validation |
|---|---|---|---|
| **1a — grant semantics, transport-free** | redemption tick without a switch; `grantId` dedup path that bypasses content de-dup; received-order redemption; defined overflow policy; loopback pair harness; reset invalidation with #440 | **~3–5 days** | **fully ROM-free in CI** |
| **1b — real transport** | pick (a)/(b)/(c); lifecycle gating on `Game_Suspend`/`Game_Resume`; bounded receive queue; move `PLAYER_UPDATE`-class work off the network thread if reusing Anchor; ID-space mapping if Archipelago | **~1.5–3 weeks** | mostly operator-gated |

Dependencies: **#440 must land first or alongside** (§5). `BUILD_REMOTE_CONTROL`
stays undeclared until 1b actually ships something.

### Go / no-go

**GO on 1a.** It is small, it is entirely CI-lockable, it fixes four real semantic
gaps that exist today regardless of netplay, and it is a prerequisite for every
transport option — so it cannot be invalidated by the 1b decision. It also converts
the 1b choice from a paper comparison into an experiment.

**DEFER 1b** pending (i) #440, and (ii) a maintainer call on the two open questions
in §7. If forced to choose today: **Archipelago via `apclientpp`**, on licensing and
on the strength of the Archipelago-SoH precedent — *not* Anchor, despite Anchor being
the code already in the tree.

**NO-GO on lighting up Anchor as-is** (increment 0 in the findings doc's table). The
four hazards in §6 are all live on that path, its team-state model (whole-save
replacement) is the wrong shape for an item-grant increment, and its server has no
license. The vendored client's value is as a reference implementation and a socket,
not as a foundation.
