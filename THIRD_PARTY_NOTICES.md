# Third-party notices

RedShipBlueShip's own code is MIT (see [`LICENSE`](LICENSE)). It is built on top
of, and links against, a large amount of third-party work that keeps its own
license. This file inventories all of it.

Everything below was verified against this repository's tree at the commit that
introduced this file, and against each upstream's own license file — not from
memory and not from a package manifest. Where a claim could not be verified, it
is listed under [Unresolved license status](#unresolved-license-status) rather
than guessed at.

**Re-verified 2026-09-21** for the elections pass: the font `name` tables were
re-parsed from the shipped files; the upstream license text of every component
whose license this file describes as a choice was fetched read-only at the
revision actually in use; and GLEW's licensing was corrected (it is cumulative,
not a choice — see [Elections](#elections)). The mechanical part of this file is
now locked by a CTest row, `FontLicense`
(`src/common/tests/test_font_license.c`).

**No copyrighted Nintendo asset ships in this repository.** Game assets are
extracted at build time from original Ocarina of Time and Majora's Mask ROMs the
user supplies.

## Summary of the licensing picture

* Every component that is compiled into `redship`, or shipped beside it, is
  under a **permissive** license: MIT, MIT No Attribution, CC0-1.0, zlib,
  BSD-3-Clause, the PNG Reference Library License v2, the bzip2 license, or the
  SIL Open Font License 1.1 for fonts.
* Two components let the user **choose** which license to take them under. This
  project has chosen, and the choices are recorded with their upstream evidence
  under [Elections](#elections). Every other component offers no choice at all,
  and is used under the single license its upstream publishes.
* **No GPL, LGPL, AGPL or MPL code is present in the tree or linked into the
  binary.** The only copyleft references anywhere in this repository are three
  documentation passages (`docs/adr/0006-netplay-transport-scope.md:204`,
  `docs/adr/0007-grant-relay-netplay.md:59`,
  `docs/netplay-increment-1-spike.md:343`) that name GPL-3.0 prior art
  explicitly in order to record that it was **not** read, adapted or vendored.
* **One** component has an unresolved status and is called out in its own
  section: the Ship of Harkinian upstream publishes no license file at all. The
  second such item, the `Fipps-Regular.otf` font, was resolved on 2026-09-21 by
  deleting it — see [Unresolved license status](#unresolved-license-status).

## Inventory

### Vendored game ports (in-tree source trees)

| Component | Path in tree | License | Copyright | Upstream | License text in-tree |
|---|---|---|---|---|---|
| Ship of Harkinian (OoT port) | `games/oot/` | **None published upstream** — see [Unresolved license status](#unresolved-license-status) | not stated upstream | https://github.com/HarbourMasters/Shipwright | no |
| 2Ship2Harkinian (MM port) | `games/mm/` | CC0-1.0 (Creative Commons Zero v1.0 Universal, a public-domain dedication) | dedicated to the public domain by the 2Ship2Harkinian authors | https://github.com/HarbourMasters/2ship2harkinian | **yes** — `games/mm/LICENSE`, copied byte-for-byte from upstream `develop` on 2026-09-21 (7048 bytes, matching the size GitHub's contents API reports for that path) |
| 3drando (OoT-Randomizer derivative inside SoH) | `games/oot/soh/Enhancements/randomizer/3drando/` | MIT | Copyright (c) 2017 Amazing Ampharos | https://github.com/TestRunnerSRL/OoT-Randomizer | **yes** — `games/oot/soh/Enhancements/randomizer/3drando/LICENSE.md` |

Both game trees derive from the zeldaret decompilation projects
(https://github.com/zeldaret/oot, https://github.com/zeldaret/mm), which
likewise publish no license file. That inheritance is upstream's and is noted
here for completeness; it is part of the same unresolved status as SoH.

### Submodules

| Component | Path in tree | License | Copyright | Upstream | License text in-tree |
|---|---|---|---|---|---|
| libultraship | `libultraship/` | MIT | Copyright (c) 2022 kenix3 kenixwhisperwind@gmail.com | https://github.com/spencerduncan/libultraship (fork of https://github.com/Kenix3/libultraship) | **yes** — `libultraship/LICENSE` |
| Fast3D | `libultraship/src/fast/`, `libultraship/include/fast/` | MIT | Copyright (c) 2020 Emill, MaikelChan | vendored inside libultraship | **yes** — `libultraship/src/fast/LICENSE.txt`, `libultraship/include/fast/LICENSE.txt` |
| ZAPDTR | `ZAPDTR/` | MIT | Copyright (c) 2020 Zelda Reverse Engineering Team | https://github.com/spencerduncan/ZAPDTR (fork of https://github.com/HarbourMasters/ZAPDTR) | **yes** — `ZAPDTR/LICENSE` |
| OTRExporter | `OTRExporter/` | MIT | Copyright (c) 2022 Harbour Masters | https://github.com/spencerduncan/OTRExporter (fork of https://github.com/HarbourMasters/OTRExporter) | **yes** — `OTRExporter/LICENSE` |
| libgfxd (vendored in ZAPDTR) | `ZAPDTR/lib/libgfxd/` | MIT | Copyright (c) 2016-2021 glank (glankk@github.com) | https://github.com/glankk/libgfxd | **yes** — `ZAPDTR/lib/libgfxd/LICENSE` |
| TinyXML-2 (vendored in ZAPDTR) | `ZAPDTR/lib/tinyxml2/` | zlib | Original code by Lee Thomason (www.grinninglizard.com) | https://github.com/leethomason/tinyxml2 | **yes** — notice in the header of `ZAPDTR/lib/tinyxml2/tinyxml2.h` and `.cpp` |

### Vendored build scripts

| Component | Path in tree | License | Copyright | Upstream | License text in-tree |
|---|---|---|---|---|---|
| Automate-VCPKG | `CMake/automate-vcpkg.cmake`, `libultraship/cmake/automate-vcpkg.cmake` | MIT | Copyright (c) 2019 REGoth-project (Andre Taulien) | https://github.com/REGoth-project/Automate-VCPKG | **yes** — at the end of the file |

### Fetched and compiled at build time (CMake `FetchContent` / `file(DOWNLOAD)`)

These are not in the tree, but they are compiled into `redship` and their
notices must accompany a distributed build.

| Component | Declared at | License | Copyright | Upstream |
|---|---|---|---|---|
| Dear ImGui (v1.91.9b-docking, patched) | `libultraship/cmake/dependencies/common.cmake` | MIT | Copyright (c) 2014-2025 Omar Cornut | https://github.com/ocornut/imgui |
| stb_image (pinned `0bc88af4`) | `libultraship/cmake/dependencies/common.cmake` | MIT **or** public domain, at the user's choice — **MIT elected**, see [Elections](#elections) | Copyright (c) 2017 Sean Barrett | https://github.com/nothings/stb |
| dr_libs (pinned `da35f9d6`) | `games/oot/CMakeLists.txt:463`, `games/mm/CMakeLists.txt:36` | Unlicense (public domain) **or** MIT No Attribution, at the user's choice — **MIT-0 elected**, see [Elections](#elections) | David Reid | https://github.com/mackron/dr_libs |
| thread-pool (v4.1.0) | `libultraship/cmake/dependencies/common.cmake` | MIT | Copyright (c) 2024 Barak Shoshany | https://github.com/bshoshany/thread-pool |
| prism-processor (pinned `bbcbc7e3`) | `libultraship/cmake/dependencies/common.cmake` | MIT | KiritoDv | https://github.com/KiritoDv/prism-processor |
| StormLib (v9.25, optional — `INCLUDE_MPQ_SUPPORT`) | `libultraship/cmake/dependencies/common.cmake` | MIT | Copyright (c) Ladislav Zezula | https://github.com/ladislav-zezula/StormLib |
| libgfxd (optional — `GFX_DEBUG_DISASSEMBLER`) | `libultraship/cmake/dependencies/common.cmake` | MIT | Copyright (c) 2016-2021 glank | https://github.com/glankk/libgfxd |
| SDL_GameControllerDB (`gamecontrollerdb.txt`, downloaded and installed beside the binary) | `games/oot/CMakeLists.txt:907`, `games/mm/CMakeLists.txt:1061` | zlib | the SDL_GameControllerDB contributors | https://github.com/mdqinc/SDL_GameControllerDB |
| linuxdeploy (AppImage packaging tool only; not linked) | `CMake/Packaging.cmake:62` | MIT | Copyright (c) TheAssassin and contributors | https://github.com/linuxdeploy/linuxdeploy |

### External libraries linked from the system or vcpkg

Requested by `CMakeLists.txt:38` and
`libultraship/cmake/dependencies/windows-vcpkg.cmake`. On a static Windows
vcpkg build these end up inside the shipped executable, so their notices apply
to the distributed binary.

| Component | License | Copyright | Upstream |
|---|---|---|---|
| SDL2 | zlib | Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org> | https://github.com/libsdl-org/SDL |
| SDL2_net | zlib | Copyright (C) 1997-2026 Sam Lantinga | https://github.com/libsdl-org/SDL_net |
| GLEW | Modified BSD **and** MIT (**not** a choice — corrected 2026-09-21; see [Elections](#elections)) | Copyright (C) 2002-2007 Milan Ikits, Marcelo E. Magallon; Copyright (C) 2002 Lev Povalahev; Mesa 3-D: Copyright (C) 1999-2007 Brian Paul; Copyright (c) 2007 The Khronos Group Inc. | https://github.com/nigels-com/glew |
| GLFW | zlib/libpng | Copyright (c) 2002-2006 Marcus Geelnard; Copyright (c) 2006-2019 Camilla Lowy | https://github.com/glfw/glfw |
| zlib | zlib | (C) 1995-2026 Jean-loup Gailly and Mark Adler | https://github.com/madler/zlib |
| bzip2 | bzip2 license (BSD-style) | Copyright (C) 1996-2019 Julian R Seward | https://sourceware.org/bzip2/ |
| libzip | BSD-3-Clause | Copyright (C) 1999-2026 Dieter Baron and Thomas Klausner | https://github.com/nih-at/libzip |
| libpng | PNG Reference Library License v2 | Copyright (c) 1995-2024 The PNG Reference Library Authors | https://github.com/glennrp/libpng |
| nlohmann/json | MIT | Copyright (c) 2013-2025 Niels Lohmann | https://github.com/nlohmann/json |
| TinyXML-2 (vcpkg copy) | zlib | Lee Thomason | https://github.com/leethomason/tinyxml2 |
| spdlog | MIT | Copyright (c) 2016 - present, Gabi Melman and spdlog contributors | https://github.com/gabime/spdlog |
| libogg | BSD-3-Clause | Copyright (c) 2002, Xiph.org Foundation | https://github.com/xiph/ogg |
| libvorbis | BSD-3-Clause | Copyright (c) 2002-2020 Xiph.org Foundation | https://github.com/xiph/vorbis |
| opus | BSD-3-Clause | Copyright (c) 2001-2011 Xiph.Org, Skype Limited, Octasic, Jean-Marc Valin, Timothy B. Terriberry, CSIRO, Gregory Maxwell, Mark Borgerding, Erik de Castro Lopo | https://github.com/xiph/opus |
| opusfile | BSD-3-Clause | Copyright (c) 1994-2013 Xiph.Org Foundation and contributors | https://github.com/xiph/opusfile |
| OpenGL (system) | platform-provided; no bundled code | — | — |

### Fonts

Both game trees carry the same custom font set
(`games/oot/assets/custom/fonts/`, `games/mm/assets/custom/fonts/`). The license
column below is taken from each font file's own `name` table (name ID 13, the
license description) — not from a distribution page.

| Font | Path in tree | License | Copyright (name ID 0) | Upstream |
|---|---|---|---|---|
| Inconsolata Regular | `games/{oot,mm}/assets/custom/fonts/Inconsolata-Regular.ttf` | SIL Open Font License 1.1 | Copyright 2006 The Inconsolata Project Authors | https://github.com/cyrealtype/Inconsolata |
| Montserrat Regular | `games/{oot,mm}/assets/custom/fonts/Montserrat-Regular.ttf` | SIL Open Font License 1.1 | Copyright 2011 The Montserrat Project Authors | https://github.com/JulietaUla/Montserrat |
| Noto Sans JP Regular | `games/oot/assets/custom/fonts/NotoSansJP-Regular.ttf` | SIL Open Font License 1.1 | (c) 2014-2021 Adobe, with Reserved Font Name 'Source' | https://github.com/notofonts/noto-cjk |
| Press Start 2P Regular | `games/{oot,mm}/assets/custom/fonts/PressStart2P-Regular.ttf` | SIL Open Font License 1.1 | Copyright 2012 The Press Start 2P Project Authors (cody@zone38.net), with Reserved Font Name "Press Start 2P" | https://fonts.google.com/specimen/Press+Start+2P |

`Fipps-Regular.otf` used to be a fifth row here, with an unresolved status. It
was **deleted from both trees on 2026-09-21** rather than resolved; see
[Unresolved license status](#unresolved-license-status).

The SIL Open Font License 1.1 requires its text, and each font's copyright
notice, to travel with the font files (OFL 1.1 condition 2). Both are now in the
tree beside them: **`games/oot/assets/custom/fonts/OFL.txt`** and
**`games/mm/assets/custom/fonts/OFL.txt`**. Each names every font in the table
above with that font's `name`-table copyright line, and reproduces the OFL 1.1
body verbatim. Because this repository's custom-asset step packs everything under
`assets/custom/` into `soh.o2r` / `2ship.o2r` verbatim
(`OTRExporter/OTRExporter/Main.cpp`), the notice travels inside the shipped
archives as well as in the source tree.

**The two copies are byte-identical, and must stay that way.** They are not a
per-directory notice, and the reason is the archive layer rather than tidiness:
both archives are packed to the same path (`fonts/OFL.txt`) and are mounted into
one flat libultraship `ArchiveManager` in a single-executable build, where
resolution is last-added-wins with no priority field. A path carried by both
archives with different bytes therefore resolves differently depending on which
game booted first — issue #595. A first draft of these notices listed only each
directory's own fonts and `src/common/tests/test_curated_archive_order.c` caught
it. So one notice covers the whole shipped set, and the per-tree difference is
recorded in its "carried in" column instead. The byte-identity is asserted by the
`FontLicense` row, with that reason attached.

The copyright column above was re-verified on 2026-09-21 by parsing each shipped
file's own `name` table (IDs 0, 7, 13, 14). Two details worth recording: the
Montserrat copy in this tree states `Copyright 2011 The Montserrat Project
Authors`, where upstream's current `OFL.txt` says 2024 — the in-tree notice is
the one that governs the in-tree file, and is what `OFL.txt` carries; and Noto
Sans JP's name ID 7 asserts `Source is a trademark of Adobe`, which is reproduced
in `games/oot/assets/custom/fonts/OFL.txt` alongside its copyright line.

`src/common/tests/test_font_license.c` (CTest row `FontLicense`, `redship` tier)
asserts all of this mechanically: no `Fipps*` file in either fonts directory, the
expected fonts still present, an `OFL.txt` in each directory carrying the OFL 1.1
body and every covered font's notice, `games/mm/LICENSE` being the CC0 text, and
no source file naming the removed font.

## OoTMM

[OoTMM](https://github.com/OoTMM/OoTMM) is RedShipBlueShip's design reference
for cross-game randomization, and the source of MM trick names
(`packages/core/src/settings/tricks.ts`, per issue #578 and ADR 0010 D10).

* **License of record: MIT.** OoTMM's root `LICENSE` is verbatim MIT, 1072
  bytes, `Copyright (c) 2020-2022 OoTMM Team`.
* **Known discrepancy, verified 2026-09-17:** `packages/core/package.json:12`
  declares `"license": "ISC"`. ADR 0010 D10 already recorded this as
  near-certain scaffolding leftovers (the `author` and `description` fields in
  the same object are empty strings). The root `LICENSE` governs. This is
  recorded here so nobody rediscovers it; per standing operator policy, **no
  report is filed upstream about it.** ADR 0010 D10's text proposes such a
  report; that clause is superseded by the later no-upstream-reports directive
  and should be read as void.
* **What IS vendored, and where** (#578 part 1, landed 2026-09-20; this bullet
  replaces the earlier "nothing from OoTMM is vendored today"):
  * `games/mm/2s2h/Rando/StaticData/TrickIds.h` — the 85 `MM_*` trick KEYS of
    `packages/core/src/settings/tricks.ts`, read at `master` on 2026-09-20, in
    source order with the `MM_` prefix replaced by `MMRT_`.
  * `games/mm/2s2h/Rando/StaticData/Tricks.cpp` — those 85 rows' DISPLAY NAMES
    and TOOLTIPS, transcribed from the same file.

  Both files carry an ATTRIBUTION header naming OoTMM, the MIT license and the
  `Copyright (c) 2020-2022 OoTMM Team` line, per ADR 0010 D10. `MMRT_GBT_BOSS_KEY_ICE`
  and the area/tag classification axes in those files are ours, not OoTMM's.
  Nothing else from OoTMM is vendored: every other reference to it in
  `games/**`, `src/**` and `CMake/**` remains a prose comment citing precedent,
  and no OoTMM logic wiring was ported (the bindings are hand-authored against
  2ship's own `RandoRegionId` graph).
* MIT permits this with attribution. The MIT text is in this file under
  [Appendix: license texts](#appendix-license-texts); OoTMM's copyright line
  above is the line that substitutes into it.
* Per D10: algorithms **reimplemented from reading** OoTMM are not a port;
  world-data YAML/CSV or name tables taken wholesale are.

## mm-rando is not a source of any bytes or names

[mm-rando](https://github.com/az64/mm-rando) (the Majora's Mask Randomizer) is
**GPL-3.0**, and so is
[MMRecompRando](https://github.com/RecompRando/MMRecompRando). Neither is a
source of anything in this repository — not code, not data, not identifier
names, not logic-rule text, and not settings vocabulary. No file here is derived
from either project, by copying, translation, transcription or paraphrase, and
neither is read as a reference when authoring logic. The GPL-3.0 references in
`docs/adr/0006-netplay-transport-scope.md`,
`docs/adr/0007-grant-relay-netplay.md` and
`docs/netplay-increment-1-spike.md` exist solely to record that exclusion. This
is a deliberate licensing boundary: importing GPL-3.0 material would make the
MIT grant in `LICENSE` false. It must stay intact.

## Elections

Some upstreams do not publish one license: they publish two or more and let the
recipient pick. Picking is not optional in practice — a redistributor has to say
which one it took, or its own notice file is unreadable. This section is that
statement.

The rules this table follows, so that it can be audited rather than trusted:

* An election is recorded **only** where the upstream's own text, at the version
  this project actually uses, offers the option. The pinned revision is named.
* Where a choice exists, the **MIT-family or CC0 option is elected** — the most
  permissive one with an explicit grant, so the notice obligation is a copyright
  line rather than a public-domain theory that not every jurisdiction accepts.
* A single-license component is **never** relabelled. "MIT" appearing in the
  inventory for imgui or spdlog is not an election; it is their only license.
* Where no MIT/CC0 option is offered, the row says **no election available** and
  the required license text is carried instead.

### Elections made

| Component | Version in use | Options the upstream offers | Elected | Evidence (fetched read-only 2026-09-21) |
|---|---|---|---|---|
| stb_image | `stb_image.h` pinned at `0bc88af4de5fb022db643c2d8e549a0927749354` (`libultraship/cmake/dependencies/common.cmake:55`) | MIT **or** public domain (the Unlicense) | **MIT** | `LICENSE` at that commit, line 1: `This software is available under 2 licenses -- choose whichever you prefer.` Line 3 heads the elected branch: `ALTERNATIVE A - MIT License`, with `Copyright (c) 2017 Sean Barrett`. (Line 21 is `ALTERNATIVE B - Public Domain (www.unlicense.org)`, not taken.) |
| dr_libs (`dr_wav.h`, `dr_mp3.h`, `dr_flac.h`) | repository pinned at `da35f9d6c7374a95353fd1df1d394d44ab66cf01` (`games/oot/CMakeLists.txt`, `games/mm/CMakeLists.txt`) | the Unlicense (public domain) **or** MIT No Attribution | **MIT No Attribution (MIT-0)** | `LICENSE` at that commit, lines 1-2: `This software is available as a choice of the following licenses. Choose whichever you prefer.` Line 30 heads the elected branch: `ALTERNATIVE 2 - MIT No Attribution`. (Line 5 is `ALTERNATIVE 1 - Public Domain (www.unlicense.org)`, not taken.) The same two alternatives are repeated at the end of each `dr_*.h` header. |

MIT-0 is MIT with the attribution condition removed; its text is in the
[Appendix](#mit-no-attribution-mit-0). Electing it rather than the Unlicense
means dr_libs is used under an express grant, which is the same posture as every
other component here.

### No election available

Every other component in the inventory publishes exactly one license, so there
is nothing to elect. That negative claim was measured rather than assumed: on
2026-09-21 the upstream license text of each component below was fetched
read-only and scanned for choice-of-license language (`choose whichever`,
`choice of the following`, `available under 2 licenses`, `ALTERNATIVE n`, `at
your option`). Two components matched — the two elected above. Every other file
scanned as a single license. The one component not re-fetched this pass is
**bzip2**, whose canonical distribution is not a Git host this scan could reach
(`https://sourceware.org/bzip2/`); its single BSD-style license is carried from
the earlier verification and is listed below unchanged.

The ones where somebody might expect a choice, and why there is none:

| Component | Why no election | What is carried instead |
|---|---|---|
| GLEW | **Not a choice — the earlier "tri-licensed" wording in this file was wrong, corrected 2026-09-21.** `LICENSE.txt` is three *cumulative* notices over three bodies of code GLEW is assembled from: GLEW itself (Modified BSD, lines 1-28), the Mesa 3-D graphics library (MIT, lines 31-51) and Khronos Group material (MIT-style, lines 54-73). There is no "choose whichever you prefer" sentence anywhere in the file, so all three apply at once. | Both the BSD-3-Clause and MIT texts in the [Appendix](#appendix-license-texts), plus all four copyright lines in the inventory table. |
| GLFW | "zlib/libpng license" is **one** license under two names, not two options. `LICENSE.md` states a single set of terms under Marcus Geelnard's and Camilla Löwy's copyrights. | The [zlib License](#zlib-license) text. |
| SDL2, SDL2_net, zlib, TinyXML-2, SDL_GameControllerDB | zlib license only. | The [zlib License](#zlib-license) text. |
| libzip, libogg, libvorbis, opus, opusfile | BSD-3-Clause only. | The [BSD 3-Clause License](#bsd-3-clause-license) text. |
| libpng | PNG Reference Library License v2 only. The v1/v2 split is a *version* boundary, not an offer. | Referenced by URL below. |
| bzip2 | The bzip2 license (BSD-style) only. | Referenced by URL below. |
| Inconsolata, Montserrat, Noto Sans JP, Press Start 2P | SIL Open Font License 1.1 only; the OFL is explicitly non-relicensable (condition 5). | `games/{oot,mm}/assets/custom/fonts/OFL.txt`, in the tree beside the fonts. |
| 2Ship2Harkinian (`games/mm/`) | CC0-1.0 only. Already the CC0 posture this project would elect — but it is the upstream's single license, not a choice made here. | `games/mm/LICENSE`. |
| 3drando (`games/oot/soh/Enhancements/randomizer/3drando/`) | MIT only. | `.../3drando/LICENSE.md`, already in the tree. |
| libultraship, Fast3D, ZAPDTR, OTRExporter, libgfxd, Automate-VCPKG | MIT only. | Each one's own `LICENSE` in the tree (see the inventory's "License text in-tree" column). |
| Dear ImGui, thread-pool, prism-processor, StormLib, nlohmann/json, spdlog, linuxdeploy | MIT only. | The [MIT License](#mit-license) text with each component's copyright line. |
| OoTMM | **Not a choice.** The root `LICENSE` is MIT and governs; `packages/core/package.json:12`'s `"license": "ISC"` is a scaffolding leftover, recorded under [OoTMM](#ootmm) above. A conflicting metadata field is a discrepancy to note, not an option to elect. | The [MIT License](#mit-license) text with `Copyright (c) 2020-2022 OoTMM Team`. |
| Ship of Harkinian (`games/oot/`) | **No license at all** is published upstream, so there is nothing to choose between. See [Unresolved license status](#unresolved-license-status). | Nothing; the status is stated rather than papered over. |
| OpenGL (system) | Platform-provided; no bundled code. | — |

## Unresolved license status

**One** item is honestly unresolved. It is not copyleft, so it does not block the
MIT grant over RedShipBlueShip's own code, and it is inherited rather than
introduced here. It is recorded so that a redistribution decision is made with
the facts.

1. **Ship of Harkinian (`games/oot/`) has no published license.** Verified
   2026-09-17: `HarbourMasters/Shipwright` at `develop` has no root `LICENSE`,
   `COPYING` or `NOTICE`; GitHub's license API returns 404 for the repository;
   a filename search across the repository returns exactly one license file,
   `soh/soh/Enhancements/randomizer/3drando/LICENSE.md` (the MIT notice this
   tree already carries at the corresponding path); and its `README.md` makes no
   license statement. Under default copyright, no express grant accompanies that
   code. The MM half is unaffected — 2Ship2Harkinian's root `LICENSE` is
   CC0-1.0, and is now carried at `games/mm/LICENSE`.

   **This is still unresolved and is not softened here.** No election is
   possible, because an election needs an upstream offer and there is none: a
   repository that publishes no license has not offered anything to choose
   between. Nothing in this pass changed that fact, and nothing was reported to
   or requested from that upstream. The pre-release caveat below stands.

### Resolved by removal

**`Fipps-Regular.otf` — deleted from both trees on 2026-09-21.** The font stated
`Copyright (c) 2007 by Stefanie Koerner (pheist). All rights reserved.` at name
IDs 0 and 10, asserted a trademark at name ID 7, and carried no license
description (name ID 13) and no license URL (name ID 14) — no grant of any kind.
It was widely redistributed as an open font, but nothing in the file or in this
tree substantiated a specific grant, so rather than claim one this project
stopped shipping it. It arrived in the initial commit as part of the vendored
Ship of Harkinian and 2Ship2Harkinian trees; nothing this project authored added
it.

It was never the default overlay font (`gOverlayFont` defaults to
`Press Start 2P`), so the removal is a menu option disappearing, not a visual
change for anyone who had not gone looking for it. A player whose config still
names it is mapped back to `Press Start 2P` by
`SOH::ResolveOverlayFontName` in `games/oot/soh/OTRGlobals.cpp` before the name
reaches libultraship, which is also what stops
`Ship::GameOverlay::SetCurrentFont`'s `mFonts[name]` (`operator[]`) inserting a
null-valued entry that the font combo would then list as a dead row. Recorded in
`docs/known-issues.md`.

## Open items for the operator

These are documentation/packaging gaps, not blockers:

* Decide the disposition of the one remaining unresolved item above (Ship of
  Harkinian's absent license) before any public binary release. Note that it
  cannot be resolved by an election, only by a decision about redistribution.

## Appendix: license texts

Each MIT/BSD component above carries its own copyright line, which substitutes
into the corresponding template below. Licenses whose text is already in the
tree (see the "License text in-tree" column) are not duplicated here.

### MIT License

```
MIT License

Copyright (c) <the copyright holders named in the tables above>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

### zlib License

Applies to SDL2, SDL2_net, GLFW, zlib, TinyXML-2 and SDL_GameControllerDB.

```
This software is provided 'as-is', without any express or implied warranty. In
no event will the authors be held liable for any damages arising from the use of
this software.

Permission is granted to anyone to use this software for any purpose, including
commercial applications, and to alter it and redistribute it freely, subject to
the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim
   that you wrote the original software. If you use this software in a product,
   an acknowledgment in the product documentation would be appreciated but is
   not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
```

### BSD 3-Clause License

Applies to libzip, libogg, libvorbis, opus, opusfile and (as one of its three
options) GLEW.

```
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

### MIT No Attribution (MIT-0)

The option **elected** for dr_libs (see [Elections](#elections)). Reproduced
verbatim from `LICENSE` at `da35f9d6c7374a95353fd1df1d394d44ab66cf01`,
`ALTERNATIVE 2`.

```
Copyright 2020 David Reid

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

### The Unlicense

The option **not** taken for either component that offers it (dr_libs'
`ALTERNATIVE 1`, stb_image's `ALTERNATIVE B`). Kept here because the offers name
it, and because a reader auditing the elections above should be able to see what
was declined.

```
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.

In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
```

### Licenses whose text is in this tree beside the component

These two are not reproduced here because they are carried where the license
itself requires them to be — next to the files they cover, which is also where
the packaging step picks them up:

* **CC0 1.0 Universal** (2Ship2Harkinian, `games/mm/`) — `games/mm/LICENSE`,
  7048 bytes, copied byte-for-byte from upstream on 2026-09-21.
* **SIL Open Font License 1.1** (the fonts above) —
  `games/oot/assets/custom/fonts/OFL.txt` and
  `games/mm/assets/custom/fonts/OFL.txt`, each with the covered fonts' own
  copyright notices.

### Licenses referenced by URL

These are reproduced in full by their upstreams; a distributed build should
carry them alongside the components they cover.

* **PNG Reference Library License v2** (libpng) —
  https://github.com/glennrp/libpng/blob/master/LICENSE
* **bzip2 license** — https://sourceware.org/bzip2/
