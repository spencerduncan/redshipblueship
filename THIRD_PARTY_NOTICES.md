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

**Corrected again 2026-09-21 after review.** The first pass of that re-verification
read each upstream's *root* `LICENSE` and nothing else, which made it miss whole
classes of obligation. Everything below now reflects these corrections, each
verified against the named upstream file at the revision this tree uses: the
embedded **Font Awesome 4** icon face (compiled into the binary, absent from the
Fonts table entirely); **Dear ImGui's vendored stb headers**, which do offer a
choice and are compiled, so a third election is recorded; **fmt**, pulled in by
spdlog's default vcpkg feature; **espeak-ng** (GPL-3.0, optional, Linux) and
**single-header-metal-cpp** (Apache-2.0, Apple platforms), neither of which was
inventoried; the three third-party notices **vendored inside libultraship's
`StrHash64`**; four copyright lines that did not match their upstream's text
(prism-processor, StormLib, SDL_GameControllerDB, and GLEW's own formatting);
**bzip2**, which the previous pass claimed it could not fetch and which is in fact
served as git from the host it names; and **GLEW's `LICENSE.txt`**, which is now
reproduced verbatim instead of being approximated by a generic BSD-3-Clause
template whose clause 3 says the opposite of GLEW's.

**No copyrighted Nintendo asset ships in this repository.** Game assets are
extracted at build time from original Ocarina of Time and Majora's Mask ROMs the
user supplies.

## Summary of the licensing picture

* Every component compiled into a **default Windows or Linux `redship`**, or
  shipped beside it, is under a **permissive** license: MIT, MIT No Attribution,
  CC0-1.0, zlib, BSD-3-Clause, the PNG Reference Library License v2, the bzip2
  license, or the SIL Open Font License 1.1 for fonts.
* **Two platform/option-conditional exceptions to that sentence**, both added to
  the inventory on 2026-09-21 after review found them missing:
  * **single-header-metal-cpp** is **Apache-2.0**, and
    `libultraship/cmake/dependencies/mac.cmake` /`ios.cmake` fetch it
    unconditionally on Apple platforms. Apache-2.0 is permissive but not one of
    the licenses enumerated above, and it carries a NOTICE-propagation condition
    (§4(d)) the others do not.
  * **espeak-ng** is **GPL-3.0**. It is optional and Linux-only, its headers are
    compiled when `find_library(ESPEAK espeak-ng)` succeeds, and the library
    itself is reached with `dlopen` at runtime rather than linked. CI's Linux
    jobs install `libespeak-ng-dev`, so they do compile against it. See the row
    in [External libraries](#external-libraries-linked-from-the-system-or-vcpkg)
    for exactly what that means for redistribution.
* **Three** components let the recipient **choose** which license to take them
  under. This project has chosen, and the choices are recorded with their
  upstream evidence under [Elections](#elections). Every other component offers
  no choice at all, and is used under the single license its upstream publishes.
* **No GPL, LGPL, AGPL or MPL source is present in this tree, and no copyleft
  code is linked into `redship` at build time** — statically or dynamically — in
  any configuration this repository builds. The one qualification, stated rather
  than buried: on Linux, when espeak-ng is installed, GPL-3.0 **headers** are
  compiled into the speech-synthesizer TUs and the GPL-3.0 **library** is loaded
  with `dlopen("libespeak-ng.so", ...)` at runtime
  (`games/oot/soh/Enhancements/speechsynthesizer/ESpeakSpeechSynthesizer.{h,cpp}`).
  A redistributor of a Linux binary built that way must reach its own conclusion
  about that arrangement; this file's job is to say it is there. Beyond that, the
  only copyleft references anywhere in this repository are three documentation
  passages (`docs/adr/0006-netplay-transport-scope.md:204`,
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
| libcore `Crc` / SHA-1, vendored inside libultraship as `StrHash64` | `libultraship/src/ship/utils/StrHash64.cpp`, `libultraship/include/ship/utils/StrHash64.h` | **three cumulative notices**: MIT, zlib, and BSD-3-Clause (ReichlSoft) | Copyright (c) 2006 Anton Samokhvalov (MIT); Copyright (C) 1995-2004 Jean-loup Gailly and Mark Adler (zlib); Copyright (c) 2003, Dominik Reichl <dominik.reichl@t-online.de>, All rights reserved (BSD-3-Clause, ReichlSoft) | libcore 0.22.7 (`lib/hash/Crc.cpp`), as recorded in the file's own header | **yes** — all three notices are reproduced at the head of both files |
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
| stb headers **vendored inside Dear ImGui** — `imstb_rectpack.h`, `imstb_truetype.h`, `imstb_textedit.h`, compiled via `imgui_draw.cpp` and `imgui_widgets.cpp` | same ImGui `FetchContent` at tag `v1.91.9b-docking` | MIT **or** public domain, at the recipient's choice — **MIT elected**, see [Elections](#elections) | Copyright (c) 2017 Sean Barrett | vendored inside https://github.com/ocornut/imgui |
| stb_image (pinned `0bc88af4`) | `libultraship/cmake/dependencies/common.cmake` | MIT **or** public domain, at the user's choice — **MIT elected**, see [Elections](#elections) | Copyright (c) 2017 Sean Barrett | https://github.com/nothings/stb |
| dr_libs (pinned `da35f9d6`) | `games/oot/CMakeLists.txt:463`, `games/mm/CMakeLists.txt:36` | Unlicense (public domain) **or** MIT No Attribution, at the user's choice — **MIT-0 elected**, see [Elections](#elections) | David Reid | https://github.com/mackron/dr_libs |
| thread-pool (v4.1.0) | `libultraship/cmake/dependencies/common.cmake` | MIT | Copyright (c) 2024 Barak Shoshany | https://github.com/bshoshany/thread-pool |
| prism-processor (pinned `bbcbc7e3`) | `libultraship/cmake/dependencies/common.cmake` | MIT | Copyright (c) 2025 Lywx & coco875 admin@undervolt.dev | https://github.com/KiritoDv/prism-processor |
| StormLib (v9.25, optional — `INCLUDE_MPQ_SUPPORT`) | `libultraship/cmake/dependencies/common.cmake` | MIT | Copyright (c) 1999-2013 Ladislav Zezula | https://github.com/ladislav-zezula/StormLib |
| libgfxd (optional — `GFX_DEBUG_DISASSEMBLER`) | `libultraship/cmake/dependencies/common.cmake` | MIT | Copyright (c) 2016-2021 glank | https://github.com/glankk/libgfxd |
| SDL_GameControllerDB (`gamecontrollerdb.txt`, downloaded and installed beside the binary) | `games/oot/CMakeLists.txt:907`, `games/mm/CMakeLists.txt:1061` | zlib | Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org> | https://github.com/mdqinc/SDL_GameControllerDB |
| single-header-metal-cpp (**Apple platforms only**) | `libultraship/cmake/dependencies/mac.cmake:18-20`, `ios.cmake:73-75` | **Apache-2.0** | Copyright Apple Inc. (metal-cpp); single-header repackaging by briaguya-ai | https://github.com/briaguya-ai/single-header-metal-cpp |
| linuxdeploy (AppImage packaging tool only; not linked) | `CMake/Packaging.cmake:62` | MIT | Copyright (c) TheAssassin and contributors | https://github.com/linuxdeploy/linuxdeploy |

`single-header-metal-cpp` is fetched unconditionally by the macOS and iOS
dependency files, so an Apple build compiles it in. Apache-2.0 is permissive, but
unlike every other row here it conditions redistribution on propagating any
`NOTICE` file the upstream ships (§4(d)) and on stating changes (§4(b)). This
project builds and tests only Windows and Linux; an Apple redistribution needs
that notice carried and is not covered by the enumeration in the summary above.

### External libraries linked from the system or vcpkg

Requested by `CMakeLists.txt:38` and
`libultraship/cmake/dependencies/windows-vcpkg.cmake`. On a static Windows
vcpkg build these end up inside the shipped executable, so their notices apply
to the distributed binary.

| Component | License | Copyright | Upstream |
|---|---|---|---|
| SDL2 | zlib | Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org> | https://github.com/libsdl-org/SDL |
| SDL2_net | zlib | Copyright (C) 1997-2026 Sam Lantinga | https://github.com/libsdl-org/SDL_net |
| GLEW | Modified BSD **and** MIT (**not** a choice — corrected 2026-09-21; see [Elections](#elections)). Its own `LICENSE.txt` is reproduced verbatim in the [Appendix](#glew-licensetxt-reproduced-verbatim) | Copyright (C) 2002-2007, Milan Ikits <milan ikits[]ieee org>; Copyright (C) 2002-2007, Marcelo E. Magallon <mmagallo[]debian org>; Copyright (C) 2002, Lev Povalahev; Mesa 3-D: Copyright (C) 1999-2007 Brian Paul; Copyright (c) 2007 The Khronos Group Inc. | https://github.com/nigels-com/glew |
| GLFW | zlib/libpng | Copyright (c) 2002-2006 Marcus Geelnard; Copyright (c) 2006-2019 Camilla Lowy | https://github.com/glfw/glfw |
| zlib | zlib | (C) 1995-2026 Jean-loup Gailly and Mark Adler | https://github.com/madler/zlib |
| bzip2 | bzip2 license (BSD-style) | Copyright (C) 1996-2019 Julian R Seward | https://sourceware.org/bzip2/ |
| libzip | BSD-3-Clause | Copyright (C) 1999-2026 Dieter Baron and Thomas Klausner | https://github.com/nih-at/libzip |
| libpng | PNG Reference Library License v2 | Copyright (c) 1995-2024 The PNG Reference Library Authors | https://github.com/glennrp/libpng |
| nlohmann/json | MIT | Copyright (c) 2013-2025 Niels Lohmann | https://github.com/nlohmann/json |
| TinyXML-2 (vcpkg copy) | zlib | Lee Thomason | https://github.com/leethomason/tinyxml2 |
| spdlog | MIT | Copyright (c) 2016 - present, Gabi Melman and spdlog contributors | https://github.com/gabime/spdlog |
| fmt (**pulled in by spdlog**, not requested directly) | MIT | Copyright (c) 2012 - present, Victor Zverovich and {fmt} contributors | https://github.com/fmtlib/fmt |
| libogg | BSD-3-Clause | Copyright (c) 2002, Xiph.org Foundation | https://github.com/xiph/ogg |
| libvorbis | BSD-3-Clause | Copyright (c) 2002-2020 Xiph.org Foundation | https://github.com/xiph/vorbis |
| opus | BSD-3-Clause | Copyright (c) 2001-2011 Xiph.Org, Skype Limited, Octasic, Jean-Marc Valin, Timothy B. Terriberry, CSIRO, Gregory Maxwell, Mark Borgerding, Erik de Castro Lopo | https://github.com/xiph/opus |
| opusfile | BSD-3-Clause | Copyright (c) 1994-2013 Xiph.Org Foundation and contributors | https://github.com/xiph/opusfile |
| espeak-ng (**optional, Linux; GPL-3.0** — see the paragraph below) | GPL-3.0 | Copyright (C) 2005-2014 Jonathan Duddington; Copyright (C) 2015-2025 Reece H. Dunn and the espeak-ng contributors | https://github.com/espeak-ng/espeak-ng |
| OpenGL (system) | platform-provided; no bundled code | — | — |

**fmt** is in this table because it arrives whether or not anybody asks for it.
`CMakeLists.txt:38` requests `spdlog` from vcpkg; the vcpkg `spdlog` port declares
`"default-features": ["fmt", "tz-offset"]`, and the `fmt` feature depends on the
`fmt` port — so a plain `spdlog` request installs and links fmt. spdlog's own
`LICENSE` ends by saying so: "This software depends on the fmt lib (MIT License),
and users must comply to its license". MIT's condition is the copyright line, and
it is now here.

**espeak-ng** is the one copyleft dependency this build can touch, and its shape
matters. `games/oot/CMakeLists.txt:193-196` runs `find_library(ESPEAK espeak-ng)`
and *excludes* `soh/Enhancements/speechsynthesizer/ESpeak*` when it is absent, so
on a machine without it nothing about it is compiled. When it is present —
which includes CI's Linux jobs, since `.github/workflows/apt-deps.txt` installs
`libespeak-ng-dev` — `ESpeakSpeechSynthesizer.h` includes
`<espeak-ng/speak_lib.h>` and the library is obtained at runtime with
`dlopen("libespeak-ng.so", ...)` rather than being linked. Windows builds do not
have it at all. It is inventoried here because "the build compiles against a
GPL-3.0 header" is exactly the fact a redistributor needs, and because an
inventory that lists a packaging tool and a downloaded text file while omitting
this would not be an audit. Its text is not reproduced below: GPL-3.0 is long,
this project ships no espeak-ng code, and the obligation attaches to whoever
distributes a binary built with that path enabled —
https://github.com/espeak-ng/espeak-ng/blob/master/COPYING.

**The operator ruled on 2026-09-21 to KEEP espeak-ng**, because it is the
accessibility feature on Linux — it is what drives the screen-reader speech path,
and dropping it to simplify a license question would remove a capability from the
players who need it most. Nothing above is softened by that ruling: the GPL-3.0
header compilation and the runtime `dlopen` are still exactly as described, and a
redistributor of a Linux binary built with that path enabled still has to reach
its own conclusion about them. The decision recorded here is that the feature
stays and the question is answered at distribution time, not that the question
went away.

### Fonts

Both game trees carry the same custom font set
(`games/oot/assets/custom/fonts/`, `games/mm/assets/custom/fonts/`). The license
column below is taken from each font file's own `name` table (name ID 13, the
license description) — not from a distribution page — except where the row says
otherwise.

**A font does not have to be a file to ship.** The fifth row below is
base85-compressed into a libultraship header and pushed into the ImGui atlas at
startup, so it is *inside the executable*: it was missing from this table until
2026-09-21 because the table had been built by listing the two asset directories.
Both halves are now covered here and in `OFL.txt`.

| Font | Path in tree | License | Copyright (name ID 0) | Upstream |
|---|---|---|---|---|
| Inconsolata Regular | `games/{oot,mm}/assets/custom/fonts/Inconsolata-Regular.ttf` | SIL Open Font License 1.1 | Copyright 2006 The Inconsolata Project Authors (https://github.com/cyrealtype/Inconsolata) | https://github.com/cyrealtype/Inconsolata |
| Montserrat Regular | `games/{oot,mm}/assets/custom/fonts/Montserrat-Regular.ttf` | SIL Open Font License 1.1 | Copyright 2011 The Montserrat Project Authors (https://github.com/JulietaUla/Montserrat) | https://github.com/JulietaUla/Montserrat |
| Noto Sans JP Regular | `games/oot/assets/custom/fonts/NotoSansJP-Regular.ttf` | SIL Open Font License 1.1 | (c) 2014-2021 Adobe (http://www.adobe.com/), with Reserved Font Name 'Source'. — plus name ID 7: `Source is a trademark of Adobe in the United States and/or other countries.` | https://github.com/notofonts/noto-cjk |
| Press Start 2P Regular | `games/{oot,mm}/assets/custom/fonts/PressStart2P-Regular.ttf` | SIL Open Font License 1.1 | Copyright 2012 The Press Start 2P Project Authors (cody@zone38.net), with Reserved Font Name "Press Start 2P" | https://fonts.google.com/specimen/Press+Start+2P |
| **Font Awesome 4** (`fontawesome-webfont.ttf`, 165548 bytes) — **embedded, not a file** | `libultraship/include/ship/window/gui/Fonts.h` (base85 array `fontawesome_compressed_data_base85`) | SIL Open Font License 1.1 — **stated by upstream's `README.md` at ref `4.x`**, not by the font's `name` table, which carries no name ID 13; its name ID 14 license URL is `http://fontawesome.io/license/` | Copyright Dave Gandy 2016. All rights reserved. | https://github.com/FortAwesome/Font-Awesome (4.x) |
| IconsFontAwesome4.h (icon-name macros for the face above; no font data) | `libultraship/include/ship/window/gui/IconsFontAwesome4.h` | generated file — see the note below | generated by `GenerateIconFontCppHeaders.py` from `FortAwesome/Font-Awesome` `4.x` `src/icons.yml` | https://github.com/juliettef/IconFontCppHeaders |

Font Awesome 4 is loaded unconditionally: `libultraship/src/ship/window/gui/
Gui.cpp:153` calls `AddFontFromMemoryCompressedBase85TTF` on that array, and both
ports do it again (`games/oot/soh/OTRGlobals.cpp:2393`,
`games/mm/2s2h/BenPort.cpp:520`), so every window RedShipBlueShip draws renders
its icons from it. Upstream's `README.md` at ref `4.x` says, under "License": "The
Font Awesome font is licensed under the SIL OFL 1.1" — with the CSS/LESS/Sass
under MIT and the documentation under CC BY 3.0, none of which this project uses.
Its name-ID-0 string does read "All rights reserved", which is common boilerplate
in OFL-licensed faces and is reproduced unaltered in `OFL.txt` because that is
what the OFL requires; unlike the font removed below, this one states a license
URL (name ID 14) and its upstream states a grant, so it is licensed, not
ungranted. `IconsFontAwesome4.h` contains only `#define`s of codepoint names
derived from that upstream's `src/icons.yml`; the generator carries no license
statement in the emitted header, and its provenance is recorded here so nobody has
to rediscover where a header with no copyright line came from.

`Fipps-Regular.otf` used to be a fifth row here, with an unresolved status. It
was **deleted from both trees on 2026-09-21** rather than resolved, and the third
copy, in the `OTRExporter` submodule, was deleted fork-side the same day; see
[Unresolved license status](#unresolved-license-status).

The SIL Open Font License 1.1 requires its text, and each font's copyright
notice, to travel with the font files (OFL 1.1 condition 2). Both are now in the
tree beside them: **`games/oot/assets/custom/fonts/OFL.txt`** and
**`games/mm/assets/custom/fonts/OFL.txt`**. Each names every font in the table
above — including the embedded Font Awesome face, which has no directory to sit
in — with that font's `name`-table copyright line, and reproduces the OFL 1.1
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
asserts the mechanical parts of this, and it is worth being exact about which,
because a first draft of this paragraph overclaimed:

* Each fonts directory is **enumerated**, and every `.ttf`/`.otf` in it must be a
  font this file inventories. Re-adding the removed font under any other name
  fails the row, as does adding a fifth face nobody has written a notice for.
  (The earlier version only rejected filenames beginning `Fipps`, which a rename
  walked straight past.)
* Each font file's own `name` table is **parsed in the test**, and its name-ID-0
  copyright string must appear verbatim in `OFL.txt`. So the binding asserted is
  between the shipped bytes and the shipped notice — swapping in a differently
  dated build of the same family fails until the notice is updated. Noto Sans JP's
  name ID 7 trademark line and the embedded Font Awesome notice are additionally
  required by literal, since neither comes from a `name` table the row can parse
  (Font Awesome is not a file in the tree).
* Both `OFL.txt` copies must carry the OFL 1.1 body and be **byte-identical**
  (see above for why the archive layer forces that), `games/mm/LICENSE` must be
  the CC0 text, and no source or build file under `games/`, `src/`, `rsbs/`,
  `CMake/` or `OTRExporter/` may name the removed font.
* Every font file in the pinned submodule's `OTRExporter/assets/fonts` must carry
  a **license grant in its own `name` table** (ID 13 or ID 14). This is a
  different and weaker rule than the custom-asset trees get, for the reason given
  under [Resolved by removal](#resolved-by-removal); what it buys is that moving
  the gitlink back to a commit carrying the ungranted font fails the row.

What it does **not** assert: anything about the contents of a built `.o2r`,
anything about MM's `2s2h/BenPort.cpp`, which no configuration in this repository
compiles, and — in `OTRExporter/assets/fonts` specifically — nothing about
inventory completeness or notice texts, only that each font present grants
something.

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
| stb code **vendored inside Dear ImGui** — `imstb_rectpack.h`, `imstb_truetype.h`, `imstb_textedit.h` | ImGui tag `v1.91.9b-docking` (`libultraship/cmake/dependencies/common.cmake`) | MIT **or** public domain (the Unlicense) | **MIT** | The offer is inside each vendored header, not in ImGui's root `LICENSE.txt` (which is MIT-only and is why the previous pass missed these). At that tag: `imstb_truetype.h:5047` `This software is available under 2 licenses -- choose whichever you prefer.`, `:5049` `ALTERNATIVE A - MIT License`, `:5050` `Copyright (c) 2017 Sean Barrett`, `:5067` `ALTERNATIVE B - Public Domain (www.unlicense.org)`. The identical sentence is at `imstb_rectpack.h:589` (`ALTERNATIVE A` at `:591`) and `imstb_textedit.h:1431` (`ALTERNATIVE A` at `:1433`). All three are compiled: `imgui_draw.cpp` includes rectpack and truetype, `imgui_widgets.cpp` includes textedit, and both are in the `ImGui` target's sources. |

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
your option`).

**How that scan was wrong, and how it was fixed.** The first pass read each
upstream's **root `LICENSE` file only** and reported two matches. That coverage
rule cannot see a choice offered inside a *vendored source file*, which is exactly
where the third one was: Dear ImGui's root `LICENSE.txt` is MIT-only, while the
three `imstb_*.h` headers it bundles — two of which are compiled into `redship` —
each carry stb's "choose whichever you prefer" sentence verbatim. The scan is now
per *compiled file* for the vendoring cases, and the count is **three**: stb_image,
dr_libs, and imgui's bundled stb headers. Anyone re-auditing should assume the same
failure mode elsewhere and check vendored trees, not just roots — the entry for
libultraship's `StrHash64` in the inventory above is the other thing this
correction turned up (three cumulative notices, not a choice).

**bzip2 was re-fetched too, and the earlier excuse for not doing so was wrong.**
The previous text said its canonical distribution "is not a Git host this scan
could reach". It is: `https://sourceware.org/git/?p=bzip2.git;a=summary` answers
200, and `…;a=blob_plain;f=LICENSE;hb=HEAD` returns the license text, whose line 4
reads `documentation, are copyright (C) 1996-2019 Julian R Seward.  All` —
matching this file's inventory row — with no choice-of-license language anywhere in
its 42 lines. So bzip2 is verified on the same footing as everything else, and the
audit has no advertised hole.

The ones where somebody might expect a choice, and why there is none:

| Component | Why no election | What is carried instead |
|---|---|---|
| GLEW | **Not a choice — the earlier "tri-licensed" wording in this file was wrong, corrected 2026-09-21.** `LICENSE.txt` is three *cumulative* notices over three bodies of code GLEW is assembled from: GLEW itself (Modified BSD, lines 1-28), the Mesa 3-D graphics library (MIT, lines 31-51) and Khronos Group material (MIT-style, lines 54-73). There is no "choose whichever you prefer" sentence anywhere in the file, so all three apply at once. | **GLEW's own `LICENSE.txt`, reproduced verbatim** in the [Appendix](#glew-licensetxt-reproduced-verbatim). It is not pointed at the generic templates: GLEW's clause 3 is a *permission* ("The name of the author may be used to endorse or promote products derived from this software without specific prior written permission"), where the BSD-3-Clause template's clause 3 is the opposite prohibition; its disclaimer says `THE COPYRIGHT OWNER OR CONTRIBUTORS` where the template says `HOLDER`; its Mesa disclaimer names `BRIAN PAUL`; and its Khronos notice is written over "Materials", not "Software". Its own clause 2 requires binary redistributions to reproduce *that* list of conditions, so a lookalike does not discharge it. |
| stb code vendored inside Dear ImGui | A choice **is** offered — see [Elections made](#elections-made). Listed here only so the previous version of this row, which treated imgui as a single MIT component, is visibly superseded. | MIT elected; the [MIT License](#mit-license) text with `Copyright (c) 2017 Sean Barrett`. |
| libultraship's `StrHash64` (vendored libcore `Crc`) | **Not a choice — three cumulative notices**, like GLEW: an MIT grant (Anton Samokhvalov), the zlib terms (Gailly and Adler) and a three-clause BSD grant naming ReichlSoft (Dominik Reichl), each marked in the file as applying "to some parts of this code". | The three notices as they stand at the head of `libultraship/include/ship/utils/StrHash64.h` and `.cpp`, which is where they already are; the Reichl BSD-3-Clause copyright line is reproduced in the inventory table above because clause 2 requires a binary distribution to carry it. |
| single-header-metal-cpp | Apache-2.0 only; the license itself offers nothing to elect. Apple platforms only. | Referenced by URL below. Its §4(d) NOTICE condition is the reason it is called out in the summary rather than folded into the permissive list. |
| espeak-ng | GPL-3.0 only. Optional, Linux only. | Referenced by URL below; see the paragraph under [External libraries](#external-libraries-linked-from-the-system-or-vcpkg). |
| fmt | MIT only. | The [MIT License](#mit-license) text with `Copyright (c) 2012 - present, Victor Zverovich and {fmt} contributors`. |
| Font Awesome 4 (embedded) | SIL Open Font License 1.1 only, per upstream's `README.md` at `4.x`; the OFL is explicitly non-relicensable (condition 5). | `games/{oot,mm}/assets/custom/fonts/OFL.txt`, which carries its notice even though the face is embedded rather than a file in those directories. |
| GLFW | "zlib/libpng license" is **one** license under two names, not two options. `LICENSE.md` states a single set of terms under Marcus Geelnard's and Camilla Löwy's copyrights. | The [zlib License](#zlib-license) text. |
| SDL2, SDL2_net, zlib, TinyXML-2, SDL_GameControllerDB | zlib license only. | The [zlib License](#zlib-license) text. |
| libzip, libogg, libvorbis, opus, opusfile | BSD-3-Clause only. | The [BSD 3-Clause License](#bsd-3-clause-license) text. |
| libpng | PNG Reference Library License v2 only. The v1/v2 split is a *version* boundary, not an offer. | Referenced by URL below. |
| bzip2 | The bzip2 license (BSD-style) only. | Referenced by URL below. |
| Inconsolata, Montserrat, Noto Sans JP, Press Start 2P | SIL Open Font License 1.1 only; the OFL is explicitly non-relicensable (condition 5). | `games/{oot,mm}/assets/custom/fonts/OFL.txt`, in the tree beside the fonts. |
| libzip, libogg, libvorbis, opus, opusfile (BSD-3-Clause) — repeated here for the appendix's sake | BSD-3-Clause only. | The generic [BSD 3-Clause License](#bsd-3-clause-license) text, which **is** their wording. GLEW is deliberately *not* in that list any more; see its row above. |
| 2Ship2Harkinian (`games/mm/`) | CC0-1.0 only. Already the CC0 posture this project would elect — but it is the upstream's single license, not a choice made here. | `games/mm/LICENSE`. |
| 3drando (`games/oot/soh/Enhancements/randomizer/3drando/`) | MIT only. | `.../3drando/LICENSE.md`, already in the tree. |
| libultraship, Fast3D, ZAPDTR, OTRExporter, libgfxd, Automate-VCPKG | MIT only. | Each one's own `LICENSE` in the tree (see the inventory's "License text in-tree" column). |
| Dear ImGui (its own code), thread-pool, prism-processor, StormLib, nlohmann/json, spdlog, linuxdeploy | MIT only. For Dear ImGui this means its own `LICENSE.txt`; the stb headers it vendors are a separate row above. | The [MIT License](#mit-license) text with each component's copyright line. |
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

It was never the default overlay font (`CVAR_GAME_OVERLAY_FONT`, which expands to
`gSettings.OverlayFont` in this build — `CMake/lus-cvars.cmake:16` over
`CMake/soh-cvars.cmake`'s `gSettings` prefix, both set before libultraship's own
`gOverlayFont` default and therefore winning the CACHE race; `gOverlayFont` is the
pre-migration spelling that `games/oot/soh/config/ConfigMigrators.h:1534` renames
away — defaults to `Press Start 2P`), so the removal is a menu option
disappearing, not a visual change for anyone who had not gone looking for it. A
player whose config still names it is mapped back to `Press Start 2P` by
`SOH::ResolveOverlayFontName` in `games/oot/soh/OTRGlobals.cpp` before the name
reaches libultraship, which is also what stops
`Ship::GameOverlay::SetCurrentFont`'s `mFonts[name]` (`operator[]`) inserting a
null-valued entry that the font combo would then list as a dead row.

**One consequence of resolving rather than rejecting, stated plainly.** Because the
resolved name IS loaded, `SetCurrentFont` now reaches its tail, which writes the
name into `CVAR_GAME_OVERLAY_FONT` and schedules a config flush. So the first
launch after this change **overwrites** a stale selection in
`shipofharkinian.json` with `Press Start 2P`; the previous behaviour left the
stale string in the file untouched (and the overlay drawn in ImGui's built-in
face, because `mCurrentFont` stayed at its initial `"Default"`). Rendering in a
font this project ships was preferred over preserving a name that no build
contains. No save data is affected — this is a settings key. Recorded in
`docs/known-issues.md`.

**The third, byte-identical copy — the one in the submodule — was removed
fork-side on 2026-09-21 at `a26d3937`.** It was
`OTRExporter/assets/fonts/Fipps-Regular.otf`: blob
`9334dad594277ba8339786217d75260e58436dc8`, md5
`f939d3db2e61212c288325fc8b0bb255`, the same 34220 bytes as the two copies deleted
above, present at the previously pinned gitlink `a9567801` of
`https://github.com/spencerduncan/OTRExporter`, this project's own fork. It was
disclosed here because `git submodule update --init` is a documented build step, so
every source checkout materialised it, and because the sentence "deleted from both
trees" would otherwise have read as "gone".

The gitlink now points at
`a26d3937b0aa73c21db7ff969010c8129140d400`, the merge commit that fork PR
`spencerduncan/OTRExporter#1` produced, and it is the new tip of
`claude/namespace-exporter-globals`. Its first parent is the previously pinned and
tested `a95678017c73ebeacd68bd5156bc80ff8d50a6f0`; its second is
`ca696842bfc90966d704e7fe43ef7d42a6bfc25a`, the one deletion commit. `git diff
--stat` between the old pin and the new is exactly one file — `assets/fonts/
Fipps-Regular.otf`, 34220 bytes, gone — and `git log --oneline` between them is
exactly those two commits. Nothing else in the exporter moved, and the three
commits this project depends on remain ancestors of the new pin. No checkout of
this project materialises the font any more, in any tree, at any pin.

The PR was merged with a **merge commit** rather than squashed or rebased,
deliberately: a squash would have rewritten `a9567801` — the SHA this project has
actually built and tested against — out of existence. The branch the deletion was
authored on is `claude/remove-fipps-font`, stacked on
`claude/namespace-exporter-globals` and **not** on the fork's default branch
`develop`, which lacks the three commits this project depends on (the per-variant
exporter-globals namespacing that ended the exit-time double destruction, the
single-exe IPO disable, and the MSVC Release debug-info fix). Basing this work on
`develop`, or merging it there, would have been a regression.

What the copy did and did not affect while it was there, kept for the record:
**it never reached any shipped archive.** `CMakeLists.txt` passes
`--custom-assets-path` as `games/oot/assets/custom` or `games/mm/assets/custom` and
nothing else (four call sites), `OTRExporter`'s own sources reference `assets/fonts`
nowhere, and no `install()` or packaging step touches `OTRExporter/assets`. So
`soh.o2r`, `2ship.o2r` and every distributed binary were clean before this removal
as well as after it.

**The `FontLicense` row now scans that directory**, which it previously and
explicitly did not. The rule it applies there is not the custom-asset trees' rule:
nothing packs `OTRExporter/assets/fonts` and there is no `OFL.txt` beside it, so
demanding an inventory and a matching notice would fail on the fork's contents
rather than on a problem. What is required instead is a **grant** in each font's
own `name` table — a license description (ID 13) or a license URL (ID 14). The
removed font had neither; the one file that remains,
`OTRExporter/assets/fonts/PressStart2P-Regular.ttf`, states OFL 1.1 at ID 13 and
`http://scripts.sil.org/OFL` at ID 14. So moving the pin back to `a9567801` turns
the row red by name, and so does a renamed or newly added ungranted font, which a
filename check would not catch. `OTRExporter/` also joined the source/build-file
scan for the removed font's name. (Noted, not widened here: that fork's
`PressStart2P-Regular.ttf` has no `OFL.txt` beside it. It is packed into nothing
and shipped by nothing, so no distribution of this project relies on that notice;
the two copies that ARE shipped, under `games/*/assets/custom/fonts/`, each carry
one.)

## Open items for the operator

These are documentation/packaging gaps, not blockers:

* Decide the disposition of the one remaining unresolved item above (Ship of
  Harkinian's absent license) before any public binary release. Note that it
  cannot be resolved by an election, only by a decision about redistribution.
* ~~**Delete `assets/fonts/Fipps-Regular.otf` in `spencerduncan/OTRExporter` and
  bump the gitlink here.**~~ **Done 2026-09-21.** The operator ruled that day to go
  ahead with the fork change; the file was deleted on the fork's
  `claude/remove-fipps-font` branch (stacked on
  `claude/namespace-exporter-globals`, never on `develop`), merged there with a
  merge commit, and the gitlink here was moved to that merge commit
  `a26d3937`. See [Resolved by removal](#resolved-by-removal) for the
  evidence and for the `FontLicense` scan that now keeps it out.
* **Carry Apache-2.0's `NOTICE` requirement if an Apple build is ever
  distributed** (single-header-metal-cpp, §4(d)). Windows and Linux builds are
  unaffected.
* **Whether a GPL-3.0 `dlopen` dependency is acceptable in a distributed Linux
  binary** is still an open question, but it is no longer a question about whether
  to keep the dependency: the operator ruled on 2026-09-21 that espeak-ng **stays**,
  because it is the Linux accessibility feature. The build does still degrade
  cleanly without it (`find_library` failing simply drops the speech-synthesizer
  TUs), so configuring a particular release without it remains available as a
  packaging choice — it is not the default and not the plan.

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

Applies to libzip, libogg, libvorbis, opus and opusfile.

**GLEW is deliberately not in that list.** An earlier version of this line said
"and (as one of its three options) GLEW", which contradicted the [Elections](#elections)
table 120-odd lines above — GLEW's three notices are cumulative, not options — and
pointed GLEW's reader at wording GLEW does not use. GLEW's full text is its own
section below.

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

### GLEW (`LICENSE.txt`, reproduced verbatim)

Reproduced in full rather than approximated, because GLEW's clause 2 conditions
binary redistribution on reproducing *this* list of conditions and *this*
disclaimer, and the generic BSD-3-Clause template above differs from it in
substance: the template's clause 3 forbids using the copyright holder's name to
endorse products, where GLEW's third bullet *permits* using the author's name;
the template says `THE COPYRIGHT HOLDER OR CONTRIBUTORS` where GLEW says
`THE COPYRIGHT OWNER OR CONTRIBUTORS`; GLEW's Mesa disclaimer names `BRIAN PAUL`;
and GLEW's Khronos notice is written over "Materials" rather than "Software".
All three notices below apply at once — none of them is an option
(see [Elections](#elections)). Fetched read-only from `nigels-com/glew` on
2026-09-21; 73 lines, trailing whitespace and all.

```
The OpenGL Extension Wrangler Library
Copyright (C) 2002-2007, Milan Ikits <milan ikits[]ieee org>
Copyright (C) 2002-2007, Marcelo E. Magallon <mmagallo[]debian org>
Copyright (C) 2002, Lev Povalahev
All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, 
  this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, 
  this list of conditions and the following disclaimer in the documentation 
  and/or other materials provided with the distribution.
* The name of the author may be used to endorse or promote products 
  derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
THE POSSIBILITY OF SUCH DAMAGE.


Mesa 3-D graphics library
Version:  7.0

Copyright (C) 1999-2007  Brian Paul   All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


Copyright (c) 2007 The Khronos Group Inc.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and/or associated documentation files (the
"Materials"), to deal in the Materials without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Materials, and to
permit persons to whom the Materials are furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Materials.

THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
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
