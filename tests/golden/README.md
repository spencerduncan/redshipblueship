# tests/golden — the pinned worlds

Each `.txt` here is a generated-world digest that a CTest row in the `rando` tier
compares one live generation against (`CMake/CheckGoldenDigest.cmake`). They are
the only thing in this tree that can make "the generated world did not change"
fail; the `SeedDeterminism`-style rows diff two runs of the same binary against
each other and stay green through any deterministic move (#688).

**Do not hand-edit these files.** Re-pin with

```
cmake --build build-cmake --target regen-golden-digests    # Linux: under xvfb-run
```

in its own commit, stating which fields moved and why the new world is the
intended one. A `<name>.<Windows|Linux|Darwin>.txt` file overrides `<name>.txt` on
that platform and may exist only where a platform difference was measured.

Policy, field-by-field reading, and the portability measurement:
[`docs/determinism-goldens.md`](../../docs/determinism-goldens.md).
