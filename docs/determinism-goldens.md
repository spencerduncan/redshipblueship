# Golden determinism digests: what they pin, and how to re-pin them

RedShipBlueShip generates one paired OoT+MM world from one seed. Two different
properties of that generator get confused constantly, and until #688 only one of
them was tested:

| Property | Question | Enforced by |
|---|---|---|
| **Reproducibility** | does the same seed produce the same world *twice in a row*? | `SeedDeterminism`, `MMPairedAttemptDeterminism` (self-diff rows) |
| **Stability** | does the same seed still produce *the world we shipped*? | `GoldenSeedDigestDefault`, `GoldenSeedDigestProfileV1`, `GoldenPairedAttemptDigest` (golden rows) |

The self-diff rows run one seed twice and compare the two runs **against each
other**. Nothing in them compares anything to a stored value, so a change that
moves every placement *deterministically* passes all of them, green and
unremarked. That is not hypothetical: a reachability gate that computed its
closure from a stale simulated inventory silently removed nine reachable hosts
from the cross-game pool, and every determinism row passed on that binary.

The golden rows close that gap. Each runs one generation and compares its digest
against a text file committed under `tests/golden/`. **A green self-diff row is
never evidence that a world did not change.** If you need to make that claim,
cite a golden row, or a measured before/after comparison.

## The files

```
tests/golden/seed-digest-default.txt      # --test rando-determinism, SHIPPED profile
tests/golden/seed-digest-profile-v1.txt   # --test rando-determinism, RSBS_DIAG_CVARS=gRandoSettings.ShuffleSongs=2
tests/golden/paired-attempt-digest.txt    # --test mm-paired-attempt, the ladder world
```

Each is the digest text the corresponding dispatch writes, one `key=value` per
line. The comparison is line-by-line after stripping CR, never a byte compare:
the digest writers use stdio text mode, so the same world writes LF on Linux and
CRLF on Windows, and a byte compare would report every field as moved on one of
them. The committed bytes are LF regardless of where a re-pin ran, because
`.gitattributes` carries `* text=auto eol=lf`.

Two OoT profiles rather than one, because a single golden pins one settings
profile's fill and says nothing about whether a change is settings-sensitive. The
`default` one is what a player actually gets; `profile-v1` is the pinned profile
the self-diff rows use, so the golden and the self-diff describe the same world
and can be read against each other. Both resolve combo direction `BOTH` at the
shipped defaults, so both cover **both crossing directions** — the reverse table
(`foreignOoTHash`, `foreignOoTCount`, and the per-slot `foreignOoT<n>` lines: OoT
checks hosting MM items) and the forward table (`foreignCount` and the per-slot
`foreign<n>` lines: MM checks hosting OoT items) are in every one of these
digests.

## Reading a failure

A mismatch is reported field by field, split into two kinds, because they demand
different reviews:

* **OUTPUT fields moved** — the world changed under the same question. Examples:
  `placementHash`, `placedCount`, `foreignOoTHash`, `foreignOoT<n>`,
  `mmPlacementHash`, `mmReachableHash`, `foreign<n>`, `mmFinalSeed`,
  `winningAttempt`. Something in the PR changed what the fill decided.
* **INPUT fields moved** — the fill was asked a different question:
  `seed`, `settingsHash`, `comboSettingsHash`, `comboSettings`,
  `ladderMasterSeed`, `sourceIsRando`. Somebody changed the pinned seed, the
  pinned settings profile, a settings default, or the canonical form a
  fingerprint hashes. An input move with outputs following it is expected; an
  input move *alone* means a fingerprint stopped describing the same record.

Any field name the checker does not recognise as an input is treated as an
output, so a field added to a digest later defaults to the strict reading.

A **missing** golden file is a failure, not a free pass. A row that silently pins
nothing is exactly the defect #688 was filed about.

## Re-pinning

One command, and it is the only supported way:

```
cmake --build build-cmake --target regen-golden-digests      # Linux: wrap in xvfb-run
```

It regenerates every golden from the binary in that build directory, under
exactly the dispatch and environment its CTest row checks it with — both come
from the single `REDSHIP_GOLDEN_DIGESTS` table in `CMake/SingleExecutable.cmake`,
so a golden cannot be re-pinned under a different profile than the row checks it
under. It needs what the `rando` tier needs: `oot.o2r`/`mm.o2r`/`soh.o2r` staged
beside the binary and a GL-capable display.

The target is deliberately not part of `all` and is not a CTest row. Re-pinning is
an act of authorship; a target that regenerated goldens as a side effect of a
build would turn the oracle back into a no-op.

**The diff of the golden files is the review artifact.** The rules:

1. A re-pin lands in **its own commit**, containing the golden files and nothing
   that is not needed to explain them.
2. The commit message names **which fields moved** and **why the new world is the
   intended one**. "Re-pin goldens" is not a re-pin message; neither is "tests
   were failing".
3. If the change was *not* supposed to move a world, do not re-pin. The red row
   is the bug report.
4. Re-pinning is a save-invalidating act in spirit: a player generating the same
   seed before and after gets different worlds. Say so in the PR body, as the
   pre-release save policy in `.claude/worker-prompts.md` requires.

## Platform portability

The golden is resolved as, in order:

```
tests/golden/<name>.<host-system>.txt   # Windows / Linux / Darwin
tests/golden/<name>.txt                 # portable — the normal case
```

Only portable files exist today: the same seed produces the same digest on
Windows (MSVC) and Linux (GCC). That is a *measured* claim — the goldens were
generated on a Windows workstation and the Linux CI leg checks those same files —
and it stays answerable at any time from two logs, because every golden row prints
its full digest to the CTest log before comparing.

It is also a claim with a mechanism behind it, which is why it was worth
measuring rather than assuming. The fill's randomness is `ShipUtils::next32` (a
PCG-style `state = state * 6364136223846793005 + 11634580027462260723` with an
integer xorshift and `std::rotr`) drawn through `ShipUtils::Random`'s rejection
loop. No `std::shuffle`, no `std::uniform_int_distribution`, no floating point in
the draw path — all three of which are implementation-defined and would differ
between libstdc++ and MSVC's STL. Ordered containers (`std::map`, `std::set`) and
fixed enum ranges carry the iteration order the digests fold.

**A per-platform golden may be added only when a platform difference has been
measured, and this page must then say which field differs and why.** Adding one to
make a row go green would convert a real finding — *the same seed gives two
players different worlds* — into a silenced test.

## Adding a golden

Add one line to `REDSHIP_GOLDEN_DIGESTS` in `CMake/SingleExecutable.cmake`
(`<ctest-name>|<golden-name>|<dispatch>|<digest-env-var>|<extra-env>`), then run
the regen target and commit the new file. The dispatch must write its digest to
the named environment variable's path and must emit one `key=value` per line;
free text cannot be diffed field by field and the checker refuses it.
