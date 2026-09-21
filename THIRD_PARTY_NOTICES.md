# Third-party notices

RedShipBlueShip's own code is MIT (see [`LICENSE`](LICENSE)). It is built on top
of, and links against, a large amount of third-party work that keeps its own
license. This file inventories all of it.

Everything below was verified against this repository's tree at the commit that
introduced this file, and against each upstream's own license file — not from
memory and not from a package manifest. Where a claim could not be verified, it
is listed under [Unresolved license status](#unresolved-license-status) rather
than guessed at.

**No copyrighted Nintendo asset ships in this repository.** Game assets are
extracted at build time from original Ocarina of Time and Majora's Mask ROMs the
user supplies.

## Summary of the licensing picture

* Every component that is compiled into `redship`, or shipped beside it, is
  under a **permissive** license: MIT, CC0-1.0, zlib, BSD-3-Clause, the
  Unlicense, the PNG Reference Library License v2, the bzip2 license, or the SIL
  Open Font License 1.1 for fonts.
* **No GPL, LGPL, AGPL or MPL code is present in the tree or linked into the
  binary.** The only copyleft references anywhere in this repository are three
  documentation passages (`docs/adr/0006-netplay-transport-scope.md:204`,
  `docs/adr/0007-grant-relay-netplay.md:59`,
  `docs/netplay-increment-1-spike.md:343`) that name GPL-3.0 prior art
  explicitly in order to record that it was **not** read, adapted or vendored.
* Two components have an unresolved status and are called out in their own
  section: the Ship of Harkinian upstream publishes no license file at all, and
  the `Fipps-Regular.otf` font asserts "All rights reserved" with no license
  identifier.

## Inventory

### Vendored game ports (in-tree source trees)

| Component | Path in tree | License | Copyright | Upstream | License text in-tree |
|---|---|---|---|---|---|
| Ship of Harkinian (OoT port) | `games/oot/` | **None published upstream** — see [Unresolved license status](#unresolved-license-status) | not stated upstream | https://github.com/HarbourMasters/Shipwright | no |
| 2Ship2Harkinian (MM port) | `games/mm/` | CC0-1.0 (Creative Commons Zero v1.0 Universal, a public-domain dedication) | dedicated to the public domain by the 2Ship2Harkinian authors | https://github.com/HarbourMasters/2ship2harkinian | no (upstream `LICENSE`, 7048 bytes, was not copied with the snapshot) |
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
| stb_image (pinned `0bc88af4`) | `libultraship/cmake/dependencies/common.cmake` | MIT **or** public domain (dual, at the user's choice) | Copyright (c) 2017 Sean Barrett | https://github.com/nothings/stb |
| dr_libs (pinned `da35f9d6`) | `games/oot/CMakeLists.txt:463`, `games/mm/CMakeLists.txt:36` | Unlicense (public domain) **or** MIT-0 (dual, at the user's choice) | David Reid | https://github.com/mackron/dr_libs |
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
| GLEW | Modified BSD (BSD-3-Clause) / MIT / GLX public domain, tri-licensed | Copyright (C) 2002-2007 Milan Ikits, Marcelo E. Magallon; Copyright (C) 2002 Lev Povalahev | https://github.com/nigels-com/glew |
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
| Fipps Regular | `games/{oot,mm}/assets/custom/fonts/Fipps-Regular.otf` | **unresolved** — see below | Copyright (c) 2007 by Stefanie Koerner (pheist). All rights reserved. | http://pheist.net |

The SIL Open Font License 1.1 requires its text to travel with the font files.
It is not currently in the tree next to them; see
[Open items](#open-items-for-the-operator).

## OoTMM

[OoTMM](https://github.com/OoTMM/OoTMM) is RedShipBlueShip's design reference
for cross-game randomization, and the planned source of MM trick names
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
* **Nothing from OoTMM is vendored today.** No OoTMM file, data table or name
  list is present in this repository. Every reference to OoTMM in `games/**`,
  `src/**` and `CMake/**` is a prose comment citing precedent. When a port does
  happen, the ported files carry OoTMM's copyright line, and this section gains
  OoTMM's MIT text and the concrete paths (ADR 0010 D10).
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

## Unresolved license status

Two items are honestly unresolved. Neither is copyleft, so neither blocks the
MIT grant over RedShipBlueShip's own code, and both are inherited rather than
introduced here. They are recorded so that a redistribution decision is made
with the facts.

1. **Ship of Harkinian (`games/oot/`) has no published license.** Verified
   2026-09-17: `HarbourMasters/Shipwright` at `develop` has no root `LICENSE`,
   `COPYING` or `NOTICE`; GitHub's license API returns 404 for the repository;
   a filename search across the repository returns exactly one license file,
   `soh/soh/Enhancements/randomizer/3drando/LICENSE.md` (the MIT notice this
   tree already carries at the corresponding path); and its `README.md` makes no
   license statement. Under default copyright, no express grant accompanies that
   code. The MM half is unaffected — 2Ship2Harkinian's root `LICENSE` is
   CC0-1.0.
2. **`Fipps-Regular.otf` states "All rights reserved."** Its `name` table
   contains no license description (name ID 13) and no license URL (name ID
   14); name IDs 0 and 10 read `Copyright (c) 2007 by Stefanie Koerner
   (pheist). All rights reserved.` and name ID 7 asserts a trademark. The font
   is widely redistributed as an open font, but nothing in the file or in this
   tree substantiates a specific grant, so none is claimed here.

## Open items for the operator

These are documentation/packaging gaps, not blockers:

* Bundle the SIL Open Font License 1.1 text alongside
  `games/{oot,mm}/assets/custom/fonts/` (the OFL requires it to travel with the
  fonts).
* Copy 2Ship2Harkinian's CC0-1.0 `LICENSE` into `games/mm/` so the MM tree's
  grant is visible in this repository rather than only upstream.
* Decide the disposition of the two unresolved items above before any public
  binary release.
* The copyright line in `LICENSE` uses `Spencer Duncan`, the majority git author
  name on this repository (`Penelope Duncan` and `Penny` also appear in the
  history); change it if a different attribution is wanted.

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

### The Unlicense

One of the two options dr_libs offers; the other is MIT-0.

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

### Licenses referenced by URL

These are reproduced in full by their upstreams; a distributed build should
carry them alongside the components they cover.

* **CC0 1.0 Universal** (2Ship2Harkinian) —
  https://creativecommons.org/publicdomain/zero/1.0/legalcode
* **SIL Open Font License 1.1** (the fonts above) —
  https://scripts.sil.org/OFL
* **PNG Reference Library License v2** (libpng) —
  https://github.com/glennrp/libpng/blob/master/LICENSE
* **bzip2 license** — https://sourceware.org/bzip2/
