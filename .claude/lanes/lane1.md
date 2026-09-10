You are Lane 1 of eight in the 2026-09-10 wave: **ADR 0009 decision 4b — the MM death-decline exit is an autosave point iff Autosave is on.**

- **Branch:** `claude/adr0009-4b-death-decline-autosave`
- **Serves:** ADR 0009 (`docs/adr/0009-combo-settings-and-reverse-pool.md`), decision 4 (a durable commit is the WHOLE file) and 4a (quit-to-title is a durable commit, decided 2026-08-05). 4b is the one branch 4a left implicit: the kaleido death prompt's *decline* ("don't continue") leg, which today reaches OoT through `MM_Combo_GameOverExitToOoT` (PR #625, #590).

## Scope

Decide and record 4b in the ADR, then make the code match: when the player declines to continue on MM's death prompt and the switch hands them to OoT, that exit is a durable autosave commit **if and only if** the Autosave enhancement is on. With Autosave off it is not a commit — the player who declined asked for no save, and a whole-file commit would write one. Either way the health revive PR #625 applies stays conditional on a dead bar.

## Owns

- `docs/adr/0009-combo-settings-and-reverse-pool.md` — new decision 4b, amended status line.
- The MM kaleido death-prompt leg (`games/mm/src/overlays/kaleido_scope/`, the death-prompt handling that reaches the exit).
- The MM exit/commit functions in `games/mm/2s2h/GameExports_SingleExe.cpp`: `MM_Combo_OwlSaveExitToOoT` (`:1805`), `MM_Combo_GameOverExitToOoT` (`:1903`) and the commit choke point they marshal through (#569). **Function-scoped claim** — lane 6 edits the hot-swap freeze in the same file; rebase, don't reorder.
- Any lock you add rides the existing `MM*` rows (append-only registration per `SHARED.md`).

## Do not touch

The F10 dead-bar route (#626) is lane 6's. `MM_Play_ConsumeStartupEntrance` is lane 8's. ADR 0010/0011 are not yours.

## Verify

Local ROM-staged build plus both ctest tiers (`redship`, `rando`) before the PR is merged; the autosave re-arm behaviour from #614/#629 must not regress. Commit subjects name the concrete change; PR body ends with the generated-with line.
