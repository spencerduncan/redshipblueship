# Golden determinism digests: what they pin, and how to re-pin them

RedShipBlueShip generates one paired OoT+MM world from one seed. Two different
properties of that generator get confused constantly, and until #688 only one of
them was tested:

| Property | Question | Enforced by |
|---|---|---|
| **Reproducibility** | does the same seed produce the same world *twice in a row*? | `SeedDeterminism`, `MMPairedAttemptDeterminism` (self-diff rows) |
| **Stability** | does the same seed still produce *the world we shipped*? | `GoldenSeedDigestDefault`, `GoldenSeedDigestProfileV1`, `GoldenPairedAttemptDigest`, `GoldenSeedDigestArmedCaps` (golden rows) |

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
tests/golden/seed-digest-armed-caps.txt   # --test rando-armed-caps-digest, every give-capability family armed (the composed bag)
```

`seed-digest-armed-caps` exists because the shipped profile arms none of the four
MM give-capability families (souls, ocarina buttons, swim, clocks; #681), so under
the other goldens no capability-gated row ever enters the single bag. Its dispatch
arms all four through MM's own option CVars, generates the same seed with the same
OoT settings as `seed-digest-default` (so `settingsHash` and `placementHash` match
that file), and then composes the single bag (ADR 0010 increment 3) from OoT's
deferred general-pass pool and MM's pool under the frozen armed profile, without
running the fill: the armed MM fill hits its wall-clock budget on this seed, and a
golden riding a machine-speed timeout would be flaky. What it pins is the bag:
the published caps (`armedGiveCaps`), each game's pool size (`armedOoTPoolRows`,
`armedMMPoolRows`), the admitted rows (`armedBagRows`, `armedBagCapabilityRows`,
`armedBagDigest`) and each game's disposition counts (`armedOoT`, `armedMM`). A
changed arming row in the O8 table, a changed compose rule or a reordered pool all
move it. The dispatch refuses to write a digest whose world did not publish every
family or whose bag holds no capability row. Like the other goldens it is pinned
archive-free and, in a ROM-staged tree, checked ROM-mounted too. (Until increment 3
it pinned the retired reverse pass's armed draw,
`foreignOoT<n>`.)

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
self-diff describe the same world and can be read against each other. Since #702
the archive set no longer separates these worlds from a player's: the ROM-mounted
environment generates the same digest as the archive-free one, and a ROM-staged run
checks that (see "The archive set is part of the pin"). They are still generated
headlessly, with the empty exclude and trick sets the harness passes in, which is a
separate question from the archive set. Both resolve combo direction `BOTH` at the
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
`2ship.o2r`, `redship-oot.o2r`, `redship-mm.o2r`). Move `oot.o2r` and `mm.o2r` out of the build directory
first — and if you forget, the target **stops with an error naming both files** rather
than silently pinning a world CI cannot reproduce. Reason and override in "The archive
set is part of the pin" below.

Re-pinning does **not** use the archive-free sandbox the checking rows use in a
ROM-staged tree (same section). That is a decision, not an oversight: a sandbox that
is subtly wrong turns a *checking* row red and somebody reads the field diff, but
would make a *re-pin* silently record a world nobody asked for — which is the whole
reason the refusal exists.

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

## Comparing two binaries by hand: never read a file a bare `--test` did not write

The golden rows answer one question: does *this* binary still produce the pinned
worlds? Sometimes a lane needs a different comparison — a main binary against a
branch binary under a profile no golden pins, or a sensitivity control. There is
one trap in doing that by hand, and it has already produced wrong claims (#710).

**A bare `redship --test rando-determinism` writes no digest file.** The three
digest dispatches — `rando-determinism`, `rando-armed-caps-digest` (both
`RSBS_SEED_DIGEST_OUT`) and `mm-paired-attempt` (`RSBS_ATTEMPT_DIGEST_OUT`) — write
a file only when that variable names one, and print the digest to stdout
otherwise. Only the CTest rows set it. So after a hand-run, `seed-determinism-run1.txt`,
`paired-attempt-run1.txt` and `golden-*-actual.txt` in the build directory are still
whatever the last ctest run wrote, with a plausible mtime and plausible contents.
"Compare the main binary's artifact against the branch's" then compares one stale
file with itself and reports no difference, which reads as a pass. PR #703's first
revision made two confident sensitivity claims that way; re-run, the digests were
byte-identical in every direction, including for controls that should have moved.

Since #710 a digest dispatch run with its variable unset (or empty) prints a
one-line `[digest-out] NOTICE:` that it wrote **no** file, naming the variable and
the rows that do write one — before the generation log and again as its last line.
If you see that notice, whatever file you were about to read is not from this run.

The recipes, in order of preference:

1. **Did my change move a pinned world?** Run the golden rows
   (`ctest --test-dir build-cmake -R "^Golden"` selects all four today), never a hand
   comparison. Moving a
   world on purpose is a re-pin (above).
2. **The same seed and profile, two binaries, through the rows.** In each build
   directory run `ctest --test-dir <build> -R "^(SeedDeterminism|MMPairedAttemptDeterminism)$"`,
   then compare `<build>/seed-determinism-run1.txt` (and `paired-attempt-run1.txt`)
   across the two directories. Each row deletes its previous files before it runs
   and fails if a run wrote none, so a file that exists after a green row is that
   row's. Stage the same archive set in both build directories, so the archive set
   is not a second variable in the comparison: before #702 it moved the OoT world,
   and the golden rows still check that the two environments agree (next section).
3. **A profile no row pins (a sensitivity control, an `RSBS_DIAG_CVARS`
   experiment).** Hand-run with a path of your own that no row writes, deleted
   first, and check that it exists afterwards. From PowerShell:

   ```
   Remove-Item -ErrorAction Ignore $env:TEMP\main-probe.txt
   $env:SDL_AUDIODRIVER = 'dummy'; $env:RSBS_DISABLE_OTR_INIT = '1'
   $env:RSBS_DIAG_CVARS = 'gRandoSettings.ShuffleSongs=2'   # the SeedDeterminism profile; omit for the shipped one
   $env:RSBS_SEED_DIGEST_OUT = "$env:TEMP\main-probe.txt"
   .\build-cmake\redship.exe --test rando-determinism
   Test-Path $env:TEMP\main-probe.txt                        # must be True, or there is nothing to compare
   ```

   and the same with the other binary and another path. `RSBS_ATTEMPT_DIGEST_OUT`
   with `--test mm-paired-attempt` for the paired ladder world.

A comparison that reports **no difference** proves nothing until a control that
*must* differ (another seed, another profile) has been run the same way and did
differ. Byte-identical results in every direction, including for a control that
should have moved, is the signature of comparing a file with itself.

Two rows lock this: `DigestOutHandRun` (redship tier: the resolver's unset / empty /
set answers, the notice text, and a source check that every digest dispatch resolves
its path through one function) and `HandRunDigestHonesty` (rando tier,
`CMake/CheckHandRunDigest.cmake`: the real `rando-determinism` and
`mm-paired-attempt` dispatches run with the variable unset print the notice, leave a
planted stale file untouched and create no file; set, the file appears).

## The archive set is part of the pin

A golden pins a world **and the archive set that generated it**. The goldens are
generated **archive-free**: the port archives (`soh.o2r`, `2ship.o2r`,
`redship-oot.o2r`, `redship-mm.o2r`) and no ROM-derived `oot.o2r`/`mm.o2r`, because that is the
environment hosted CI has, and a runner can never have ROM-derived archives.

**Since #702 (2026-09-27) the ROM-mounted world is the same world**, and every
golden row checks that wherever both environments exist. Until then it was not.
With `oot.o2r` mounted, the 2,449 per-area exclude-location options (option groups
`RSG_EXCLUDES_KOKIRI_FOREST` .. `RSG_EXCLUDES_GANONS_CASTLE`) contributed
**nothing** to the settings string `Playthrough_Init` hashes; without it, all 2,449
did (3,087 folded option lines against 638, the 638 shared lines byte-equal). Since
the fill is re-seeded with `Hash(seed + settingsStr)`, that one difference moved
the whole OoT world and everything keyed on the settings fingerprint: the pinned
world was not the world a player generated.

The cause was init order, proved with a probe that logged the live exclude lists
and the group sizes at each step, same binary, same seed:

| Environment | Order | Exclude options in the groups at fold time | Lines folded | `settingsHash` |
|---|---|---|---|---|
| archive-free | `AddExcludedOptions` (fills the lists), then the harness's `CreateOptions` (copies them) | 2,449 | 3,087 | `01CBE129` |
| ROM-mounted | the SoH menu's `CreateOptions` (copies empty lists), then `AddExcludedOptions`, then the harness's `CreateOptions` (returns at once on the #340 guard; not logged, see below) | 0 (the lists held 2,449) | 638 | `4029E439` |

`CreateOptions()` builds each `RSG_EXCLUDES_*` group as a **copy** of
`mExcludeLocationsOptionsAreas[area]`, and those lists were filled only by
`Context::AddExcludedOptions()`. With a game archive mounted `InitOTRImpl` sets up
the SoH menu (`SohGui::SetupMenuElements`, gated on `hasGameArchive`), whose
randomizer page calls `CreateOptions()` before `InitOTRImpl` reaches
`AddExcludedOptions()`; without one the menu is skipped and the harness's own
`CreateOptions()` runs after the lists are full. The fix makes `CreateOptions()`
fill the lists itself before it builds the groups
(`Settings::PopulateExcludeLocationsOptions()`, the one filler, which
`AddExcludedOptions()` now calls too), so the folded option set no longer depends
on who ran first. The archive-free world did not move (all four goldens are
unchanged); the ROM-mounted world moved onto it.

The harness calls `CreateOptions()` in the ROM-mounted run too
(`Rando_HeadlessSeedTest` in `3drando/menu.cpp`, the bridge the golden dispatches
use), and it does so after `InitOTRImpl` has filled the lists. That later call
changes nothing only because of a once-guard, `if (mOptionsCreated) return;`, which
#340 (`c5c0fc25`) added to `CreateOptions()` and which the vendored upstream
`settings.cpp` (`639ea0d8`) does not have: it returns before building anything, so
the menu's empty-list copy is the one that sticks. The probe sat after that guard,
so it logged only the call that got past it, and the harness's early-returning call
does not appear in its output (the table's third step is read from the code, not
from the probe).

### What the settings fingerprint folds (decided 2026-09-27, #702)

`settingsStr` folds every `Setting`-category option of every non-subgroup option
group **including all 2,449 exclude-location options**, in both environments. The
default for this decision was upstream parity — fold what stock SoH folds — and it
departs from stock SoH's *behaviour* on purpose:

* **What stock SoH does.** Stock SoH's own boot has the same order: `SetupGuiElements`
  → `Gui::SetMenu` → `SohMenu::InitElement` → `AddMenuRandomizer` → `CreateOptions()`,
  and only then `AddExcludedOptions()` (the vendored tree's initial commit shows
  it). So a stock SoH seed hash folds **none** of the excludes. But its fold has a
  dedicated branch for exactly these groups (`i >= RSG_EXCLUDES_KOKIRI_FOREST && i <=
  RSG_EXCLUDES_GANONS_CASTLE`, reading each location's value from the context), so
  folding them is what the upstream code is written to do; the empty result is the
  same ordering accident this change removes.
* **Why fold them.** The excluded-location set is a generator input: it changes the
  pool and therefore the world. A fingerprint that leaves it out gives two different
  exclude sets the same `sharedRandoSettingsHash` and the same fill seed — the
  "digest narrower than the input set is vacuous" failure ADR 0009 decision 1's
  amendment names, and the reason the MM half's `mmProfileDigest` already folds
  `gRando.ExcludedChecks`. Omitting them would make the paired world's identity cover
  MM's excludes and not OoT's.
* **What it costs.** Nothing on the pinned side: the golden worlds were generated
  archive-free, where the excludes were already folded, so no golden moved. On the
  player side, a seed generated with ROM archives mounted before this change gives a
  different OoT world after it (see the PR's playtest note). Pre-release, saves and
  worlds may move; nothing stored in a save is recomputed from the fingerprint.

Folding 2,449 `Included` strings that are identical for every default seed adds no
discrimination for a default seed, and the fold is cheap; the value is that a seed
with exclusions gets its own fingerprint.

### How the rows use the two environments

* **A ROM-staged local `rando` run checks every golden in BOTH environments.** Each
  golden row runs its dispatch twice: once in an archive-free sandbox,
  `build-cmake/golden-archive-free/<golden-name>/` (a hard link to the binary and to
  the **port archives only**), and once in the build directory itself with
  `oot.o2r`/`mm.o2r` mounted — the archive set a player has, though not the whole of
  a player's bring-up: both runs are the headless dispatch, with the row's
  `RSBS_DISABLE_OTR_INIT=1` and the harness's empty exclude and trick sets, not the
  file-select Generate path. Both digests are compared to the same golden, **and to
  each other**; a difference between the two is reported as `THE TWO ENVIRONMENTS
  GENERATED DIFFERENT WORLDS`, separately from a moved golden, because it is a
  different bug. The message names both candidate causes — #702 regressed, or
  build-directory content the sandbox does not carry (`mods/`, `assets/`) moved the
  world — and lists what the build directory's `mods/` holds. That cross-comparison is the lock on
  `settingsHash` (and every other field) being archive-independent: the same binary,
  two environments, one digest. On the unfixed code all four rows went red this way
  (`settingsHash 01CBE129 -> 4029E439` on `seed-digest-default`).

  The executable has to be in the sandbox because
  `Ship::Context::LocateFileAcrossAppDirs` searches the app-config directory (the
  cwd, in a portable build) **and the executable's own directory** before falling
  back to `./`, so moving only the working directory would still find
  `build-cmake/oot.o2r`. The port set is an **allowlist** (`soh.o2r`, `2ship.o2r`,
  `redship-oot.o2r`, `redship-mm.o2r`), so a ROM-derived archive the checker has never heard of cannot
  leak in by not being named — and because that makes a re-scan of the sandbox for
  ROM names unfailable by construction, the two checks that *can* fail are the ones
  the code makes instead: the port archives are resolved from the build directory
  **and from the binary's own directory** (they are not always the same directory)
  and a sandbox that ends up with none of them **fails**, rather than generating a
  no-archive world and blaming the fill; and `$SHIP_HOME`, the one directory the
  loader probes that the allowlist cannot control, is refused when a ROM-derived
  archive sits in it.

  **What travels into the sandbox, exhaustively:** the binary, the port archives the
  environment actually has, and `shipofharkinian.json` (so the window backend and
  every configured CVar come along). **What deliberately does not:** `mods/` — which
  since #670/#704 is resolved through the same probe list and therefore *is*
  load-bearing for the resource set — plus `assets/`, `gamecontrollerdb.txt`,
  `imgui.ini`, `Randomizer/`, `randomizer-mm/`, `Save/` and any earlier run's
  output. Dropping them is the intended reading (a hosted runner has none of them),
  but it is **not** the same claim as "the archive set is the only difference from a
  run in the build directory". The ROM-mounted run, by contrast, runs in the build
  directory as it is, `mods/` included; a tree with mods staged that changes the
  world will turn that half red, and the right reading is "mods moved the world",
  not "#702 regressed" — which is why the failure message offers both readings and
  prints the `mods/` listing rather than naming #702 alone.

  If the sandbox cannot be built, the row **fails** — it does not skip. A sandbox
  that cannot be built is a broken harness, not a false world move, and the failure
  message names both ways out (fix the reason it gives, or move `oot.o2r`/`mm.o2r`
  out of the build directory, which needs no sandbox at all — and then the row runs
  archive-free only).
* **Hosted CI runs each golden once, archive-free**, and the row's closing line says
  the ROM-mounted half did not run. The ROM half of the lock is enforced only where
  ROM archives are staged, which is the operator's local merge gate.
* **`RandoSettingsFoldExcludes`** is the direct lock on the mechanism: after a real
  generation in the build directory it asserts that each of the 32 exclude groups
  holds its area's exclude options (counted independently from the static location
  table, none zero) and that the fold read exactly that many lines from each. It was
  red on the unfixed code in a ROM-staged tree (every group held 0 of its 12-208 options,
  638 lines folded) and is green with and without the fix on CI, which cannot mount a
  ROM archive.
* **Re-pin with the port archives only — and the machinery enforces that.**
  `regen-golden-digests` **refuses** to re-pin an archive-free golden while
  `oot.o2r`/`mm.o2r` sit in the build directory, and the error names both paths to
  move. Since #702 a ROM-mounted re-pin should record the same world, but the refusal
  stays: a re-pin records one run, the one environment CI can reproduce is the one to
  record it in, and the COMPARE rows are what establish that the other environment
  agrees. **The deliberate override is its own target:**

  ```
  cmake --build build-cmake --target regen-golden-digests-rom-mounted
  ```

  It forwards `-DALLOW_ROM_ARCHIVE_REGEN=ON`, warns loudly, and pins the
  ROM-mounted world. It is a target rather than a `-D` on the build command because
  `cmake --build` does not forward `-D` to the inner `cmake -P`, and a cache entry
  would persist: somebody would set it once and every later re-pin in that tree would
  quietly pin the ROM-mounted world. For one PR this override was documented in four
  places while no caller forwarded the variable at all, so it could not be reached by
  any spelling; the checker now **fails** a REGEN caller that leaves it undefined.

## Which gate runs these rows

Two CI legs, and the answer was measured rather than reasoned about — the first
version of this machinery claimed "the Linux and Windows CI legs" while only Linux
ran them, and the correction very nearly went the other way, into a doc explaining
why Windows *could not*.

| Gate | Runs the golden rows? | How |
|---|---|---|
| Linux CI (`build-linux`) | **yes**, all four | `ctest --label-regex '^rando$'` under `xvfb-run` |
| Windows CI (`build-windows`) | **yes**, all four | `ctest --label-regex '^rando$'` — the whole tier, like Linux (#709) |
| Operator's local ROM-staged run | **yes**, all four, in **both** environments | each row generates in an archive-free sandbox under `build-cmake/golden-archive-free/` AND in the build directory with `oot.o2r`/`mm.o2r` mounted, and compares both to the golden and to each other (#702) — see "The archive set is part of the pin" |
| Operator's local archive-free run | **yes**, all four | this is how the goldens are generated and re-pinned |

The golden rows carry `LABEL rando`, and for one PR the Windows job ran the `redship`
label only, so they were enforced on exactly one gate. The expectation was that
Windows *could not* run them: the `rando` rows bring up a Fast3dWindow, and a hosted
`windows-latest` runner's OpenGL was assumed to be the GDI generic 1.1
implementation. **Measured instead of assumed, and the assumption was wrong** — first
the three golden rows (3/3 in 5.4 s, run 35648094332, job 106493621321; the job then
selected them by `--tests-regex '^Golden'`), then the whole tier (#709):

| `rando` tier on `windows-latest` | Rows | ctest time | Step time |
|---|---|---|---|
| PR #723, run 36226677282, attempt 1 (job 108362021770) | 28/28 passed, 0 skipped | 46.1 s | 46 s |
| PR #723, run 36226677282, attempt 2 (job 108370383924) | 28/28 passed, 0 skipped | 68.0 s | 69 s |
| Linux leg, same PR, attempt 1 (xvfb-run) | 28/28 passed | 38.1 s | — |
| Before: Golden-only step, the three main runs of 2026-09-22 | 3/3 | — | 7-10 s |

So the Windows job runs the whole label, at a cost of about a minute of job time.

Things to keep in view rather than rediscover:

* The rows are selected **by label** on both legs. A new golden row needs no
  particular name to be enforced on Windows (it did until #709: a row not named
  `Golden...` would have run on Linux only, and configure refused such a name).
* Both legs check the **same committed bytes**, which is what makes "MSVC and GCC
  generate the same world for the same seed" a property CI re-verifies on every PR
  rather than a measurement somebody took once. See "Platform portability".
* The golden rows are enforced in a ROM-staged local run too, and more widely than
  on CI: CI's environment *is* archive-free, while the local row **constructs** an
  archive-free environment and also runs in the ROM-mounted one, which CI can never
  have. If that construction fails,
  the row **fails** — there is no skip path on any gate, so a `SKIPPED` golden row
  means somebody re-added one. The earlier version of this machinery skipped there,
  which left the enforcement of these two rows resting on a human noticing a skip
  message; the reason the skip existed (a COMPARE in a ROM-staged tree is red about a
  move that did not happen) never applied to "the sandbox could not be built".

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
**same committed golden bytes** — Linux/GCC under `xvfb-run`, Windows/MSVC in its
`rando` tier step — so "MSVC and GCC produce the same world for the same
seed" is a property CI would go red about, on whichever leg diverged. That is why the
Windows step exists and why it must not be dropped: without it the property reverts to
folklore, and a golden re-pinned on one platform would silently stop saying anything
about the other. It also means a re-pin from **either** platform is fine, which is the
opposite of the rule this page briefly carried when only Linux ran the rows.

Every golden row prints its full digest before comparing (with the environment it
generated in, `work-dir` or `archive-free-sandbox`, on the same line), and **both**
jobs dump the digest artifacts in every run, pass or fail — `cat` on Linux,
`Get-Content` on Windows, each behind `if: always()` — so both platforms' worlds can
be read off their logs directly. The Windows half of that was missing for one PR
while this page claimed it: the fix and the measurement that found it are in
`.github/workflows/generate-builds.yml`.

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

`<archive-free-only>` is `ON` when the golden is pinned archive-free and the row must
prove the ROM-mounted world agrees (every golden today). The field kept its name from
when the two worlds differed (#702). What `ON` does, exactly, when
`oot.o2r`/`mm.o2r` are present in the build directory:

* the **CTest row** builds an archive-free sandbox under
  `<build>/golden-archive-free/<golden-name>/` — a hard link to the binary, the port
  archives, `shipofharkinian.json` — and runs its dispatch *there*, then runs it
  again in the build directory with the ROM archives mounted, and compares both
  digests to the golden and to each other. If that sandbox cannot be built the row
  **fails**; it never skips. (`ON` used to mean "skip in a ROM-staged tree", then
  "run in the sandbox only"; each old meaning outlived its code in this paragraph for
  a PR, in the one section a future golden-adder reads to learn what the field does.)
* the **re-pin targets refuse**: `regen-golden-digests` stops with an error naming
  `oot.o2r` and `mm.o2r`, and only `regen-golden-digests-rom-mounted` (which forwards
  `-DALLOW_ROM_ARCHIVE_REGEN=ON`) will pin the ROM-mounted world.

Both behaviours are described in "The archive set is part of the pin". With `OFF` the
row runs once, in the build directory, whatever is mounted there, and nothing compares
the two environments; there is no golden that wants that today. Its closing line
says which case it was: "ran ROM-mounted only and nothing compared the two
environments" when `oot.o2r`/`mm.o2r` were present, "the ROM-mounted half of this row
did not run" when they were not. A five-field line aborts configure at `list(GET _golden_fields 5 ...)` with
`list index: 5 out of range`; all three consumers — the CTest loop and the two re-pin
targets — read all six.

**The row's name is free.** The `LABEL rando` the loop applies gets it run on both CI
legs and locally (see "Which gate runs these rows"). Naming it `Golden...` is still
the convention, but since #709 nothing selects on the name, and configure no longer
refuses another one.

Then run the regen target and commit the new file. The dispatch must write its
digest to the named environment variable's path and must emit one `key=value` per
line; free text cannot be diffed field by field and the checker refuses it.
