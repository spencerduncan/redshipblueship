You are Lane 3 of eight in the 2026-09-10 wave: **#623 — `build-windows` compiles both games and ~90 test rows, then runs zero of them. Run the `redship` tier there.**

- **Branch:** `claude/623-windows-ci-redship-tier`
- **Serves:** #623. A green Windows job today proves the link, not the tests.

## Scope

Add the ROM-free, display-free `redship` tier to the `build-windows` job in `.github/workflows/generate-builds.yml` (`build-windows:` at `:505`), mirroring what `build-linux` already does at `:381`: `ctest --test-dir build-cmake --output-on-failure --label-regex "^redship$" --no-tests=error`. If the tier reads the staged port archives, stage them the way Linux does (`:293-312`, #560). The `rando` tier needs a display (`xvfb-run` on Linux, `:390`); leave it Linux-only unless you have a Windows display path you can prove, and say so in the PR.

## Owns

`.github/workflows/generate-builds.yml` only.

## Watch for

- Windows links with `/FORCE:MULTIPLE` (#387), so a test that passes there and fails on Linux is a Linux-only signal — not your problem, but do not read a Windows green as symbol coverage.
- Path separators and the `build-cmake` directory name in `--test-dir`.
- The ~46 min Windows build; keep the tier's timeout in line with `redship_add_test()`'s defaults, do not raise it to hide a hang.
- `--no-tests=error` is load-bearing: the whole point of #623 is that "no tests found" must be red.

## Verify

CI is the verification for this lane — the PR's own `build-windows` run must show the tier executing with a non-zero row count. No local build is needed, but do not merge on a partial run.
