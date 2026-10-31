# Upstream backport audit: Ship of Harkinian, 2Ship2Harkinian and libultraship (2026-10)

**Applies to:** `main` at `7c1aecf9` (2026-10-26). The finder records were taken against `main` at `96b8ce9a` (2026-10-18); the 80 commits between the two are accounted for in "Changed since the finder snapshot". Import bases: Ship of Harkinian `e149e09e0` (2026-01-11, audited to `dd702b067`), 2Ship2Harkinian `d05565623` (2026-01-10, audited to `f1dda4c28`), libultraship fork merge-base `8c55f607` (2026-01-11, audited to `da6daf1a` on `port-maintenance`).
**Last updated:** 2026-10-26.

This document turns the 2026-10-18 upstream audit into a backport plan: which upstream commits since our import bases are worth bringing into RedShipBlueShip, grouped into bundles a lane can land as one pull request or a short chain, with a priority, an effort, the prerequisites, the RedShipBlueShip adaptation and the test impact of each. It is a planning document for this tracker only. Nothing in it is a report to HarbourMasters or to any other external repository, and nothing here files an issue; the operator's planning session turns bundles into lanes.

It builds on [`upstream-issue-xref-2026-10-26.md`](upstream-issue-xref-2026-10-26.md), which cross-referenced the open issues against the same data and ended with ten fix families. Those families appear here as bundles (O3 hints, O4 speak clauses, O11 table bounds, L1 texture cache, M1 skeletons, O19 tracker pricing and counting, M3 MM logic holes, M14 dungeon key models, O36 reticles), and the bundles the xref document did not cover are new: the OoT item-pool bugs (O1), the ZR to Lost Woods entrance leak and the Dodongo's Cavern side-room pots (O6), the decoupled-entrance savewarp override (O7), the always-on engine crash set (O11), the unoptimised OoT half of the Windows Release build (O26), the MM 5.0 feature set (M4 to M16) and the libultraship hardening sets (L1 to L5).

How to read a bundle: the id is `L` (libultraship), `O` (Ocarina of Time, `games/oot/`) or `M` (Majora's Mask, `games/mm/`) plus a number. Priority P0 means a crash, corruption, softlock or determinism fault, or a bundle that unblocks others; P1 is a correctness fault a player meets in ordinary paired play; P2 is a visible improvement or a prerequisite; P3 is a feature, a refactor or a decision. Effort is the finder's S, M, L or XL per commit, summed by count. "Finder's call" is the finder's recommendation after the 2026-10-18 verifier pass: `port`, `port-with-adaptation` or `defer`. Every sha is the upstream commit; every path is ours.

## Method and counts

The 2026-10-18 workflow ran a finder over every first-parent commit in the three deltas: 681 Ship of Harkinian commits, 274 2Ship2Harkinian commits and 110 libultraship commits, 1,065 records in all. Each record carries a verdict (`ported`, `partially-ported`, `absent`, `not-applicable`), a category, a value, an effort, a recommendation, a rationale, evidence citing our paths, `depends_on` and `touches_ours`. The adversarial-verify stage completed 114 records (144 lens results: 72 presence, 42 fit, 30 combined, all on Ship of Harkinian) before the session limit ended the run; the critic and synthesis stages never ran, so this document is the synthesis. The records live outside the repository in the workflow's scratchpad (`audit-final.json`) and journal (`wf_2550f0b1-fb1/journal.jsonl`) under the operator's Claude temp directory; nothing here re-ran a finder.

What this document adds: the tallies below, the grouping into bundles by hand (respecting `depends_on`), the ten fix families of the xref document, and a spot-check on 2026-10-26 of 47 claims across the top bundles by three read-only agents that read our tree at `7c1aecf9` beside the upstream diffs (the "Spot-check results" section). Presence was checked by symbol, never by byte identity: both game trees carry an `OoT_` or `MM_` prefix sweep, and the earlier sync waves ported incrementally with edits.

Reliability: of the 114 records the 2026-10-18 verifier sampled, 7 that the finder called absent were in fact present (6 percent), and 8 lens results refuted a finder's value or fit. The 2026-10-26 spot-check found one move in each direction (`38e684fda` present through PR #1074; `7a7f942b8` absent although filed as ported). Expect a similar rate in the unverified bundles, and verify the premise in code before implementing any row, as the lanes already do.

### Records by repository and verdict

| Repository | Records | Ported | Partially ported | Absent | Not applicable |
|---|---|---|---|---|---|
| SoH | 681 | 41 | 23 | 556 | 61 |
| 2S2H | 274 | 14 | 11 | 218 | 31 |
| LUS | 110 | 0 | 4 | 91 | 15 |
| All | 1065 | 55 | 38 | 865 | 107 |

The 7 records the verifier re-filed as ported are counted under their finder verdict (absent) above; they are listed under "Already present".

### Absent and partially ported records by the finder's call

| Repository | port | port-with-adaptation | defer | skip | Candidates (port, adaptation, defer) |
|---|---|---|---|---|---|
| SoH | 155 | 249 | 114 | 54 | 518 |
| 2S2H | 37 | 83 | 84 | 25 | 204 |
| LUS | 21 | 15 | 33 | 26 | 69 |
| All | 213 | 347 | 231 | 105 | 791 |

### Candidates by value, effort and category

| Repository | high | medium | low | none | S | M | L | XL | needs a LUS change |
|---|---|---|---|---|---|---|---|---|---|
| SoH | 60 | 204 | 248 | 6 | 352 | 101 | 48 | 17 | 12 |
| 2S2H | 25 | 75 | 104 | 0 | 107 | 57 | 20 | 20 | 11 |
| LUS | 4 | 24 | 41 | 0 | 42 | 14 | 4 | 9 | 50 |
| All | 89 | 303 | 393 | 6 | 501 | 172 | 72 | 46 | 73 |

| Category | Candidates | Category | Candidates |
|---|---|---|---|
| bugfix-logic | 200 | cosmetic-asset | 17 |
| feature-enhancement | 125 | perf | 17 |
| feature-rando | 83 | docs-ci | 15 |
| bugfix-visual | 75 | platform | 15 |
| refactor-infra | 71 | netplay | 7 |
| bugfix-other | 67 | feature-accessibility | 6 |
| bugfix-crash | 54 | decomp-sync | 5 |
| feature-ui | 34 |  |  |

### Bundle coverage

The 67 bundles below (plus the libultraship bump axis) place 788 records: by priority, P0 holds 119 commits, P1 194, P2 207, P3 260, and the bump axis 8. 0 candidate records are not in any bundle; they are listed in Appendix A with the reason.

## Executive summary

Five bundles carry most of the value, and four of them are small:

1. **O26, one line: the OoT half of the Windows Release binary is compiled without optimisation** (SoH `f00f5b625`, #7188). Verified against `build-cmake/build.ninja` on 2026-10-26: every `soh_src`, `soh_port`, `soh_enh` and `soh_rando` object is compiled with `/Oi /Gy /W3` and no `/O2`, because `games/oot/CMakeLists.txt` clears the root's flags and its x64 Release block never adds an optimisation level. MM and libultraship get `-O2`. The fix is `/O2` in that block. Rebuild, run both tiers, and expect the frame-time and generation-time numbers to move.
2. **O11, the always-on engine crash set** (20 commits, mostly S). The skybox heap overrun (`5ecff16bd`) writes `PlayState.skyboxCtx` through a `GameState`-sized allocation and fires again on every switch back into OoT; the prerendered-background camera guard (`e9268c446`, `f2c34d8c1`) is reachable through rando kill traps; the unsized `gItemSlots` and the item tracker's unbounded `INV_CONTENT(item.id)` (`29c7e7568`, `7a7f942b8`) are the open half of xref family 3. Our tree has no `AlwaysOnFixes.cpp`; creating it is the adaptation.
3. **O1, O3 and O4, the item pool, hint generation and speak clauses** (24 commits, all S). Under Own Dungeon no map or compass ever enters the pool, Farore's Wind is never placed, and Shuffle Bean Souls places ten items that have no item entry (O1). Junk gossip stones are never generated and all six trials get hints (O3, closes #1162). Logic and game disagree about which NPCs need a jabber nut (O4, closes #1168). Every one of these re-pins OoT goldens, so land them as one lane with one re-pin.
4. **L1 and M1, the #1063 texture-pack crash family on both sides.** L1 is a fork PR of ten small libultraship fixes (texture-cache use-after-free, null texture addresses, the TEXEL1 slot, the framebuffer read format). M1 closes a verified live gap: MM's Player registers into OoT's shared skeleton patcher on every scene and never unregisters, so an Alt Assets toggle later in the session writes through dangling pointers.
5. **M3, Majora's Mask logic correctness** (21 commits). Every soul, time and swim row the finder named is absent, and `e07b6f731` reveals that our `Ship_Random` excludes its upper bound while the call sites still pass `size - 1`: the random starting half-day can never be Night 3 and gossip stones never pick their last hint. All of these move the MM and paired goldens; name the move.

Close behind: O9 (the randomizer thread is joinable at exit, so quitting during a generation is `std::terminate`), O12 (ADPCM over-read and audio-heap overflow in both games' shared mixer), M2 (ten MM crash and UB fixes) and O18's speed-modifier half-port (a pre-v6 config loses those settings on upgrade).

**The libultraship prerequisite.** Both upstreams pin `port-maintenance`; our fork has none of its 110 commits since the merge-base `8c55f607`. 97 finder records (73 libultraship, 13 SoH, 11 2S2H) are marked `lus_required`. Two paths exist. The cherry-pick path (L1 to L5, 36 commits, mostly S) brings the crash hardening, shutdown and config safety, controller fixes, resource-loading fixes and small rendering fixes as fork PRs that need no game-tree migration. The migration path (the LB axis) moves the fork to `port-maintenance` and then needs SoH `8a14cd109` (63 files) and 2S2H `65fb1b6e3` plus `16774f2d7` in the game trees, after which L7 to L11 (surround sound, the HD texture line, the shader pipeline, the Context refactors) become available. This document recommends the cherry-pick path now and the migration as an ADR once L1 to L5 are in. One housekeeping item comes first either way: the submodule pin (`fcbbc18c`, 2026-10-21) is reachable only through the fork's `claude/unload-filter-owner` branch, 22 commits ahead of the fork's `main` (`6db32f6`, 2026-01-17), so merging that branch into the fork's `main` keeps the gitlink fetchable for fresh clones.

**Decisions the operator owns** (each is named at the bundle that needs it): MM save-tier growth (version bump and migration, or another accepted break, before M5, M6 and M7); new world-generation options and the combo option identity (O29, O30, M6); shared-song rulings for Sun's Song and Saria's Song (M7, M8); the MM tycoon wallet under the shared tier (M16); hosting MM cosmetics inside OoT's editor (M14); savestates in paired sessions (O35); Torch versus ZAPDTR, Windows and Linux ARM64, and following upstream decomp renames (O37); the libultraship migration (LB).

## libultraship (fork at `libultraship/`)

Our fork's merge-base with upstream is `8c55f607`; the fork adds its own commits (the #557 texture-name fix `477633a`, the per-game resource caches and unload-owner work of #1000 and #1092) and none of upstream's 110 `port-maintenance` commits. 91 of the 110 are absent, 4 partially ported, 15 not applicable (upstream CI, docs, platforms we do not ship, the tinycc scripting system). 69 are candidates; 33 of them are `defer`, almost all because they only make sense after the texture-import refactor or the shader pipeline that the migration path brings. Every bundle here is a fork PR against the operator's libultraship fork, which is not an upstream report.

| Id | Bundle | Priority | Effort | Commits | LUS | Goldens move |
|---|---|---|---|---|---|---|
| L1 | Texture-cache and interpreter crash hardening | P0 | M | 10 (Sx8 Mx2) | fork PR | no |
| L2 | Shutdown, crash handler and config safety | P1 | M | 13 (Sx12 Mx1) | fork PR | no |
| L3 | Controller and input fixes | P1 | S | 5 (Sx4 Mx1) | fork PR | no |
| L4 | Small rendering-correctness fixes | P2 | S | 5 (Sx5) | fork PR | no |
| L5 | Resource loading: fewer copies, .meta priority, GL state | P2 | M | 6 (Sx3 Mx2 Lx1) | fork PR | no |
| L6 | DirectX WARP software fallback | P3 | M | 1 (Mx1) | fork PR | no |
| L7 | Surround sound (Sound Matrix Decoder) and CoreAudio | P3 | L | 6 (Sx2 Mx2 Lx2) | fork PR | no |
| L8 | Texture import refactor line (HD texture packs) | P3 | XL | 12 (Sx3 Mx2 Lx1 XLx6) | fork PR | no |
| L9 | Shader pipeline: PrismProcessor, primitive depth, blend colour | P3 | XL | 10 (Sx5 Mx1 Lx2 XLx2) | fork PR | no |
| L10 | Constant-size texture-scroll interpolation (G_SETTILESIZE_LERP) | P2 | L | 5 (Sx1 Mx1 Lx2 XLx1) | fork PR | no |
| L11 | libultraship infrastructure to take with a bump decision | P3 | XL | 12 (Sx6 Mx4 XLx2) | fork PR | no |
| LB | The libultraship bump axis (both game trees) | decision | XL | 8 (Lx1 XLx7) | migration | yes |

### L1. Texture-cache and interpreter crash hardening

Priority P0, effort M (10 commits: Sx8 Mx2).

- **Prerequisites:** A fork PR against the operator's libultraship fork. Settle the fork branch first: our pin sits on `claude/*` branches past the fork's `main`.
- **RSBS adaptation:** Take 62d22224, c5a5a934 and f7600e02 as they are (unmodified functions). Take only the cache-size half of a93ee524 (`TEXTURE_CACHE_MAX_SIZE` is 500 in the fork; fork 477633a already turned the texture array into a growing vector). Take 67861a90 only with 3097debf and bbb565bd, and drop its reference to 787ec0bd (the dynamic-loading system we skip). Take adaad35d's DX11 and DXGI guards; the Metal half waits for macOS.
- **Test impact:** redship tier; the texture soak that caught #557 (`mCurrentTile >= 2` oracle); no goldens.
- **Note:** The #1063 texture-pack crash family (xref family 4). Each is a real use-after-free, overrun or null dereference on the render thread, and RSBS renders two games' texture sets through one cache.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `62d22224` | #1041 | Fix texture cache crashes: rehash prevention and dangling pointer nulling (#1041) | S | port | After an LRU eviction, delete or clear, the next triangle that reuses a slot without a texture reload writes sampler state into a freed hash-map node. |
| `c5a5a934` | #1008 | Add null guards to ImportTexture* functions to prevent access violation (#1008) | S | port | It turns a render-thread access violation (0xC0000005) on a null loaded-texture address into a logged skip. |
| `a93ee524` | #1006 | Change OpenGL textures array to vector, and double texture cache (#1006) (partially ported) | S | port | Doubling the texture cache cuts LRU evictions and re-uploads, which matters more in a process that renders two games' texture sets. |
| `49d640d3` | #1049 | Fix ASan and UBSan issues in graphics interpreter and Metal backend (#1049) | S | port | Removes a use-after-free write on the per-display-list return path (ASan), signed-shift UB in the render-mode macro, and Metal null and overflow bugs that matter for ... |
| `adaad35d` | #1040 | Backend safety guards: null-checks, zero-dimension guards, and resource leak fixes (#1040) (partially ported) | S | port-with-adaptation | Fixes DX11/DXGI out-of-bounds texture-id reads, crashes in the window procedure during early init and late teardown, a staging-texture leak, and GPU errors from zero- ... |
| `67861a90` | #1042 | Filter bad addresses in gfx_check_image_signature before dereferencing (#1042) | M | port-with-adaptation | Hardens Fast3D against games passing null, low, or kernel-range pointers through OTR path opcodes, SETTIMG and the __OTR__ signature check, and falls back to the othe ... |
| `3097debf` | #1052 | Fix 64-bit address filter to cover ARM64 user-space (#1052) | S | port-with-adaptation | Without it, the 67861a90 filter treats valid ARM64 user-space pointers above 47 bits as garbage and silently skips the resources they point to (relevant to our macOS/ ... |
| `bbb565bd` | #1176 | fast3d: accept in-module low pointers in the SETTIMG guard on non-Windows (#1176) | M | defer |  |
| `f7600e02` | #1055 | Fix 1-cycle mode TEXEL1 sampling incorrect texture slot (#1055) | S | port | In 1-cycle mode the RDP's TEXEL1 is effectively TEXEL0, but Fast3D samples the next tile. |
| `46bdbdd1` | #1045 | Attempt to fix ReadFramebufferToCPU across all three backends (#1045) | S | port-with-adaptation | Per the commit message, GL_RGBA + GL_UNSIGNED_SHORT_5_5_5_1 makes some NVIDIA drivers write 8 bytes per pixel. |

### L2. Shutdown, crash handler and config safety

Priority P1, effort M (13 commits: Sx12 Mx1).

- **Prerequisites:** Fork PR. 7cb10226 (WASAPI hang) assumes 5c8b975d's `DoClose` and 7f2baa10's mutex; adapt it to the fork's pre-split `WasapiAudioPlayer` instead (SoH 808a112b9 is the bump that carried it). The fork's player has no destructor and no mutex, and its `Release()` still does `delete this` although a `shared_ptr` owns the object (`WasapiAudioPlayer.cpp:161-164`); the fork PR should fix that shape rather than copy upstream's.
- **RSBS adaptation:** Apply 766a6bad before cdba319b (same file). 956f1082's `gfx_direct3d11.cpp` hunk needs 939500eb (L6), so skip that one hunk. d4f9b849 is the same temp-and-rename pattern `src/common/save.cpp` already uses for `.redsave`.
- **Test impact:** redship tier; a manual Ctrl-C and window-close on Windows and Linux; no goldens.
- **Note:** Both games share one `shipofharkinian.json`, so a truncated config loses every setting. Windows is the primary platform and its tracebacks are unusable today (cdba319b).

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `766a6bad` | #1188 | fix(crash): replace exit(1) in ShutdownHandler with SDL_QUIT event (#1188) | S | port-with-adaptation | On Linux/macOS, Ctrl-C or SIGTERM runs exit() inside a signal handler. |
| `cdba319b` | #1190 | Fix Windows crash handler producing unusable tracebacks (#1190) | S | port-with-adaptation | Windows x64 tracebacks are currently unusable: unseeded stack walk, stale symbol names, absolute ASLR addresses. |
| `d4f9b849` | #1183 | Use atomic write for Config::Save (#1183) | S | port | A crash or power loss during Config::Save can leave the config truncated, losing all settings for both games, which share one config. |
| `1289e28b` | #1167 | Fix NullAudioPlayer::Buffered() returning 0 (audio-thread spin/hang) (#1167) | S | port |  |
| `18fc792b` | #977 | Fix memory leaks in Gui and ConsoleWindow (#977) | S | port |  |
| `a331dbb1` | #963 | SDL init: fix log (#963) | S | port |  |
| `956f1082` | #1024 | use snprintf over sprintf (#1024) | S | port |  |
| `2cc049d2` | #1126 | Fix warnings (#1126) | S | port |  |
| `fef66f52` | #1030 | fix: correct StringHelper::Strip erase count (#1030) | S | port |  |
| `c3ad09cd` | #1032 | fix: restore StringHelper::Implode implementation (#1032) | S | port |  |
| `7cb10226` | #1212 | Fix wasapi hang (#1212) | S | port-with-adaptation |  |
| `7f2baa10` | #1001 | Fix race condition in wasapi. (#1001) | S | defer |  |
| `SoH 808a112b9` | #7128 | Bump libultraship for the WASAPI shutdown hang (#7128) | M | port-with-adaptation | WASAPI is the default Windows audio backend. |

### L3. Controller and input fixes

Priority P1, effort S (5 commits: Sx4 Mx1).

- **Prerequisites:** Fork PR for the LUS half; the SoH half is one Settings row (1d9de8544). Skip the 2S2H row 10bfaa0f0: one shared window means one setting.
- **RSBS adaptation:** Our fork has no `Fast3dGui`, so 8a67fa4b's call goes next to the SDL ImGui backend init in `Gui.cpp` and after each hotplug event. The fork sets SDL's background-joystick hint unconditionally (`Gui.cpp:200-207`) and its `WM_KILLFOCUS` handler only clears `mInFocus`; #1153 added a test-only override of the hint (`src/common/test_window_focus.cpp:106`). The toggle is the player-facing version of that override.
- **Test impact:** redship tier; UI snapshot of the Settings page; the controller-proof test rows of #1153 stay green.
- **Note:** A controller connected after launch cannot navigate the menus today (8a67fa4b).

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `8a67fa4b` | #1138 | Bind imgui's gamepad backend to the controller libultraship is using (#1112) (#1138) | S | port-with-adaptation | Without it, a controller connected after launch, or any controller other than the first, cannot navigate the ImGui menus. |
| `b79cecca` | #975 | Initialize WheelHandler::mBufferedCoords to fix undefined behavior (#975) | S | port | The read of an uninitialized float is UB and can produce spurious mouse-wheel axis input on the first frames. |
| `61bf1fbe` | #994 | Add ability to toggle allowing background inputs (#994) | S | port-with-adaptation | It lets players stop controller input from reaching the game while the window is unfocused: live on DXGI via WM_KILLFOCUS/WM_SETFOCUS, and at init via the SDL hint on ... |
| `SoH 1d9de8544` | #6214 | Implement toggle for allowing background inputs (#6214) | M | port-with-adaptation | Adds a Settings checkbox (default on, matching current behavior) that stops controller input reaching the game while the window is unfocused. |
| `2S2H 10bfaa0f0` | #1898 | Wire up background input toggle (#1898) | S | defer |  |

### L4. Small rendering-correctness fixes

Priority P2, effort S (5 commits: Sx5).

- **Prerequisites:** Fork PR. 97d8e300: only the `GfxSpTri1` hunk applies; its four `ImportTexture*` hunks edit #1043 code we lack (L8).
- **RSBS adaptation:** ddac21e0 is one line and changes occlusion for draws that enable G_ZBUFFER with a non-Z render mode; play both games' Lens of Truth and shadow scenes afterwards.
- **Test impact:** redship tier; a visual pass on both games.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `ddac21e0` | #1059 | Fix depth test to require Z_CMP (#1059) | S | port | With this, Fast3D matches the N64: a triangle is depth-compared only when the render mode sets Z_CMP, not whenever G_ZBUFFER geometry mode is on. |
| `41eeeb24` | #985 | [DX, Mac] Allow Viewports to translate above the top of the screen (#985) | S | port |  |
| `0b76d895` | #1010 | Fixed point filtering on metal (#1010) | S | port | On Metal it applies three-point filtering per texture instead of to every sampler, and it fixes the noise-alpha shader path, which as written references undefined ide ... |
| `97d8e300` | #1164 | fix: round interpolated texture tile sizes (#1164) | S | port-with-adaptation | With frame interpolation on, fractional tile coordinates can truncate to N-1. |
| `f0837c0c` | #1124 | gsSPPopMatrix xml command (#1124) | S | port |  |

### L5. Resource loading: fewer copies, .meta priority, GL state

Priority P2, effort M (6 commits: Sx3 Mx2 Lx1).

- **Prerequisites:** Fork PR. 9a1a2bd9 assumes 18c5343a's `File::BufferOffset`; port them together or adapt. Our per-game resource caches (#1000) and the archive re-add fix (#1045) sit next to this code, so re-read `ArchiveManager` before applying.
- **RSBS adaptation:** 6f3ab4e6: take the `attribLocations >= 0` guard as is; take the redundant-state cache only with invalidation in `DeleteTexture` and at the raw framebuffer bind sites (fork 04256d2). 2917d0f4 is opt-in and off; take it only with a measurement.
- **Test impact:** redship and integration tiers (archive mounting order is exercised by the crossing rows); no goldens.
- **Note:** With oot.o2r, mm.o2r, soh.o2r, packed mods and loose mods all mounted, a mod's `.meta` alias can today shadow a lower archive's real asset (9a1a2bd9).

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `18c5343a` | #1027 | perf: avoid memory copies in resource loading (#1027) | S | port | Removes up to two full copies of every asset during loading: no header/body split, and textures point into the decompressed buffer instead of memcpy'ing it. |
| `59faf043` | #1022 | Avoid temporary string allocations in hot per-frame paths (#1022) | S | port | Transparent hashing removes a heap allocation from every CVarGet* lookup by const char*, which both games do many times per frame. |
| `9a1a2bd9` | #1168 | Resolve .meta aliases by archive priority (#1168) | M | port-with-adaptation | Fixes two bugs. |
| `6f3ab4e6` | #982 | Fix OpenGL api violations and reduce state switching (#982) | M | port-with-adaptation | Take the attribLocations>=0 guard as is: it stops GL_INVALID_VALUE errors at index 4294967295. |
| `2917d0f4` | #1175 | Add opt-in memoization of texture path resolution (#1175) | S | defer |  |
| `SoH 985590246` | #7010 | bump lus (#7010) | L | defer | Fixes a mod's .meta alias shadowing a lower archive's real asset, and a lower-priority .meta losing to a higher archive. |

### L6. DirectX WARP software fallback

Priority P3, effort M (1 commits: Mx1).

- **Prerequisites:** Fork PR.
- **RSBS adaptation:** As written the null `mDevice` dereference makes the fallback unreachable in exactly the failing case; null-check `mDevice` first.
- **Test impact:** Manual only (a machine without a D3D11 device).

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `939500eb` | #967 | Add fallback to WARP (software renderer) for DirectX (#967) | M | port-with-adaptation | It adds a software-renderer fallback and readable error dialogs for Windows machines whose GPU cannot create a D3D11 device. |

### L7. Surround sound (Sound Matrix Decoder) and CoreAudio

Priority P3, effort L (6 commits: Sx2 Mx2 Lx2).

- **Prerequisites:** Fork PR that restructures `AudioPlayer` (Play becomes DoPlay/DoClose), then the two game-side `SetAudioChannels` ports. CoreAudio only when the macOS build returns.
- **RSBS adaptation:** The audio device is shared and owned by the OoT-created context; MM's `SetAudioChannels` must route through the same player.
- **Test impact:** redship tier; manual listening test; the silent-test contract (`SDL_AUDIODRIVER=dummy`, `Window.AudioBackend null`) must still hold.
- **Note:** Also unblocks 7cb10226's clean `DoClose` path in L2.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `5c8b975d` | #986 | Sound Matrix Decoder (#986) | L | port-with-adaptation | It upmixes the stereo mixer output to 5.1 through a matrix decoder and lets the in-game 'Surround' sound option switch the LUS output device at runtime. |
| `61bc9402` | #981 | Support CoreAudio on Apple platforms #980 (#981) | M | defer |  |
| `SoH a36544683` | #6204 | Basic Surround Sound support. (#6204) | L | defer | Selecting the in-game 'Surround' sound mode would switch LUS to a 5.1 matrix decoder; other modes switch it back to stereo. |
| `2S2H 0852b5211` | #1516 | Basic surround sound support. (#1516) | M | defer |  |
| `2S2H ce2563a26` | #1746 | feat: add CoreAudio audio backend option (#1746) | S | defer |  |
| `SoH fcf7d4bd1` | #6270 | Add CoreAudio support (#6270) | S | defer |  |

### L8. Texture import refactor line (HD texture packs)

Priority P3, effort XL (12 commits: Sx3 Mx2 Lx1 XLx6).

- **Prerequisites:** Take as a whole or not at all: 2492cc35 alone brings regressions upstream needed about ten later commits to fix. 709efa73 changes TLUT handling, so 2S2H 19efaaefc (map palette) must land in the same change. Decision: are HD texture packs a goal for this release line.
- **RSBS adaptation:** This is the other half of the #1063 family. The crash-hardening half (L1) does not depend on it.
- **Test impact:** redship tier; the texture soak; a visual pass of both games with and without an HD pack.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `2492cc35` | #1043 | Texture import refactor: DRAM stride, 2D copy loops, and dimension clamping (#1043) | XL | defer | Rewrites every per-format texture decoder to use the real DRAM stride with 2D copy loops, clamps to SetTileSize, guards zero sizes, and forces mip tiles >= 2 back to ... |
| `ec194d6b` | #1118 | Skip some clamps and stride calculations for replacement textures (#1118) | L | defer | HD/4k texture-pack fixes: it clamps HD replacements to the tile region, takes the LoadBlock stride from the resource height, and packs RAW uploads by the HD stride. |
| `60c9bb71` | #1135 | fix(fast3d): treat an undefined tile region as zero size (#1135) | XL | defer |  |
| `e8a16187` | #1151 | fast3d: clamp masked over-loaded textures to their wrap period (#1151) | M | defer |  |
| `621f09ba` | #1143 | Fix Metal crash when LoadBlock uploads a texture as one giant line (#1143) | S | defer |  |
| `62e973ae` | #1239 | Unify the pyramidlike test (#1239) | XL | defer |  |
| `ac72d8b3` | #1306 | Merge pull request #1306 from bassdr/restore-hd-tile-region-clamp | XL | defer |  |
| `da6daf1a` | #1314 | Merge pull request #1314 from JeodC/fix-imagerect-hd-scale | XL | defer |  |
| `709efa73` | #1044 | Palette/TLUT handling: multi-palette CI4 support and DRAM-based cache keys (#1044) | M | defer |  |
| `SoH 6b4edce43` | #7390 | apply LUS update (#7390) | S | port |  |
| `SoH 988b53665` | #7003 | Bump libultraship to pull in HD texture-pack regression fixes (#7003) | XL | defer |  |
| `2S2H 19efaaefc` | #1835 | Adjust palette hack for maps (#1835) | S | defer | Porting this on our current LUS would break dungeon-map room colours. |

### L9. Shader pipeline: PrismProcessor, primitive depth, blend colour

Priority P3, effort XL (10 commits: Sx5 Mx1 Lx2 XLx2).

- **Prerequisites:** A LUS bump decision (see the libultraship section): our shader files predate upstream's merged `.glsl`, and 2S2H's `f56c7939e` cosmetics depend on it.
- **RSBS adaptation:** Our games route the Lens of Truth through code the blend-colour change alters; test both games' Lens.
- **Test impact:** redship tier; a visual pass; no goldens.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `5cb82faa` | #972 | Add PrismProcessor for Dynamic Shader Loading (#972) | XL | defer |  |
| `7fc824ca` | #1053 | Fixed latest compilation issues (#1053) | S | defer |  |
| `7f9b86a5` | #1247 | Fixes/safety for shader push/pop (#1247) | XL | defer |  |
| `7cfca877` | #1080 | implement G_ZS_PRIM (G_SETPRIMDEPTH) (#1080) | L | defer | It implements N64 primitive depth: fragment depth comes from the G_SETPRIMDEPTH register under G_ZS_PRIM, as both games' Lens of Truth masking and some transitions ex ... |
| `80b652be` | #1021 | Implement GfxDpSetBlendColor (#1021) | S | defer |  |
| `0a702f55` | #1081 | Re-introduce GfxDpSetBlendColor (#1081) | S | defer |  |
| `7461ee01` | #1046 | Rendering features: partial depth clears, FB texture passthrough, coverage sim, and widescreen fixes (#1046) | M | defer |  |
| `5dd092aa` | #1173 | Fix partial depth clears drifting at non-4:3 aspect ratios (#1173) | L | defer |  |
| `f30fe0ed` | #1109 | Add fixedAspect flag to framebuffers (#1109) | S | defer |  |
| `2S2H 6eeeae034` | #1804 | Sync shaders from LUS (#1804) | S | defer |  |

### L10. Constant-size texture-scroll interpolation (G_SETTILESIZE_LERP)

Priority P2, effort L (5 commits: Sx1 Mx1 Lx2 XLx1).

- **Prerequisites:** The LUS opcode (c57da1b4) first, then both game halves together. 2S2H a4360a25c (MM texture scrolling interpolation) can land before the opcode: its decomp hunks have no LUS dependency; ecf24c6a3 restores two function bodies it removed.
- **RSBS adaptation:** Frame interpolation already works in MM through the shared gfx processor (#1091); MM's scrolling textures still step at 20 fps next to a smooth OoT until a4360a25c lands.
- **Test impact:** redship tier; a visual pass at 60 fps in both games.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `c57da1b4` | #1141 | Add G_SETTILESIZE_LERP for constant-size texture scroll interpolation (#1141) | M | port-with-adaptation | A new RDP command carries both tile-size endpoints, and the interpreter lerps them at draw time by a per-frame mInterpolationT. |
| `SoH c4e92a70f` | #6902 | Constant-size texture scroll display lists via G_SETTILESIZE_LERP (#6902) | XL | defer | Texture-scroll display-list memory currently grows with InterpolationFPS. |
| `2S2H f11371f75` | #1812 | Port LERP interpolation improvements from ship (#1812) | L | defer | Replaces the per-interpolated-frame INTERP display lists with one constant-size LERP command and computes interpolation T per sub-frame. |
| `2S2H a4360a25c` | #1332 | Texture scrolling interpolation, Take 2 (#1332) | L | port-with-adaptation | Interpolates MM's scrolling textures (water, lava, effects) at high frame rates. |
| `2S2H ecf24c6a3` | #1486 | Revert changes to original Gfx_TexScroll & Gfx_TwoTexScroll (#1486) (partially ported) | S | defer |  |

### L11. libultraship infrastructure to take with a bump decision

Priority P3, effort XL (12 commits: Sx6 Mx4 XLx2).

- **Prerequisites:** The port-maintenance migration decision; the macOS items only when that build returns.
- **RSBS adaptation:** 3015249d (SHA-256 check on the cached stb download) is small and standalone; take it with any fork PR.
- **Test impact:** Build only.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `1d982914` | #1103 | Untangle various parts from the Context destructor (#1103) | XL | defer |  |
| `8813b823` | #1097 | Remove cross-layer includes from ship namespace (#1097) | XL | defer |  |
| `08920ab8` | #1009 | Tweakable mouse state (capture, visibility) (#1009) | M | defer |  |
| `5bf70d91` | #1033 | tests: add initial unit test suite (#1033) | M | defer |  |
| `59da6550` | #1066 | test: add comprehensive test coverage across libultraship (#1066) | M | defer |  |
| `7e3ee6e8` | #1029 | build: update prism bbcbc7e -> e313fa8 (#1029) | S | defer |  |
| `3015249d` | #1249 | Merge pull request #1249 from bassdr/fix/stb-download-url-port-maintenance | S | port |  |
| `f57f4d25` | #1139 | Add ImGui FileBrowserWindow as a fallback file dialog (#1139) | M | defer |  |
| `2bfbde3a` | #1157 | Add Palette4bpp Support for Texture Loading into ImGui (#1157) | S | defer |  |
| `8ffa903c` | #970 | Fix window resize crash on macOS with Metal (#970) | S | defer |  |
| `c4485fe9` | #1036 | macOS: Create bundleID directory in Application Support when checking… (#1036) | S | defer |  |
| `5ea80db9` | #1060 | Update spdlog (#1060) | S | defer |  |

### LB. The libultraship bump axis (both game trees)

Priority decision, effort XL (8 commits: Lx1 XLx7).

- **Prerequisites:** An ADR. Both upstreams pin `port-maintenance`; our fork has none of its 110 commits, and each game tree needs a mechanical API migration (SoH 8a14cd109 touches 63 files; 2S2H 65fb1b6e3 and 16774f2d7 touch the menu, trackers and `ShipUtils`).
- **RSBS adaptation:** The bundles L1 to L5 are the per-commit alternative: fork cherry-picks that need no game-tree migration. The report recommends that path now and the full migration only once the cherry-pick list is exhausted.
- **Test impact:** Everything: both ctest tiers, integration, UI snapshots.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `SoH 18225f0e6` | #7380 | Pin latest libultraship's port-maintenance branch (#7380) | XL | defer | This is a cumulative port-maintenance pin. |
| `SoH 8a14cd109` | #6629 | Move to port-maintainance LUS branch to fix crash on close (#6629) | XL | defer | A 63-file mechanical API migration (GetInstance to GetRawInstance and similar) that only compiles against the port-maintenance LUS. |
| `SoH d841357fd` | #6283 | Update libultraship (#6283) | L | defer | No SoH source change; the payload is LUS fixes (WASAPI race, GL state, texture null guards) that our fork lacks. |
| `SoH 004adaae1` | #6601 | Update LUS to build in GCC 16 (#6601) (partially ported) | XL | defer |  |
| `2S2H 65fb1b6e3` | #1801 | Bump LUS to port-maintenance (#1801) | XL | defer | This is the MM half of the LUS port-maintenance migration and only makes sense together with a fork LUS bump that also covers SoH's bumps. |
| `2S2H 1eb6eb850` | #1844 | bump LUS (#1844) | XL | defer | This is a submodule-only bump, so the value is entirely in the LUS commits. |
| `2S2H 337ec608d` | #1925 | Bump LUS (#1925) | XL | defer | This is a submodule-only bump carrying real robustness fixes: - non-atomic config save, which can truncate shipofharkinian.json on a crash or kill - a POSIX shutdown- ... |
| `2S2H 16774f2d7` | #1805 | bump lus again (#1805) | XL | defer | The 2S2H side is only an API follow-up to a LUS bump. |

## Ship of Harkinian (`games/oot/`)

681 commits since `e149e09e0`: 41 ported, 23 partially ported, 556 absent, 61 not applicable. 518 are candidates. The finder's top categories are logic fixes (154), enhancements (62), infrastructure refactors (53) and other bug fixes (50). Two syntax gaps shape almost every rando port: our region tables keep the older `Entrance(..., []{...})` lambda form where upstream now writes `ENTRANCE(...)` macros (`077fda874`, #6031), and our enums live in `randomizerTypes.h` where upstream generates them from X-macro headers (`2b650a497`, #6223). Until O27 lands, every logic hunk is translated by hand; the bundles below say so where it matters.

| Id | Bundle | Priority | Effort | Commits | LUS | Goldens move |
|---|---|---|---|---|---|---|
| O1 | Item pool: maps, compasses, Farore's Wind, bean souls, hearts | P0 | S | 5 (Sx5) | no | yes |
| O2 | Starting state and savefile fixes | P1 | S | 9 (Sx9) | no | yes |
| O3 | Hint generation | P1 | S | 10 (Sx10) | no | yes |
| O4 | Shuffle Speak and Shuffle Grab logic clauses | P1 | S | 9 (Sx8 Mx1) | no | yes |
| O5 | Shuffle Climb and Shuffle Open Chest logic fixes | P1 | M | 9 (Sx6 Mx3) | no | yes |
| O6 | Logic correctness singles | P1 | M | 25 (Sx23 Mx2) | no | yes |
| O7 | Entrance shuffle: savewarps, grottos, trackers | P1 | S | 9 (Sx8 Mx1) | no | yes |
| O8 | Bombchu accounting | P1 | S | 6 (Sx4 Mx2) | no | yes |
| O9 | Rando generation thread safety and safe file writes | P0 | M | 5 (Sx3 Mx2) | no | no |
| O10 | Menu and preset robustness | P1 | S | 13 (Sx11 Mx2) | no | no |
| O11 | Always-on engine crash and softlock fixes (AlwaysOnFixes.cpp) | P0 | M | 21 (Sx20 Mx1) | no | no |
| O12 | Audio engine hardening (shared mixer, both games) | P0 | M | 13 (Sx8 Mx4 XLx1) | no | no |
| O13 | Save-file compatibility and SaveManager | P1 | S | 10 (Sx9 Mx1) | no | no |
| O14 | Timesaver and cutscene-skip correctness | P1 | M | 33 (Sx32 Mx1) | no | no |
| O15 | Cutscene-skip features: time of day, warp cutscenes, quick boss deaths | P2 | M | 7 (Sx2 Mx5) | no | no |
| O16 | Enemy randomizer fixes | P2 | M | 17 (Sx13 Mx1 Lx3) | no | no |
| O17 | Frame interpolation | P2 | M | 5 (Sx2 Mx3) | no | no |
| O18 | Camera, free look, controls and the speed-modifier half-port | P1 | M | 14 (Sx9 Mx5) | no | no |
| O19 | Item Tracker and Check Tracker | P2 | L | 17 (Sx13 Mx2 Lx2) | no | no |
| O20 | Custom messages and text | P2 | S | 20 (Sx16 Mx3 Lx1) | no | yes |
| O21 | Small QoL enhancements with SoH rows | P2 | M | 34 (Sx31 Mx3) | no | no |
| O22 | Better Save Menu | P3 | L | 5 (Sx3 Lx2) | no | no |
| O23 | Hint Tracker window and hinted-check marking | P2 | L | 3 (Mx1 Lx2) | no | no |
| O24 | Custom item icons, OOTRS music packs, file-select quests | P3 | L | 4 (Sx1 Mx1 Lx2) | no | no |
| O25 | Accessibility | P2 | S | 2 (Sx2) | no | no |
| O26 | Build, extraction and generation performance | P0 | S | 28 (Sx22 Mx6) | no | no |
| O27 | Rando infrastructure prerequisites | P2 | L | 23 (Sx12 Mx6 Lx4 XLx1) | no | yes |
| O28 | Region refactors, tricks and the 9.2.x merge bundles | P2 | L | 22 (Sx6 Mx10 Lx5 XLx1) | no | yes |
| O29 | New OoT shuffles (OoTMM parity) | P3 | XL | 29 (Sx15 Mx4 Lx4 XLx6) | no | yes |
| O30 | Rando options that need coordinator decisions | P3 | XL | 15 (Sx2 Mx7 Lx4 XLx2) | no | yes |
| O31 | Models, assets and get-item draws | P3 | M | 17 (Sx14 Mx3) | no | no |
| O32 | Custom equipment and model modding | P3 | L | 11 (Sx4 Mx5 Lx2) | no | no |
| O33 | Netplay: Anchor, Crowd Control and Sail | P3 | M | 10 (Sx8 Mx2) | no | no |
| O34 | Ivan co-op | P3 | M | 5 (Sx3 Mx2) | no | no |
| O35 | Developer tools | P3 | L | 15 (Sx7 Mx3 Lx5) | no | no |
| O36 | Hookify refactors (take only as prerequisites) | P3 | M | 21 (Sx9 Mx8 Lx4) | no | no |
| O37 | Platform, asset pipeline and decomp-naming decisions | P3 | XL | 11 (Sx3 Mx1 Lx5 XLx2) | no | no |

### O1. Item pool: maps, compasses, Farore's Wind, bean souls, hearts

Priority P0, effort S (5 commits: Sx5).

- **Prerequisites:** None.
- **RSBS adaptation:** 60205ec0b: re-add the ten `Item(...)` rows and `SetCustomDrawFunc(Randomizer_DrawBeanSprout)`; skip the whitespace. 65c7c3e4a changes the OoT heart pool shape (11 HC + 24 PoH becomes 8 + 36) and must be reconciled with the per-family shared-health policy of PR #744 (`src/common/shared_items.h`, `test_shared_quantity_policy.c`).
- **Test impact:** OoT placement and hint goldens re-pin (maps and compasses occupy locations junk filled before); rando tier; `test_shared_quantity_policy`.
- **Note:** Verified 2026-10-26: `item_pool.cpp:855` and `:859` pass `false` as the count, so no map or compass enters the pool; there is no `AddItemToPool(RG_FARORES_WIND, ...)` (`:183-184`); the ten bean-soul rows are missing from `item_list.cpp` outright while `item_pool.cpp:564-575` still adds the souls, so Shuffle Bean Souls places ten items with no item entry; the heart pool is still `maxHearts >> 2` (`:889-892`). A port of 65c7c3e4a breaks no assertion (`test_combo_logic_measure.c` B7 pins only 44 + 6) but makes `shared_items.h:744-748` and the `SqpShippedShape` fixture stale.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `9991a95ab` | #6180 | fix maps and compasses not being added to the pool (#6180) | S | port | For Own Dungeon (our default), Any Dungeon, Overworld and Anywhere, no dungeon map or compass ever enters the OoT pool, so they can't be obtained, and map-dependent f ... |
| `c360b2dee` | #6176 | Fix missing farores (#6176) | S | port | Farore's Wind is never in the OoT rando pool, so players can't get it and the slot goes to junk. |
| `0f948efb5` | #6334 | Keep Nayru's Love & Farore's Wind in scarce item pool (#6334) | S | port | Scarce item pool keeps one Nayru's Love and one Farore's Wind, matching N64 rando. |
| `60205ec0b` | #6156 | item list: bring back bean souls (#6156) | S | port-with-adaptation | Shuffle Bean Souls places 10 items whose table rows are empty, so getting one has no valid GetItemEntry and the combo coordinator ignores them. |
| `65c7c3e4a` | #6226 | Fix Heart Containers/Pieces Item Pool Calculation (#6226) | S | port-with-adaptation | The Balanced pool at 3 starting hearts changes from 11 heart containers + 24 pieces to 8 + 36 (vanilla's container count). |

### O2. Starting state and savefile fixes

Priority P1, effort S (9 commits: Sx9).

- **Prerequisites:** d62c5879a: port the Weird Egg half only; the Zelda's Letter half belongs to 1ea720607 (O29).
- **RSBS adaptation:** 7effe2891: the wallet fill moves after Link's Pocket; our shared wallet tier (#1058) reads OoT's flag, so check the fill against the shared tier.
- **Test impact:** rando tier; `oot_abandoned_session_test` and the starting-items golden profile; OoT goldens move only through 7006ef740 (hint targets).
- **Note:** d67dcb92f: 'Start with Ocarina of Time' removes the item from the world but the save gets the Fairy Ocarina, so the Ocarina of Time is unobtainable on that file.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `d67dcb92f` | #6227 | Bug Fix: Setting Start with OoT only gives Fairy Ocarina in Rando (#6227) | S | port | With 'Start with Ocarina: Ocarina of Time', logic assumes the Ocarina of Time is owned and removes it from the world, but the save only receives the Fairy Ocarina. |
| `7effe2891` | #6846 | move wallet fill after Link's Pocket (#6846) | S | port |  |
| `29b9b4a5a` | #6706 | Skip Epona Race: don't set EVENTCHKINF_EPONA_OBTAINED until collecting Epona's Song (#6706) | S | port | Setting the Epona flag at file start despawns the Lon Lon gates and allows an item-less, out-of-logic Epona steal. |
| `b15ac97de` | #6358 | Fix adult trade items spoiling during file load in rando (#6358) | S | port | With Shuffle Adult Trade on, the vanilla spoil-revert runs on file load because the rando VB override is not registered yet, so the adult trade slot gets overwritten. |
| `585530f68` | #6921 | Fix mixed up StartingItemGive between Weird Egg & Zelda's Letter (#6921) | S | port |  |
| `371ef6f84` | #7157 | Leave Start Randomizer disabled after generation fails (#7157) | S | port |  |
| `7006ef740` | #6146 | Don't put unshuffled master sword into playthrough (#6146) (partially ported) | S | port-with-adaptation |  |
| `04ccd608b` | #7222 | Fix equipping sword over swordless while fishing/riding (#7222) | S | port-with-adaptation | If a swordless rando player equips or obtains a sword while temp-B is active (fishing, Epona, minigames), the swordless restore wipes the sword off B. |
| `d62c5879a` | #6962 | Fix problems with Zelda's Letter & Weird Egg hatching (#6962) | S | port-with-adaptation | In rando the Weird Egg hatched only if the child-trade slot was currently showing the egg. |

### O3. Hint generation

Priority P1, effort S (10 commits: Sx10).

- **Prerequisites:** None. Sub-commits worth taking in the same lane: 0dcd52e5c (#6551, warp shuffle without warp hint text) and ca38dba19 (#6477, hint clarities restored), both inside merge commits listed under O28.
- **RSBS adaptation:** 983d04d36: use `OoT_gPlayState` in the static-hint builders.
- **Test impact:** OoT hint goldens re-pin for seeds with junk weight above zero (2f262f492) and with 1 to 3 required trials (af485bf64); rando tier.
- **Note:** Closes #1162 (xref family 1). Verified: `hints.cpp:525` still reads `junkIdx = distTable.size()` (upstream `size() - 1`); `hints.cpp:470-471` puts the required trials into an unused local so all six are hinted; `hint.cpp:254` resets `namesTemp` inside the per-location loop. 0dcd52e5c and ea76550fc reached develop through the merge 35a2ce4bf; both are absent here (`StaticHints.cpp:186-192`, `death_mountain_trail.cpp:60`).

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `2f262f492` | #6824 | [Bugfix] Junk stone hints not generated (#6824) | S | port | An off-by-one means weighted junk rolls never produce junk stones. |
| `af485bf64` | #7046 | Fix bugs in hints (#7046) | S | port | With 1-3 required trials, hint slots go to all six trials instead of only the required ones. |
| `983d04d36` | #6194 | Restores missing hint types that got lost in a merge (#6194) | S | port-with-adaptation | With Mido's Hint or Boss Door Hints on, the NPC or door shows vanilla text, so the menu option does nothing in game. |
| `18bf4b315` | #6546 | RC_HF_OCARINA_OF_TIME_ITEM gated on RSK_OOT_HINT (#6546) | S | port | With the Ocarina of Time dedicated hint on, the HF Ocarina of Time item still takes an always gossip-stone hint, so that item is hinted twice and a stone is wasted. |
| `317454574` | #6331 | Fix Ganondorf hint formatting (#6331) | S | port |  |
| `c3ffaa918` | #6295 | Don't autoformat altar hints twice. (#6295) | S | port |  |
| `99e60b81f` | #6332 | fix rando fire temple goron text formatting (#6332) | S | port |  |
| `ba08197d3` | #6912 | Avoid double line break when revealing MQness of spirit temple (#6912) | S | port |  |
| `49aa33038` | #6320 | Restores Greg's textbox icon (#6320) | S | port |  |
| `98ec5519c` | #6409 | Improve flavour of Navi's boss key hint (#6409) | S | port |  |

### O4. Shuffle Speak and Shuffle Grab logic clauses

Priority P1, effort S (9 commits: Sx8 Mx1).

- **Prerequisites:** Shuffle Speak (e2db315ff) and Shuffle Grab (18b00e7bc) are ported. Sub-commit ea76550fc (#6564, owl talk includes Kokiri) rides along from 35a2ce4bf.
- **RSBS adaptation:** a393f48c7 and a547d51ab must ship together (Malon's Lon Lon Ranch lines share text ids). c63f57850 needs e6947c733 first.
- **Test impact:** OoT goldens move only through 0702b11b9 with Shuffle Speak off; rando tier.
- **Note:** Closes #1168 (xref family 2): logic and game disagree on which NPCs need a jabber nut, so a seed can require an unobtainable check. Verified absent: `haunted_wasteland.cpp:26` (no parentheses), `lon_lon_ranch.cpp:14`, `thieves_hideout.cpp:15,33,57,71` (the `LOGIC_TH_COULD_FREE_*` events already exist, only the rows need switching), `location_list.cpp:223-224` (3601/0x0E and 3857/0x0F still swapped).

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `c906c71f7` | #6357 | Song from Malon: requires hylian jabbernut (#6357) | S | port | Logic and game disagree under Shuffle Speak: logic can expect Song from Malon without the Hylian 'jabbernut' the game requires. |
| `1413ceb33` | #7121 | Fixed carpenter guard hylian jabbernut requirement (#7121) | S | port-with-adaptation | With Shuffle Speak on, logic treated the four carpenter checks as needing only a Gerudo kill, and the Boss Intro skip let the player free carpenters without Hylian. |
| `2bc50f605` | #7220 | Fix deep slope vs double cell carpenter mix up (#7220) | S | port | The two Thieves' Hideout carpenter checks have swapped actor params and collectible flags, so the key the game hands out at one cell is the item logic placed at the o ... |
| `0702b11b9` | #6285 | fix carpet man logic (#6285) | S | port | An operator-precedence bug lets logic treat the carpet salesman as reachable with only a jumpslash, ignoring Speak Hylian (Shuffle Speak is ported) and wallet capacity. |
| `63597ec63` | #6324 | Prevent winning Talon's game without str0. Prevent starting Diving Game without Zora Jabbernut (#6324) | S | port | Makes the game enforce what Shuffle Grab and Shuffle Speak (both ported) intend. |
| `e6947c733` | #6255 | Business Scrubs: don't speak without jabber nut (#6255) | S | port | With Shuffle Speak on, business scrubs (EnDns) start talking by themselves within 130 units. |
| `c63f57850` | #6265 | Deku Theater: better handling of timesaver being disabled for shuffle speak (#6265) | S | port | In rando with Shuffle Speak (or when the player is more than 30 units from the stage), the vanilla Judge path runs and EnDntJiji offers the vanilla stick/nut upgrade ... |
| `a393f48c7` | #6299 | Fix missable Malon check (#6299) | M | port-with-adaptation | In rando, Malon can leave Hyrule Castle before the player has both received the Weird Egg check and woken Talon, making that check missable, which can make a seed unb ... |
| `a547d51ab` | #6308 | Fix Malon text on Lon Lon Ranch in rando (#6308) | S | port-with-adaptation | Restricts a393f48c7's replacement Malon text to Hyrule Castle, so her Lon Lon Ranch lines (same text ids 0x2047-0x2049) are not overwritten. |

### O5. Shuffle Climb and Shuffle Open Chest logic fixes

Priority P1, effort M (9 commits: Sx6 Mx3).

- **Prerequisites:** Shuffle Climb (596b714fa) and Shuffle Open Chest (0de6edfe) are ported. 09b0fdbc9 and 848106039 reference `RT_HOOKSHOT_LADDERS` and `CanClimbLadder` from 60ec65592 (O28): drop those clauses until O28 lands.
- **RSBS adaptation:** Our region tables keep the older `Entrance(..., []{...})` lambda syntax, not upstream's `ENTRANCE` macro; translate by hand. New regions go into `randomizerTypes.h` (b6cdc6c49 adds `Kak Behind Potion Shop`, 848106039 adds `Entrance Side Bridge`). ec27775e8: only the DMC wall PoH clause applies now; the rest belongs to the DMC refactor (O28).
- **Test impact:** OoT placement goldens re-pin (regions change); rando tier.
- **Note:** Every fix here is logic that is too permissive for an ability shuffle we already ship, so a seed can place progression where the player cannot reach it.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `743103fbe` | #6244 | Fix Logic in Deku Tree (#6244) | S | port-with-adaptation | With Shuffle Open Chest (ported here, 0de6edfe) or Shuffle Climb, logic treats the Deku Tree map chest as obtainable from 2F without the open-chest ability, or from 1 ... |
| `b6cdc6c49` | #6288 | Fix kak backyard logic with shuffle climb (#6288) | M | port-with-adaptation | Fixes logic for the already-ported Shuffle Climb: reaching the potion shop's back door from Kak Backyard needs the ladder (RG_CLIMB). |
| `fe5dce24b` | #6296 | Misc fixes for Shuffle Climb (#6296) | M | port-with-adaptation | Fixes logic for the already-ported Shuffle Climb: DC Armos Room needs Climb with Power Bracelet; a Spirit typo linked the locked door to 2F instead of 1F; two Deku Tr ... |
| `ec27775e8` | #6679 | Fix Logic issues in DMC > Fire Temple and DMC wall PoH (#6679) | M | port-with-adaptation | With Shuffle Climb on, our logic considers the DMC wall PoH reachable without Climb, so a seed can put required progression there out of reach. |
| `848106039` | #6950 | fix climb requirements for MQ DC Torch room (#6950) | S | port-with-adaptation | With Shuffle Climb on, our logic can assume MQ Dodongo's Cavern torch-room exits without climb, which can make a seed's logical route impossible. |
| `7be5a4fea` | #7367 | Fix missing open chest logic in Jabu MQ (#7367) | S | port | With Shuffle Open Chest on, logic treats this chest as openable without the Open Chest ability. |
| `688913270` | #7014 | Fix potential key softlock with hover boosts in MQ spirit (#7014) | S | port-with-adaptation | With the hover-boost trick on, adult can spend an MQ Spirit key without reaching the Statue Room. |
| `ed4868f62` | #6851 | Check for the Wolfos Gate before letting child out of SFM without climb. (#6851) | S | port-with-adaptation | Logic currently lets child leave the meadow back through the Wolfos gate without ever opening it, which matters when arriving by warp song, entrance shuffle or the cl ... |
| `09b0fdbc9` | #6380 | Fix GV Waterfall Alcove Logic (#6380) | S | port-with-adaptation | Logic wrongly lets plain Hookshot plus Climb, or Longshot without Climb, reach the GV waterfall alcove. |

### O6. Logic correctness singles

Priority P1, effort M (25 commits: Sx23 Mx2).

- **Prerequisites:** c238f0745 and c5896c7ef must ship together (the HasItem refactor and its boss-soul regression fix). ff0209e76's two Fire Temple hunks wait for 53f688df5 (O28). 041295f05 assumes 2bfae47b7 (#6233, marked not applicable; translate its `RR_LH_LAB_UNDERWATER` exits by hand).
- **RSBS adaptation:** Lambda-syntax translation throughout. 0e99b30e9 (Closed Forest forces a child start) touches `src/common/tests/test_combo_single_bag.c`. 3cfe4ddff needs a hook site: either port aedae12e6's `VB_ITEM00_TIMER_TICK` or inline the exemption in `z_en_item00.c`.
- **Test impact:** OoT placement goldens re-pin; rando tier; the determinism rows.
- **Note:** Each is a one-to-five-line logic error in a region table: the ZR to Lost Woods leak (5e802f232), the Dodongo's Cavern side-room pots a room early (78e3683ef), the Light Medallion missing from Medallion Locked Trials (6f01d68c3), and swapped chest params in Spirit MQ (e987cdae2). Verified absent: `zoras_river.cpp:64`, `dodongos_cavern.cpp:63-64` (a logic-only port can keep the old `RC_` names), `ganons_castle.cpp:35,358` (the other five trials already carry the gate), `forest_temple.cpp:110` (the MQ twin at `:639` already uses `&&`).

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `04eb8d160` | #6248 | Forest: fix hover recoil trick using wrong operator (#6248) | S | port | Logic currently grants access to the Forest Temple NW courtyard upper area with the Megaton Hammer alone, with no trick enabled. |
| `5e802f232` | #6154 | Fix ZR/LW entrance shuffle leak (#6154) | S | port | With overworld entrance shuffle on, logic assumes a fixed ZR->Lost Woods path even after the underwater shortcut is shuffled somewhere else. |
| `6f01d68c3` | #6314 | Fix missing logic for Medallion Locked Trials (#6314) | S | port | With Medallion Locked Trials on, the game bars the Light Trial without the Light Medallion, but logic does not require it. |
| `7eb5acf22` | #6254 | Fix wrong logic var in spirit shared (#6254) | S | port | Spirit Temple shared-region key logic checks the forward-adult event where it means the reverse one. |
| `e987cdae2` | #6275 | Fix misc issues (#6275) | S | port | The swapped params make the Spirit MQ Beamos Room and Chest Switch chests give and track each other's checks, which breaks logic for two chests with different access. |
| `1e1ba54cf` | #6251 | Water Temple: fix waterfall GS logic (#6251) | S | port | The falling-platform Gold Skulltula token must be collected, not just killed, from range. |
| `210c5d588` | #6818 | fix oversight on rising target ledge > pillar high water (#6818) | S | port-with-adaptation | This is a one-term logic fix: the pillar high-water path from the rising-target ledge needs low or mid water. |
| `293adcf9e` | #6247 | fix water level logic (#6247) | S | port-with-adaptation | Fixes how the Water Temple 'could raise to high from mid' event is computed. |
| `f3d881144` | #7324 | Fix water entrance graph typo (#7324) | S | port-with-adaptation | A one-token logic fix. |
| `ff0209e76` | #7283 | fix logical typos (#7283) | S | port-with-adaptation | These operator-precedence typos let logic skip requirements, e.g. |
| `f04b570d2` | #7181 | Fix logic typo (#7181) | S | port |  |
| `abcb3ad94` | #6674 | adult-only crate (#6674) | S | port | Logic treats the GF above-jail crate as reachable as child, when only adult can reach it. |
| `adf31d5eb` | #6743 | Fix Bottom of the Well Coffin Logic (#6743) | S | port | The fire/bow requirement was on the wrong coffin heart, so with freestanding shuffle, logic can expect progression in a heart that is unreachable without a fire source. |
| `78e3683ef` | #7107 | Side room pot 5 and 6 logic fix (#7107) | S | port-with-adaptation | Two pots physically near Lower Lizalfos are logically placed one room early. |
| `c3c80df37` | #6691 | Don't prevent child from accessing Ganon's Castle or GV Tent (#6691) | S | port | It drops two over-strict child-access restrictions that make entrance-shuffle validation reject worlds more often, and guards a null dereference when writing the spoiler. |
| `94a02dd53` | #6363 | Granny: don't sell blue potion / item without claim check when adult trade not shuffled (#6363) | S | port |  |
| `45b932159` | #7295 | [Bugfix] - Vanilla Shopsanity Prices (#7295) | S | port | With Vanilla prices selected, four shop/scrub slots sell at the wrong price, and wallet logic follows those wrong prices (e.g. |
| `8fd41474c` | #6207 | Fix logic thinking child exiting OGC GFF leads to castle grounds, not past gate (#6207) | S | port-with-adaptation | Adds two 'Castle Grounds From Great Fairy/Grotto' routing regions so that, with interior/grotto entrance shuffle, the logic sends a child leaving either fairy fountai ... |
| `0e99b30e9` | #6282 | Add StartingAge limits to Closed Forest (#6282) | M | port-with-adaptation | With Closed Forest and no forest-escaping entrance shuffle, an adult start lets the player save-load as child inside the forest and get stuck. |
| `041295f05` | #6671 | Fix Guard pots being breakable with hookshot in logic (#6671) | M | port-with-adaptation | Our logic lets interior checks (Market Guard House pots, house pots, LH Lab) be satisfied by items the game forbids in those scenes, which can produce seeds that are ... |
| `19df71112` | #6284 | remove castle moat spawn override (#6284) | S | port |  |
| `d54a69833` | #7230 | avoid big poe collector not giving reward when handing bottle after collecting 1000 points worth of big poes on field (#7230) | S | port-with-adaptation | Field big poes credit +100 each in rando, so a player at 1000 points who hands over a bottled poe gets pushed to 1100 and misses the vanilla exact-1000 payout. |
| `3cfe4ddff` | #7183 | Prevent despawning shuffled gerudo guard drops (#7183) | S | port-with-adaptation | Shuffled key or heart-piece drops from enemies such as the Gerudo guards lose the never-despawn exemption after conversion to ITEM00_SOH_DUMMY, so a randomized check ... |
| `c238f0745` | #6798 | fix regression, lost boss souls from HasItem (#6798) | S | port-with-adaptation |  |
| `c5896c7ef` | #6795 | Combine HasBossSoul & CanOpenOverworldDoor into HasItem (#6795) | S | port-with-adaptation |  |

### O7. Entrance shuffle: savewarps, grottos, trackers

Priority P1, effort S (9 commits: Sx8 Mx1).

- **Prerequisites:** d1d6a564e expects the two earlier swim-void fixes: c93b6188c (here) and sub-commit 69681e608 (#6529, grotto swim void). 92cb7f159 assumes 077fda874's macro form for one `ganons_castle.cpp` hunk; translate it.
- **RSBS adaptation:** Use `OoT_gPlayState` and `OoT_gSaveContext` in `randomizer_grotto.c` and `randomizer_entrance.c`. 9eafd15fe (a grotto grabbed before the fade-in ends) needs a VB in `Door_Ana`; our tree has the same MM Bunny Hood speed path.
- **Test impact:** Entrance goldens (profile `entrances`) re-pin through 92cb7f159; rando tier; the crossing-pin lock of #1054 stays green.
- **Note:** bdccc868b is the decoupled-entrance savewarp override: `entrance.cpp:1710-1714` keeps the old guard, so decoupled entrances keep `destinationIndex = -1`. fe78ae21a is wider than a stale variable: `randomizer_entrance.c:152,165,209` compare `index`, which ends at `ENTR_HYRULE_FIELD_16_3 + 1`, so the boss-door savewarp match never fires.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `bdccc868b` | #7085 | Fix entrance overrides being different between loading spoiler vs starting seed (#7085) | S | port-with-adaptation |  |
| `fe78ae21a` | #6676 | Properly send Link to the beginning to the dungeon in boss room shuffle (#6676) | S | port | With boss room shuffle on, saving or dying in a shuffled boss room should return Link to the dungeon's start; our stale loop variable makes it fall back to the origin ... |
| `849928689` | #6721 | Only store grotto entrance for voiding out when grottos mixed, or in decoupled (#6721) | S | port-with-adaptation | With grotto shuffle on but not mixed or decoupled, voiding out after a normal grotto exit should respawn at the pre-grotto entrance, not the grotto's return point. |
| `d1d6a564e` | #6854 | Bugfix, grotto boulder block respawn from water voidout (#6854) | S | port-with-adaptation | Without Swim, a void-out in water near a grotto can respawn the player on the wrong side of the grotto boulder, and the grotto start-jump runs at half water speed. |
| `c93b6188c` | #6360 | Fix swimvoid in Zora River respawning in unloaded room (#6360) | S | port | With entrance rando, voiding out in water without Swim in Zora's River could respawn Link in a room that is not loaded, per the upstream #6360 message. |
| `9d42aedc9` | #6888 | Bugfix, make entrance tracker track blue warps (#6888) | S | port | With decoupled entrances on, blue warps are real shuffled entrances, but the tracker hides them, so players lose route information. |
| `aa25eb4d5` | #7276 | mark ganon's tower entrances as oneExit (#7276) | S | port |  |
| `92cb7f159` | #6189 | fix crash when generating decoupled entrances with boss shuffle (#6189) | M | port-with-adaptation | Fixes a generation crash with Decoupled Entrances plus Boss Entrance Shuffle, a regression from the Ganon's Tower shuffle we ported. |
| `9eafd15fe` | #7271 | Fix grotto entrance misbehaving when entering grotto before last transition not done (#7271) | S | port-with-adaptation | Running fast in the bunny hood, the player can reach a grotto hole before the scene's fade-in ends, and per upstream the entrance then misbehaves. |

### O8. Bombchu accounting

Priority P1, effort S (6 commits: Sx4 Mx2).

- **Prerequisites:** cc63cc4ab and c42004bc4 rewrite the same `AddCheckToLogic` line; apply in order. 2ba5c40da (#7134) is not here: the verifier found it changes no behaviour.
- **RSBS adaptation:** Our shared bombchu count credits a far pack at pickup (#1077); the logic side here is OoT's solver only, which the coordinator wraps.
- **Test impact:** OoT goldens re-pin where the playthrough paring changes (31c0d8554); rando tier.
- **Note:** The #1164 family. None of these is the bomb-bag grant path that #1164 describes; the xref doc keeps that lead RSBS-local. Verified absent: `fill.cpp:474` still reads `IsBombchus(ignore) && IsBombchus(locItem, true)` with no `!`; `logic.cpp:2298-2301` sets `ITEM_BOMBCHU` for every pack without the bag-option checks. Bombchus are a shared PROGRESSION family here (#1061, #1077), so run both ctest tiers.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `cc63cc4ab` | #7117 | fix logic bugs with Bombchus (#7117) | S | port-with-adaptation | When fill is told to ignore bombchus, the inverted check applies the wrong items. |
| `c42004bc4` | #7274 | Fix AddCheckToLogic bombchu handling (#7274) | S | port | During playthrough paring, an ignored bombchu or shop item was not filtered correctly: shop bombchus were inconsistently included and the shop-LogicVal branch never ran. |
| `31c0d8554` | #7275 | Fix playthrough paring to not rely on checks past wincon (#7275) | M | port-with-adaptation | Items reachable only once the win condition is met (in or after the final sphere) could stand in for a removed playthrough item during paring. |
| `483849b97` | #6717 | Single bombchu bag mode should give refills (#6717) | S | port | In single-bag bombchu mode, later bombchu items are treated as already-owned instead of acting as refills. |
| `44909ab23` | #6949 | Rando: Adds Bombchu Refills to junk pool if drops are on (#6949) | S | port-with-adaptation |  |
| `360f4882d` | #6727 | rando: option to allow swordless epona items (#6727) | M | port-with-adaptation | Adds a rando option (default off) to re-allow the vanilla swordless-on-Epona C-item glitch. |

### O9. Rando generation thread safety and safe file writes

Priority P0, effort M (5 commits: Sx3 Mx2).

- **Prerequisites:** None.
- **RSBS adaptation:** a7cd6c811: the paired creation event (`OoT_RunPairedCreationE...` in `OTRGlobals.cpp`) and the seed-package binding of #1116 sit on the same path; the wait must cover them. 9d81cb03c: decide what a failed `.sav` write does to the generation `SaveSection` already staged.
- **Test impact:** redship and rando tiers; `AllTests` slot-0 rows (#1136, #1121 made these sensitive to write paths).
- **Note:** Verified: `DeinitOTR` (`OTRGlobals.cpp:2310-2331`) does not join `randoThread`, so quitting during a generation reaches `return 0` with a joinable `std::thread` (`randomizer.cpp:3476`), which is `std::terminate`. The after-generation case is mostly covered by the reap #1044 added to `FileSelectTick`. a7cd6c811 is partial here: the file select reaps a finished worker every frame, but `FileChoose_LoadGame` does not wait and there is no atomic flag, only the `RandoGenerating` CVar.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `a7cd6c811` | #7136 | Avoid randomizer generation races with file select (#7136) | M | port-with-adaptation | Today a player can start or create a file while randoThread is still writing Rando::Context, and a generation can start during the file-load transition. |
| `69a7cdb17` | #7296 | [Bugfix] Fix crash on exit after rando gen (#7296) | S | port-with-adaptation | After a generation started from file select or the menu, quitting without the randomizer menu's spoiler widget being drawn leaves randoThread joinable, which aborts t ... |
| `9d81cb03c` | #7299 | WriteFilesSafely (#7299) (partially ported) | M | port-with-adaptation | Today a failed or partial write (disk full, crash after rename) can replace a good .sav with a truncated or zeroed file, because the temp file is renamed regardless o ... |
| `7cdf69b20` | #7317 | Use WriteFileSafely more (#7317) | S | defer |  |
| `e4b6f1ab8` | #6786 | clamp settings (#6786) | S | port-with-adaptation | A saved randomizer CVar beyond the option's current count (for example after option lists change between versions or config migrations) reads past the vector and cras ... |

### O10. Menu and preset robustness

Priority P1, effort S (13 commits: Sx11 Mx2).

- **Prerequisites:** ed7e4d029 assumes the intermediate Locations-page commits (b83ca39c9, 21a270c0b, 716f6bc04, 04ed1825f, ccfa31a24); port the page as its final form.
- **RSBS adaptation:** b043cdf40 is a one-character off-by-one that drops the excluded Ganon's Castle locations from the spoiler JSON. 00a51c650: keep the hidden `##` suffix out of the character-count wrap in `Menu::MenuDrawItem`. Our unified menu adds the MM and Cross-Game pages to the same search, so ImGui id collisions are more likely here than upstream.
- **Test impact:** redship tier; UI snapshot harness for the Randomizer, Locations and Settings pages.
- **Note:** a35f33118: a stored CVar value with no combobox entry throws `std::out_of_range` while the menu draws and brings the process down. Not spot-checked on 2026-10-26.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `a35f33118` | #7062 | Keep an unlisted combobox value from killing the menu (#7062) | S | port | A stored CVar value with no entry in the combobox map throws std::out_of_range from map::at while the menu draws, which brings the menu (and the process) down as soon ... |
| `4a875dca4` | #6806 | fix(menu): guard backend name lookups against unknown backends (#6806) | S | port |  |
| `5875ed880` | #6712 | Fix Malformed Preset Filenames (#6712) | S | port | A malformed or non-JSON file in the presets folder makes json::parse throw out of LoadPresets, crashing the game. |
| `b83ca39c9` | #6263 | Make tricks and locations lists re-evaluate after preset application. (#6263) | S | port | After a randomizer preset is applied, the Locations and Tricks pages keep their cached excludedLocations/enabledTricks sets. |
| `21a270c0b` | #6228 | Bug Fix: MQ Dungeon setting not updating locations in Locations tab (#6228) | S | port | The randomizer Locations (exclusion) tab now shows MQ locations when MQ Dungeons is set to Selection, and refreshes when the MQ setting or count changes. |
| `716f6bc04` | #6352 | Always call `RandomizerCheckObjects::UpdateImGuiVisibility` for locations view (#6352) | S | port-with-adaptation |  |
| `00a51c650` | #6771 | fix imgui id conflicts (#6764) (#6771) | S | port-with-adaptation | On the search page, randomizer options that share a label with an enhancement (for example "Blue Fire Arrows") collide on ImGui ID and misbehave. |
| `bbb4c6499` | #6765 | fix id conflicts (#6765) | S | port |  |
| `5508cf363` | #7312 | Add migrations for presets and drag-and-drops (#7312) | M | port-with-adaptation | Runs the config version updaters over drag-and-dropped configs and over applied rando presets, so old files no longer silently apply stale CVar names. |
| `71d1f5a9f` | #6705 | fix(audio): Properly filter-out unavailable audio backends from UI (#6705) | S | port-with-adaptation | On Linux/macOS the Audio API menu offers WASAPI. |
| `8a3ec35de` | #5994 | Add Edit buttons for custom presets (#5994) | S | port-with-adaptation |  |
| `ed7e4d029` | #6817 | Rando: UX improvements of Locations page (#6817) | M | port-with-adaptation | Reworks the rando Locations page: one shared filter for both columns, Open All/Collapse All per column, Exclude Visible/Include Visible bulk actions, a factored SaveE ... |
| `b043cdf40` | #7127 | Fix loop condition in WriteExcludedLocations function (#7127) | S | port |  |

### O11. Always-on engine crash and softlock fixes (AlwaysOnFixes.cpp)

Priority P0, effort M (21 commits: Sx20 Mx1).

- **Prerequisites:** Our tree has no `games/oot/soh/Enhancements/AlwaysOnFixes.cpp`; e9268c446 creates it upstream and 8602c6d15 consolidates every hook into one `RegisterAlwaysOnFixes`. Create the file first, then land the rest in its final form.
- **RSBS adaptation:** `OoT_` prefixes on the hook bodies. 61523391e grows `OoT_gGfxPools` to 1M entries each; `Gfx` is 16 bytes on 64-bit, so size the pools against the single-exe heap. 29c7e7568 is the `gItemSlots` bounds guard of xref family 3 (#1048 sized `sItemActions`, not this table).
- **Test impact:** redship tier; the ROM-free table-bounds lock row pattern of #985 for 29c7e7568; integration crossing rows for 5ecff16bd (the skybox path re-enters on every cross-game switch).
- **Note:** Verified: `graph.c:540-546` still casts the first gamestate (TitleSetup, `sizeof(GameState)`) to PlayState and calls `OoT_Skybox_Setup`, writing past its allocation; `sHasSetupSkybox` is cleared in `OoT_Graph_ResetRunFrameContext` from `OoT_Game_Suspend`, so the overrun fires again on every switch back into OoT, into a system arena that is rebuilt each time (which hides it). `z_room.c:416-418` has the camId guard only under NoRestrictItems or CrowdControl. `z_parameter.c:1897-1899` still indexes the unsized `OoT_gItemSlots` with any id. 7a7f942b8 is listed here although the 2026-10-18 verifier filed it as ported: `randomizer_item_tracker.cpp:827,648` call `INV_CONTENT(item.id)` with no bound and are fed `RG_` ids such as `RG_ROCS_FEATHER` (`:145`), a live out-of-bounds read.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `5ecff16bd` | #7045 | Fix heap corruption due to skybox code using GameState as PlayState (#7045) | S | port | Removes a write of PlayState.skyboxCtx into a much smaller non-Play GameState allocation, which is real heap corruption. |
| `e8b943968` | #7102 | Disable2DBackgrounds Fixes (#7102) | S | port-with-adaptation | With Disable 2D Backgrounds on, this syncs the replacement sky to the current time of day, keeps the cloudy adult weather in the Temple of Time and Market ruins, and ... |
| `e9268c446` | #6731 | Fix Outside ToT crash when using din's or dying (#6731) | S | port-with-adaptation | Dying, voiding out or changing camera (Din's Fire) in a prerendered-background scene such as Outside Temple of Time crashes the game. |
| `f2c34d8c1` | #6379 | Fix Entrance Rando Grotto Voidout Crash when voiding back to an area with a background image (#6379) | M | port-with-adaptation | With entrance rando, voiding out of a grotto back into a scene with a prerendered background (camId -1) crashes. |
| `4b6678b82` | #6810 | Vanilla bugfix, Deku Shield burning drop crash (#6810) | S | port-with-adaptation | Fixes a vanilla crash: the dropped burning Deku Shield assumes segment 0x0C still holds Link's gCullBackDList, which an intermediate draw (e.g. |
| `53b9db5b9` | #6805 | Vanilla bugfix, Hookshot with parent + Hookshot nospawn (#6805) | S | port-with-adaptation | Fixes two vanilla hookshot softlocks: the hookshot actor failing to spawn (e.g. |
| `984ccf80e` | #6834 | Vanilla bugfix, Goron Link first talk softlock (#6834) | S | port-with-adaptation | Fixes a vanilla softlock: if Link starts his first talk with Goron Link (before getting the tunic) and moves out of range, the text cannot advance while Goron Link sl ... |
| `8602c6d15` | #6408 | fix: restore BGM after Stalfos group defeat in MQ Water Temple (#6408) | S | port-with-adaptation | Fixes a vanilla bug where the miniboss music sticks after a Stalfos group dies in the same frame (MQ Water Temple), because Actor_FindNearby still counts just-killed ... |
| `033613a6c` | #6804 | Vanilla bugfix, ladder cutscene softlock (#6804) | S | port-with-adaptation | Dismounting a ladder during a cutscene, or while using a cutscene item (player->unk_6AD >= 3), stops the animation update and softlocks the game. |
| `2e300d3ac` | #7247 | force empty bottle OI to use the milk effect to avoid UB (#7247) | S | port-with-adaptation | Removes undefined behavior (an out-of-bounds static array read whose effect depends on the compiler and build) when the empty-bottle OI glitch is used, and forces the ... |
| `a44197712` | #6874 | Bugfix, prevent looping fanfares from softlocking HBA (#6874) | S | port-with-adaptation | A custom fanfare set to loop leaves Audio_IsSequencePlaying(NA_BGM_HORSE_GOAL) true forever, which softlocks Horseback Archery. |
| `3c565ed6e` | #6826 | Vanilla bugfix, allow fishing with blank B (#6826) | S | port-with-adaptation | Fixes the vanilla bug where the fishing pole is put away immediately when Link has nothing on B; it is always on in rando and a checkbox in vanilla. |
| `73df35c6a` | #6847 | Bugfix, child Hookshot first person in Hylia lab (#6847) | S | port-with-adaptation | Child Link aiming the Hookshot/Longshot inside Lakeside Laboratory gets the adult aim camera, because the child (slingshot) aim mode is invalid in that scene; this is ... |
| `6e7a9b47f` | #6640 | Fix undefined left shifts of 1 by 31 (#6640) | S | port | Shifting a signed 1 into bit 31 is UB, and switch flag 0x3F reaches it in ordinary play (Potion Shop Granny). |
| `29c7e7568` | #7021 | Fix Gold Skulltula tokens overwriting the Bow inventory slot (#7021) | S | port | Item_Give and Item_CheckObtainability index gItemSlots and sExtraItemBases with any item id, reading past the tables (UB). |
| `0d41af697` | #6253 | Ivan: Fix intermittent crash with certain items (#6253) | S | port | When Ivan (co-op fairy) uses the bow, slingshot, stick or hookshot, a Player-only function writes stateFlags2 past the end of the much smaller EnPartner allocation, c ... |
| `61523391e` | #7122 | Fix flare dancer crash (#7122) | S | port-with-adaptation | Grows the OoT display-list buffers to 1M entries each so heavy scenes, here enemy-rando Flare Dancer, cannot overflow them, then re-enables Flare Dancer. |
| `92afa8144` | #6958 | Fix a crash when opening the debug menu with a corrupted savegame (#6958) | S | port | The save editor dereferences end() for any inventory byte missing from itemMapping, and can write items[-1] through the -1 selectedIndex sentinel. |
| `e9ef09eee` | #6359 | Fix RBA gauntlet colors to match console (#6359) | S | port | Fixes an out-of-bounds read in Player_DrawImpl when an RBA glitch sets the strength upgrade above 3: it adds the N64 out-of-bounds color values to the table. |
| `f27ddede5` | #7052 | Fix mirror decal not rendering on some walls (#7052) | S | port-with-adaptation |  |
| `7a7f942b8` | #7103 | Fix item tracker fishing pole & roc's feather (#7103) (filed as ported on 2026-10-18) | S | skip | The useful part for us is the bounds guard before INV_CONTENT, which stops an out-of-bounds read of OoT_gItemSlots for RG_/high ITEM_ tracker entries on every frame t ... |

### O12. Audio engine hardening (shared mixer, both games)

Priority P0, effort M (13 commits: Sx8 Mx4 XLx1).

- **Prerequisites:** 945f70222 before 4e39d06ac. 9308ba794 and 2S2H 818e7a6bb (16-bit sound-font ids) are one XL change across the shared `AudioSequence` resource and both games' loaders; defer them behind the rest.
- **RSBS adaptation:** The mixer is shared: b6bf97e2f's `aLoadBuffer` hunk changes MM's callers too (`games/mm/src/audio/lib/synthesis.c`), and 17a4c2cf8 covers both games at once. b9bcc5e90 caches audio-thread CVars in atomics; both games' audio threads read them, so one `AudioSettings` TU in `src/common/` or in OoT's tree with MM reading the same atomics.
- **Test impact:** redship tier; the silent-test contract; a manual custom-music pack session.
- **Note:** Verified absent in both games: the ADPCM load pad and `ROUND_DOWN_16` memcpy (`audio_synthesis.c:899-903`, `mixer.c:107,111`, MM `synthesis.c:1209-1227`); `permanentCache[32]` with an unbounded write (`audio_heap.c:1012-1021`, MM `heap.c:1107-1110`); the `return true` stub in `IsFontLoadComplete` (`audio_load.c:341-342`); the audio thread reads CVars in five places. 945f70222 is OoT-only: MM's `load.c:805` already assigns `realId`.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `b6bf97e2f` | #6364 | Fix ADPCM sample buffer overread in audio synthesis (#6364) | M | port-with-adaptation | Removes an out-of-bounds read around every ADPCM sample load, which segfaults under strict allocators and is UB everywhere. |
| `4e39d06ac` | #6932 | fix(audio): bound audio-heap cache tables (SIGSEGV with large custom music packs) (#6932) (partially ported) | S | port-with-adaptation | Fixes memory corruption of gAudioContext (audio-thread crash) once more than 32 permanent allocations happen, which large custom music packs reach. |
| `945f70222` | #7105 | Restore the realId assignment in AudioLoad_AsyncLoadInner (#7105) | S | port | An uninitialized local is used as an array index for the load-status tables and cache allocations: UB that can read or write out of bounds on the async-load path. |
| `fdb7e194c` | #6916 | fix(audio): bounds-check fontId to stop OOB crash with large SAF packs (#6916) | S | port | With large custom-audio packs, a fontId past fontMapSize writes past the fontLoadStatus heap allocation, which corrupts the heap and causes random crashes. |
| `eb4142835` | #6736 | Fix custom music corruption past 256 sequences (#5989) (#6736) (partially ported) | M | port-with-adaptation | With large music packs, the resolved replacement id (which can exceed 255) rode a shared per-player slot that the async audio thread consumed. |
| `b9bcc5e90` | #7265 | Fix audio thread race on CVar reads during preset apply (#7265) | M | port-with-adaptation | Applying presets mutates the CVar map while the audio thread reads it, which is undefined behavior and can crash. |
| `17a4c2cf8` | #7137 | Reopen the OPUS decoder when a note moves to another sample (#7137) | M | port-with-adaptation | A note reused for another streamed OPUS sample keeps decoding the previous track, so the wrong custom music plays. |
| `8bf26365b` | #7129 | Honour the loop end when reading streamed music (#7129) | S | port | Streamed custom music (S16/OPUS) reads past the loop end before jumping back, so the loop seam pops. |
| `e07916487` | #7291 | AudioSequenceFactory: fix mono streamed tracks not playing (#7291) | S | port | Custom mono streamed music (Stereo=false sequences in mods) is silent: a stray 0x00 delay byte leaves channel->instrument unset. |
| `003a009eb` | #6796 | Better Octave Drop logic (#6796) | S | port |  |
| `0210374cc` | #5685 | Enable missing music entries in audio editor (#5685) | S | port |  |
| `57dc840d6` | #7372 | Audio editor: support setting track to None (#7372) | S | port-with-adaptation |  |
| `9308ba794` | #7260 | Carry sound font ids in 16 bits (#7260) | XL | defer | Packs with more than about 217 streamed tracks push font ids past 255, which wrap to vanilla fonts or 'no font'; upstream widens font ids to 16 bits. |

### O13. Save-file compatibility and SaveManager

Priority P1, effort S (10 commits: Sx9 Mx1).

- **Prerequisites:** 4c43ef030 before acdbc651d (same file).
- **RSBS adaptation:** RSBS keeps the `soh` app short name and save layout, so a player's pre-9.1 SoH saves in the shared directory are affected exactly as upstream describes. 2418d51df: port the `savestates.cpp` half only.
- **Test impact:** redship tier; the save-writing rows of #1121 and #1136.
- **Note:** 4c43ef030 is a startup crash when a legacy `oot_save.sav` exists in the app directory but the working directory differs.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `4c43ef030` | #7090 | Fix crash when loading unversioned oot_save.sav (#7090) | S | port | If the legacy file exists in the app directory but the working directory differs, or if the file is truncated, the vector is empty or short and indexing it is undefin ... |
| `acdbc651d` | #7132 | Fix compat issues loading pre-9.1 saves on 9.2+ (#7132) (partially ported) | M | port-with-adaptation | Without this, pre-9.1 SoH vanilla saves in the shared Save directory are renamed as outdated rando saves or quarantined as corrupt instead of loading. |
| `eab279c0b` | #6157 | Fix boot crash when v8 saves are present (#6157) | S | port | A one-line default, so an old SoH save without filenameLanguage loads instead of being quarantined as corrupt. |
| `44888584c` | #6687 | Fix extended file select info showing random triforce piece counts (#6687) | S | port | After a restart, or after copying a file, the extended file-select info shows uninitialized triforce-piece counts and fishing-rod state, because the startup meta path ... |
| `3ea7aa9b7` | #7304 | Fix boss rush name being garbage with non-PAL encoding (#7304) | S | port |  |
| `d46bfd971` | #6298 | Avoid auto saving in file select (#6298) | S | port |  |
| `2418d51df` | #6838 | Fix for actors / crawlspaces / doors not loading after loading a savestate (#6838) (partially ported) | S | port-with-adaptation | Savestates do not capture the transition-actor list's spawned/negated ids, so loading a state can leave doors or crawlspaces unspawned. |
| `44f680a0b` | #7008 | Fix Save Editor inventory edits leaving C/D-pad buttons desynced (#7008) | S | port-with-adaptation |  |
| `3206425e1` | #6229 | fixes #6008 (#6229) | S | port |  |
| `57eea55bd` | #7263 | Fix a song enlarging on the quest page while the cursor is on L/R (#7263) | S | port |  |

### O14. Timesaver and cutscene-skip correctness

Priority P1, effort M (33 commits: Sx32 Mx1).

- **Prerequisites:** Mandatory pairs: a1aa04681 with 6e6fee887 (Nabooru flag), 2c5762a0f with 2fd7e052c (Dampe), 1133ae221 with 3a12eeaa7 (lake swim). e2a82213c supersedes 1580d7c3d (#6842); do not port #6842.
- **RSBS adaptation:** `OoT_Flags_SetTreasure`, `OoT_gPlayState`, `func_8002DF38(OoT_gPlayState, NULL, 7)` in place of `Player_SetCsAction`. eccbc72cb touches `oot_gameover_revive_test.cpp`. b1fea3fe7: port it in its #6463 form (sub-commit 876d512a4) in the `OnItemReceive` handler. b07c64ecf restores the windmill Song of Storms cutscene behind a VB and folds twelve per-song `VB_GIVE_ITEM_*` enums into one.
- **Test impact:** redship tier (the revive test); integration rows that cross after a blue warp; no goldens.
- **Note:** e6d6b6d7d: a dungeon entered at night exits to a day that still has night actors, which changes which checks exist. Rando turns Story skips on by default, so every one of these is reachable in a paired seed.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `e6d6b6d7d` | #6350 | Fix loading wrong day/night scene after blue warp in rando (#6350) | S | port | Skipping the blue-warp cutscene sets daytime but leaves nightFlag set. |
| `f31172c64` | #6258 | remove extra zf condition + add vb call (#6258) | S | port | With dungeon or boss entrance shuffle on, our code runs the blue-warp destination override on any arrival at the Zora's Fountain Jabu blue-warp entrance index, not on ... |
| `07414fd72` | #7112 | Clear fire timer when blue warp skip bypasses chamber of sage (#7112) | S | port-with-adaptation | With blue-warp skip, the Volvagia heat timer carries over into Death Mountain Crater because the Chamber of Sages, which normally clears it, is bypassed. |
| `ca1e4c225` | #7254 | Fix Skip Forced Text softlock (#7254) | S | port-with-adaptation | Fixes a softlock when Skip Forced Dialog is turned on during a forced EnWonderTalk2 textbox. |
| `e2a82213c` | #7158 | Reduce big octo cutscene skip (#7158) | S | port | The one-point skip gives the player control while the Big Octo platform cutscene is still running. |
| `6e6fee887` | #6751 | Fix Nabooru despawning from spirit when collecting an unrelated check (#6751) | S | port-with-adaptation | In rando with Story cutscenes skipped, opening the Silver Gauntlets chest never sets the Nabooru-captured flag, so child Nabooru keeps spawning (z_en_nb.c:1107); #633 ... |
| `a1aa04681` | #6330 | ensure flag is correctly set for randomiser (#6330) | S | port-with-adaptation | In rando with Story cutscene skip on, getting the Silver Gauntlets chest never flags Nabooru as captured, so child Nabooru stays at the Spirit crawlspace. |
| `2fd7e052c` | #6999 | Cleanup and fix Dampe's race both prizes regression (#6999) | S | port-with-adaptation | Moves the rando Flags_SetTreasure(0x1E) into the DampeBothPrizes hook so the hookshot-check flag is set whichever VB handler runs first, fixing a regression in 2c5762a0f. |
| `2c5762a0f` | #6980 | [Enhancement] Dampe's Race awards both rewards at once (#6980) | S | port-with-adaptation |  |
| `3a12eeaa7` | #6887 | cleanup lake hylia swim spawn hook (#6887) | S | port-with-adaptation | This is the final form of #6873's rando fix: adult Link entering Lake Hylia from Zora's Domain with the water low spawns air-swimming, because the waterbox y-surface ... |
| `1133ae221` | #6873 | Bugfix, rando Domain to Lake entry air swim (#6873) | S | port | Adult Link entering Lake Hylia from Zora's Domain while the water is low no longer starts swimming in mid-air. |
| `852fb86cf` | #6845 | Rando, prevent carrying abducted Ruto into Bigocto room (#6845) | S | port-with-adaptation | Rando respawns Ruto after her abduction, and carrying her back through Jabu doors 21/3 into Big Octo's room breaks that room's actor logic. |
| `2da233c40` | #6185 | Improve forest temple poe cutscene hack (#6185) | S | port-with-adaptation | The Forest Temple poe-cutscene skip sets its switch flags when an ACTOR_EN_ST inits, which enemy randomizer can remove; upstream keys it on ACTOR_EN_DOOR so it fires ... |
| `de170e974` | #6703 | skip text: cancel in shops (#6703) | S | port | With Skip Text on, holding B in a shop is treated as a held cancel, so the shopkeeper exits instead of the hold advancing text as if A were mashed. |
| `e4f019f56` | #7286 | SwitchAge: prevent Link walking without collision for a bit (#7286) | S | port | Zeroing entranceSpeed stops Link from walking for a while with no collision after an age swap. |
| `eccbc72cb` | #7239 | Fix and hookify full health on spawn (#7239) | S | port-with-adaptation | Full Health on Spawn now fills to capacity whenever health is below capacity, not only below 3 hearts. |
| `02c7632a3` | #7211 | Fix boolean logic error breaking Talon cutscene skip (#7211) | S | port | The inverted check ends Talon's castle camera cutscene immediately when the skip is off, and lets it play when the skip is on. |
| `2ab81a9ba` | #7125 | Fix King Zora talkstate not updating (#7125) | S | port-with-adaptation | With Early Eyeball Frog on, King Zora's talk state was not updated while he was off-screen. |
| `4e3b8d0b1` | #6355 | Fix happy mask salesman to check flags instead of inventory for SOLD OUT (#6355) | S | port-with-adaptation | With the Mask Select enhancement, SOLD OUT gets replaced in the child trade slot, so the shopkeeper never takes the payback branch and the mask quest locks. |
| `0254c1f3a` | #7048 | Allow GI skip for silver gauntlets chest (#7048) | S | port-with-adaptation |  |
| `a5f590395` | #7162 | water temple: quadruple water level change when skipping cutscene (#7162) | S | port-with-adaptation | When one-point cutscenes are skipped (always in rando), Water Temple water changes level 4x faster. |
| `b4c41c9bf` | #7159 | Jabu fish cutscene skip: immediately transition (#7159) | S | port-with-adaptation | With Skip Misc Interactions, feeding the fish to Jabu-Jabu transitions straight into the dungeon (ER-aware) instead of pausing awkwardly, and the fish can no longer b ... |
| `446c6c480` | #6762 | SkipIntro: still start at 12:00 in non-rando game (#6762) | S | port |  |
| `401605532` | #7092 | Set ToD in non-rando LACS & MSCS skips (#7092) | S | port |  |
| `09ca2ad38` | #7184 | Maintain correct position/camera for Master Sword cutscene skip (#7184) | S | port-with-adaptation |  |
| `d4d88de82` | #6592 | Change trial barrier cutscene skip to Story (#6592) | S | port |  |
| `2fc787d64` | #6848 | Enhancement, add SoT blocks to one point cutscene skip (#6848) | S | port |  |
| `c7e4e8cce` | #6141 | Owl Travel cutscenes skipped as One Point cutscenes (#6141) | S | port-with-adaptation |  |
| `80fb8a8aa` | #6853 | Bugfix, restore A alpha on pause menu age change (#6853) | S | port-with-adaptation |  |
| `894b8e6c8` | #6192 | Glitch-Aiding CS Tweaks/Fixes/Addition (#6192) | S | port-with-adaptation |  |
| `d7b4e5a24` | #6367 | Fix heavy block in front of light trial having unending quake (#6367) | S | port-with-adaptation | With faster block lift or cutscene skip on (rando default), the screen keeps shaking after the heavy block in front of the Light Trial is destroyed. |
| `b07c64ecf` | #6310 | Fix song of storms cutscene (#6310) | M | port |  |
| `b1fea3fe7` | #6264 | Bean Souls: don't lose flag when soul found in same location as bean patch (#6264) | S | port-with-adaptation | With Skip Planting Beans, picking up the bean pack (or the soul) in a bean-patch scene leaves the live play-state switch flags unset, so the scene's in-memory flags o ... |

### O15. Cutscene-skip features: time of day, warp cutscenes, quick boss deaths

Priority P2, effort M (7 commits: Sx2 Mx5).

- **Prerequisites:** 4bd2356b1 before 0ce4c0fcc. 6a3c9b760 moves the ER warp-song override onto a new `OnWarpSongLeave` hook; 60dabb21e adds the option to the All/None buttons.
- **RSBS adaptation:** ad7d0a66d's owl-flight hunk assumes c7e4e8cce (O14). The hook is shared infrastructure: MM's shared Skips rows (#1082) may want the same toggles later.
- **Test impact:** redship tier; UI snapshot of the Skips page.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `ad7d0a66d` | #7235 | Set time of day when skipping sun song cutscene (#7235) | M | port-with-adaptation | Adds a simulator that walks the real cutscene script and sets the clock to what it would read after watching. |
| `6a3c9b760` | #7059 | Add a Skip Warp Cutscenes enhancement (#7059) | M | port-with-adaptation | Adds a Skip Warp Cutscenes toggle (default on in rando, so rando warps stay as they are). |
| `60dabb21e` | #7160 | Add Skip Warp Cutscene option to All/None cutscene skip buttons (#7160) | S | defer |  |
| `0ce4c0fcc` | #7321 | Hookify Ganondorf/Ganon skips (#7321) | M | port-with-adaptation | Replaces the hard-coded rando/boss-rush checks in the Ganondorf and Ganon fights with VB hooks under the BossIntro, Story and QuickBossDeaths skip CVars, so vanilla p ... |
| `4bd2356b1` | #7192 | QuickBossDeaths.cpp (#7192) | M | port-with-adaptation | Moves Quick Boss Deaths into one hook file and extends it from Phantom Ganon (plus a Twinrova hack) to all eight dungeon bosses. |
| `5d1a39666` | #7001 | Polish, Twinrova intro cutscene skip with Kotake/Koume appear on climb (#7001) | M | port-with-adaptation |  |
| `6606eefbf` | #6138 | Add option to disable Link spinning with the Goron Pot (#6138) | S | port-with-adaptation |  |

### O16. Enemy randomizer fixes

Priority P2, effort M (17 commits: Sx13 Mx1 Lx3).

- **Prerequisites:** Our `enemyrandomizer.cpp` predates upstream's three cleanups (b35883e1f, 4e82ea192, e93ea5b91). Every fix above 3a8c68be6 fits the older file; 3a8c68be6 needs the cleanups, so defer it or take the cleanups as a lane of their own.
- **RSBS adaptation:** 461cc0930 moves the gameplay mods off the seeded 3drando RNG and fixes Mirrored World's seeded mode; take it with 2da8cf50c's revert of its settings hunks (not a first-parent record) and keep ExtraTraps on the seeded path.
- **Test impact:** redship tier; the free-gossip-stone and trap-type purity rows of #1049 (RNG separation touches what they pin).
- **Note:** Several are room-clear softlocks in rando (3a3d93f73, 05f199e12, 917a9b78d).

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `0cf2b3701` | #7054 | Apply Enemy Rando Dark Link fixes to Crowd Control & Sail (#7054) | S | port-with-adaptation | Without this, a Dark Link spawned by Crowd Control or Sail can void out the player, deal fall damage, corrupt the ice-floor statics, or hand the player to Like-Likes. |
| `6420c435b` | #7038 | Enemy rando, fix Dark Link crush etc voidouts (#7038) | S | port-with-adaptation | Moves VB_TRIGGER_VOIDOUT up so it gates the whole crush/void block. |
| `05f199e12` | #7079 | Enemy rando, GTG hammer room air spawn fix (#7079) | S | port-with-adaptation | One enemy spawn in the Gerudo Training Ground hammer room misses the floor raycast and the replacement spawns in the air. |
| `3a3d93f73` | #7078 | Enemy rando, exclude Withered Deku Babas from clear rooms (#7078) | S | port-with-adaptation | Withered Deku Babas are stationary and can spawn in the air or on chests, and they must die to clear a room, which is a softlock risk. |
| `479aa26ef` | #7249 | Fix the seeding for Dark Link's spot in enemy rando. (#7249) | S | port-with-adaptation | Seeded enemy randomizer picks a different enemy for Dark Link's spot depending on which way Link approaches. |
| `cd38af795` | #7257 | Fix enemy rando issues in MQ spirit (#7257) | S | port-with-adaptation | Stops randomized Lizalfos in Spirit's silver-rupee bridge room from spawning in the room above, which can block a room clear. |
| `917a9b78d` | #7065 | Enemy rando, fix healthy big Guays not dying (#7065) | S | port-with-adaptation | When a health-scaled Guay survives a hit it enters the death path but keeps flying (Update only moves it while health != 0). |
| `9772a603e` | #7055 | Enemy rando, remove switch flag setting from Wolfos (#7055) | S | port-with-adaptation | Randomized Wolfos currently set room switch flag 0 when killed and vanish where it is already set. |
| `7e9cf1eb1` | #7049 | Enemy rando, set EnTp EnSkb EnPeehat collider positions on actor init (#7049) | S | port-with-adaptation | Randomized Tailpasaran, Stalchild and Peahat set collider positions only in their draw functions, so they sit at the world origin until first drawn; upstream sets the ... |
| `5f52af7ef` | #7101 | Enemy rando, make Keese not fall if not dead + fall downscale (#7101) | S | port-with-adaptation |  |
| `238560349` | #6855 | Bugfix, add Dodongo fire in enemy randomizer (#6855) | S | port-with-adaptation |  |
| `0ccff93fa` | #6456 | add gf signs to enemy rando obj dep exlusion list (#6456) | S | port-with-adaptation |  |
| `461cc0930` | #6537 | don't use 3drando/random.hpp outside 3drando (#6537) | M | port-with-adaptation | Moving the gameplay mods (mirrored world, enemy rando, audio, raw actions) off the shared seeded 3drando RNG onto local ShipUtils state stops scene loads from reseedi ... |
| `faed544b2` | #7272 | rename Gerudo Fighter to Gerudo Thief (#7272) | S | port |  |
| `3a8c68be6` | #7005 | Enemy rando, let enemies air walk over voidout floor in clear rooms (#7005) | L | defer | Adds an opt-in setting so ground enemies in clear or timed rooms float over void floors, plus a backup kill of enemies more than 1000 units (1500 in Fire Temple room ... |
| `4e82ea192` | #6365 | Enemy rando cleanup 2 (#6365) | L | port-with-adaptation | Large refactor: merges the header into ExtraModes/EnemyRandomizer.cpp, moves the menu widgets there (RegisterEnemyRandomizerWidgets) and reworks the enemy lists. |
| `b35883e1f` | #6327 | Enemy rando cleanup (#6327) | L | defer | Drops the canRandomize parameter from Actor_Spawn across about 110 actor files, moves enemy-rando spawn decisions into explicit VB hooks (Dark Link, collapse Stalfos, ... |

### O17. Frame interpolation

Priority P2, effort M (5 commits: Sx2 Mx3).

- **Prerequisites:** 35297e863 before 2884ad6de before 316c02f70 (same file context). 8a4580119's MM twin is 2S2H f11371f75 (L10).
- **RSBS adaptation:** Keep the RSBS hunks in `Graph_ProcessGfxCommands` (#1091 made the shared processor read the active game's recorder); carry each replacement's step alongside the vector.
- **Test impact:** redship tier; a visual pass at 60 fps.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `35297e863` | #7342 | Fix 3 interpolation issues (#7342) | M | port-with-adaptation | Above 20 fps this fixes freshly spawned actors (e.g. |
| `2884ad6de` | #7335 | Interpolate ribbon trail vertices (sword slashes, arrows, ...) (#7335) | M | port-with-adaptation | Above 20 fps, sword slashes, boomerang and arrow trails stop popping segment by segment; the newest trail end is blended between samples. |
| `316c02f70` | #7282 | Interpolate skinned limb vertices (horse body and legs) (#7282) | M | port-with-adaptation | Above 20 fps, Epona's body and legs (CPU-skinned SKIN_LIMB_TYPE_ANIMATED limbs) are smoothed by blending the previous and current vertex poses before each displayed f ... |
| `8a4580119` | #6901 | optimize frame interpolation memory (#6901) | S | port-with-adaptation | Builds each interpolated frame's matrix map just before that frame is drawn, instead of holding all of them per game tick. |
| `b24f707ec` | #7360 | Fix child link model blipping after ledge climb when interpolating (#7360) | S | port-with-adaptation |  |

### O18. Camera, free look, controls and the speed-modifier half-port

Priority P1, effort M (14 commits: Sx9 Mx5).

- **Prerequisites:** 2a335b1cd first: the finder says our tree is half-ported (the v6 config migration renames the old WalkModifier keys while `z_player.c` still reads them, so imported settings are lost). 04c5d50b3's `BtnSelector` widget before 2af5d2112.
- **RSBS adaptation:** `gSettings.FreeLook.*` is shared by both games here (#1089), so 100bae3b9's default-distance option applies to MM too. 497d34570 with its follow-up 4f546bc87.
- **Test impact:** redship tier; UI snapshot of the Controls and Cheats pages; `ConfigUpdaters` migration rows.
- **Note:** Verified: the v6 updater (`ConfigMigrators.h:1550-1559`, `ConfigUpdaters.cpp:148-154`) copies `gSettings.WalkModifier.{SpeedToggle,DoesntChangeJump,Mapping1,Mod1Btn}` to `gCheats.SpeedModifier.*` and clears the rest, but nothing reads `gCheats.SpeedModifier.*`: `z_player.c:7159-7171` and the input editor still read `CVAR_SETTING("WalkModifier.*")`. A pre-v6 config loses its speed-modifier settings on upgrade (the half-port arrived with #243, `4bb11dc1`). Fresh configs are unaffected.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `2a335b1cd` | #6277 | move speed modifier settings to cheats (#6277) (partially ported) | M | port-with-adaptation | Half-ported state: the v6 migration wipes or renames the old WalkModifier keys of any pre-v6 config, while the game still reads those old keys, so imported settings a ... |
| `04c5d50b3` | #6158 | Ported BtnSelector from 2ship (#6158) | M | port-with-adaptation | Adds a button-combination picker widget (UIWidgets BtnSelector, about 130 lines). |
| `2af5d2112` | #6322 | [Enhancement] Reworked targetting (#6322) | M | port-with-adaptation | Optional targeting rework: in Toggle mode, pressing Z while locked on always releases the target, and a configurable button combo switches targets. |
| `618b7694b` | #6861 | SpeedModifiers.cpp (#6861) | M | defer |  |
| `497d34570` | #6756 | fix: free look releases the door peek camera (#6756) | S | port | With Free Look on, the camera stays frozen after every door until Link moves or a button is pressed. |
| `4f546bc87` | #7270 | Fix Link being out of bounds while loading into scene on epona with spinning free cam (#7270) | S | port-with-adaptation | With free look spinning the camera, the horse may not be drawn on scene load, so Link is placed using a world position as an offset and ends up out of bounds. |
| `100bae3b9` | #6757 | feat(camera): option for free look to follow the default camera distance (#6757) | S | port-with-adaptation | An opt-in free-look setting that uses the game's per-mode default camera distance instead of a fixed one (closes upstream #4050). |
| `8cf4ff5f1` | #6808 | fix: free look continues from forced view when leaving a fixed camera (#4953) (#6808) | S | port | When free look resumes after a forced or fixed camera drove the view (Spirit Temple alcoves, crawl exits), it re-seeds camX/camY from the current view instead of snap ... |
| `ff7eb482d` | #6920 | Don't point free cam at sky coming out of crawlspace (#6920) | S | port-with-adaptation |  |
| `3e1318edc` | #6709 | uncomment code in z_camera.c (#6709) | S | port |  |
| `25ee1ec3a` | #6387 | Fix mouse camera Y-axis inversion (#6387) | S | port | With mouse look enabled, vertical mouse movement is inverted for the third-person camera and for first-person aiming. |
| `49edb25c4` | #6891 | Optional 3rd-Person Mouse Camera Controls (#6891) | S | port |  |
| `d37a37846` | #7163 | Don't apply inverted X axis aiming to first person movement (#7163) | S | port |  |
| `e09e15e7e` | #7116 | Gamepad Mapper (#7116) | M | port-with-adaptation | An in-game SDL gamepad mapper that builds gamecontrollerdb-style mappings for unrecognized or mis-mapped pads, saves them to usergamepadmappings.txt and loads that fi ... |

### O19. Item Tracker and Check Tracker

Priority P2, effort L (17 commits: Sx13 Mx2 Lx2).

- **Prerequisites:** ec2a3d7aa needs 352a4e926 (#6023), 8b7109f60 (#6578), 7be4a5941 (#7019), 0bae4961a (#7087) and d12c0d4ed (#6843); the finder marked three of those not applicable on their own, but they are this bundle's base. f5264be19 with ce8dd7931 and 12e17c68a as one change.
- **RSBS adaptation:** 7363e1c26: under Triforce Hunt the Ganon's Boss Key chest is a real check but hidden from the exclusion and visibility lists. The combo tracker overlay (`src/common/ComboTrackerWindow.cpp`, `TrackerAdapterSingleExe.cpp`) wraps SoH's tracker; port the SoH side and then mirror counts into the OoT adapter (xref families 6 and 9: #1142, #1158, #1166). f5264be19 is the mechanism behind #990 and #993; #1052 already arms recalculation on arrival, so port the region-resolution half.
- **Test impact:** redship tier (tracker reachability rows of #1098); UI snapshot harness for the tracker; no goldens.
- **Note:** Commits that belong here but are not first-parent records: eea5129a8 (#7361, TCG keys on the item tracker) and 0821c2e31 (#6142, boss souls on the item tracker). The `INV_CONTENT` bound of 7a7f942b8 is in O11.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `ec2a3d7aa` | #6750 | Fix numerous Small Key Tracking bugs, excess items now more reliably turn into Blupees (#6750) | L | port-with-adaptation | Counts small keys as current plus doors already opened, with per-dungeon door tables hard-coded in DungeonInfo. |
| `18f8770ea` | #7151 | item tracker: don't render items for disabled settings (#7151) | S | port-with-adaptation |  |
| `4d7b885db` | #7298 | Fix item tracker not working during credits (#7298) | S | port-with-adaptation |  |
| `2ba368e7e` | #6822 | [Bugfix] Skull house checks showing as 0 rupees in tracker (#6822) | S | port |  |
| `6bf5ec514` | #6742 | Fix bean guy showing in check tracker when unshuffled (#6742) | S | port |  |
| `e2e51ade1` | #6725 | Move Owl Locations to where they are in the world (#6725) | S | port |  |
| `94cebb35b` | #7041 | Fix GF Near Grotto North/East naming (#7041) | S | port |  |
| `8f51dfbaf` | #6376 | Item Tracker: don't disable Personal Notes (#6376) | S | port |  |
| `2f12677b2` | #6351 | Refactor Maps & Compasses coloring hook, remove PreFunc (#6351) | S | port |  |
| `614ec084f` | #6340 | update item list (#6340) | S | port | With chest-matches-contents, heart pieces, heart containers and Double Defense now show the heart chest and heart textures instead of the minor look, which matches Oo ... |
| `10e429d8f` | #6597 | Don't Spoil Ice Trap (#6597) | S | port | For the first frames of an ice-trap get-item, the ice block is not drawn and the fake item keeps the normal get-item matrix. |
| `fae74d4a5` | #6184 | Kaleido Tracker - Add new Shuffles (#6184) | S | port | Adds the already-ported ability shuffles (crawl, climb, grab, open chest, swim, bean souls, Roc's Feather) to the pause-menu rando tracker, plus C-Left/C-Right paging. |
| `0bae4961a` | #7087 | Draw keys on keyring with code, consolidate dungeon key counting code (#7087) | L | defer |  |
| `f5264be19` | #6165 | Recalculate Available Checks from Current Region (#6165) | M | port-with-adaptation | The check tracker's Available Checks count is computed from the region Link is actually in (resolved from the current entrance via EntranceShuffler::GetEntranceByInde ... |
| `ce8dd7931` | #6237 | Added StartingAgeTime to RecalculateAvailableChecks. (#6237) | M | port-with-adaptation | Builds on #6165: the tracker's available-checks search starts at Link's current age and time of day (child/adult x day/night) instead of the file's starting age, and ... |
| `12e17c68a` | #7300 | don't set nextEntranceIndex in Play_Init, breaks FWWW (#7300) | S | port-with-adaptation |  |
| `7363e1c26` | #6447 | Don't hide Ganon's Boss Key Chest location while Triforce Hunt enabled (#6447) | S | port |  |

### O20. Custom messages and text

Priority P2, effort S (20 commits: Sx16 Mx3 Lx1).

- **Prerequisites:** c9414b4d4 before c9491183f (the `capital[a]` fix, otherwise every inserted name is capitalised). 404ec05fc and 94f950f8d wait for the trap and language work (O27).
- **RSBS adaptation:** MerchantMessages has an RSBS foreign-host branch; apply each text fix on both branches.
- **Test impact:** OoT hint goldens pin hint text, so 52a08daf0 and f35f134c9 re-pin them; rando tier.
- **Note:** c9414b4d4 and bb6a80b48 are regressions we inherited through our port of #5101: a likely crash in randomized rupee names, a dead registrar for the Goron hints, and fortress guards that no longer ask about jail but still read the choice.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `c9414b4d4` | #6174 | Misc fixes mostly related to custom text refactor (#6174) (partially ported) | M | port-with-adaptation | These are follow-up fixes to the custom-message refactor we already ported (#5101). |
| `451820cac` | #6170 | Fix skulltula messages (#6170) (partially ported) | S | port |  |
| `c9491183f` | #6915 | Optimize CustomMessageManager.cpp (#6915) | S | port-with-adaptation | Cuts string and vector copies in the message manager and makes Capitalize and GetAllMessages actually take effect; callers switch to MF_CLEAN. |
| `bb6a80b48` | #6879 | fix regression where gerudo guards stopped asking if you want to be jailed (#6879) | S | port-with-adaptation | We inherited the #5101 regression: friendly fortress guards no longer show the 'Want me to throw you in jail?' yes/no prompt, yet the capture handler still acts on ms ... |
| `86dea7d1b` | #7305 | Fallback to English for spoiler entries with TODO_TRANSLATE (#7305) | S | port-with-adaptation |  |
| `52a08daf0` | #6507 | Fix Hint translation (#6507) | S | port-with-adaptation |  |
| `f35f134c9` | #7053 | Fix grammar (#7053) | M | port-with-adaptation |  |
| `d3dc95d8f` | #6943 | cleanup region names (#6943) | S | port-with-adaptation |  |
| `7899b6a04` | #6347 | Grass names: don't list prefix, prefix is added already (#6347) | S | port-with-adaptation |  |
| `63859efa4` | #6335 | Don't refer to grass patch as bush (#6335) | S | port |  |
| `c021a0e8a` | #6225 | Fix KD Without Bombs Trick Name (#6225) | S | port |  |
| `78c68ae3d` | #6183 | Fewer Tunic Requirements: remove lies (#6183) | S | port |  |
| `45c199b47` | #7033 | Minor Typo: Fix Roc's Feather Trap name typo (#7033) | S | port-with-adaptation |  |
| `692ef416e` | #6391 | Fix RT_GROUND_JUMP_HARD description (#6391) | S | port-with-adaptation |  |
| `581af4741` | #6978 | Fix typos (#6978) | S | port |  |
| `f5e113c5a` | #6525 | fix Roc's translation (#6525) | S | port-with-adaptation |  |
| `7767cb6d3` | #6337 | Reduce trailing newlines in tooltips/logs (#6337) (partially ported) | S | port-with-adaptation |  |
| `490e13616` | #7051 | fix warnings (#7051) (partially ported) | S | port-with-adaptation |  |
| `404ec05fc` | #7044 | Fix scrub message having doubled articles, add articles to fake names (#7044) | L | defer |  |
| `94f950f8d` | #7315 | German (#7315) | M | defer |  |

### O21. Small QoL enhancements with SoH rows

Priority P2, effort M (34 commits: Sx31 Mx3).

- **Prerequisites:** Ingo trio together (5af79defb, 35dcba594, f9512bf44; 35dcba594's hook condition is a CVar name string, always true, fixed by the trio's final form). 4587ca6db before 2b3521195 and bfaec6be7. 8bc72638c before 847e3cd32.
- **RSBS adaptation:** Every row goes through SoH's widget pattern under the UI parity directive; MM parity rows follow later through the hosting pattern (M10). 6a9567b36 is a HUD fix (small-key counters of 10 or more overlap).
- **Test impact:** redship tier; UI snapshot harness per page touched.
- **Note:** 8215b97cb (MQ Water Temple lock remap, forced in rando) and bf7177256 (flex drops lost without the container) are rando-correctness fixes dressed as options.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `a629af2e9` | #6604 | [Enhancement] Improved Roll (#6604) | S | port-with-adaptation | Adds two opt-in checkboxes: chain a new roll with A after frame 15 to keep max roll speed, and slight stick steering while rolling (disabled while Z is held). |
| `a93fec263` | #7373 | Add setting to limit minimum heat/water timer (#7373) | S | port | Adds a 0-150 second slider that floors the starting heat and underwater timer, which is otherwise based on hearts. |
| `ef014a35e` | #6663 | Enhancement to disable heat haze (#6663) | S | port | An accessibility toggle that removes the Death Mountain / Fire Temple heat-haze screen distortion. |
| `739225d22` | #6694 | Add 6th notch to text speed which makes text speed instant (#6694) | S | port | A 6th text-speed notch fills the whole text box in one frame, so hints can be read quickly without skipping them. |
| `6deff6d74` | #6326 | Enforce mask select properly on completed (#6326) | S | port | When the rando Mask Quest setting is Completed (or Shuffled), mask cycling in the inventory is forced on and the menu checkbox is greyed out. |
| `4587ca6db` | #6517 | Horseback archery settings (#6517) | S | port | Adds Horseback Archery options under the existing minigame section: instant win, every hit scores 100, arrow count 15-40. |
| `2b3521195` | #6982 | [Enhancement] Award both prizes for high score in Horseback Archery (#6982) | M | port-with-adaptation | Lets one high-score Horseback Archery run award both prizes (1000- and 1500-point), which in rando collapses two checks into one attempt. |
| `bfaec6be7` | #6985 | Fix Horseback Archery Instant Win glitch (#6985) | S | defer | Makes HBA instant win end the game through the normal path (minigameState=3, eventInf /= 0x100, warp to ENTR_GERUDOS_FORTRESS_16) instead of jumping actionFunc to Tal ... |
| `5af79defb` | #6975 | Ingo Race Once: add option to skip both races (#6975) | S | port-with-adaptation | Turns Ingo's race into a Twice/Once/None combo; None warps straight to the Epona-won state through VB_RACE_INGO, saving two races for players who do not use rando Ski ... |
| `35dcba594` | #6965 | [Enhancement] Ingo Race Once (#6965) | S | port-with-adaptation |  |
| `f9512bf44` | #6991 | Ingo Race: skip rematch (#6991) | S | port-with-adaptation | Extends the None mode of 5af79defb to the rematch path, so the eventInf bits are kept and the skip fires correctly. |
| `8215b97cb` | #6575 | Add MQ water lock fix, force it in rando (#6575) | S | port | In MQ Water Temple the second 1F small-key lock shares a flag with some Stalfos and disappears before the player reaches it. |
| `bf7177256` | #7047 | FixFlexDrops (#7047) | S | port | Item_DropCollectibleRandom's flex drop picks seeds/arrows/bombs by low ammo without checking that the player owns the container, so the drop is lost. |
| `6a3170082` | #6235 | Fixes some Stone of Agony bugs and refactors it to a VB Hook (#6235) | S | port-with-adaptation | The Visual Stone of Agony icon no longer disappears whenever any stateFlags1 bit is set (shield, Z-target). |
| `592647649` | #6778 | Add an option to Rupee Dash to disable rupee reduction scaling (#6778) | S | port |  |
| `b2f0cae9e` | #6476 | Added slider for Rupee Diving Game's time limit (#6476) | S | port |  |
| `16ee01404` | #6197 | small patch to match 2ship (#6197) | S | port |  |
| `0ed782ddc` | #7023 | Enhancement, drop throw-only actors toggle (#7023) | S | port-with-adaptation |  |
| `69f151e79` | #6136 | i control boom (#6136) | S | port-with-adaptation |  |
| `c36468ccb` | #6110 | Add bounce off walls toggle (#6110) | S | port-with-adaptation |  |
| `3250cc27f` | #6569 | [Enhancement] Restore Saria's gesture animation in her house (#6569) | S | port |  |
| `9f6ffc9f2` | #6373 | z_en_ko.c: port over more improvements from decomp, add hook to fix vanilla forest quest state bug (#6373) | S | port-with-adaptation |  |
| `172b373fb` | #7089 | Adjust fishing prize threshold when minigame weight adjusted (#7089) | S | port-with-adaptation |  |
| `a089fa60b` | #7031 | Restoration, hover fishing condition + XZ speed (#7031) | S | port-with-adaptation |  |
| `2656c20f7` | #6231 | check if rando for quest kaleido menu panel (#6231) | S | port |  |
| `07944d785` | #7016 | Allow buying tunic as either age in rando even without shopsanity. (#7016) | S | port |  |
| `b6151c131` | #6688 | Allow all 8 items in shop to be randomized in No Logic (#6688) | S | port |  |
| `fc2f52505` | #6670 | fix halfmilk RBA (#6670) | S | port |  |
| `8bc72638c` | #6568 | Add 2nd water control switch and sync floating key (#6568) | M | port-with-adaptation | Moves the rando Lake Hylia water switch into a hook file, adds a second synced switch on the fishing island, keeps the floating Water Temple lock tracking the water l ... |
| `847e3cd32` | #6884 | Maintenance, edits and addition to Lake Hylia water control (#6884) | M | defer | Builds on upstream's #6568 rework (second switch on the fishing island, floating-key sync). |
| `12d8fad5a` | #7364 | Hide shuffle indicators on excluded checks (#7364) | S | port-with-adaptation |  |
| `86fdb9bfe` | #6883 | Fix rando items in gameplay stats (#6883) | S | port-with-adaptation |  |
| `7459f6b90` | #7311 | Start with misc flags set in debug saves (#7311) | S | port-with-adaptation |  |
| `6a9567b36` | #6371 | Fix rendering double digit key count (#6371) | S | port |  |

### O22. Better Save Menu

Priority P3, effort L (5 commits: Sx3 Lx2).

- **Prerequisites:** Port the six-commit family together or not at all (b69cb6607 completes it and is marked not applicable alone). As committed, 8cb0c7459 registers its hooks unconditionally; c71203ad0 fixes that.
- **RSBS adaptation:** Return to Spawn interacts with the crossing's spawn pins (#1054) and the paired `.redsave` commit model; design before porting.
- **Test impact:** redship tier; integration crossing rows.
- **Note:** An escape from rare entrance-rando softlocks.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `8cb0c7459` | #6937 | Better Save Menu: Ability to Reset to Spawn from anywhere (#6937) | L | port-with-adaptation | An opt-in textbox save menu that, after saving, offers Continue, Restart (dungeon entrance) or Return to Spawn (rando overworld-spawn aware). |
| `cf7e8022b` | #6963 | Fix some issues found with the BetterSaveMenu (#6963) | S | port-with-adaptation | Follow-up to 8cb0c7459. |
| `c71203ad0` | #6970 | Respect the Better Save Menu enable toggle (#6970) | S | defer |  |
| `90c6b34e8` | #6984 | BetterSaveMenu: Less redundant override and run exit and load hooks (#6984) | L | defer | This follow-up to the Better Save Menu ('Reset to Spawn from anywhere') feature, which our tree does not have. |
| `576b30c64` | #7273 | Better Save Menu: hard code Return to Spawn in vanilla (#7273) | S | defer |  |

### O23. Hint Tracker window and hinted-check marking

Priority P2, effort L (3 commits: Mx1 Lx2).

- **Prerequisites:** 1445da337 first (it adds the `OnRandoHintRevealed` hook); 783139310 can take the hook slice alone.
- **RSBS adaptation:** A window is allowed here under the page-vs-window rule (it is an on-the-fly gameplay tool). Read state per save goes through the combo record, not SoH's save.
- **Test impact:** redship tier; UI snapshot harness.
- **Note:** Relevant to the Trackers scoping (#1131, #1161).

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `1445da337` | #6906 | Add Hint Tracker window for randomizer saves (#6906) | L | port-with-adaptation | Adds a spoiler-free dockable Hint Tracker window with a Locations tab and a read-hints Journal. |
| `783139310` | #6946 | Rando: Mark checks as seen in the Check Tracker from explicit item hints (#6946) | M | port-with-adaptation | With Clear hint wording, a gossip or static hint that names a unique item marks its check as Seen in the Check Tracker. |
| `81009e9dc` | #6934 | Rando: Hint tracker journal improvements (#6934) | L | defer | This is a follow-up to the Hint Tracker window: found-item dimming, searchable settings, category ranking, and a fix so ranking uses checkObtainability=false. |

### O24. Custom item icons, OOTRS music packs, file-select quests

Priority P3, effort L (4 commits: Sx1 Mx1 Lx2).

- **Prerequisites:** 5c8ff7655 needs the sized `gItemAgeReqs` (#985, present). 2dd51f128 with 01a4f3693.
- **RSBS adaptation:** 5c8ff7655's `ITEM_CUSTOM` path is where MM's foreign items could draw their icons in OoT textboxes (the #607 class).
- **Test impact:** redship tier; UI snapshot of File Select.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `5c8ff7655` | #6343 | Custom Item Icons in Textboxes (#6343) | L | port-with-adaptation | Lets the OoT get-item textbox draw any texture path (24 or 32 px) for rando items without an inventory icon (boss souls, Triforce piece, keys, bean souls, Greg and ot ... |
| `253e67247` | #7056 | Adds native OOTRS archive support to remove need for custom packing. (#7056) | L | port-with-adaptation | Mounts OoT Randomizer .ootrs music packs dropped in mods/ as virtual custom/music/<pack>/<name>_bgm/_fanfare sequences, with no repacking. |
| `2dd51f128` | #7093 | [Enhancement] Hide quests in File Select (#7093) | M | port-with-adaptation |  |
| `01a4f3693` | #7210 | Refactor file select quest visibility, fix single quest handling (#7210) | S | defer |  |

### O25. Accessibility

Priority P2, effort S (2 commits: Sx2).

- **Prerequisites:** None. The operator kept espeak-ng on Linux.
- **RSBS adaptation:** ea348bc9f: the same F9 path for MM needs an `ESPEAK` define in `games/mm/CMakeLists.txt`.
- **Test impact:** Linux build; manual.
- **Note:** ef014a35e (heat haze toggle) is in O21.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `c9e3ef765` | #7207 | Interrupt espeak on a new utterance (#7207) | S | port | On Linux, every menu item scrolled past queued another TTS utterance with no way to skip ahead. |
| `ea348bc9f` | #7191 | a11y: let F9 toggle TTS on Linux (#7191) | S | port | It lets the F9 hotkey toggle text-to-speech on Linux, where the espeak-ng backend exists and the operator chose to keep it for accessibility. |

### O26. Build, extraction and generation performance

Priority P0, effort S (28 commits: Sx22 Mx6).

- **Prerequisites:** None.
- **RSBS adaptation:** f00f5b625: one line in `games/oot/CMakeLists.txt`'s x64 Release block (see the verification table for what the OoT objects receive today). 87db67588: adapt the `std::array` tables to our layout; the global `ctx`/`logic` externs exist. b939163c3: port the `GetRoms` full-path fix only; the AppImage data-home half needs a decision because our app short name is `soh`. c642e8c4d: cherry-pick only the `guMtx` cast hunks; `/WX` is against the suppress-warnings policy.
- **Test impact:** Full local build; both ctest tiers; the creation-budget rows of #959 for 87db67588.
- **Note:** Verified 2026-10-26 against `build-cmake/build.ninja`: the Release command line for `soh_src` and `soh_port` objects is `-std:clatest -MT /MP /utf-8 /Oi /Gy /W3 /bigobj /sdl- /permissive- /EHsc /w` with no `/O2`, because `games/oot/CMakeLists.txt:48-56` clears `CMAKE_C/CXX_FLAGS{,_RELEASE}` and the x64 Release block (`:592-596`) adds only `/Oi;/Gy;/W3`; MM and libultraship keep the root's `-O2`. `NDEBUG` still reaches OoT through compile definitions. The whole OoT half of the shipped Windows binary runs at MSVC's default `/Od`. 87db67588 cuts OoT solver time inside the 30 s creation budget (the multiplicity verdict of 2026-09-22); `logic.cpp:1795,1806,1877,1925` still use `std::map` with `.at()`.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `f00f5b625` | #7188 | Optimise the 64-bit Windows Release build (#7188) | S | port | The OoT half of the shipped Windows Release redship binary (soh_src, soh_port, soh_enh, soh_rando) is compiled unoptimised (cl defaults to /Od), while MM and LUS are -O2. |
| `87db67588` | #7369 | optimize rando generation (#7369) | M | port-with-adaptation | Reachability-search speedups: an early skip for already-added locations, array lookups replacing std::map in HasItem/ApplyItemEffect, a bitset target filter, GetAreas ... |
| `14d9d4e8b` | #7060 | Pick up newly added source files without a manual re-configure (#7060) | S | port-with-adaptation |  |
| `1c856a7be` | #7050 | Silence cmake warnings (#7050) | S | port-with-adaptation |  |
| `f6f21477b` | #7039 | fix clang with -lm (#7039) | S | port-with-adaptation |  |
| `f5244973b` | #7024 | Fix clang constexpr issues with system fmt (#7024) | S | port-with-adaptation |  |
| `27e6fe015` | #6981 | Add ASAN cmake option (#6981) | M | port-with-adaptation |  |
| `04d0ebd38` | #7262 | Build soh.o2r as part of ALL, only when its inputs change (#7262) | M | port-with-adaptation | Local `install` and cpack can ship a stale soh.o2r or 2ship.o2r that carries the current version string, so the version check cannot catch it. |
| `f95c23ef6` | #6811 | No FMA (#6811) | S | port-with-adaptation | This enforces strict IEEE-754 floating point with no FMA contraction on every platform, so arm64 builds (macOS Apple Silicon, Linux aarch64) compute physics and colli ... |
| `9782faba4` | #7063 | Torch CMake and Extraction Flow cleanup (#7063) (partially ported) | S | port-with-adaptation | On a regen, stale oot.o2r/oot-mq.o2r archives are removed only from cwd. |
| `e447143e4` | #6243 | Fix the extractor's assumption that found files are always in the working directory (x64/build/soh). (#6243) | S | port-with-adaptation | With a separate data directory, a ROM found there is listed by bare name and then opened relative to the cwd, so extraction from the data directory fails. |
| `b939163c3` | #7252 | publish appimage unzipped, store to ~/.local/share/soh when SHIP_HOME unset (#7252) | M | port-with-adaptation | The GetRoms full-path fix is a real Linux ROM auto-detect bug and ports verbatim (S). |
| `e894b7618` | #6797 | Ignore clangd config (#6797) | S | port |  |
| `29948f27a` | #6918 | take nlohmann::json by const reference instead of copying (#6918) | S | port |  |
| `dd2e952cf` | #6685 | Don't use shared pointers for the CS helpers (#6685) | S | port |  |
| `bd7191875` | #6589 | Add color to the log level in the console log (#6589) | S | port |  |
| `2S2H 31690aed6` | #1476 | Color log level (#1476) | S | port-with-adaptation |  |
| `2S2H 7dd94464c` | #1643 | Improve Nix support (#1643) (partially ported) | S | port |  |
| `2S2H 14cdcf679` | #1932 | Ensure branch names show up for PR builds (#1932) | S | port-with-adaptation |  |
| `2S2H d89ecccee` | #1813 | Cut build times: PCH tuning and a hook instantiation fix (#1813) | M | port-with-adaptation |  |
| `b6df96c53` | #6304 | Add dependencies to docs/BUILDING.md (#6304) | S | port |  |
| `80de5cc17` | #6531 | example flake - add missing deps, pin clang14 (#6531) | S | port-with-adaptation |  |
| `1894ffca4` | #6740 | docs: fix missing sdl2-net (#6740) | S | port-with-adaptation |  |
| `c71cc6895` | #6161 | Update references to soh.otr/oot.otr/oot-mq.otr to .o2r (#6161) | S | port-with-adaptation |  |
| `2S2H 6f3bff5c2` | #1719 | add audio dependencies to build instructions (#1719) | S | port-with-adaptation |  |
| `e1462b07e` | #6748 | fix and improve test builds on distros workflow (#6748) | M | port-with-adaptation |  |
| `00ac1d566` | #6307 | spdlog: don't pass printf format string (#6307) | S | port |  |
| `c642e8c4d` | #6733 | Remove all warnings on Windows and treat them as errors (#6733) | S | port-with-adaptation |  |

### O27. Rando infrastructure prerequisites

Priority P2, effort L (23 commits: Sx12 Mx6 Lx4 XLx1).

- **Prerequisites:** Port fe5dce24b and a393f48c7 (O4, O5) before 2b650a497, or into the generated headers after. 4cae79fd8 saves tricks as name tags, so our trick vocabulary (#578 line) and presets must carry the legacy map.
- **RSBS adaptation:** 0127cbcf6 fixes an assert that only exists once 4cae79fd8 and sub-commit 94b650ec6 (#6414) are both in. 2b650a497 moves every OoT rando enum into X-macro headers; our combo coordinator and tests include `randomizerTypes.h` directly, so the include graph in `src/common/` changes with it. 316d8e6a1 builds its legacy-number table from our current enum order, not upstream's.
- **Test impact:** Both ctest tiers; the goldens must not move (pure refactor) and the determinism rows prove it only as self-diffs, so compare a digest before and after by hand.
- **Note:** Dev infrastructure that every later upstream rando commit edits. Without it, each new-check shuffle (O29) must be translated by hand.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `2b650a497` | #6223 | Stringify Randomizer Enums (#6223) | L | port-with-adaptation | Moves every OoT rando enum into X-macro headers and generates enum<->string maps. |
| `077fda874` | #6031 | Stringify More Conditions (#6031) | M | defer |  |
| `4cae79fd8` | #6267 | Change numbered tricks in preset and settings to use codes. (#6267) | L | port-with-adaptation | Saves enabled tricks as stable 3-8 character name tags instead of enum indices, with a legacy numeric map for old configs and presets. |
| `316d8e6a1` | #7302 | Excluded Locations Migration (#7302) | L | defer | Stores excluded checks by RC_ name instead of enum index, so a user's exclusions survive any later RC insertion (the new-check shuffles in this chunk insert RCs mid-e ... |
| `8dd03ef09` | #6302 | Trap Improvements (#6302) | M | port-with-adaptation | Moves the trap code (trick-name table, trick-model reroll, junk-to-trap roll, ice trap messages) into Rando::Traps and adds English ice trap messages that show the di ... |
| `90823fad5` | #6673 | Identify clean up (#6673) | M | defer |  |
| `6d65db617` | #7141 | Fix missing entries  in RC to randinf and new identity pattern to make future misses more obvious (#7141) | S | port-with-adaptation |  |
| `94a5311cb` | #6541 | port text.hpp to custom-message as text.cpp/text.h (#6541) | S | port |  |
| `aa5379a8e` | #6538 | remove 3drando/custom_messages (#6538) | S | port |  |
| `6b160fac1` | #6186 | Entrance/Location: remove Getuint32_t method (#6186) | S | port |  |
| `f474a8692` | #6152 | Cleanup various things noticed while fixing rando generation / debug warp (#6152) | M | defer |  |
| `a92f07e1f` | #6143 | Fix quest leak assert triggering in no logic (#6143) | S | port-with-adaptation |  |
| `eaad45879` | #6690 | remove unused code from option.cpp (#6690) | S | port |  |
| `7c5cccaf9` | #6443 | Dedupe settings.cpp (#6443) | S | port |  |
| `8d2d5c982` | #6825 | Be consistent with check names for deku scrub grotto beehives (#6825) | S | port |  |
| `40ad4fed8` | #6317 | Merge Visible Collision and Hammer Through Collision (#6317) | S | port-with-adaptation |  |
| `5a8c6d036` | #7025 | Make Thieves' Hideout distinct region (#7025) | M | port-with-adaptation |  |
| `f19ad1e75` | #6309 | timesaver_hook_handlers: use COND_HOOK (#6309) | S | port-with-adaptation |  |
| `0127cbcf6` | #6439 | Fix debug assert caused by bad trick code (#6439) | S | defer |  |
| `495a2b17d` | #7148 | revise randomizerEnumStrings, include fmt compatibility (#7148) | L | defer |  |
| `8ca5ca60a` | #7130 | Lang Improvements (#7130) | XL | defer |  |
| `e0a1b2352` | #6172 | Language System Basis (#6172) (partially ported) | M | defer |  |
| `a236e9ec8` | #6947 | randomizer/rng.h (#6947) | S | defer |  |

### O28. Region refactors, tricks and the 9.2.x merge bundles

Priority P2, effort L (22 commits: Sx6 Mx10 Lx5 XLx1).

- **Prerequisites:** O27 (or hand translation of the `ENTRANCE`/`EVENT_ACCESS` macros). 53f688df5 with 31ba9e206 (mandatory). 139381a0e with sub-commit 88099e6eb (#6487), or MQ Fire Trial generation fails.
- **RSBS adaptation:** 73a739b7a tightens tricks that eca767dcb (decided against) introduced; only its two standalone hunks apply. 92ff8407f only fixes a break that 2d529b315's rename causes. 55b52a26a changes bean-planting semantics to per-patch events. 27d35e5a9 over-reaches once (`shadow_temple.cpp:344` opens the maze and boss-key area without the boat); review each link. The five merge commits are 9.1.2 and 9.2.x release bundles to split: the finder's picks are 972ed2216 (#6442, ice-trap models leak between generations), efc4086c6 (#6445), 8513fd880 (#6481, cosmetic and audio options wrongly seeded), 40425c8c2 (#6469, seeded warp traps), ca38dba19 (#6477), 0dcd52e5c (#6551) and ea76550fc (#6564).
- **Test impact:** OoT placement, hint and entrance goldens all re-pin; rando tier; name the move in the PR.
- **Note:** 60ec65592 also splits out GF Tower and LW Under-Bridge regions, which the finder says fixes our broken Gerudo Fortress wasteland-gate logic.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `7522f31cc` | #6294 | Refactor DMC logic (#6294) | L | port-with-adaptation | Rewrites Death Mountain Crater logic into finer regions with per-entrance heart thresholds. |
| `60ec65592` | #6329 | Add trick for Hookshotting ladders (#6329) | M | port-with-adaptation | Adds a 'Hookshot Ladders' trick: ladders are climbable with Hookshot/Longshot when Shuffle Climb is on. |
| `a40ca58ab` | #6370 | Misc logic fixes (#6370) | L | port-with-adaptation | Tightens several too-permissive logic paths: ZR front exit, Ice Cavern child ground-jump, DC MQ Dodongo room needing bracelet or ladder, GF outskirts. |
| `53f688df5` | #7147 | Fix logic for fire near boss (#7147) | M | port-with-adaptation | This splits the vanilla and MQ Fire Temple near-boss room into pillar, door and target sub-regions and tightens several exits: flame maze to near-boss now needs TakeD ... |
| `31ba9e206` | #7195 | fix fire boss typo (#7195) | S | port-with-adaptation | This is a mandatory follow-up to 53f688df5: without it the MQ code clobbers the vanilla pillar region. |
| `139381a0e` | #6336 | Add Fire Trial to Fewer Tunic Requirements (#6336) | M | port-with-adaptation | Applies Fewer Tunic Requirements heart-timer logic to Ganon's Castle Fire Trial, vanilla and MQ, splitting it into door-side regions. |
| `27d35e5a9` | #6510 | Fix logic issues noticed while rebasing doorsanity (#6510) | S | port-with-adaptation | Corrects region links that point at the wrong sub-region and fixes several region names. |
| `91425a39a` | #6318 | Add Death Mountain Trail Child Climb Without Shield (#6318) | M | port-with-adaptation | Default logic now requires child to have the Hylian Shield or Nayru's Love to climb past the DMT eruption to the summit; a new novice trick restores the old behavior. |
| `322884388` | #6431 | Merge Boulder Undershoots into Boulder Collision (#6431) | M | port-with-adaptation | Renames the trick to Flawed Boulder Collision and widens it to shots from below. |
| `c76d17552` | #6840 | DC MQ: RT_VISIBLE_COLLISION to reach above stairs with hookshot (#6840) | S | port-with-adaptation |  |
| `47d138302` | #7109 | MQ DC: standing in corner slingshot can hit through eye through boulder (#7109) | S | defer |  |
| `e86e0ff69` | #6453 | Add Voidout Collection trick (#6453) | M | port-with-adaptation | Adds a Novice trick for collecting items by jumping and voiding out. |
| `b52c31f85` | #7026 | Get rid of Logic::Hearts, update Logic::EffectiveHealth to be 16/heart (#7026) | M | port-with-adaptation | Moves logic health to OoT units (16 per heart) and fixes the rounding of the damage multiplier and double defense. |
| `2d529b315` | #7293 | Damage boost trick implementation Part 1 (#7293) | XL | defer |  |
| `92ff8407f` | #7375 | Fix AssumeEntrancePool breaking due to renamed region (#7375) | S | port |  |
| `55b52a26a` | #5842 | replace specialized bean logic with events (#5842) | M | defer |  |
| `73a739b7a` | #7145 | Fix various trick issues (#7145) | S | port-with-adaptation |  |
| `178471bf2` | #6425 | Merge pull request #6425 from HarbourMasters/develop-ackbar (partially ported) | L | port-with-adaptation | The merge bundles real fixes, including several rando logic bugs that apply verbatim to our older-syntax region tables: BotW lens/Open Chest precedence, Volvagia tuni ... |
| `2b336a458` | #6446 | Merge pull request #6446 from HarbourMasters/develop-ackbar | M | port-with-adaptation | Bundle of 9.2.x hotfixes worth splitting. |
| `35a2ce4bf` | #6715 | Merge pull request #6715 from serprex/ackbar (partially ported) | L | port-with-adaptation | The rest of the 9.2.x stable fixes. |
| `a2245f7a4` | #6164 | Merge pull request #6164 from serprex/merge912 (partially ported) | M | port-with-adaptation | This first-parent merge brings the 9.1.2 release fixes into develop. |
| `4cae72463` | #6497 | Merge pull request #6497 from serprex/develop922 (partially ported) | L | port-with-adaptation | The highest-value pieces are ca38dba19, which restores the Hint Clarity option for item, merchant and skulltula-reward hints (a regression we inherited through our po ... |

### O29. New OoT shuffles (OoTMM parity)

Priority P3, effort XL (29 commits: Sx15 Mx4 Lx4 XLx6).

- **Prerequisites:** O27 first. Every new world-generation option needs the combo option-identity handling of ADR 0009/0010/0011 (the trap-type rows of #1107 are the precedent for an option the world freezes) and a classification in the single-bag coordinator. The finder names Shuffle Silver (b76e303ce) and Treasure Chest Game (6706b49d1) as features OoTMM ships.
- **RSBS adaptation:** New `RC_` entries change OoT's check table and every golden. Signs and Wonder Items split regions and edit the entrance table. 1ea720607 removes Skip Child Zelda and the Kakariko Gate option.
- **Test impact:** Every OoT golden re-pins; rando tier; the seed-package fingerprint (#1116) changes.
- **Note:** Order by dependency: Signs, then Wonder Items, then Icicles and Red Ice (which also needs 5576f93ef), then rocksanity; Silver, Treasure Chest Game, Butterfly Fairies, Beggar and the three single-check shuffles are independent of that chain.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `317c057e8` | #6406 | [Rando] Shuffle Signs (#6406) | L | defer | New Shuffle Signs option (off/dungeons/overworld/all), adding about 78 sign, statue and pedestal checks with sparkle CSMC particles. |
| `db0d179b3` | #6454 | add missing KF and Shadow signs, eat baguette (#6454) | S | defer |  |
| `4e1e180d2` | #6458 | add spirit signs + unique init func (#6458) | S | defer |  |
| `a6ceda989` | #6571 | Fix KF to LW sign region (#6571) | S | defer |  |
| `1ae55a0a7` | #6876 | Add water switch signs to shuffle signs (#6876) | S | defer |  |
| `08cb0dcf7` | #7057 | Fix graveyard sign checks at night (#7057) | S | defer |  |
| `412e9e262` | #6342 | [Rando] Shuffle Wonder Items (#6342) | XL | defer | New Shuffle Wonder Items option: about 200 invisible collectible-spot checks with particle CSMC, plus hooks in EnWonderItem and Heishi1/2. |
| `1f1698721` | #6675 | extend NTSC10 spawn to NTSC11 (#6675) | S | defer |  |
| `67191665d` | #6467 | Add trick for jump to GTG eye statue (#6467) | S | defer |  |
| `49e740b6f` | #6462 | [Rando] Shuffle Icicles and Red Ice (#6462) | XL | defer | Adds two optional shuffles: stalagmites and stalactites drop an item, and melted red ice gives one. |
| `68d4c6049` | #6836 | Add to queue if kz red ice removed (#6836) | S | defer |  |
| `eeca7626d` | #5015 | rocksanity (#5015) | XL | defer | Adds Shuffle Rocks and Shuffle Boulders (off/dungeons/overworld/all), a new trick, Logic::CanBreakRocks, and actor hooks in En_Ishi, Obj_Bombiwa and Obj_Hamishi, acro ... |
| `fa44542e6` | #6749 | Remove duplicate prefixes for rocks and trees (#6749) | S | port-with-adaptation |  |
| `b76e303ce` | #5659 | Shuffle Silver (#5659) | XL | defer | Full silver rupee shuffle (Off/On/Wallet/Start With) across 11 vanilla and 6 MQ rooms, with about 157 new checks, logic rewrites in 8 dungeons, a z_en_g_switch rework ... |
| `17a7fde00` | #7084 | Shuffle Silver: fix gtg room setting room cleared instead of flag (#7084) | S | defer |  |
| `6706b49d1` | #7020 | Shuffle Treasure Chest Game (#7020) | L | defer | Turns on the long-stubbed Treasure Chest Game shuffle: both chests per room, the shopkeeper check, and six Chest Game small keys or a keyring. |
| `4868b0712` | #7118 | Prevent double give by treasure chest game shopkeeper (#7118) | S | defer |  |
| `a5a182618` | #7133 | Fix check tracker chest game key logic (#7133) | S | defer |  |
| `a461d8f6f` | #6430 | [Rando] Shuffle Butterfly Fairies (#6430) | M | port-with-adaptation | New Shuffle Butterfly Fairies option: 10 overworld checks (HC, LW, GY, ZR, ZF, LH) granted when a butterfly turns into a fairy, through a new VB in EnButte. |
| `772fe2bb9` | #6619 | add grotto butterflies (#6619) | M | defer | Adds 9 grotto butterfly-fairy checks to Shuffle Butterfly Fairies. |
| `1fd3a467c` | #6839 | Add Kakariko watchtower butterfly fairy (#6839) | S | defer |  |
| `55b804a67` | #6833 | Add Easy Butterfly Fairy Cvar (#6833) | S | port-with-adaptation |  |
| `50aed798a` | #6455 | [Rando] Shuffle Beggar (#6455) | M | port-with-adaptation | New Shuffle Beggar option: 6 checks (Market child-day beggar and Kakariko adult beggar, one each for bugs, fish and blue fire) with custom trade text, replacing the r ... |
| `994504188` | #6472 | replace ShuffleBeggar accents with utf-8 (#6472) | S | defer |  |
| `a353b6e7c` | #7205 | Shuffle Scarecrow Song (#7205) | L | defer | This adds a new shuffled OoT item/check (Scarecrow's Song from the Lake Hylia scarecrow) and replaces Skip Scarecrow's Song with a starting-item option, across 42 fil ... |
| `dd702b067` | #7388 | [Rando] Shuffle Fisherman's Hat (#7388) | M | port-with-adaptation |  |
| `96be197fd` | #7377 | Fire Temple Goron Shuffle (#7377) | L | defer |  |
| `1ea720607` | #6938 | Shuffle Zelda's Letter (#6938) | XL | defer | Adds Shuffle Zelda's Letter and a 3-way Weird Egg option, and removes Skip Child Zelda and Kakariko Gate (the gate now opens via starting with the letter), with new T ... |
| `3175ea9c2` | #7104 | Remove completed mask quest option, add all masks to starting items menu (#7104) | XL | defer |  |

### O30. Rando options that need coordinator decisions

Priority P3, effort XL (15 commits: Sx2 Mx7 Lx4 XLx2).

- **Prerequisites:** Each needs a ruling: starting items against the single-bag pool trimming and multiplicity contract (728a4f1c8), dungeon-reward modes against the coordinator (12dddc5e8), triforce piece placement across both worlds (88a5b3ff4), shop gating as a world-gen option (864908586), and the GBK/LACS decoupling against the combo win condition (015440fdf, 55 files).
- **RSBS adaptation:** 92a1d260d is self-contained in `fill.cpp` and runs before the combo deferral, so it is the cheapest to take. a6be06874 only with 257cf79b0. d34f664a2 only with 728a4f1c8.
- **Test impact:** OoT goldens re-pin; rando tier; the goal options of wave 7b.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `728a4f1c8` | #6723 | Complete rando starting items, make icon based like save editor (#6723) | XL | defer | It adds about 34 starting-item settings, removes started copies from the OoT pool, applies them to the save, and adds an icon-based Starting Items menu page. |
| `29668a06d` | #7037 | Progressive BGS (#7037) | M | port-with-adaptation | Adds a Progressive Goron Sword option: the first copy is the Giant's Knife, the second is Biggoron's Sword, and Medigoron only repairs a broken knife. |
| `92a1d260d` | #6213 | Choose Link's Pocket Dungeon Reward Type (#6213) | M | port-with-adaptation | New player option to force Link's Pocket to hold a Spiritual Stone or a Medallion, under both End-of-Dungeon and Link's Pocket=Dungeon Reward modes. |
| `12dddc5e8` | #6500 | Dungeon Rewards Own Dungeon + Light Medallion Handling Refactor (#6500) | L | defer | Adds an 'Own Dungeon' dungeon-reward shuffle and Light Medallion / Link's Pocket reward-type choices, and rewrites RandomizeDungeonRewards. |
| `864908586` | #6700 | add option gating shields / tunics in shop behind finding one first (#6700) | L | defer | New option: non-randomized shop shields and tunics stay locked until a world copy is found. |
| `88a5b3ff4` | #6710 | Add an option to limit the location of triforce pieces (#6710) | L | defer | Option to restrict triforce pieces to dungeons or the overworld. |
| `015440fdf` | #5838 | Decouple GBK from LACS (#5838) | XL | defer | This is a large world-generation rework across 55 files. |
| `c7180762d` | #6001 | Add button to rando all rando settings (#6001) | M | defer |  |
| `7b01a7bf2` | #6693 | Early Granny's Potion Shop (#6693) | M | defer |  |
| `97f4fd592` | #6588 | Ice Trap Name Setting (#6588) | M | defer |  |
| `fa5172a58` | #6986 | Add advanced option for teleport trap (#6986) | M | defer |  |
| `da4e6dc33` | #6971 | Progressive Shuffle Open Chest (#6971) | M | port-with-adaptation |  |
| `a6be06874` | #7365 | Add consumable Nayru's Love (#7365) | L | port-with-adaptation |  |
| `257cf79b0` | #7386 | fix invalid return in nayru's love uses (#7386) (filed as ported on 2026-10-18) | S | skip | A required follow-up to consumable Nayru's Love: without it the handler returns early instead of setting *should. |
| `d34f664a2` | #6844 | disable starting wallet when child wallet is shuffled (#6844) | S | defer |  |

### O31. Models, assets and get-item draws

Priority P3, effort M (17 commits: Sx14 Mx3).

- **Prerequisites:** a75c8113d, 86a55cb1f and 10cde0b8a as one change; af6d46514 with 88006aee2 and b7b565aaf; eb272ef68 with 0744396e0 and 948b84d8f.
- **RSBS adaptation:** Every new get-item model needs a `ForeignModelOoT` recipe so MM shelves can show it (the #607 class; `src/common/tests/test_foreign_model.c`). 1ee0f5a65 realigns the `GID` enum with the draw table and touches that test.
- **Test impact:** redship tier (`test_foreign_model`); a visual pass of shops in both games.
- **Note:** 1ee0f5a65 is P2 on its own: an out-of-bounds draw-table index from an Anchor peer, and an enum realignment that later upstream asset commits assume.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `1ee0f5a65` | #6678 | Fix anchor get item crash (#6678) | S | port-with-adaptation | Fixes an out-of-bounds draw-table index (a crash) when an Anchor peer sends a get-item draw id beyond GID_MAXIMUM. |
| `a75c8113d` | #7015 | Progressive Deku Nut Bag and Stick Bag models (#7015) | M | port-with-adaptation | Adds distinct get-item models for the Deku Stick and Nut Bag rando items, which currently borrow the plain stick/nut model, using about 46 XML assets plus two draw fu ... |
| `86a55cb1f` | #7156 | render stick/nut bag for upgrades past first (#7156) | S | port-with-adaptation | Without it the new bag models show only on the first bag. |
| `10cde0b8a` | #6886 | gGiScaleMtx (#6886) | S | port-with-adaptation |  |
| `eb272ef68` | #6783 | Update Visuals for Ability Shuffles (#6783) | M | port-with-adaptation | Gives the ability-shuffle items we already ship (Climb, Crawl, Open Chests, Grab) their own get-item models and item-tracker icons instead of stand-ins, and removes t ... |
| `0744396e0` | #6759 | Redo Fishing Pole Gi Model (#6759) | S | port-with-adaptation |  |
| `948b84d8f` | #6758 | asset purge (#6758) | S | port |  |
| `af6d46514` | #6777 | soh.o2r optimization part 1 (#6777) | M | port-with-adaptation |  |
| `88006aee2` | #6791 | triforce shard scale fix (#6791) | S | defer |  |
| `b7b565aaf` | #6866 | remove set env color from key dl (#6866) | S | defer |  |
| `e50d03dab` | #6766 | Updates the CMC models so fade in should work now (#6766) | S | port |  |
| `0d2454ed6` |  | title card ia8 fix | S | port |  |
| `d901223ad` | #6695 | Crop input viewer images (#6695) | S | port-with-adaptation |  |
| `11fd0e0e7` | #7190 | Fix bow body cosmetic color (#7190) | S | port-with-adaptation | The bow body colour the player picks is multiplied by 4 and saturates, and cached currentColor values can go stale against the CVar. |
| `6e4c010ff` | #6113 | [Custom equipment] fix fallback (#6113) | S | port-with-adaptation | It reads the sword from the equip value instead of the B button, makes custom-resource lookups resolve mod-registered DLs, and fixes BGS/Giant's Knife hand DLs for ch ... |
| `8b3d91506` | #7167 | Fix title card margins (#7167) | S | port |  |
| `6f14e7b16` | #6953 | Restoration, fix Japanese subtitle + MQ logo (#6953) | S | port |  |

### O32. Custom equipment and model modding

Priority P3, effort L (11 commits: Sx4 Mx5 Lx2).

- **Prerequisites:** A bundle: 0e5083931 is a thousand-line rewrite that sits on three earlier commits we lack (8c4d4738c, bf37645d7, 30dcd7946).
- **RSBS adaptation:** `ResourceManagerHelpers.cpp` is shared with MM's custom-model path (029ac2de9 in M1).
- **Test impact:** redship tier; a mod pack session.
- **Note:** Mod support only; nothing here changes play without a pack.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `0e5083931` | #6708 | [Modding] Refactor CustomEquipment (#6708) | L | defer | This is a ~1000-line rewrite of custom-equipment mod support (hand/sheath/shield DLs, custom hookshot chain and tip, limb-draw hooks), and it sits on three earlier up ... |
| `30dcd7946` | #6543 | add fps custom equips (#6543) | S | port |  |
| `3c48ccd70` | #6862 | 1 Cache + Fix logical errors (#6862) | M | defer |  |
| `8e810fb96` | #6928 | cleanup redundant code + fix waist dl (#6928) | S | defer |  |
| `6a5172019` | #6754 | Custom Hand DL's for each tunic take 3 (#6754) | M | defer |  |
| `cb215ede8` | #6780 | Custom Bottle Contents (#6780) | M | defer |  |
| `228f9fece` | #6642 | [Modding] Add Adult Mask DL for mod compability (#6642) | S | defer |  |
| `262958a2e` | #6433 | Alt Toggle for Custom animation (#6433) | S | port-with-adaptation |  |
| `c91f7b6ce` | #6792 | dynamic cosmetic manifests (#6792) | M | defer |  |
| `4ea17f693` | #6429 | Dynamic Cosmetics Editor for Custom Models (#6429) | L | defer |  |
| `2S2H d0982dc21` | #1624 | Alt Toggle for Custom animation (#1624) | M | port-with-adaptation |  |

### O33. Netplay: Anchor, Crowd Control and Sail

Priority P3, effort M (10 commits: Sx8 Mx2).

- **Prerequisites:** d7981bf03 and ef042be5e together (the try/catch makes the `.at()` throws safe). A netplay transport decision for 5f0c0c8e2.
- **RSBS adaptation:** `OoT_` prefixes in the packet handlers. 8a0315a2d's `IsPlayerInControl` gate is useful beyond Crowd Control.
- **Test impact:** redship tier.
- **Note:** The finder reports the Anchor client is not lit in RSBS; these are hardening for when it is.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `d7981bf03` | #6166 | Some tweaks to anchor (#6166) | S | port-with-adaptation | It moves PLAYER_UPDATE handling onto the game-thread queue, which removes a data race on `clients`, and wraps packet dispatch in try/catch so a malformed packet canno ... |
| `ef042be5e` | #6393 | avoid undefined behavior when handling anchor packets (#6393) | S | port-with-adaptation | Hardens Anchor packet parsing against missing fields by switching to .at(). |
| `8a0315a2d` | #6591 | Fix Crowd Control Wolfos despawning when scene switch 0 is set (#6591) | S | port-with-adaptation | Crowd Control's Wolfos spawned with params 0 and instantly self-killed in scenes where switch 0 is set. |
| `cb974d123` | #6944 | Fix Sail network hooks (#6944) | S | port |  |
| `005044868` | #6341 | CC fixes (#6341) | S | port |  |
| `43f77c13f` | #6383 | Fix memory leaks in debugconsole & CrowdControl (#6383) | M | port-with-adaptation |  |
| `5f0c0c8e2` | #6372 | Added minimap icons for other players in Anchor (#6372) | M | defer |  |
| `b8634f1d8` | #6384 | Fix `numLoaded` leak for Anchor (#6384) | S | port-with-adaptation |  |
| `942134669` | #6859 | Ensure great fairy rewards are synced in vanilla (#6859) | S | defer |  |
| `2c9187f27` | #6926 | Bugfix, make CC invisible Link equips invisible (#6926) | S | port |  |

### O34. Ivan co-op

Priority P3, effort M (5 commits: Sx3 Mx2).

- **Prerequisites:** 1c58bcc3e first. 0d41af697 (the Ivan heap-corruption fix) is in O11.
- **Test impact:** redship tier.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `1c58bcc3e` | #6707 | Ivan: Hookify and improve spawning (#6707) | S | port-with-adaptation |  |
| `37ac3d51d` | #6753 | Ivan: Megaton Hammer rework (#6753) | S | port |  |
| `8a1547a6e` | #6735 | Ivan: New Farore’s Wind (#6735) | M | port |  |
| `65ffd5445` | #6768 | Ivan: Allow Ivan to break pots and crates while far from Link (#6768) | M | defer |  |
| `b09c0733e` | #6815 | Ivan: Move ActorDB registration into IvanCoop.cpp (#6815) | S | defer |  |

### O35. Developer tools

Priority P3, effort L (15 commits: Sx7 Mx3 Lx5).

- **Prerequisites:** c94ae8ccf (macro savestates) needs a decision on whether savestates are supported in paired sessions at all.
- **Test impact:** redship tier.
- **Note:** Developer-facing only.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `426b37ddf` | #6474 | Save editor v2 (#6474) | L | defer |  |
| `651078f0b` | #6163 | Added search boxes for Save Editor flags. (#6163) | S | port-with-adaptation |  |
| `dfba6b319` | #6191 | Fixes missing flag filter theme color (#6191) | S | port |  |
| `4c04c7615` | #7198 | Save editor flag improvements (#7198) | M | defer |  |
| `53a663577` | #6863 | ActorViewer, add hex id and params (#6863) | S | port |  |
| `1af0f4e39` | #6148 | Added Scarecrow Spawn to the Collision Viewer. (#6148) | S | port-with-adaptation |  |
| `0d4986f2c` | #6137 | Add Clear List button to Mod Menu (#6137) | S | port |  |
| `5a322418f` | #6649 | mod menu group select (#6649) | M | defer |  |
| `be0f8fec9` | #6151 | Fix boot to debug warp screen not applying time savers (#6151) | S | port |  |
| `43ea33287` | #5839 | [Enhancement] Time Splits v2 (#5839) | L | defer |  |
| `c94ae8ccf` | #6904 | wire decomp static savestates with macros (#6904) | L | defer |  |
| `66bb7888b` | #6914 | Add save state wiring for Barinade, Bongo Bongo, & Morpha (#6914) | L | defer |  |
| `982f598e0` | #7042 | Savestates, add `sEffectContext` (#7042) | S | defer |  |
| `dfaed9086` | #6927 | Savestates, player serialization (#6927) | M | defer |  |
| `fb657cc5d` | #6954 | Savestates, more AudioOcarina serialization (#6954) | L | defer |  |

### O36. Hookify refactors (take only as prerequisites)

Priority P3, effort M (21 commits: Sx9 Mx8 Lx4).

- **Prerequisites:** Each is behaviour-preserving; take one only when a later fix lands in the file it creates (5576f93ef before Shuffle Icicles; 6340ad3d5 is xref family 10's reticle hook, the OoT half of #1173).
- **Test impact:** redship tier; no goldens.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `1f57f72ac` | #6366 | Hookify Sunlight Arrows (#6366) | M | port-with-adaptation |  |
| `23606325e` | #6208 | Hookify Goron Pot (#6208) | S | port |  |
| `236c9c81f` | #6968 | Hookify Bombchu Bowling options (#6968) | S | port-with-adaptation |  |
| `5c55c01c6` | #6969 | Hookify frogs' Ocarina game (#6969) | S | port-with-adaptation |  |
| `9d1e5d281` | #6976 | Hookify Lost Woods Ocarina game (#6976) | M | port-with-adaptation |  |
| `cca66cdc5` | #6972 | Hookify Shooting Gallery options (#6972) | S | port-with-adaptation |  |
| `af8cac9e6` | #6973 | Move minigame hooks into Minigames folder (#6973) | S | port-with-adaptation |  |
| `b81d47d79` | #6881 | Hookify text speed options (#6881) | M | defer |  |
| `bd8a5825f` | #7088 | Hookify masks / timeless equipment (#7088) | L | defer |  |
| `97fb13cbe` | #7189 | Hookify FP movement (#7189) | M | defer |  |
| `6340ad3d5` | #6279 | Hookify Hookshot Reticle Changes (#6279) | S | port-with-adaptation |  |
| `d4c1118d1` | #6305 | Hookify DisableKokiriDrawDistance (#6305) | M | defer |  |
| `d9ebce9f9` | #6278 | hookify additional reticles (#6278) | S | defer |  |
| `5576f93ef` | #6354 | Hookify blue fire arrows (#6354) | M | port-with-adaptation | Moves the Blue Fire Arrows enhancement and rando option into OnActorInit/VB hooks, refreshes z_bg_ice_shelter.c from decomp (BgIceShelter_* names, RED_ICE_* enum), an ... |
| `e628cf057` | #7209 | refactor blue fire arrows (#7209) | S | port-with-adaptation | Upstream replaces the 'attacker is an arrow whose child is an ice arrow' check with a check of the hitting collider's DMG_ARROW_ICE flag. |
| `ca0a17cfe` | #7070 | bombchu bowling setting: first item (#7070) | M | defer |  |
| `27a66925b` | #7219 | Add restoration for hookshot as child to softlock (#7219) | M | defer |  |
| `0d2346d32` | #6802 | Reduce LUS header bloat & improve build speed (#6802) | L | defer |  |
| `633e92096` | #6813 | Cleanup unused includes & remove remaining LUS umbrella includes (#6813) | L | defer |  |
| `b424e4d57` | #6689 | Sort some files into their correct folder (#6689) | S | defer |  |
| `9baa114f6` | #7091 | Hookify fish, refactor fishsanity (#7091) | L | defer |  |

### O37. Platform, asset pipeline and decomp-naming decisions

Priority P3, effort XL (11 commits: Sx3 Mx1 Lx5 XLx2).

- **Prerequisites:** Operator decisions: Torch versus ZAPDTR for OoT while MM stays on ZAPDTR (c00dd24ca); Windows and Linux ARM64; following upstream decomp renames at all (the four SoH renames and 2S2H f1dda4c28 are naming only, and MM symbols collide with f2f4413bf's camera names).
- **Test impact:** Full build on each platform taken.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `c00dd24ca` | #6989 | Replace ZAPDTR and OTRExporter with Torch (#6989) | XL | defer | Switching OoT asset extraction to Torch rewrites the build and asset pipeline (20k yml files, soh-torch and soh-o2r-packer). |
| `95d8f7eb5` | #6952 | Bump ZAPD for maybe the last time (#6952) | M | defer |  |
| `f45d65381` | #6635 | Add Windows ARM64 build support (#6635) | L | defer |  |
| `1595a94df` | #7281 | Add ARM64 Linux CI (#7281) | S | defer |  |
| `17c57c64c` | #7094 | Try fix mac build flakiness in CI with mdutil -a -i off (#7094) | S | defer |  |
| `78dc6d970` | #7186 | Don't erase __attribute__ under clang-cl (#7186) | S | port |  |
| `06b512faa` | #6470 | port over unk renaming from upstream (#6470) | L | defer |  |
| `50837308b` | #6890 | Decomp, update `z_en_holl.c` (#6890) | L | defer |  |
| `5cf03a4cd` | #6877 | pull decomp changes: actor functions, z_en_horse.c (#6877) | L | defer |  |
| `c7ef690bc` | #6498 | func_$hex renaming from upstream (#6498) | L | defer |  |
| `2S2H f1dda4c28` | #1504 | Merge pull request #1504 from Eblo/decomp-cherry-pick-the-squeakuel | XL | defer |  |

## 2Ship2Harkinian (`games/mm/`)

274 commits since `d05565623`: 14 ported, 11 partially ported, 218 absent, 31 not applicable. 204 are candidates, and 84 of those are `defer`, most of them because the enhancement TU they touch is not linked in the single executable, or because the option they add grows an MM enum that the `.redsave` MM tier persists as a raw image. Two facts verified on 2026-10-26 set the frame for every MM bundle. First, the hosting pattern exists and is documented in code: a hosted MM enhancement TU is appended to `ship__Enh_registrars` in `games/mm/CMakeLists.txt` (the #1071 carve-out, linked `WHOLE_ARCHIVE` and audited by `check-registrar-elision.sh` and `mm_registrar_coverage_test.cpp`), its hook body carries `RSBS_SINGLE_EXE_REQUIRE(MM_gPlayState != NULL)`, its `RegisterShipInitFunc` runs from `S2H::ShipInit::InitAll()` and re-arms through `ShipInitBridge_SingleExe.cpp`, and its row is either a converged key in `kConvergedKeys` (an existing SoH row drives it, as PRs #1082 and #1093 did) or an MM-only entry in `kHostedMmEnhancements` (`src/common/cvar_shared_keys.h`). Second, MM-tier growth is a save break today: PR #1125 appended 53 `RC_` ids and one `RO_` id without a version bump, and `RSBS_SAVE_VERSION` is still `4u`; the lanes that grow `RC_`, `RO_` or `RE_` need the growth decision first.

| Id | Bundle | Priority | Effort | Commits | LUS | Goldens move |
|---|---|---|---|---|---|---|
| M1 | Skeleton registration and alt-asset toggles | P0 | M | 6 (Sx3 Mx2 Lx1) | no | no |
| M2 | Crash and undefined-behaviour fixes | P0 | S | 10 (Sx9 Lx1) | no | no |
| M3 | Rando logic correctness: souls, time, swim and the Ship_Random bound | P0 | M | 21 (Sx15 Mx5 Lx1) | no | yes |
| M4 | Rando item, shop and check behaviour | P1 | M | 16 (Sx14 Mx2) | no | yes |
| M5 | New MM shuffles: wonder items, beehives, butterflies, keaton grass | P2 | L | 9 (Sx4 Mx1 Lx4) | no | yes |
| M6 | Dungeon item confinement, skeleton key and pool caps | P3 | XL | 8 (Mx2 Lx1 XLx5) | no | yes |
| M7 | Hints | P2 | M | 8 (Sx1 Mx4 Lx1 XLx2) | no | yes |
| M8 | Songs and ocarina | P2 | M | 8 (Sx6 Mx1 Lx1) | no | yes |
| M9 | Song of Time reset NPC | P2 | S | 3 (Sx2 Mx1) | no | no |
| M10 | Default-on fixes and parity rows that converge onto OoT keys | P1 | M | 13 (Sx6 Mx6 Lx1) | no | no |
| M11 | Accessibility | P2 | S | 3 (Sx1 Mx2) | no | no |
| M12 | MM-only QoL enhancements (hosting pattern) | P3 | L | 46 (Sx25 Mx17 Lx4) | no | no |
| M13 | Large MM enhancements | P3 | XL | 6 (Sx2 Mx2 Lx1 XLx1) | no | no |
| M14 | Cosmetics, models and the MM cosmetics editor | P3 | XL | 7 (Sx1 Mx1 XLx5) | yes | no |
| M15 | Visuals under frame interpolation | P2 | S | 2 (Sx2) | no | no |
| M16 | Traps, junk cycling and wallets | P2 | L | 8 (Sx1 Mx4 Lx1 XLx2) | no | yes |
| M17 | Audio | P3 | M | 2 (Mx2) | no | no |
| M18 | Native MM trackers (reference only) | P3 | S | 3 (Sx1 Mx2) | no | no |
| M19 | MM infra and docs | P3 | S | 4 (Sx4) | no | no |

### M1. Skeleton registration and alt-asset toggles

Priority P0, effort M (6 commits: Sx3 Mx2 Lx1).

- **Prerequisites:** None. Land 26146e975 and f86e04b4d together with d6ac2550b's dedupe in OoT's shared `SkeletonPatcher` (`games/oot/soh/resource/type/Skeleton.cpp`).
- **RSBS adaptation:** MM's Player is destroyed on every scene change, so each MM scene leaves two dangling `SkelAnime` pointers in the shared patcher; an Alt Assets toggle later makes OoT's `UpdateSkeletons` write through them. 4243fd92f must also tell MM-layout entries apart so the patcher does not write OoT-only fields. 029ac2de9: port the `Player_IsCustomLinkModel` hunks without the Kafei clause.
- **Test impact:** redship and integration tiers (a crossing then an Alt Assets toggle); the ODR tripwire.
- **Note:** Xref family 5 (the #1063 family's MM half). Verified a live gap: `MM_Player_Destroy` (`z_player.c:13506`) has no unregister, while `MM_Player_InitCommon` registers into OoT's shared patcher through `SkelAnime_InitPlayer` (`ResourceManagerHelpers.cpp:701-726`); the Link-path filter in `Skeleton.cpp:41-43,78-85` matches only OoT's Link paths, so MM Link's entry stays and dangles until OoT's `ClearSkeletons` (`game.c:495`). Kafei (`z_en_test3.c:559`) registers the same way. Neither patcher dedupes (`Skeleton.cpp:62-104`); MM's own patcher is dead code here (`BenPort.cpp:1768`). PR #1092 did not touch either `Skeleton.cpp`.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `26146e975` | #1980 | Unregister player's skeleton (#1980) | S | port | MM's Player is destroyed on every scene change, so each MM scene leaves two dangling SkelAnime pointers in the shared skeleton patcher. |
| `f86e04b4d` | #1800 | unregister kafei skeleton on actor deletion (#1800) | S | port | Without the unregister, every Kafei (EnTest3) instance leaves a dangling SkelAnime pointer in OoT's skeleton patcher. |
| `d6ac2550b` | #1689 | fix stale SkelAnime cache on alt toggle (#1689) | S | port-with-adaptation | Dedupes skeleton registration and removes every matching entry on unregister, so an alt-asset toggle does not walk stale SkelAnime pointers. |
| `4243fd92f` | #1623 | proper alt toggle support (#1623) (partially ported) | L | port-with-adaptation | Makes toggling alt assets in MM refresh player faces, FD sheaths and the skeleton pointers, including Flex dListCount. |
| `029ac2de9` | #1796 | XML Skeleton Support (#1796) (partially ported) | M | port-with-adaptation | Custom (SoH-style XML) player models load in MM but draw wrong: MM's LOD and waist display lists override them. |
| `5436bfe6a` | #1606 | [Modding] Fierce Deity Sheaths (#1606) | M | defer |  |

### M2. Crash and undefined-behaviour fixes

Priority P0, effort S (10 commits: Sx9 Lx1).

- **Prerequisites:** 16ef59bae with sub-commit 96611c3b3 (#1901, save delay when voiding out during an owl save).
- **RSBS adaptation:** `MM_` prefixes. 7f3c8327f lands in `BenPortHudSingleExe.cpp`; mirror it into `BenPort.cpp` for parity. 16ef59bae's `Sram_OpenSave` half runs through `MM_Sram_OpenSave`, which the restore path calls.
- **Test impact:** redship tier; integration rows (`MM_Sram_OpenSave` runs on every arrival).
- **Note:** All verified absent: `z_en_ot.c:63-73` has no reset entry (our profiles support one, see `ovl_Bg_Crace_Movebg`); `z_en_attack_niw.c:400` reads `parent->update` before the null check; `z_en_test4.c:61,67,122,128` index `[CURRENT_DAY - 1]` unguarded; `z_sram_NES.c:1641` has no `OWL_WARP_MAX` guard (low reach: MM's flash load is stubbed); `z_en_nb.c:500` is a plain `Sram_IncrementDay`; `BenPortHudSingleExe.cpp:67` subtracts a `uint32_t` width. 385f5c075 has low reach today because MM's UnrestrictedItems key is parked.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `bb1c03b60` | #1469 | Merge pull request #1469 from HarbourMasters/develop-mion | S | port | The seahorse actor keeps static EnOt* pointers to other seahorse instances. |
| `5edc11259` | #1935 | Reordered if checks to prevent null pointer reference and crash (#1935) | S | port | Fixes a null-pointer dereference crash when an attacking cucco's parent is gone while the player is in range. |
| `385f5c075` | #1849 | Bugfix, Deku Hookshot pull out crash (#1849) | S | port | It fixes a crash (float overflow / bad raycast) when Deku Link pulls out the Hookshot. |
| `16ef59bae` | #1462 | WIP Tweaks to fix compatibility with 0th/4th day glitches (#1462) | L | port-with-adaptation | Two separable UB fixes that should land as a small S lane. |
| `c2f50b3cc` | #1479 | fix soft lock (#1479) | S | port | Under clock shuffle, the Anju's-grandmother story advances the day with Sram_IncrementDay, which can land the player in a half-day they do not own (a softlock). |
| `86d78d17f` | #1843 | Fix remaining '2' on file select code (#1843) | S | port |  |
| `b2872844e` | #1965 | fix: avoid integer pointer arithmetic in player animations (#1965) | S | port |  |
| `7f3c8327f` | #1846 | Merge pull request #1846 from HarbourMasters/develop-keiichi | S | port | When the window is narrower than 4:3, unsigned wraparound sends the computed HUD X off-screen, so MM's A button and three-day-clock hour hand disappear. |
| `5c35f92ef` | #1644 | Format strings before printing (#1644) | S | port |  |
| `c74ad0e38` | #1753 | Fix mod menu child scope (#1753) | S | port-with-adaptation |  |

### M3. Rando logic correctness: souls, time, swim and the Ship_Random bound

Priority P0, effort M (21 commits: Sx15 Mx5 Lx1).

- **Prerequisites:** 29e30e6b4 inserts `RE_MAIL_LETTER_TO_KAFEI` mid-enum upstream (`Types.h:3162-3163`); here it must be appended, because `randoEvents` is `u8[RE_MAX]` inside `RandoSaveInfo` and the `.redsave` MM tier is a raw `SaveContext` image, so an insert renumbers every later `RE_` id and shifts every later field. 99204fb69 needs c2f50b3cc (M2). The five merge commits (4f2548d9d, 0b1277a5a, b4b4299e4, 1ace9c2fe, 56d96dc6b) are to be split; the finder's picks are named in each record.
- **RSBS adaptation:** MM's region tables feed the production coordinator crawl, so every row here changes paired placements. e07b6f731 (`Ship_Random` exclusive bound: the last element can never be picked in hints, shuffles, the starting half-day and weighted junk) moves every MM and paired golden; name it. 1e57e04d4 and f1dee0072 fix MM's solo fills only, which the paired fill replaces.
- **Test impact:** MM and paired goldens re-pin; rando tier; the MM logic golden profile of #1001.
- **Note:** Xref family 7 extended. Every row verified absent (`Central.cpp:403`, `TimeLogic.cpp:62`, `MilkRoad.cpp:164,189`, `SnowheadTemple.cpp:41`, `South.cpp:288`, six `CAN_USE_PROJECTILE` rows for the giant bee, `TerminaField.cpp:123,184`). e07b6f731: our `Ship_Random` (`ShipUtils.cpp:331-343`) excludes its upper bound and the call sites still pass `size - 1`: `StartingItems.cpp:59` never picks `RI_TIME_NIGHT_3`, `EnGs.cpp:169,211,508` never pick the last hint (the `:151-155` comment says the bound was kept so no hint moves), and the three solo fills (`GlitchlessLogic.cpp:73,268,332`, `NearlyNoLogic.cpp:14,81`, `NoLogic.cpp:14`) share it. Paired worlds reach StartingItems and EnGs; the solo fills run only unpaired.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `cc5d87143` | #1473 | Add ITEM_MASK_ROMANI to RC_MILK_BAR_CIRCUS_LEADER_MASK (#1473) | S | port | Logic can currently expect the Circus Leader's Mask without the Romani Mask, which is needed to be in the Milk Bar at night for the performance, so a seed can require ... |
| `9b699dee1` | #1506 | Some time logic fixes (#1506) | S | port | With time shuffle on, logic assumed the player could wait through item-gated time restrictions. |
| `29e30e6b4` | #1864 | Various fixes around kafei's quest + time shuffle (#1864) | S | port-with-adaptation | Logic currently assumes holding the Letter to Kafei means it was mailed. |
| `9224b9f48` | #1860 | Fix logic for pots in snowhead boss room (#1860) | S | port | With boss souls and pots shuffled, the boss-room pots are logically reachable without Goht's soul. |
| `bcc562a3e` | #1503 | Require soul for Bad Bat tree Heart Piece (#1503) | S | port | With enemy souls shuffled, logic currently places items at the Bad Bat tree Heart Piece without requiring the Bad Bat soul, so a seed can expect an item the player ca ... |
| `d80a5cf2e` | #1515 | Added CanKillEnemy condition for ENEMY_DROP_GIANT_BEE checks (#1515) | S | port | With enemy souls and enemy drops shuffled, the beehive Giant Bee drops are in logic without the bee soul. |
| `f237dd293` | #1483 | [Rando] Logically require souls for Peahat and Dodongo grotto chests (#1483) | S | port-with-adaptation | With enemy souls shuffled, these two grotto chests open only after the enemies die, but logic does not require the souls, so a seed can expect a chest the player cann ... |
| `a960c7f44` | #1454 | Revise enemy souls and drops (#1454) | M | port-with-adaptation | With enemy souls shuffled, logic currently puts about 25 Gold Skulltula checks in logic without the Skulltula soul, so the solver can require a token the player canno ... |
| `180318962` | #1727 | Allow zora for clock tower roof checks (#1727) | S | port | Logic now accepts Zora fins for hitting Skull Kid on the Clock Tower roof (the Song of Time and Ocarina checks), matching what the game allows. |
| `4f2548d9d` | #1554 | Merge pull request #1554 from HarbourMasters/develop-keiichi (partially ported) | S | port-with-adaptation | Three of eight merged rando PRs are already in, and the time-state join exists in RSBS's own form. |
| `0b1277a5a` | #1546 #1550 #1559 #1560 #1561 #1562 #1563 #1565 #1568 #1569 #1577 #1578 | Merge branch 'develop-keiichi' of github.com:HarbourMasters/2ship2harkinian into develop | L | port-with-adaptation | Mostly MM rando logic correctness: swim and soul requirements for water enemies, the explosive definition, and the Bad Bat HP final form. |
| `b4b4299e4` | #1927 | Merge pull request #1927 from HarbourMasters/develop-battler | M | port-with-adaptation | The bundle holds six MM logic correctness fixes that apply to our tree now: moon Link trial room gating, icicle room, the Zora Cape swim requirement plus missing exit ... |
| `1ace9c2fe` | #1702 | Merge pull request #1702 from HarbourMasters/develop-keiichi (partially ported) | M | port-with-adaptation | The absent pieces worth taking are rando logic and check-map correctness. |
| `56d96dc6b` | #1909 | Merge pull request #1909 from HarbourMasters/develop-battler (partially ported) | M | port-with-adaptation | A bundle of 5.0.1 fixes, several of them rando-correctness: - #1904: the Blast Mask check is granted when Sakon is killed or ignored. |
| `99204fb69` | #1488 | Fixes for suns song usage in time shuffle (#1488) | S | port-with-adaptation | Under time shuffle, an unowned-time skip now clears an active Sun's Song fast-time state and sets the correct day number. |
| `c0c746334` | #1472 | Use computed starting items for pool removal (#1472) | S | port-with-adaptation | With Shuffle Time on and no starting time item chosen, GrantStartingItems grants a fallback half-day, but GeneratePools never removes it, so the pool carries a duplic ... |
| `b39664f88` | #1839 | Always shuffle first bomb bag (#1839) | S | skip (not applicable) | Makes the Bomb Shop's first bomb bag always shuffled, like the second; intentionally skipped in RSBS because the single-bag shared bomb-bag pool and `#737` fixed-content contract pin MM's default unshuffled-shop exceptions to `RC_CURIOSITY_SHOP_SPECIAL_ITEM` and `RC_BOMB_SHOP_ITEM_04_OR_CURIOSITY_SHOP_ITEM`. |
| `e07b6f731` | #1777 | Merge pull request #1777 from garrettjoecox/fix-ship-random-usage | S | port-with-adaptation | Off-by-one fixes after Ship_Random became exclusive: the last element could never be picked in hints, shuffles, the random starting half-day and weighted junk placement. |
| `1e57e04d4` | #1497 | Fix glitchless not handling force junk items corretly (#1497) | S | port |  |
| `f1dee0072` | #1500 | Fix a glitchless gen issue, tweak logging, and tweak metrics (#1500) | S | port |  |
| `431dea3c6` | #1892 | develop-battler -> develop (#1892) | M | port-with-adaptation | Worth taking: the Tingle-map and Bombers' Notebook fixes (10e35849a), so those checks can be obtained again after a Song of Time reset, and the effect-slot epoch and ... |

### M4. Rando item, shop and check behaviour

Priority P1, effort M (16 commits: Sx14 Mx2).

- **Prerequisites:** cb0bab40d's hunk context assumes 8775c32bb (M16) and its trap half needs 254cc60e1 (M16); port the shop half now. b011f16bc inserts an enumerator at value 0 upstream; append it here instead.
- **RSBS adaptation:** 99ff06284 stores exclusions by check name; our MM Locations page (#1106) persists them in the combo record, so adopt the name keying there. 77cbefd28's toast goes through `src/common/notification_bridge`. c20da56bd: `gPlayState` becomes `MM_gPlayState`.
- **Test impact:** rando tier; MM goldens move through bfcb60476 (plentiful) and b011f16bc; UI snapshot of MM Randomizer for the new rows.
- **Note:** Verified: `EnGirlA.cpp:183-194` gives `RI_JUNK` unconverted and our `CurrentJunkItem()` takes no check id; `Traps.cpp:378` allows only `OBJECT_RR`. 99ff06284 is a different design from ours: MM exclusions are a comma-separated list of `RandoCheckId` integers frozen as a per-ordinal bitset `randoSaveExcludedChecks[(RC_MAX+7)/8]` (`z64save.h:456`, `Foreign.cpp:248-260`), stable only while `RC_` stays append-only.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `cb0bab40d` | #1811 | Fix static trap models leaking, and shop items not converting junk (#1811) | S | port-with-adaptation | Buying a shop slot that holds junk filler currently costs rupees and gives nothing, though the slot is drawn and named as the current junk item (DrawItem.cpp:520, Ite ... |
| `5fa55eb05` | #1857 | Fix like like trap not dropping shield when dying (#1857) | S | port | With traps shuffled, a trap-spawned Like Like that eats the Hero's Shield never drops it on death, because the shield object is not loaded, so the player loses the sh ... |
| `c03a59dae` | #1810 | Make randomized checks no longer use custom texture after first obtain (#1810) | S | port | After a pot, crate, barrel or grass check is obtained once, later cycles draw it with its vanilla texture, so the player can see what is still outstanding. |
| `bda545f36` | #1614 | Change Non-CMC Pot Model to Major (#1614) | S | port |  |
| `1db0c273f` | #1450 | [Rando] Add missing segments to enemy soul draw funcs (#1450) | S | port-with-adaptation |  |
| `e81ca1a89` | #1706 | fix (#1706) | S | port-with-adaptation |  |
| `cfd1116a4` | #1775 | Fix scene names on check tracker (like winter/poison) (#1775) | S | port-with-adaptation |  |
| `77cbefd28` | #1478 | Emit notification on seed generation failure (#1478) | S | port-with-adaptation | Shows a 'Seed Failure:' toast with the reason instead of failing silently, rewrites the error strings, validates Shuffle Time has a time item, and hardens spoiler import. |
| `99ff06284` | #1817 | Improvements to excluded checks (#1817) | M | port-with-adaptation | Excluded checks whose vanilla item is junk now stay vanilla and add nothing to the pool, which avoids pool imbalance. |
| `1d30a5cf6` | #1629 | Make more checks repeatable (#1629) | M | port-with-adaptation | After a Song of Time reset these checks can be done again: boss warps, owl statues, Darmani, Mikau, the Hina item draw and the Giants' Oath (ConvertItem decides what ... |
| `2188b01ac` | #1781 | Merge pull request #1781 from garrettjoecox/adjust-terminal-time-defau | S | port |  |
| `835a61a60` | #1613 | Rename "DINALFOS" to "DINOLFOS" (#1613) | S | port |  |
| `8061b4443` | #1782 | Merge pull request #1782 from garrettjoecox/house-keeping | S | port-with-adaptation |  |
| `bfcb60476` | #1514 | More metrics tweaks (#1514) | S | port-with-adaptation | Plentiful now always adds a second copy of every major item, mask and key, plus non-map/compass lesser items. |
| `b011f16bc` | #1896 | Add vanilla option for moon trials (#1896) | S | port-with-adaptation | Adds a Vanilla moon-trial access mode: the children ask for 1/2/3/4 masks as in the original, the rando text/mask overrides are off, and logic follows. |
| `c20da56bd` | #1769 | Merge pull request #1769 from garrettjoecox/souls-grayscale | S | port-with-adaptation | Enemies whose soul you lack are drawn grayscale, which makes the LIVE Enemy Souls option (OptionsUiSingleExe.cpp) readable at a glance. |

### M5. New MM shuffles: wonder items, beehives, butterflies, keaton grass

Priority P2, effort L (9 commits: Sx4 Mx1 Lx4).

- **Prerequisites:** Tree Shuffle (4661df2e6) is ported as Shuffle Tree Drops (#1125, `fabe7d33`): it appended 53 `RC_` ids before `RC_MAX` (`Types.h:2300-2353`, with the append-only comment), `RCTYPE_TREE` and `RO_SHUFFLE_TREE_DROPS`, left out the 25 Gorman Track trees that 22518984e removed, and did not bump `RSBS_SAVE_VERSION` (still `4u`, `src/common/save.h:61`); the growth resized `randoSaveChecks[RC_MAX]` and `randoSaveOptions[RO_MAX]` inside the raw MM tier, which is the accepted v0.3.0 save break. Every shuffle here repeats that break unless the lane adds a version bump or a migration first; `combo_mm_options_view.h:352-360` names the hazard. 630536f68 needs Tree Shuffle's `VB_DROP_COLLECTIBLE` plumbing, which #1125 brought.
- **RSBS adaptation:** Each shuffle appends `RC_` entries (append only, never insert). Decide once whether MM-tier growth gets a version bump and migration or another accepted break. Classify the new checks for the single-bag coordinator and the Combo Tracker rows (#1149).
- **Test impact:** MM and paired goldens re-pin; rando tier; the save-compatibility rows.
- **Note:** OoTMM-style shuffles. Wonder Items adds 113 checks, beehives 19, butterflies 25.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `630536f68` | #1785 | [Rando] Shuffle Wonder Items (#1785) | L | defer | Adds 113 invisible 'wonder item' checks (EnInvisibleRuppe, EnScopecoin, item drops, the Swordsman School gong), an OoTMM-style shuffle. |
| `ddfc4e3f0` | #1869 | Sparkles for wonder items (#1869) | S | defer |  |
| `1cd9e9796` | #1871 | Fix ocean spider house wonder item logic (#1871) | S | defer | Logic correctness for wonder-item checks only. |
| `dd5a36d60` | #1822 | Shuffle keaton grass and some more missed grass (#1822) | L | defer | A sizeable expansion of MM grass shuffle (more checks, logic, models). |
| `4ce76f3de` | #1773 | Beehive shuffle (#1773) | L | defer | Adds 19 beehive checks with region logic, ObjComb VB hooks and an option, an OoTMM-style shuffle. |
| `500d70a8b` | #1872 | Ensure beehive drops are dropped, some were getting stuck in ceiling (#1872) | S | defer | Shuffled beehive contents can spawn stuck in the ceiling and become uncollectable. |
| `b690db6fd` | #1770 | Shuffle butterflies (#1770) | L | defer | Adds 25 butterfly checks (catch with Deku Stick, via ObjMure children) with region logic and an option. |
| `2e733f5e1` | #1803 | Adjust butterfly logic for spring mountain village (#1803) | S | defer |  |
| `1c35785ea` | #1547 | New Features: No random drops + No heart/healing drops (#1547) | M | defer |  |

### M6. Dungeon item confinement, skeleton key and pool caps

Priority P3, effort XL (8 commits: Mx2 Lx1 XLx5).

- **Prerequisites:** An ADR 0010 amendment: the single-bag coordinator needs an MM restricted pass with CONFINED rows before any of a0c9b12ce's modes can hold. 977544284 reshuffles the `RO` enum upstream; here it must append. 918e5f44e can be adapted onto `RO_MINIMUM_SKULLTULA_TOKENS` without 977544284.
- **RSBS adaptation:** OoTMM has own-dungeon MM key settings, so the reference supports the feature; the mechanism is ours.
- **Test impact:** MM and paired goldens; rando tier; the confinement rows of the coordinator tests.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `a0c9b12ce` | #1761 | Working dungeon item confinement (#1761) | XL | defer | Options to keep MM small keys, boss keys and stray fairies in their own dungeon, by pre-placement and per-check filtering (OoTMM has own-dungeon MM key settings). |
| `7bc1c4a8c` | #1838 | Ability to start with keys & stray fairies (#1838) | L | defer | Adds a 'Start With' value to the dungeon-item placement options: keys, boss keys and all stray fairies go into starting items and leave the pool. |
| `236aa75ba` | #1883 | Allow dungeon items to be set to vanilla locations (#1883) | XL | defer | Adds a Vanilla choice to the small key, boss key and stray fairy placement options, plus a start-with Clock Town fairy fix. |
| `4c91dcf78` | #1930 | Add option for shuffling remains in it's own dungeon (#1930) | XL | defer | Adds an Own Dungeon mode that confines each boss's remains to its own temple. |
| `528eed442` | #1928 | Shuffle songs on song locations (#1928) | XL | defer | Adds a song-shuffle mode (Vanilla / Song Locations / Anywhere) that confines MM songs to MM song checks and junks surplus song locations. |
| `b3cc36628` | #1776 | Implement skeleton key (#1776) | M | port-with-adaptation | A new MM pool item that grants every temple's full small-key count. |
| `977544284` | #1507 | Add skulltula & stray fairy pool configuration (#1507) | XL | defer | Adds MAX options that trim surplus stray fairies and skulltula tokens from the pool, and renames the 'minimum' options to REQUIRED. |
| `918e5f44e` | #1840 | Change how skulltula shuffle works (#1840) | M | port-with-adaptation | New option: N Gold Skulltulas per Spider House are randomized checks, chosen at random each seed; the rest keep their vanilla token. |

### M7. Hints

Priority P2, effort M (8 commits: Sx1 Mx4 Lx1 XLx2).

- **Prerequisites:** A new `RO` option per hint, appended: `fabe7d33` is the only RedShipBlueShip commit that ever appended an `RO_` id (`RO_SHUFFLE_TREE_DROPS`), and the identity strings fold it only when it is on; #1047 chose a separate appended byte (`randoSaveTrapTypes`) and #1102 a separate field (`randoSaveMoonCrash`) to avoid growing `randoSaveOptions[RO_MAX]`. e3bfae008 with sub-commit 74e519def (#1530, Soaring is not Hookshot). d7bd6f9c7 (Saria's Song as an MM pool item) needs a shared-song ruling first and is deferred with 7aa885216 and f443698ea.
- **RSBS adaptation:** Every location text goes through `HintArea::ReplaceItemLocation` (`HintAreaSingleExe.cpp`), because in a paired world the hinted item may sit in Hyrule. 81c3e5525's weights need a weight for OoT-hosted MM items.
- **Test impact:** MM hint goldens re-pin; rando tier; UI snapshot of MM Randomizer.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `3091256aa` |  | [RANDO] Add Transformation Mask Hints | M | port-with-adaptation | A Clock Town poster (text 0x1C18) would cycle through where the Deku, Goron and Zora masks are, or 'Link's pocket' once obtained. |
| `966bff624` | #1903 | Option for moon gossip stones to hint their mask (from vanilla) (#1903) | M | port-with-adaptation | New hint option: each of the 20 Moon-trial gossip stones names where the mask it describes in vanilla is hidden. |
| `e3bfae008` | #1482 | Song of Soaring hint (#1482) | M | port-with-adaptation | Optional hint: the Southern Swamp engraving (text 0xC02) names the scene holding the Song of Soaring. |
| `81c3e5525` | #1567 | Add weight option to gossip stone hints (#1567) | L | port-with-adaptation | Gossip stones would hint important items more often, through a weight table and a strength slider (default 50), and checks with a heavy weight get their exact locatio ... |
| `2056ebc7a` | #1595 | [Rando] Add Bank Reward Hint (#1595) | M | defer | Adds an option that turns the West Clock Town bank sign into a hint for the bank Heart Piece check. |
| `d7bd6f9c7` | #1161 | Implement sarias song hint item (#1161) | XL | defer | Adds Saria's Song to MM's pool; playing it gives a hint toward a progression check that is in logic, via new z_message.c VB hooks and a CustomMessage change. |
| `7aa885216` | #1780 | Tweaks to sarias song behavior (#1780) | XL | defer | Adds a configurable priority-items list and a counted, save-persisted use count for the Saria's Song hint, built on a hint item we lack entirely. |
| `f443698ea` | #1522 | fix weirdness with playing a restricted song when Saria's song is available (#1522) | S | defer |  |

### M8. Songs and ocarina

Priority P2, effort M (8 commits: Sx6 Mx1 Lx1).

- **Prerequisites:** 527b177c0: Double Time and Inverted Song of Time are MM-only and port cleanly; Sun's Song exists in both games, so its copy needs a shared-song rule like #1083's. afd85345d before 2a9dd1da1 (same file). The ocarina-dive trio (efee75149, 9afe75860, e9c42eed9) together.
- **RSBS adaptation:** ec9e755bb: port the MM read side only (`GameInteractor_CustomOcarinaControls` in `GameExports_SingleExe.cpp`), so custom ocarina buttons set in the shared input editor are honoured in MM.
- **Test impact:** rando tier; MM goldens through 527b177c0.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `527b177c0` | #1481 | Shuffle some songs (#1481) | L | port-with-adaptation | Lets Song of Double Time, Inverted Song of Time and Sun's Song be shuffled as items; playing them is gated on owning them via VB_SONG_AVAILABLE_TO_PLAY. |
| `ec9e755bb` | #1598 | [Enhancement] Custom Ocarina Controls (#1598) | S | port-with-adaptation | A player who sets custom ocarina buttons in the shared input editor has them honoured in OoT but ignored in MM. |
| `afd85345d` | #1467 | Require Ocarina and buttons for POW in rando (#1467) | S | port | Pause-menu owl warp currently allows Song of Soaring without the Ocarina, or with unshuffled-but-missing ocarina buttons in rando, which is a logic bypass. |
| `2a9dd1da1` | #1572 | Better owl warp menu (#1572) | M | port-with-adaptation | Owl-statue warp selection follows the control-stick direction with a navigation map instead of linear cycling, applied both to the Song of Soaring map and to Pause Ow ... |
| `efee75149` | #1842 | Fix Ocarina Dive by introducing another 2 frames of grace for the 'button mashing' (#1842) | S | port |  |
| `9afe75860` | #1847 | Limit Ocarina dive fix to hovering only (#1847) | S | port |  |
| `e9c42eed9` | #1853 | Prevent faster song playback when falling (for ocarina dive) (#1853) | S | port |  |
| `a5beb126f` | #1542 | prevent ocarina end on sun song if faster playback is on (#1542) | S | defer |  |

### M9. Song of Time reset NPC

Priority P2, effort S (3 commits: Sx2 Mx1).

- **Prerequisites:** Port as one bundle (853156049 disables the object dependency the rooftop spawn needs).
- **RSBS adaptation:** Our paired start always grants the Ocarina and the Song of Time (ruling of 2026-10-01), so the value is the shuffled-ocarina-buttons case.
- **Test impact:** rando tier; integration rows that reach the Clock Tower rooftop.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `f16d4678a` | #1798 | Add NPC to perform SoT reset on Moon in rando (#1798) | S | port-with-adaptation |  |
| `fd42de862` | #1945 | Add SoT reset NPC to clock tower rooftop (#1945) | M | port-with-adaptation | Spawns a Moon Child NPC that performs a Song of Time reset, on the Moon (f16d4678a) and on the Clock Tower rooftop (this commit). |
| `853156049` | #1953 | Disable object dependency for rando time reset NPC (#1953) | S | port-with-adaptation | Follow-up to fd42de862: the rooftop has no OBJECT_OB loaded, so the spawned reset NPC needs the object-dependency check disabled to appear. |

### M10. Default-on fixes and parity rows that converge onto OoT keys

Priority P1, effort M (13 commits: Sx6 Mx6 Lx1).

- **Prerequisites:** The hosting pattern of PRs #1082, #1093, #1079 and #1119 (`kHostedMmEnhancements`, `kConvergedKeys`, linked TUs); see the verification table for which of these already have a row.
- **RSBS adaptation:** Under one-game semantics MM follows the setting the player chose in OoT: b1edee5ff and 6bf849276 read one combo-level Z-target and audio setting; eed759261, 937292289 and 7f67743b3 read OoT's existing keys through `kConvergedKeys`; 007667bdb converges MM onto `gEnhancements.MirroredWorldMode`.
- **Test impact:** redship tier; UI snapshot harness (no new rows where a converged key exists); the hooks-registered-and-dispatched check for every new registrar.
- **Note:** b1edee5ff verified: `z_common_data.c:35` sets `zTargetSetting` to 0 and the flash read in `z_sram_NES.c:2089,2097` always fails through `mm_save_manager_stubs.c:59-64`, so with OoT on Hold, MM plays Switch; neither `GameExports_SingleExe.cpp` nor #1046 maps OoT's `gSaveContext.zTargetSetting`. None of eed759261, 937292289, 7f67743b3 or 525c1bc62 has a TU, row or converged key today; the convergence targets are `gEnhancements.UnsheatheWithoutSlashing`, `gEnhancements.RemoveExplosiveLimit` and, for 525c1bc62, the `gSettings.A11yNoJabuWobble` pattern. 7f67743b3's nearest OoT key, `gCheats.InfiniteEponaBoost`, is a different feature.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `b1edee5ff` | #1809 | Hold Z-Targetters Rejoice (#1809) | M | port-with-adaptation | Upstream moves MM's audio and Z-target settings into config so Hold-Z targeting survives. |
| `6bf849276` | #1919 | Add checkbox to change Z-Targeting mode (#1919) | M | port-with-adaptation | Hold vs Switch Z-targeting is a player preference. |
| `0aebaf297` | #1888 | Fix Circle Shadow Streaks (#1888) | M | port-with-adaptation |  |
| `e04c3ae28` | #1603 | Fix Button Env Color (#1603) | S | port-with-adaptation |  |
| `b3dcb18a9` | #1834 | Fix Goron rolling ignores magic (#1834) | S | port | With 'Goron rolling ignores magic' on, Goron Link is pushed straight into spike rolling because the speed threshold is dropped along with the magic check. |
| `3d3a01887` | #1867 | Exclude shooting gallery actors from Hyper Enemies (#1867) | S | port |  |
| `dfa4a6a95` | #1855 | Don't skip one point cs for a specific block in stone tower (#1855) | S | port |  |
| `67d853f4c` | #1499 | fix free cam collision slide (#1499) | S | port-with-adaptation |  |
| `eed759261` | #1505 | Unsheathe Sword Without Slashing (#1505) | M | port-with-adaptation | Brings MM to parity with an OoT enhancement redship already ships: drawing the sword no longer auto-slashes. |
| `937292289` | #1733 | Add enhancement to remove explosives limit (#1733) | M | port-with-adaptation | Optional: removes the cap of 3 active explosives (5 for Honey & Darling) in MM, matching OoT's existing toggle. |
| `7f67743b3` | #1675 | [Enhancement] Infinite Epona Carrots (#1675) | S | port-with-adaptation |  |
| `007667bdb` | #1636 | More Mirrored World Mode options (#1636) | L | defer | Adds SoH-style Mirrored World modes to MM (always, random, seeded random, temples, spider houses, dungeons) driven per scene at OnSceneInit. |
| `e160e6d8d` | #1502 | Port bomb billboard and grotto rotation from SoH (#1502) | M | defer |  |

### M11. Accessibility

Priority P2, effort S (3 commits: Sx1 Mx2).

- **Prerequisites:** Hosting pattern.
- **RSBS adaptation:** New registrar TUs need a link contract (`check-registrar-elision.sh` guards it). 525c1bc62's VB goes in `z_quake.c`, which is always linked.
- **Test impact:** redship tier; UI snapshot of the Accessibility rows.
- **Note:** Motion-sickness toggles; OoT's heat-haze toggle (O21) is the sibling.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `525c1bc62` | #1880 | Toggle for screen distortion (#1880) | M | port-with-adaptation | A motion-sickness accessibility toggle that stops underwater and other screen warping in MM. |
| `6a6d84ab5` | #1510 | Accessibility option: No earthquakes on day 3 (#1510) | M | port-with-adaptation | An accessibility toggle that removes the final-day camera shakes, a motion-sensitivity aid. |
| `dabc3aadc` | #1887 | zora swim Y invert (#1887) | S | port-with-adaptation |  |

### M12. MM-only QoL enhancements (hosting pattern)

Priority P3, effort L (46 commits: Sx25 Mx17 Lx4).

- **Prerequisites:** Hosting pattern per TU. Pairs: 33422741e with 5c2104608 and 1642f0090 (EndOfCycle); 366cd97bc with 5f3886a19 and 6bfd6a35a; 90ba9a5ab with fc0adde38; cd33453d8 with 2220b84bc; ed0eb99b0 with 8b9452056; f8e213ced with d66aafc37; 29fdd1db9 with 55b70ec59.
- **RSBS adaptation:** Each is a hosted row under Enhancements, MM-only where OoT has no equivalent, converged where it has (f8e213ced's Faster Bottles is OoT's Fast Bottles). 2e211bf0a makes Persistent Bunny Hood state per file; here the feature is dropped from the hosted set, and the Shared Bunny Hood rule (#1095) owns the mask.
- **Test impact:** redship tier; UI snapshot per page; the registrar-elision check.
- **Note:** 3f51ada63 (alien defence speed) shortens a long timed check in an MM rando run.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `33422741e` | #1692 | Merge pull request #1692 from JordanLongstaff/end-of-cycle-save-scarecrow-song | M | port-with-adaptation | Splits EndOfCycle.cpp into one registrar per option and adds an optional 'Song of Time does not reset the Scarecrow's Song'. |
| `5c2104608` | #1619 | Enhancement to persist Chateau state through SoT (#1619) | M | port-with-adaptation | Optional QoL: the Song of Time keeps Chateau Romani's infinite magic. |
| `1642f0090` | #1707 | fix the fix (#1707) | S | defer |  |
| `366cd97bc` | #1744 | [Enhancement] Easy Mask Equip (#1744) | L | port-with-adaptation | Optional pause-menu QoL: press A on an owned mask to put it on or take it off without assigning it to a C button, using a temporary 'ghost' button slot. |
| `5f3886a19` | #1862 | redo item button handling (#1862) | S | defer |  |
| `6bfd6a35a` | #1949 | Fast Transformation: After First Time option (#1949) | S | port-with-adaptation |  |
| `90ba9a5ab` | #1455 | [Enhancement] Equip Masks from Other Forms (#1455) | M | port-with-adaptation | MM3D-style QoL: equipping a mask while transformed returns Link to human form wearing that mask. |
| `fc0adde38` | #1495 | swap forms from fd mask (#1495) | L | defer | 3DS-style mask equipping from non-human forms, extended so Fierce Deity can swap forms. |
| `2e7d838cd` | #1734 | Fierce Deity can use transformation masks (#1734) | S | defer |  |
| `3f51ada63` | #1940 | [Enhancement] Alien speed modifier (#1940) | L | port-with-adaptation | Skip or speed-scale the Night-1 Romani Ranch alien defence, a long timed check in an MM randomizer run. |
| `a0411a1b0` | #1465 | [Enhancement] Curiousity Shop Sell Refills (#1465) | L | port-with-adaptation | Adds Seahorse, Gold Dust and Chateau Romani bottle refills to the Curiosity Shop once each is first obtained, so they can be re-bought. |
| `4ed83dbf5` | #1518 | Change Seahorse Requirement for Curiosity Shop Refills (#1518) | M | defer |  |
| `a5cbfdf1c` | #1555 | Arrow buyback at full price, always (#1555) | S | port |  |
| `2c83cc890` | #1821 | Option to auto advance text to credits (#1821) | M | port-with-adaptation |  |
| `037f2eb39` | #1558 | Picto-box on C-up (#1558) | M | port-with-adaptation |  |
| `108d36fb7` | #1594 | always show feathers (#1594) | M | port-with-adaptation |  |
| `f22849e3a` | #1690 | [Enhancement] Tingle always in Clock Town (#1690) | M | port-with-adaptation |  |
| `5c1243c38` | #1947 | [Enhancement] Difficulty options for Fisherman's Jumping Game (#1947) | S | port-with-adaptation |  |
| `462c7f044` | #1830 | Tweak the TreasureChestShopShowFullMaze enhancement to progressively show different heights (#1830) | S | port |  |
| `1f888a9ec` | #1859 | Allow free look when climbing on objects (#1859) | S | port |  |
| `7e6907f96` | #1877 | Allow freelook in more situations (#1877) | S | port |  |
| `1f0a9d25b` | #1818 | [Enhancement] Goron Rolling Spikes requires Shield button to be pressed (#1818) | M | defer |  |
| `69d3f8ac8` | #1944 | [Enhancement] Always find Rock Sirloin (#1944) | M | defer |  |
| `c821e4a22` | #1938 | Add 'Dash After Roll' enhancement (#1938) | M | defer |  |
| `d14c98863` | #1681 | Merge pull request #1681 from mckinlee/extendedAC | M | defer |  |
| `9e0c8f2e2` | #1735 | Add Boss Health Multiplier option and fix Z-Targeting health bar overflow (#1735) | S | defer |  |
| `b41f365aa` | #1792 | Make swamp archery bonus speedup apply with misc interactions (#1792) | S | defer |  |
| `c12e3c014` | #1649 | Skip Misc Interactions: Kafei running to town (#1649) | S | defer |  |
| `bdd1b6709` | #1650 | Skip Misc Interactions: Sakon's hideout door opens (#1650) | S | defer |  |
| `604532f96` | #1663 | Skip Misc Interactions: Sakon's Hideout conveyor belt cutscene (#1663) | S | defer |  |
| `48c022c36` | #1659 | Skip Misc Interactions: Couple's Mask cutscene (#1659) | S | defer |  |
| `455c05459` | #1672 | Skip One Point Cutscene: Bell in Laundry Pool (#1672) | S | defer |  |
| `cd33453d8` | #1671 | Skip Owl Statue cutscene (#1671) | S | defer |  |
| `2220b84bc` | #1873 | Activate owl statue flag right away (#1873) | S | defer |  |
| `ed0eb99b0` | #1756 | Add restoration to re-bottle soil patch bugs (#1756) | M | defer |  |
| `8b9452056` | #1766 | Fix bean patch cutscene skips (#1766) | S | defer |  |
| `f8e213ced` | #1755 | [Enhancement] Port fast bottles from SoH (#1755) | M | defer |  |
| `d66aafc37` | #1816 | Fix SkipBottleCatch turning chests into ocarinas (#1816) | S | defer |  |
| `29fdd1db9` | #1600 | [Enhancement] Extra Powder Kegs (#1600) | M | defer |  |
| `55b70ec59` | #1698 | minor fix (#1698) | S | defer |  |
| `24df3b56c` | #1461 | Reduce amount of notifications from Auto Bank Deposit (#1461) | S | defer |  |
| `f7ccda83a` | #1442 | Deku Butler animation fix (#1442) | M | port-with-adaptation |  |
| `116801c67` | #1732 | Use Notification::Emit for saves and file drops (#1732) | S | port-with-adaptation |  |
| `04a1a4319` | #1718 | Lazy load sort DL Viewer unfiltered results (#1718) | S | port-with-adaptation |  |
| `4c128e4cd` | #1850 | Merge develop-keiichi->develop | S | defer |  |
| `2e211bf0a` | #1933 | Save Persistent Bunny Hood state in file info (#1933) | M | defer |  |

### M13. Large MM enhancements

Priority P3, effort XL (6 commits: Sx2 Mx2 Lx1 XLx1).

- **Prerequisites:** Bomb Arrows (dc051156c, 1338 lines) and Song Items (cfd5cc483) each need the hosting pattern plus live kaleido and `z_parameter` changes; Colour Pictograph needs a combo-level sidecar persistence path (one paired file).
- **Test impact:** redship tier; save-compatibility rows.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `dc051156c` | #1496 | [Enhancement] Bomb Arrows (#1496) | XL | defer | A 1338-line feature: equipping Bombs on a bow button fires bomb arrows, with HUD and kaleido ammo drawing, arrow-cycle integration and a persisted equip byte. |
| `2a1766e9d` | #1677 | Changed bomb arrow HUD visual (#1677) | S | defer |  |
| `f9426d06c` | #1697 | Merge pull request #1697 from mckinlee/bomb-arrow-cycle-consumption-fix | M | defer |  |
| `cfd5cc483` | #1566 | [Enhancement] Song Items (#1566) | L | defer | Lets players equip songs to C/D-pad buttons and auto-play them: a real QoL feature (522-line provider plus VB hooks in z_message.c, z_parameter.c and z_player_lib.c). |
| `d41e3af8b` | #1696 | Merge pull request #1696 from mckinlee/songitemssoftlock | S | defer |  |
| `5a0f17cbc` | #1484 | [Enhancement] Color Pictograph (#1484) | M | defer |  |

### M14. Cosmetics, models and the MM cosmetics editor

Priority P3, effort XL (7 commits: Sx1 Mx1 XLx5).

- **Prerequisites:** 5d96fd84f (dungeon-specific key models, xref family 8, the #1172 key half) and b439220de (flippers icon) are small and independent; the rest needs a decision on hosting MM cosmetics inside OoT's Cosmetics Editor (#1092 made it patch only under OoT) and, for f56c7939e, the LUS shader work (L9).
- **RSBS adaptation:** 5d96fd84f's toggle goes on an OoT-hosted row; the assets and `DrawItem.cpp` change port as they are.
- **Test impact:** redship tier; a visual pass.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `5d96fd84f` | #1881 | Add Dungeon Specific Keys (#1881) | M | port-with-adaptation |  |
| `b439220de` | #1586 | Add Flippers Icon (#1586) | S | port |  |
| `661dc37b6` | #1633 | Port SoH Cosmetics Editor UI (#1633) | XL | defer | Rewrites MM's cosmetics editor into SoH's UI style and re-keys every cosmetic call site. |
| `f56c7939e` | #1891 | Moar Cosmetics (#1891) | XL | defer | Large cosmetics expansion: new groups, silly sizes, flat shading, rain everywhere, rupee and NPC tints, and a DrawItem.cpp rework. |
| `ff235099f` | #1617 | Dynamic Cosmetics Editor for Custom Models (#1617) | XL | defer |  |
| `ed47d2ec9` | #1793 | Fix mod load time regression (#1793) | XL | defer |  |
| `3517973e9` | #1592 | [Rando] Added option to randomize all cosmetics on seed gen (#1592) | XL | defer |  |

### M15. Visuals under frame interpolation

Priority P2, effort S (2 commits: Sx2).

- **Prerequisites:** None (#1091 made interpolation work in MM). The texture-scroll interpolation (a4360a25c) is in L10.
- **RSBS adaptation:** `MM_Matrix_GetCurrent` and `MM_SkinMatrix_MtxFMtxFMult` prefixes, which the file already uses.
- **Test impact:** redship tier; a visual pass at 60 fps.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `38cee8c92` | #1771 | Merge pull request #1771 from garrettjoecox/fix-some-shadows | S | port-with-adaptation | Fixes actor shadows popping or warping under frame interpolation when ActorShadow_Draw's isotropic check flips between frames. |
| `63fde12a1` | #1832 | Fix deku baba stick interp issue (#1832) | S | port |  |

### M16. Traps, junk cycling and wallets

Priority P2, effort L (8 commits: Sx1 Mx4 Lx1 XLx2).

- **Prerequisites:** 254cc60e1 first, then 58eef5d93 and a7779a2c0. 6eda20817 (Tycoon's Wallet, 5000 rupees) needs a decision under the shared wallet tier (#1058), where OoT's tycoon tier is 999. 31f902da3 and 5bb90c25c are decided against: PR #1102 added Moon Crash Behavior (`gRando.Options.MoonCrash`, frozen as `randoSaveMoonCrash`, carried out by `Sram_MoonCrashNewCycle`), whose default is Last Save (OoTMM's `reset`) with New Cycle opt-in or forced by Shared Song of Time; a save-on-crash mode is not among its values.
- **RSBS adaptation:** Traps disguise themselves as unobtained major items; the disguise pool must include OoT-hosted items for a paired world. 17524f083's wallet hunks assume the tycoon tier context.
- **Test impact:** MM goldens re-pin (junk is rolled per check from the seed); rando tier; the trap-type purity rows of #1049.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `254cc60e1` | #1452 | Refactor junk cycling and trap models (#1452) | L | port-with-adaptation | Junk is rolled per check from a seed and only from currently obtainable junk, so ammo the player cannot hold is never shown; a Static mode is optional. |
| `58eef5d93` | #1489 | Add static trap option (#1489) | M | defer |  |
| `a7779a2c0` | #1795 | [Rando] Improve fake item names for traps (#1795) | S | defer |  |
| `17524f083` | #1605 | vroom (#1605) | M | port-with-adaptation |  |
| `8775c32bb` | #1794 | Add rando toggle for infinite Rupee purchases (#1794) | M | port-with-adaptation |  |
| `6eda20817` | #1597 | [Rando] Tycoon's Wallet (#1597) | XL | defer | Adds a third MM progressive wallet: tier 3 holds 5000 rupees, gets a 4-digit purple HUD counter through new VB hooks, and comes with pool/convert/give/remove support. |
| `31f902da3` | #1480 | [Enhancement] Save Game on Moon Crash (#1480) | XL | defer | Saves end-of-cycle progress on moon crash, the way Song of Time does, and upstream turns it on automatically for glitchless rando. |
| `5bb90c25c` | #1521 | Move before moon crash hook (#1521) | M | defer |  |

### M17. Audio

Priority P3, effort M (2 commits: Mx2).

- **Prerequisites:** 818e7a6bb with SoH 9308ba794 (O12).
- **RSBS adaptation:** 62d60e699's categories live in MM's `AudioCollection`; the shared Audio Editor rows (#1087) are where the toggle belongs.
- **Test impact:** redship tier.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `62d60e699` | #1823 | Add categorized sequence randomization (#1823) | M | port-with-adaptation | It gives every vanilla sequence category flags (field/town/dungeon/boss/fanfare kinds...) and parses numeric category suffixes on custom sequences. |
| `818e7a6bb` | #1863 | 16 Bit Sound Font Ids (#1863) | M | defer | Widens MM sound-font ids to 16 bits, adds CRC64 font references for custom/streamed sequences, and makes permanentEntries dynamically sized with a bounds check. |

### M18. Native MM trackers (reference only)

Priority P3, effort S (3 commits: Sx1 Mx2).

- **Prerequisites:** None.
- **RSBS adaptation:** The Combo Tracker (#1149, #1154) replaces MM's native tracker; take only 079f44bae's Clock Town stray-fairy flag fix and null-texture guards, and 8e7ecc2e7's Trade Items grouping as a template for the combo rows (#1143).
- **Test impact:** redship tier; UI snapshot of the Combo Tracker.
- **Note:** Xref #1127 and #1131 name 079f44bae as the reference for scene folding.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `079f44bae` | #1492 | Check/Item Tracker improvements (#1492) | M | port-with-adaptation | Three changes in one commit. |
| `e74c7abb3` | #1508 | Menu tweaks & Fix Triforce not lighting up in tracker (#1508) | S | port-with-adaptation |  |
| `8e7ecc2e7` | #1831 | Adjust visibility of item tracker numbers & add trade items group (#1831) | M | port-with-adaptation |  |

### M19. MM infra and docs

Priority P3, effort S (4 commits: Sx4).

- **Prerequisites:** The macOS CI items only when that job is re-enabled.
- **Test impact:** None.

| Upstream sha | PR | Subject | Effort | Finder's call | Why |
|---|---|---|---|---|---|
| `b68071cf8` | #1635 | Python script to compare source files with decomp (#1635) | S | defer |  |
| `8c03f9c44` | #1638 | Upgrade GH actions to latest versions (#1638) | S | defer |  |
| `59121d713` | #1607 | Simplify macOS caching/deps (#1607) | S | defer |  |
| `862c7d802` | #1610 | More cache tweaks (#1610) | S | defer |  |

## Spot-check results (2026-10-26)

Three read-only agents checked 47 claims that decide the top bundles' priority, reading our tree at `7c1aecf9` and the upstream diffs. ABSENT means the upstream change is not in our tree; PRESENT names where it is; PARTIAL names the half. Every path is ours.

| Bundle | Upstream sha | Verdict | Our evidence | Note |
|---|---|---|---|---|
| O26 | SoH f00f5b625 | ABSENT -> PRESENT (current wave) | `games/oot/CMakeLists.txt:48-56, :592-596`; `build-cmake/build.ninja` | OoT objects compile with `/Oi /Gy /W3` and no `/O2`; MM and libultraship get `-O2`. `NDEBUG` still reaches OoT. |
| O11 | SoH 5ecff16bd | ABSENT -> PRESENT (#1323) | `games/oot/src/code/graph.c:540-546`; `z_vr_box.c:455-470` | The first gamestate (TitleSetup) is cast to PlayState for `OoT_Skybox_Setup`; the overrun repeats on every switch back into OoT. |
| O11 | SoH e9268c446 / f2c34d8c1 | ABSENT / PARTIAL | `games/oot/src/code/z_room.c:416-418` | The camId guard exists only under NoRestrictItems or CrowdControl; no `AlwaysOnFixes.cpp` anywhere. |
| O11 | SoH 0a946e5b9 | PRESENT (stronger) | `z_kaleido_scope_PAL.c:1003` | `gItemAgeReqs[ITEM_NONE + 1]` from PR #985. |
| O11 | SoH 29c7e7568 | ABSENT -> PRESENT (#1323) | `games/oot/src/code/z_parameter.c:1897-1899, :2475` | `SLOT()` indexes the unsized `OoT_gItemSlots` with any id; #1048 sized `sItemActions`, not this. |
| O11 | SoH 7a7f942b8 | ABSENT -> PRESENT (#1323) | `randomizer_item_tracker.cpp:827, :648` | `INV_CONTENT(item.id)` with no bound, fed `RG_` ids; the 2026-10-18 verifier filed it as ported. |
| O12 | SoH b6bf97e2f | ABSENT (both games) | `audio_synthesis.c:899-903`; `soh/mixer.c:107,111`; MM `synthesis.c:1209-1227` | ADPCM load pad and `ROUND_DOWN_16` unchanged. |
| O12 | SoH 4e39d06ac | ABSENT -> PRESENT (#1323) | `audio_heap.c:1012-1021`; MM `heap.c:1107-1110` | `permanentCache[32]`, unbounded write. |
| O12 | SoH fdb7e194c / 945f70222 / b9bcc5e90 | ABSENT -> PARTIAL (#1323: fdb7e194c, 945f70222) | `audio_load.c:341-342, :379-381, :1042-1055`; `audio_playback.c:98,124` | 945f70222 is OoT-only (MM `load.c:805` assigns `realId`); the audio thread reads CVars in five places. |
| O1 | SoH 9991a95ab | ABSENT -> PRESENT (#1323) | `3drando/item_pool.cpp:855, :859` | `AddFixedItemToPool(..., false)`: the `false` is the count. |
| O1 | SoH c360b2dee / 0f948efb5 | ABSENT -> PRESENT (#1323) | `item_pool.cpp:183-184` | No Farore's Wind row; Nayru's Love still `2, 1, 0, 0`. |
| O1 | SoH 60205ec0b | ABSENT -> PRESENT (#1322) | `item_list.cpp` (no `BEAN_SOUL`); `draw.cpp:953`; `item_pool.cpp:564-575` | Rows missing outright while the souls are still added to the pool. |
| O1 | SoH 65c7c3e4a | ABSENT -> PRESENT (#1223) | `item_pool.cpp:889-892`; `shared_items.h:744-748` | No assertion breaks on a port; prose and the `SqpShippedShape` fixture go stale. |
| O3 | SoH 2f262f492 / af485bf64 | ABSENT -> PRESENT (#1181) | `3drando/hints.cpp:525, :470-471`; `hint.cpp:254` | Off-by-one on `junkIdx`; required trials in an unused local; `namesTemp` reset per location. |
| O3 | SoH 0dcd52e5c / ea76550fc | ABSENT -> PRESENT (#1181, current wave) | `Messages/StaticHints.cpp:186-192`; `death_mountain_trail.cpp:60` | Both reached develop through the merge `35a2ce4bf`. |
| O4 | SoH 0702b11b9 / c906c71f7 / 1413ceb33 / 2bc50f605 | ABSENT -> PRESENT (#1178, #1205) | `haunted_wasteland.cpp:26`; `lon_lon_ranch.cpp:14`; `thieves_hideout.cpp:15,33,57,71`; `location_list.cpp:223-224` | The Thieves' Hideout speak events exist; only the rows need switching. |
| O6 | SoH 5e802f232 / 78e3683ef / 6f01d68c3 / 04eb8d160 | ABSENT -> PRESENT (#1323, current wave) | `zoras_river.cpp:64`; `dodongos_cavern.cpp:63-64`; `ganons_castle.cpp:35,358`; `forest_temple.cpp:110` | Each a one-line region-table error; the MQ twins already have the right form. |
| O7 | SoH bdccc868b / fe78ae21a | ABSENT -> PRESENT (#1323) | `entrance.cpp:1710-1714`; `randomizer_entrance.c:152,165,209` | The boss-door savewarp match never fires (`index` is a stale Hyrule Field loop variable). |
| O8 | SoH cc63cc4ab / c42004bc4 | ABSENT -> PRESENT (#1323) | `3drando/fill.cpp:474, :479`; `logic.cpp:2298-2301` | Missing `!`; packs grant chus without the bag checks. |
| O9 | SoH a7cd6c811 / 69a7cdb17 | PARTIAL / ABSENT -> PRESENT (#1323, current wave) | `randomizer.cpp:3476, :3558-3579`; `OTRGlobals.cpp:2310-2331` | The file select reaps a finished worker (#1044) but nothing waits on load, and `DeinitOTR` never joins the thread. |
| O20 | SoH 38e684fda | PRESENT | `Messages/MerchantMessages.cpp:237` | PR #1074 reads `RSK_SCRUB_TEXT_HINT`; nothing to port. |
| O26 | SoH 87db67588 | ABSENT -> PRESENT (current wave) | `logic.cpp:1795,1806,1877,1925`; `fill.cpp:458,603` | Four `std::map` lookups with `.at()`; no early skip, no bitset. |
| O18 | SoH 2a335b1cd | PARTIAL | `ConfigMigrators.h:1550-1559`; `z_player.c:7159-7171` | The v6 updater writes `gCheats.SpeedModifier.*`; nothing reads it; a pre-v6 config loses the settings. |
| L1 | LUS 62d22224 / c5a5a934 / f7600e02 / 67861a90 / 46bdbdd1 | ABSENT | `libultraship/src/fast/interpreter.cpp:408-479, :483-808, :249-255, :4547-4559`; `gfx_opengl.cpp:930` | None of the five is in the fork. |
| L1 | LUS a93ee524 | PARTIAL | `gfx_opengl.h:105-120`; `interpreter.cpp:65` | The vector half is fork `477633a`; `TEXTURE_CACHE_MAX_SIZE` is still 500. |
| L2 | LUS d4f9b849 / 766a6bad / 7cb10226 / 7f2baa10 | ABSENT | `Config.cpp:213-217`; `CrashHandler.cpp:205-207`; `WasapiAudioPlayer.cpp:76-164` | In-place config write; `exit(1)` in the POSIX signal handler; no WASAPI destructor or mutex, and `Release()` does `delete this` under a `shared_ptr`. |
| L3 | LUS 8a67fa4b / 61bf1fbe | ABSENT | `Gui.cpp:200-207`; `gfx_dxgi.cpp:509-511` | No `SetGamepadMode`; the background-joystick hint is set unconditionally (RSBS overrides it for tests only). |
| LB | fork pin | see note | gitlink `fcbbc18c` (2026-10-21) | Contained only in `origin/claude/unload-filter-owner`, 22 ahead of the fork's `main` (`6db32f6`, 2026-01-17); remote refs as of 2026-10-21, no fetch run. The worktree's submodule was not initialised; the main checkout's copy at the same sha was read. |
| M1 | 2S2H 26146e975 / f86e04b4d / d6ac2550b | ABSENT -> PRESENT (current wave) | MM `z_player.c:13506`; `z_en_test3.c:559`; `games/oot/soh/resource/type/Skeleton.cpp:62-104` | MM Link registers into OoT's patcher and dangles until `ClearSkeletons`; neither patcher dedupes; PR #1092 touched neither file. |
| M2 | 2S2H bb1c03b60 / 5edc11259 / 16ef59bae / c2f50b3cc / 7f3c8327f / 385f5c075 | ABSENT -> PRESENT (#1317, #1323) | `z_en_ot.c:63-73`; `z_en_attack_niw.c:400`; `z_en_test4.c:61-128`; `z_sram_NES.c:1641`; `z_en_nb.c:500`; `BenPortHudSingleExe.cpp:67`; `z_player_lib.c:2487,2552` | The live single-exe HUD copy wraps; the owl-save and Deku-hookshot cases have low reach today. |
| M3 | 2S2H cc5d87143 / 9b699dee1 / 9224b9f48 / bcc562a3e / d80a5cf2e / f237dd293 | ABSENT -> PRESENT (#1178, #1323) | `Central.cpp:403`; `TimeLogic.cpp:62`; `SnowheadTemple.cpp:41`; `South.cpp:288`; six `CAN_USE_PROJECTILE` rows; `TerminaField.cpp:123,184` | Every logic row the finder named. |
| M3 | 2S2H 29e30e6b4 | ABSENT | `Rando/Types.h:3162-3163`; `z64save.h:389` | Inserts `RE_MAIL_LETTER_TO_KAFEI` mid-enum; `randoEvents` is `u8[RE_MAX]` in the raw MM tier, so append instead. |
| M3 | 2S2H e07b6f731 | ABSENT -> PRESENT (#1323) | `ShipUtils.cpp:331-343`; `StartingItems.cpp:59`; `EnGs.cpp:169,211,508` | `Ship_Random` is exclusive and the call sites pass `size - 1`: Night 3 is never the random start, the last hint is never picked. |
| M4 | 2S2H cb0bab40d / 5fa55eb05 / 99ff06284 | ABSENT -> PARTIAL (#1323: 5fa55eb05; current wave: cb0bab40d shop half) | `EnGirlA.cpp:183-194`; `Traps.cpp:378`; `GeneratePools.cpp:44-48`; `z64save.h:456` | Exclusions are a per-ordinal bitset here, not names. |
| M5 | 2S2H 4661df2e6 (precedent) | PRESENT | `fabe7d33` (#1125); `Types.h:2300-2353`; `src/common/save.h:61` | 53 `RC_` ids and `RO_SHUFFLE_TREE_DROPS` appended, the Gorman trees left out, no `RSBS_SAVE_VERSION` bump. |
| M7 | RO growth precedent | see note | `git log -S 'RO_'`; `z64save.h:424`; `combo_mm_options_view.h:352-360` | Only `fabe7d33` ever appended an `RO_` id; #1047 and #1102 chose separate appended fields instead. |
| M10 | 2S2H b1edee5ff | ABSENT | `z_common_data.c:35`; `z_sram_NES.c:2089,2097`; `mm_save_manager_stubs.c:59-64` | MM always plays Switch targeting; nothing maps OoT's `zTargetSetting`. |
| M10 | 2S2H eed759261 / 937292289 / 7f67743b3 / 525c1bc62 | ABSENT | none in `games/mm` | No TU, row or converged key for any of the four. |
| M16 | 2S2H 31f902da3 | decided against | `combo_mm_options_view.h:348-351`; `z64save.h:436`; `z_sram_NES.c:1420,1457` | #1102's Moon Crash Behavior: Last Save by default, New Cycle opt-in; no save-on-crash mode. |

## Changed since the finder snapshot

`main` moved 80 commits between the finder snapshot (`96b8ce9a`, 2026-10-18) and this document (`7c1aecf9`). The records below changed status because of them; everything else in the finder data was spot-checked only where the bundle sections say so.

| Record | What changed | Bundle |
|---|---|---|
| SoH `0a946e5b9` (Roc's Feather: avoid OOB access to gItemAgeReqs (#6297)) | covered by PR #985 (`acdae0c1`): gItemAgeReqs sized to every u8 ItemID | O11 |
| 2S2H `4661df2e6` (Tree Shuffle (#1346)) | ported by PR #1125 (`fabe7d33`): Shuffle Tree Drops, with the save growth accepted as the v0.3.0 save break | M5 |
| SoH `38e684fda` (Fixed scrub text checking merchant hint setting (#6378)) | covered by PR #1074 (`ab767fd9`): the scrub's offer textbox follows Scrub Hint Text | O20 |
| 2S2H `31f902da3` ([Enhancement] Save Game on Moon Crash (#1480)) | decided against by PR #1102 (`ab4ecc6a`): Moon Crash Behavior with Last Save as the default and New Cycle opt-in, the OoTMM way; no save-on-crash mode | M16 |
| 2S2H `22518984e` (Remove Gorman Track trees from shuffle (#1511)) | included in PR #1125 (`fabe7d33`): the 25 Gorman Track trees were never added | M5 |
| SoH `7a7f942b8` (Fix item tracker fishing pole & roc's feather (#7103)) | the opposite move: the 2026-10-18 verifier filed it as ported, but the `INV_CONTENT(item.id)` bound is absent and the read is live (`randomizer_item_tracker.cpp:827,648`); it is in O11 (since ported in #1323) | O11 |

Three later PRs changed the frame rather than a record: #1082, #1093, #1079 and #1119 established the MM enhancement hosting pattern that the 2S2H finder records call a "liveness decision" (so those prerequisites are now "follow the pattern"); #1091 made frame interpolation work in MM through the shared gfx processor (so O17 and M15 are plain ports); and #1125 is the precedent, and the warning, for MM save-tier growth (M5, M6, M7).

### Landed between 7c1aecf9 and 2026-10-30 (PRs #1177–#1325 + current backport wave)

Between the `7c1aecf9` audit baseline and 2026-10-30, PRs `#1177` through `#1325` and the current backport wave landed the following upstream commits across the bundles above:

| Bundle | Landed on `main` (PRs `#1177`–`#1325`) | Landed in current backport wave |
|---|---|---|
| **O1** | `60205ec0b` (#1322), `65c7c3e4a` (#1223), `9991a95ab` + `c360b2dee` + `0f948efb5` (#1323) | — |
| **O2** | — | `d67dcb92f`, `7effe2891`, `29b9b4a5a`, `b15ac97de`, `585530f68`, `371ef6f84`, `04ccd608b` (current wave) |
| **O3** | `2f262f492`, `af485bf64`, `0dcd52e5c` (#1181); `983d04d36` (#1181, #1207) | `18bf4b315`, `317454574`, `c3ffaa918`, `99e60b81f`, `ba08197d3`, `49aa33038`, `98ec5519c`, `ea76550fc` (current wave) |
| **O4** | `0702b11b9`, `c906c71f7`, `1413ceb33`, `63597ec63` (#1178); `2bc50f605` (#1205); `c63f57850` (#1226) | `e6947c733`, `a393f48c7`, `a547d51ab` (current wave) |
| **O5 / O28** | `743103fbe`, `b6cdc6c49`, `fe5dce24b`, `ec27775e8`, `848106039`, `09b0fdbc9`, `ed4868f62`, `688913270`, `7be5a4fea` (#1291); `60ec65592` (#1318) | — |
| **O6** | `041295f05`, `2bfae47b7` (#1290); `04eb8d160`, `5e802f232`, `6f01d68c3`, `7eb5acf22`, `e987cdae2`, `1e1ba54cf`, `210c5d588`, `293adcf9e`, `f3d881144`, `abcb3ad94`, `adf31d5eb` (#1323) | `ff0209e76`, `f04b570d2`, `78e3683ef`, `c3c80df37`, `94a02dd53`, `45b932159`, `19df71112` (current wave) |
| **O7** | `fe78ae21a`, `bdccc868b` (#1323) | — |
| **O8** | `cc63cc4ab`, `c42004bc4` (#1323) | `483849b97` (current wave) |
| **O9** | `69a7cdb17`, `4c43ef030`, `eab279c0b` (#1323) | `a7cd6c811`, `e4b6f1ab8` (current wave) |
| **O10** | `5875ed880` / `7cdf69b20` (#1211) | `a35f33118`, `4a875dca4`, `b83ca39c9`, `b043cdf40` (current wave) |
| **O11** | `5ecff16bd`, `29c7e7568`, `7a7f942b8`, `e9ef09eee`, `92afa8144` (#1323) | `e4f019f56`, `02c7632a3` (current wave) |
| **O12** | `4e39d06ac` (OoT & MM), `fdb7e194c`, `945f70222` (#1323) | — |
| **O13** | `7cfab16ba` (#1292, #1312) | `d54a69833` (current wave) |
| **O14** | — | `e6d6b6d7d`, `f31172c64`, `07414fd72`, `6e6fee887`, `a1aa04681`, `1133ae221`, `3a12eeaa7`, `de170e974` (current wave) |
| **O15 / M1** | `bbc76212f` (#1177), `a221a8c08` (#1197) | `26146e975`, `f86e04b4d`, `d6ac2550b` (current wave) |
| **O19 / M18** | `#1180` (`2845baad6`, `2ba368e7e`, `6bf5ec514`), `#1188`, `#1192`, `#1202`, `#1249`, `#1263`, `#1293` (`079f44bae`), `#1309`, `#1325` | — |
| **O20** | `983d04d36` (#1207), `31ac33a7a` (#1324) | — |
| **O21** | `fb8126780` / `6340ad3d5` (#1212) | `86fdb9bfe`, `6a9567b36` (current wave) |
| **O26** | — | `f00f5b625`, `87db67588`, `b939163c3`, `e447143e4` (current wave) |
| **L2** | `0be75d87f` (#1199), `c429f3595` (#1319), `4b68e3197` (#1320) | — |
| **M2** | `16ef59bae` + `96611c3b3` (#1317); `bb1c03b60`, `5edc11259`, `385f5c075`, `c2f50b3cc`, `b2872844e`, `7f3c8327f` (#1323) | `86d78d17f`, `5c35f92ef` (current wave) |
| **M3** | `cc5d87143`, `9b699dee1` (#1178); `9224b9f48`, `bcc562a3e`, `d80a5cf2e`, `f237dd293`, `180318962`, `c0c746334`, `e07b6f731` (#1323) | `a960c7f44`, `99204fb69` (current wave; `b39664f88` verified not applicable under RSBS single-bag bomb-bag pool / `#737` contract) |
| **M4** | `5fa55eb05` (#1323) | `cb0bab40d` (shop half), `c03a59dae` (current wave) |
| **M10 / M12 / M14 / M15 / M16 / M17** | `#1213` (`5d96fd84f`), `#1242`–`#1271` (hosted MM Enhancements, Audio Editor, Cosmetics, Mirrored World `007667bdb`), `#1297` (`6eda20817` 999-rupee Tycoon wallet) | `b3dcb18a9`, `3d3a01887`, `dfa4a6a95`, `38cee8c92`, `63fde12a1` (current wave) |

## Do not port

After the 2026-10-18 verifier pass, 269 records carry the call `skip`: 62 are ported (including the 7 the verifier re-filed), 102 are not applicable to this project (upstream CI, root build files, platforms we do not ship), and 105 are absent or partially ported records that are not worth taking. The absent ones fall into these groups. Each sha is listed once; the finder's reason is paraphrased.

**Decomp-naming syncs (no behaviour change; MM symbols collide)**

SoH `1003c462a` Decomp pull, some `z_player` and global changes (#6908); SoH `29d4ff420` Decomp, update AudioOcarina (code_800EC960.c) (#6941); SoH `5a57a0cbc` Decomp, naming (OnePointCS, itemAction, blure, wall/floor types) (#7007); SoH `f2f4413bf` Decomp, some camera names (#7043); SoH `e048afd10` Decomp, three audio functions (#7069).

**Include hygiene, warning sweeps and layout-only refactors on diverged files**

SoH `0d2346d32` Reduce LUS header bloat & improve build speed (#6802); SoH `633e92096` Cleanup unused includes & remove remaining LUS umbrella includes (#6813); SoH `ffb741b1d` remove z64.h from gameinteractor.h (#6903); SoH `ce969cab9` clean up includes (#7114); SoH `060787a23` clean up headers (#7194); SoH `5a7bc9a45` Remove unnecessary includes of OTRGlobals.h in actor overlays (#6306); SoH `b65c1c831` Remove some unnecessary includes (#6385); SoH `940e3c569` More pragma once (#7081); SoH `2f32abb51` Console include cleanup (#6413); SoH `8c4d4738c` cleanup customequipment.cpp (#6396); SoH `8470f41c2` Increase warnings in Windows and fix most of them (#5858); SoH `c642e8c4d` Remove all warnings on Windows and treat them as errors (#6733); SoH `bc1252171` Silence warnings on Windows for dependencies, bump torch (#7082); SoH `e44bd8d98` prefer spdlog over luslog in C++ code (#7149); SoH `7f9d453ff` use spdlog::fmt_lib over fmt, opens up SPDLOG_USE_STD_FORMAT in future (#7030); SoH `29285ed20` Move AddElementsToPool/ElementInContainer to ShipUtils as generic utilities (#6948); SoH `fa875596f` pool_functions cleanup (#6536); SoH `f3d60e0dd` use bool over s8 (#6547); SoH `b341e02cd` Remove unused field `progressive` from Item (#7113); SoH `bb6d1d3b4` Cleanup mExcludeLocationsOptionsArea (#7131); SoH `896bf97a4` Move Tracker namespace to its own file (#7135); SoH `45449623b` MAX_MQ_DUNGEON_COUNT (#6353); SoH `c879c9706` randomizer_check_tracker: small cleanup (#6480); SoH `8e1f1aea8` Fix location list (#6276); SoH `22426a812` Resolve the Fast3dGui cast once per scope instead of per call (#7095); SoH `f3413bfd2` Remove REMOTE_CONTROL flags while building (#6732).

**Hookify-only refactors with no later consumer here**

SoH `c5835669f` Hookifying Various Cheats and FixVineFall (#6827); SoH `aedae12e6` Hookify DropsDontDie, NoFishDespawn, NoBugsDespawn (#6513); SoH `2d962bbb8` Split up mods.cpp (#6677); SoH `a4901abdc` Hookify Skeleton Unregisters (#6988); SoH `aff03cd7b` Hookify Mweep cutscene options (#7199); SoH `ae6204891` Hookify three Glitch Restoration options (#7200).

**Speedrun mode and glitch restorations off-theme for a combo randomizer**

SoH `4afcb7833` speedrun mode (#7073); SoH `3d8cd5aa8` Remove FQwc preset. Skip learning song; SoH `ecd889c20` speedrun presets: no BSM, always allow ATE (#7325); SoH `2b423c36a` GIM Restoration (#6865); SoH `e579e7b9a` Polishing, climb everything door opening + sloped ledge climb up (#6936); SoH `38acd5a17` Intro Skip: adjust Link's position & camera to match values after watching intro (#7077); SoH `7c42a63ce` Add the 2 "decoy" crates as NL crates (#6672).

**Anchor-only netplay fixes while the client is not lit**

SoH `54d7ed365` [Bug Fix] Fix Dark Link Crash/Missing Trails Bug (#6720); SoH `319207e79` Co op syncing and tower collapse softlock fixes. (#6684); SoH `aedddc21e` Sync's Ganons Trials and Fixes Softlock (#6828); SoH `f59d9172d` Desync the great fairy cutscene (#6860).

**Developer UI with no effect in the single executable**

SoH `034bf5f3c` Swap Logic combobox for No Logic checkbox. (#7196); SoH `0498d3642` Save Value Viewer Settings (#6392); SoH `88625c435` Add most values from gz to Value Viewer (#6411); 2S2H `155e8a9ae` Expose rando inf flags in save editor (#1768); 2S2H `901fde33b` add a checklist for heart pieces and containers (#1826); 2S2H `7ad02c376` Add save editor function to grant available checks (#1956); 2S2H `4bbddb8c2` Add missing Beaver Brothers heart piece to Save Editor (#1977); 2S2H `c8843f94b` [Enhancement] Port multiple warp points from SoH (#1723); 2S2H `48efa501a` Add more controller binding options (#1466); 2S2H `5cbb0fc3e` Tweaks to speed modifier cheat (#1464); 2S2H `5701f1081` unify HUD/Cosmetic editor colors (#1519); 2S2H `e63c5a238` Add Icons to File Select for Rando Seed (#1540); 2S2H `c768e773d` Allow item tracker display without playstate, and fix icons rendering as ocarinas without playstate (#1477); 2S2H `1a5c90d14` [Enhancement] Port Extraction Flow, ImGui scaling, file perm check from SoH (#1709); 2S2H `214872e7f` Better handling of Grottos & Enemy Drops in the Check tracker (#1463); 2S2H `1fdc7ba6d` Increase save buffer. Again. (#1493); 2S2H `f35f790ac` Merge pull request #1779 from HarbourMasters/develop-keiichi.

**MM presets and reorders that would renumber persisted ids**

2S2H `37fa45903` Merge pull request #1783 from garrettjoecox/update-curated-preset; 2S2H `e3310fe1b` Update voyage preset (#1820); 2S2H `8ec3b5154` Resolve any sorting issues, cause we're using maps order matters (#1490); SoH `473a77bbb` Fix generation when Shuffle Speak disabled (#6206); 2S2H `66d027625` Gracefully handle old starting items config & allow empty starting items (#1475); 2S2H `b754231c3` Merge pull request #1628 from Eblo/nnl-cows.

**MM items with no effect in paired play**

2S2H `626f922c6` Fix GC US assets conflicting with N64 US HD assets (#1726); 2S2H `1323be1e3` Merge pull request #1784 from Jameriquiah/gi-scale-matrix; 2S2H `537327d20` Erase warning from Owl Save message when persistent option is on (#1632); 2S2H `d5d5ef3b4` [Difficulty Option] Shuffle Town Archery Octorok Position (#1447); 2S2H `3465b3143` Add extra frames to new day title card to simulate load time (#1787); 2S2H `6e0ff9071` Make transformation mask hint use GetLocationNameForHint (#1772).

**libultraship scripting, event bus and write APIs no game uses**

LUS `3d530e1d` Event System with Debugger (#1047); LUS `787ec0bd` Implemented dynamic loading (#1051); LUS `8b43ee75` Better Scripting (#1068); LUS `860fcbf2` Flip tcc scripting to opt-in (#1084); LUS `3bf0be68` Added a function to expose cleaning the shader cache (#1064); LUS `128b0730` Impl ResourceManager::WriteResource (#1013); LUS `430aedcc` Implemented LoadTextureFromResource for Gui Textures (#1012); LUS `d3c03761` decode G_SETKEYR/G_SETKEYGB/G_SETCONVERT into RDP state and route CENTER/SCALE/K4/K5 to dedicated combiner inputs (#1089); LUS `a2eb3fec` Keep StatsWindow fps rendered size stable (#1020); LUS `a441bd43` Fix G_EX_INVERT_CULLING to apply to all microcode types (#1091); LUS `b374560b` Merge pull request #1232 from buddingmonkey/lus/context-partial-init; LUS `a67ee37c` Fixed folders being detected as archives too (#992); LUS `f23c09af` Add AudioDma registry to clamp osPiStartDma reads to source blob bounds (#1035); LUS `21f82121` General code improvements: memory safety, const correctness, bug fixes (#1067); LUS `1a0d62a2` Improve error handling: replace raw HRESULT throws, eliminate silent catches (#1075); LUS `e0567360` Improve ResourceManager cache lookup performance (#989); LUS `9be01a68` Fix race condition in O2rArchive by using per-thread zip handles (#1050).

**libultraship and SoH documentation and CI that RSBS keeps its own way**

LUS `74776bf1` docs: Add Doxygen documentation to all public headers and bridge APIs (#1065); LUS `a293e22c` Add Javadoc comments to all Ship directory headers (#1077); LUS `f32f472d` Update licenses in main README; LUS `cce35884` Fixes glew license file link; LUS `f40cfd33` Adds prism-processor to readme.; LUS `4eadaf99` Added a porting documentation draft file (#1005); LUS `51a3f074` docs: sync Fast3D README, expand build instructions, fix CONTRIBUTING (#1071); LUS `7f55038d` docs: update README dependency list with tree structure and correct links (#1088); LUS `00a563b8` Add Windows test CI job (#1092); SoH `97eaa8b3e` clean up comments (#6369); SoH `d912fd2c0` docs: document clang-format 14 setup and add an opt-in pre-commit hook (#6741); SoH `afcfe4f7b` Format JSON, include CI check (#7071).

**No-op or already-covered fixes**

SoH `e7dce8e05` Keep StatsWindow fps rendered stable (#6368); SoH `9f61e635d` Fix displayed token count (#6428).

Also decided against by rulings rather than by the finder: 2S2H `31f902da3` and `5bb90c25c` (Save Game on Moon Crash; #1102 chose Moon Crash Behavior with Last Save as the default), SoH `2ba5c40da` (#7134; the verifier found it changes no behaviour), and SoH `eca767dcb` (Logical bunny hood; its central feature conflicts with the shared-masks rulings of 2026-10-18 and PR #1095). SoH `c00dd24ca` (Torch) and the ARM64 commits are decisions, listed under O37, not skips.

## Uncertain items

Records whose premise, value or fit the verifiers or the spot-check could not settle. Each needs a look in code before a lane takes it.

- SoH `3cfe4ddff` (Prevent despawning shuffled gerudo guard drops (#7183)): The finder says a shuffled Gerudo guard drop vanishes about 11 s after it drops; whether it can be recovered afterwards is unverified, and the fix needs a hook site we lack (`VB_ITEM00_TIMER_TICK` from aedae12e6).
- SoH `27d35e5a9` (Fix logic issues noticed while rebasing doorsanity (#6510)): One of its region links over-reaches (`shadow_temple.cpp:344` sends Pre Boss Room to Beyond Boat); review each link rather than port the commit whole.
- SoH `bb6a80b48` (fix regression where gerudo guards stopped asking if you want to be jailed (#6879)): Whether `msgCtx.choiceIndex` is reset per textbox is uncertain; if it is not, friendly fortress guards act on a stale answer.
- SoH `61523391e` (Fix flare dancer crash (#7122)): Grows `OoT_gGfxPools` to 1M entries each; `Gfx` is 16 bytes on 64-bit, so the heap cost in the single executable must be measured first.
- SoH `257cf79b0` (fix invalid return in nayru's love uses (#7386)): Fixes a6be06874's hook; without that feature it fixes nothing. The verifier re-filed it as ported for that reason.
- SoH `0364f7076` (Fix seed generation crash (#7140)): Repairs regressions from 8ca5ca60a, which we do not have; the crash cannot happen here (verifier).
- SoH `fd696b5e5` (Fix ganondorf hint (#6789)): The bug does not exist in our tree; our `CreateStaticItemHint` already handles `RC_UNKNOWN_CHECK` its own way (verifier).
- SoH `46f775ef8` (fix a couple of small DMC oversights (#6784)): Our Death Mountain Crater table predates the #6294 refactor, so the oversights it fixes do not exist here yet; it belongs with O28.
- SoH `3249b9634` ([Bugfix] - First quiver starting item causes crash (#6889)): `RSK_STARTING_BOW` has no hits in our tree, so the crash it fixes is unreachable until O30's starting items land.
- LUS `0b76d895` (Fixed point filtering on metal (#1010)): Metal point filtering and a noise-alpha shader fix; the runtime effect is unverified and macOS is not built today.
- 2S2H `6bf849276` (Add checkbox to change Z-Targeting mode (#1919)): Whether a paired-session player can reach MM's own Z-targeting checkbox is uncertain; under one-game semantics M10 reads OoT's setting instead.
- 2S2H `385f5c075` (Bugfix, Deku Hookshot pull out crash (#1849)): A hard crash, but only reachable with MM's UnrestrictedItems cheat, whose key is parked and not in the registrar carve-out.
- 2S2H `99ff06284` (Improvements to excluded checks (#1817)): Our exclusions are a per-ordinal bitset; adopting name keying is a design change for the MM Locations page, not a port.

## Already present (highlights)

55 records are ported and 7 more were re-filed as ported by the verifier. The ones a planner is most likely to look for:

SoH `e2db315ff` Shuffle Speak; SoH `596b714fa` Shuffle Climb; SoH `6e6834af2` Shuffle Crawl; SoH `18b00e7bc` Shuffle Grab; SoH `04df48944` Shuffle Open Chest; SoH `44d351698` Shuffle Masks; SoH `f541c3f4a` Shuffle Ganon's Tower; SoH `f52b653cf` Pot CMC; SoH `7b3efb1e7` custom messages as hooks (#5101), ported as our `5ecce8f5`; SoH `5d8c3c888` the hints refactor (#6540); SoH `9ab58a8d0` rando objects not randomizing (#6150); SoH `80d3114f7` carpenters-start-free logic (#6661); SoH `79d6f54be` segfault on quit (#6212); SoH `96c4fef05` extraction task crash detection (#6386); SoH `d4d3e8bc0` scrolling texture interpolation (#6224); SoH `704ace8fd` the ImGui extraction flow (#5892); SoH `2845baad6` Identify checks for merchants and scrubs (#6641); SoH `2af265dbc` action shuffle icons on the item tracker (#6190); 2S2H `9801ddac4` Stone Tower Temple water logic (#1512), through our `473c030c`; 2S2H `144fbe555` Check Tracker improvements (#1368); 2S2H `e1bb65214` MM config updaters (#1703); 2S2H `c78278d14` the MM mod menu, as our own version (#706); 2S2H `b984640d1` menu background opacity, through the unified SoH menu.

Re-filed as ported by the 2026-10-18 verifier: SoH `46f775ef8`, `fd696b5e5`, `3249b9634`, `0364f7076`, `eca767dcb`, `257cf79b0` (each explained under "Uncertain items" or "Do not port") and `7a7f942b8`, which the 2026-10-26 spot-check moved back to absent (O11). Covered by RedShipBlueShip's own later work: SoH `0a946e5b9` (PR #985), SoH `38e684fda` (PR #1074), 2S2H `4661df2e6` (PR #1125).

## Suggested wave order

Each wave is a set of lanes that can run in parallel on disjoint files; a lane is one bundle unless noted. Every lane verifies its premise in code first, compiles only its own TUs, and is gated by the full local build plus both ctest tiers before merge, per the merge-gate directive. Goldens that move are re-pinned in a separate commit that names the cause.

**Wave 1: P0, no decisions, small.** O26's `/O2` line first and alone (one PR, full rebuild, both tiers, and a fresh baseline of frame and generation times for the other lanes to compare against). Then in parallel: O11 (creates `AlwaysOnFixes.cpp`), O1 with O3 and O4 as one rando lane (one golden re-pin), O9, O12, M1 with M2 (one MM lane), M3 (one MM rando lane, one re-pin that names `e07b6f731`), and L1 as the first fork PR after the fork's `claude/unload-filter-owner` branch is merged into its `main`.

**Wave 2: P1 correctness.** O2, O5 with O6 and O7 (one lane, one re-pin), O8, O10, O13, O14, O18 (the speed-modifier fix first), M4, M10 (the Z-target convergence first); fork PRs L2 and L3 with their SoH rows.

**Wave 3: P2 improvements and the prerequisites.** O27 as its own lane before anything else in this wave that touches region tables; then O15, O16, O17, O19, O20, O21, O23, O25, O28 (after O27), M5 (after the growth decision), M7 (after the growth decision), M8, M9, M11, M15, M16; fork PRs L4, L5 and L10 with their game halves.

**Wave 4: P3 and decisions.** O22, O24, O29, O30, O31 to O37, M6, M12, M13, M14, M17 to M19, L6 to L9, L11, and the LB migration if the operator chooses it. None of these should start before its named decision is written down.

Gate notes for every wave: run the rando tier as well as the redship tier (the rando tier drives play-state paths the redship tier does not), keep test processes silent and on the left panel, and treat any red integration row as a regression signal, since no row is permanently red.

## Appendix A: candidate records not placed in a bundle

Every absent or partially ported record that the finder recommended `port`, `port-with-adaptation` or `defer` is in a bundle above, apart from the records that later commits moved ("Changed since the finder snapshot") and SoH `2ba5c40da`, which the verifier refuted. A planner who wants a single commit finds it by sha in the bundle tables.

