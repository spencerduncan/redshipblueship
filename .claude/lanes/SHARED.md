## Shared files — no owner, append-only (applies to every lane)

Three files are written by nearly every lane. **Nobody owns them.** Append at the end of the relevant block, never reorder existing entries, and expect trivial rebases:

- `CMake/SingleExecutable.cmake`
- `src/common/test_runner.cpp`
- `.github/clang-format-paths.txt`

`redship_add_test()` already reduced test registration to one appended line, which is why this is safe — but it takes **three coordinated edits**, and the build hard-fails if you do fewer:

1. `redship_add_test(NAME Foo COMMAND redship --test foo)` in `CMake/SingleExecutable.cmake`. `LABEL` defaults to `redship` — the **display-free, ROM-free** tier that runs on every PR. Pass `LABEL rando` only if a window genuinely must come up; that tier needs xvfb and a longer timeout.
2. A `gTests[]` entry in `src/common/test_runner.cpp`. `TestRegistrationComplete` FATAL_ERRORs on a CMake row with no `gTests` entry, or the reverse.
3. If you add a new `src/common/tests/*.c`, `#include` it in `src/common/test_runner.cpp` — `redship_check_test_sources()` FATAL_ERRORs on a test source that is never included.

`games/mm/2s2h/GameExports_SingleExe.cpp` is shared at **function** granularity, not file: Lane 1 has `MM_ConsumeSharedItems`, Lane 3 has `MM_Rando_PairOnCrossGameArrival`, Lane 5 has `MM_Rando_Init`, Lane 6 has `MM_AwardSharedItem`. A function-scoped claim does not prevent a textual conflict — rebase, don't reorder.
