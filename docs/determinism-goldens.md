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
profile's fill and says nothing about whether a change is settings-sensitive.
`default` sets no `RSBS_DIAG_CVARS` at all, so it is the shipped settings profile;
`profile-v1` is the pinned profile the self-diff rows use, so that golden and that
self-diff describe the same world and can be read against each other. Neither is
literally the world a player gets — see "The archive set is part of the pin" — but
both are worlds this generator produces, and a change to the fill moves them the
same way it would move a player's. Both resolve combo direction `BOTH` at the
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

It regenerates every golden from the binary in that build directory, under exactly
the dispatch, the environment **and** the archive-sensitivity its CTest row checks it
with — all three come from the single `REDSHIP_GOLDEN_DIGESTS` table in
`CMake/SingleExecutable.cmake`, so a golden cannot be re-pinned under a different
profile, or in a different archive environment, than the row checks it under.

It needs a GL-capable display and the **port archives only** (`soh.o2r`,
`2ship.o2r`, `redship.o2r`). Move `oot.o2r` and `mm.o2r` out of the build directory
first — and if you forget, the target **stops with an error naming both files** rather
than silently pinning a world CI cannot reproduce. Reason and override in "The archive
set is part of the pin" below.

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
5. Re-pin on **either** platform. Both CI legs check the same committed bytes (see
   "Which gate runs these rows"), so a re-pin on Linux and a re-pin on Windows are
   equally verifiable and a divergence between the two toolchains turns one leg red
   rather than passing unnoticed.

## The archive set is part of the pin

A golden pins a world **and the archive set that generated it**. The two seed
goldens pin the **archive-free** world — the port archives (`soh.o2r`,
`2ship.o2r`, `redship.o2r`) and no ROM-derived `oot.o2r`/`mm.o2r` — because that is
the environment hosted CI has, and a runner can never have ROM-derived archives.

This is not a precaution. It was measured: with `oot.o2r` mounted, the 2,449
per-area exclude-location options (option groups `RSG_EXCLUDES_KOKIRI_FOREST` ..
`RSG_EXCLUDES_GANONS_CASTLE`) contribute **nothing** to the settings string
`Playthrough_Init` hashes; without it, all 2,449 do. Dumping the per-option text
both ways gives 3,087 lines against 638, and the 638 shared lines are byte-equal —
so nothing else differs. Since the fill is re-seeded with
`Hash(seed + settingsStr)`, that one difference moves the entire OoT world and
every cross-game table keyed on the settings fingerprint.

Consequences you will actually hit:

* **A ROM-staged local `rando` run SKIPS `GoldenSeedDigestDefault` and
  `GoldenSeedDigestProfileV1`**, printing the reason. That is deliberate. A red row
  there would claim "you moved the world" when nothing moved, and a permanently red
  row in the local merge gate is worse than no row. To exercise them locally, move
  `oot.o2r` and `mm.o2r` out of `build-cmake` and re-run. So the two seed rows are
  enforced by CI (both legs — see "Which gate runs these rows") and by a local
  archive-free run, and by nothing in the operator's normal ROM-staged merge gate.
* **`GoldenPairedAttemptDigest` is archive-insensitive and is enforced
  everywhere** — measured: a golden regenerated with ROM archives staged and one
  regenerated without them are byte-identical, and the ROM-staged file passed
  unchanged on archive-free Linux CI.
* **Re-pin with the port archives only — and the machinery now enforces that.**
  `regen-golden-digests` **refuses** to re-pin an archive-sensitive golden while
  `oot.o2r`/`mm.o2r` sit in the build directory, and the error names both paths to
  move. This used to be a human-only rule stated in three places, which was the
  weakest possible defense for the most damaging mistake the target can make: it
  runs with its working directory pinned to the build tree, that tree is the
  ROM-staged one in a normal local build, the local `rando` tier *skips* the rows
  that would object, and the first symptom is a red Linux leg on your PR and on
  every PR after it. `-DALLOW_ROM_ARCHIVE_REGEN=ON` is the deliberate override; it
  warns loudly and pins the ROM-mounted world.

The underlying option-construction asymmetry is a real defect in its own right (the
headless harness fingerprints a different option set than a ROM-mounted run does),
tracked as #702. Fixing it would let one golden cover both environments and remove
the skip. It is not the goldens' job to hide it.

## Which gate runs these rows

Two CI legs, and the answer was measured rather than reasoned about — the first
version of this machinery claimed "the Linux and Windows CI legs" while only Linux
ran them, and the correction very nearly went the other way, into a doc explaining
why Windows *could not*.

| Gate | Runs the golden rows? | How |
|---|---|---|
| Linux CI (`build-linux`) | **yes**, all three | `ctest --label-regex '^rando$'` under `xvfb-run` |
| Windows CI (`build-windows`) | **yes**, all three | a dedicated `--tests-regex '^Golden'` step; the `redship` label step does not include them |
| Operator's local ROM-staged run | `GoldenPairedAttemptDigest` only | the two seed rows skip — see "The archive set is part of the pin" |
| Operator's local archive-free run | **yes**, all three | this is how the goldens are generated and re-pinned |

The Windows step exists because the golden rows carry `LABEL rando` and that job runs
the `redship` label only, so for one PR they were enforced on exactly one gate. The
expectation was that Windows *could not* run them: the `rando` rows bring up a
Fast3dWindow, and a hosted `windows-latest` runner's OpenGL was assumed to be the GDI
generic 1.1 implementation. **Measured instead of assumed, and the assumption was
wrong** — 3/3 passed in 5.4 s on `windows-latest` (run 35648094332, job 106493621321).

Things to keep in view rather than rediscover:

* The rows are selected **by name**, not by label. The `rando` tier as a whole is
  still Linux-only; only these three are known to run on a hosted Windows runner. Do
  not widen the step to `--label-regex rando` without measuring it — that measurement
  is #709.
* Both legs check the **same committed bytes**, which is what makes "MSVC and GCC
  generate the same world for the same seed" a property CI re-verifies on every PR
  rather than a measurement somebody took once. See "Platform portability".
* The two seed rows are still **not** enforced in a ROM-staged local run. The
  operator's local merge gate skips them; CI is where they bite.

## Platform portability

The golden is resolved as, in order:

```
tests/golden/<name>.<host-system>.txt   # Windows / Linux / Darwin
tests/golden/<name>.txt                 # portable — the normal case
```

**Measured verdict: Windows/MSVC and Linux/GCC produce byte-identical digests for
the same seed, once the archive set is held constant.** Only portable files exist.

The measurement is worth stating because it was earned the hard way. The first
goldens were generated ROM-staged on Windows and the Linux CI leg went red on both
seed rows — which looked exactly like a compiler divergence. Hiding `oot.o2r` and
`mm.o2r` and regenerating on the *same Windows machine* reproduced the Linux digest
field for field (`settingsHash` 01CBE129, `placementHash` 98F07849, `foreignOoTHash`
9362087A, `comboSettingsHash` EAF43DC3, and every per-slot line), so the variable
was the archive set, not the platform. Windows and Linux agree.

**The measurement is re-taken on every PR, not remembered.** Both CI legs compare the
**same committed golden bytes** — Linux/GCC under `xvfb-run`, Windows/MSVC in its own
`--tests-regex '^Golden'` step — so "MSVC and GCC produce the same world for the same
seed" is a property CI would go red about, on whichever leg diverged. That is why the
Windows step exists and why it must not be dropped: without it the property reverts to
folklore, and a golden re-pinned on one platform would silently stop saying anything
about the other. It also means a re-pin from **either** platform is fine, which is the
opposite of the rule this page briefly carried when only Linux ran the rows.

Every golden row prints its full digest before comparing, and the Linux job cats the
digest artifacts in every run, pass or fail, so both platforms' worlds can be read off
their logs directly.

Portability also has a mechanism behind it, which is why it was worth measuring
rather than assuming. The fill's randomness is `ShipUtils::next32` — a PCG-style
`state = state * 6364136223846793005 + 11634580027462260723` with an integer
xorshift and `std::rotr` — drawn through `ShipUtils::Random`'s rejection loop. No
`std::shuffle`, no `std::uniform_int_distribution`, no floating point in the draw
path; all three are implementation-defined and would differ between libstdc++ and
MSVC's STL. Ordered containers (`std::map`, `std::set`) and fixed enum ranges carry
the iteration order the digests fold.

**A per-platform golden may be added only when a platform difference has been
measured, and this page must then say which field differs and why.** Adding one to
make a row go green would convert a real finding — *the same seed gives two
players different worlds* — into a silenced test.

## Adding a golden

Add one line to `REDSHIP_GOLDEN_DIGESTS` in `CMake/SingleExecutable.cmake`. The
format is **six** pipe-separated fields:

```
<ctest-name>|<golden-name>|<dispatch>|<digest-env-var>|<archive-free-only>|<extra-env>
```

`<archive-free-only>` is `ON` when the pinned world depends on the archive set —
`ON` makes the CTest row **skip** when `oot.o2r`/`mm.o2r` are present in the build
directory, and makes the regen target **refuse** to re-pin there (see "The archive
set is part of the pin"). Use `OFF` only when you have measured that the digest is
identical with and without the ROM archives. A five-field line aborts configure at
`list(GET _golden_fields 5 ...)` with `list index: 5 out of range`; both consumers
read all six.

**Name the row `Golden...`.** The `LABEL rando` the loop applies gets it run on Linux
CI automatically, but the Windows leg selects these rows by `--tests-regex '^Golden'`,
so a row named anything else is silently enforced on one leg only — the exact gap this
page's "Which gate runs these rows" section exists to close.

Then run the regen target and commit the new file. The dispatch must write its
digest to the named environment variable's path and must emit one `key=value` per
line; free text cannot be diffed field by field and the checker refuses it.
