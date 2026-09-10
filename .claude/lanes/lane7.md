You are Lane 7 of eight in the 2026-09-10 wave: **#640 — the Anchor menu registrar is elided from `soh_port`, and its widget-less page leaks `SetNextWindowPos` into the Main Game window.**

- **Branch:** `claude/640-soh-port-registrar-elision`
- **Serves:** #640 (agent tracker for the human-filed #634; `Fixes #640`, `Refs #634` — never a closing keyword on #634). The reporter's "fullscreen" and "DirectX 11" are incidental; the video shows a maximized window and the DX11 default of a CI build.

## Scope

Two defects, one class each:

1. **Registrar elision.** `games/oot/soh/Network/Anchor/Menu.cpp:243` registers the Anchor widgets only through `static RegisterMenuInitFunc menuInitFunc(RegisterAnchorMenu)`; nothing else references the TU. It lives in the `soh__` glob and is built into `soh_port`, a plain STATIC archive (`games/oot/CMakeLists.txt:271-274`) — only `soh_enh` and `soh_rando` get `WHOLE_ARCHIVE` (`:304-306`). The linker drops the member; the registrar never runs; the page has no widgets. Same class as #516 / #341. `.github/scripts/check-registrar-elision.sh:332-336` audits `libsoh_rando.a`, `libsoh_enh.a` and the `lib2ship_*` archives — never `libsoh_port.a`. Fix the elision (WHOLE_ARCHIVE `soh_port`, or an explicit reference / re-home like #516 — measure the link cost and say which) **and** extend the script to audit `libsoh_port.a` so the class is caught next time. Check `SohGui/ResolutionEditor.cpp`, another `soh_port` registrar TU with zero map lines, while you are there.
2. **The leak.** A page with no widgets still positions the main game window: `games/oot/soh/SohGui/Menu.cpp` `SetNextWindowPos` sites (`:622`, `:633`, `:704`). With the content column empty and the window wider than ~800 px, the game view is displaced to a black rectangle. Make the empty-page path not position the game window, independent of fix 1 — a future elided page must not reproduce the symptom.

## Owns

`games/oot/CMakeLists.txt`, `games/oot/soh/SohGui/Menu.cpp`, `.github/scripts/check-registrar-elision.sh`.

## Watch for

Local builds lack `BUILD_REMOTE_CONTROL`, so the Anchor TU itself may not compile locally; CI has it. The elision script's `libsoh_port.a` audit must have a non-vacuity guard (it must fail if the archive is missing, not pass).

## Verify

Local ROM-staged build plus both ctest tiers; the link-check workflow on your branch push must show the script's new audit running. Confirm the Anchor page draws its widgets in a CI-built binary if you can obtain one.
