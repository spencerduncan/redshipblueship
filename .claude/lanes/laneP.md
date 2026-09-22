# Lane P — #582: the on-screen generation progress bar

**Branch:** `claude/582-creation-progress-bar`

**Task:** Read #582 in full (the operator's 2026-08-04 ruling asks for a
VISIBLE generation-progress surface) and the merged PR #680 body: the phase
channel exists (`Combo_GenProgress_*`, a stderr leg and a registered sink),
but nothing draws, because paired creation blocks the thread that renders.
Find the seam: where creation runs relative to the render loop
(`Save_InitFile` in `games/oot/src/code/z_sram.c` → `OoT_RunPairedCreationEvent`;
OoT's own seed generation already runs on a worker thread from `SohGui` —
read how `randomizer.cpp` does it and whether the paired creation can be
pumped the same way or must present frames from inside the blocking call).
Deliver the smallest honest surface: a modal progress overlay naming the
current phase and attempt (freeze, OoT fill, MM fill attempt n of N,
crossing passes, validation, publish) that visibly updates during creation
and disappears on success or hands off to the existing failure toast. Must
not change creation order, RNG consumption, or any generated world (prove it
against a main binary), must not deadlock if the window is minimised or the
GPU backend is OpenGL (this workstation's), and must never let a wall clock
decide anything (#581 §2a). If presenting frames during the blocking call is
unsafe in this renderer, say so with evidence and deliver the worker-thread
route or a precise plan — do not fake a bar that never paints. Locks: the
sink receives phases in order with monotone progress (redship tier,
headless); the overlay's state machine (shown → updating →
dismissed/failed) over a synthetic phase stream.

**Files owned:** the progress sink/overlay under `games/oot/soh/SohGui/` or
`src/common/` (say which and why), the creation call site at function
granularity, its tests, shared append-only files.

**Keywords:** `Fixes #582` only if a bar genuinely paints during creation,
else `Refs #582`.
