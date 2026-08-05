#!/usr/bin/env python3
"""Repo invariants from issue #379 that no other CI job can see.

Each assertion here locks a cleanup whose regression is silent everywhere else:
the C/C++ jobs cannot fail on a re-added *unreferenced* stub, and nothing else
in CI reads the CMake templates or the Windows-only debug helpers. These are
source-text/AST assertions on purpose -- the artifacts being guarded either
never reach the link (dead stubs) or are not code at all (templates, CMake).

The helpers under .claude/tools/ are parsed, never imported: they bind
ctypes.windll at module scope and would raise on the Linux runner.
"""

import ast
import re
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]

CLAUDE_TOOLS = [
    REPO_ROOT / ".claude" / "tools" / "dbg374.py",
    REPO_ROOT / ".claude" / "tools" / "stacks.py",
]

PROPERTIES_TEMPLATES = [
    (REPO_ROOT / "games" / "oot" / "properties.h.in", REPO_ROOT / "games" / "oot" / "properties.h"),
    (
        REPO_ROOT / "games" / "mm" / "windows" / "properties.h.in",
        REPO_ROOT / "games" / "mm" / "windows" / "properties.h",
    ),
]

MM_STUBS = REPO_ROOT / "src" / "common" / "mm_stubs.c"
MM_CMAKELISTS = REPO_ROOT / "games" / "mm" / "CMakeLists.txt"

MM_SRC = REPO_ROOT / "games" / "mm" / "src"
# The cross-game exit DECISIONS. Each one answers "may this transition enter
# MM's TitleSetup?" for one class of exit, and each returns 0 for a standalone
# MM session so upstream 2ship behavior is never hijacked:
#   MM_Combo_OwlSaveExitToOoT   -- the SAVE exits (#532/#543): owl statue, pause
#                                  save-and-quit.
#   MM_Combo_GameOverExitToOoT  -- the DEATH exit (#590): the game-over
#                                  "don't continue" prompt.
# They are separate functions because they are separate decisions, not because
# the guard was copied: the death exit additionally has to leave MM's live
# SaveContext RESUMABLE (see games/mm/2s2h/GameExports_SingleExe.cpp), which
# would be wrong on a save exit.
TITLESETUP_GUARDS = ("MM_Combo_OwlSaveExitToOoT", "MM_Combo_GameOverExitToOoT")
TITLESETUP_EXEMPT_MARKER = "RSBS-TITLESETUP-EXEMPT"
# How far above the transition the guard call (or the exempt marker) may sit.
# Both live directly above it today; the slack is for a `STOP_GAMESTATE` line
# and a short comment, not for an unrelated `if` several statements earlier.
TITLESETUP_LOOKBACK = 24

# MM's OWN front end. A transition into TitleSetup from the title screen or the
# file-select is not an exit from a live session -- by the time either of these
# runs there is no cross-game world left to author a bootstrap over. Exempting
# whole files here (rather than per-line) is deliberate: every transition in
# them is front-end-internal, and a NEW one would be too.
TITLESETUP_FRONT_END = {
    "games/mm/src/overlays/gamestates/ovl_title/z_title.c",
    "games/mm/src/overlays/gamestates/ovl_file_choose/z_file_choose_NES.c",
}

# The CLOSED allowlist of sites the exempt marker may appear at.
#
# The marker used to be an open escape hatch: any comment carrying the string
# satisfied the lock, anywhere under games/mm/src. That is how the game-over
# "don't continue" transition (#590) sat for a release marked KNOWN HOLE while
# the invariant reported green -- the scanner passed AROUND the very path it
# was written to name. #590 closed that transition with a real guard, and this
# allowlist closes the shape: a marker at any site not listed here fails
# test_titlesetup_exempt_marker_is_pinned, so "write a comment" can never again
# be the way an ungated transition ships.
#
# Adding an entry is a deliberate act with a review attached. The bar is the
# one the sole surviving entry meets: the transition is UNREACHABLE from a live
# cross-game session, argued from code, not merely undesirable to fix.
TITLESETUP_EXEMPT_SITES = {
    # z_play.c's `save.entrance == -1` boot-chain bail. A cross-game arrival
    # cannot reach it: MM_Play_ConsumeStartupEntrance assigns a real startup
    # entrance earlier in the same function, and a standalone MM session has no
    # live cross-game state for TitleSetup to overwrite.
    "games/mm/src/code/z_play.c",
}

# The site #590 fixed. Pinned by name so the fix cannot be quietly reverted
# into an exemption again: this file must contain a GUARDED game-over
# transition and NO exempt marker at all.
TITLESETUP_GAME_OVER_FILE = "games/mm/src/overlays/kaleido_scope/ovl_kaleido_scope/z_kaleido_scope_NES.c"


def _parse(path):
    return ast.parse(path.read_text(encoding="utf-8"), filename=str(path))


def _is_dunder_main_test(node):
    """True for the `__name__ == "__main__"` comparison itself."""
    return (
        isinstance(node, ast.Compare)
        and isinstance(node.left, ast.Name)
        and node.left.id == "__name__"
        and len(node.ops) == 1
        and isinstance(node.ops[0], ast.Eq)
        and isinstance(node.comparators[0], ast.Constant)
        and node.comparators[0].value == "__main__"
    )


def _function(tree, name):
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name == name:
            return node
    return None


@pytest.mark.parametrize("path", CLAUDE_TOOLS, ids=lambda p: p.name)
def test_claude_tool_does_not_run_main_at_import(path):
    """Importing (or linting) the helper must not launch a debugger session."""
    tree = _parse(path)
    bare_calls = [
        node.value.func.id
        for node in tree.body
        if isinstance(node, ast.Expr) and isinstance(node.value, ast.Call) and isinstance(node.value.func, ast.Name)
    ]
    assert "main" not in bare_calls, f"{path.name} calls main() at import scope"

    guards = [node for node in tree.body if isinstance(node, ast.If) and _is_dunder_main_test(node.test)]
    assert guards, f"{path.name} has no `if __name__ == \"__main__\":` guard"
    guarded = {n.id for guard in guards for n in ast.walk(guard) if isinstance(n, ast.Name)}
    assert "main" in guarded, f"{path.name}'s __main__ guard never calls main()"


@pytest.mark.parametrize("path", CLAUDE_TOOLS, ids=lambda p: p.name)
def test_claude_tool_checks_argv_length(path):
    """`sys.argv[1]` must be reached only after a length check, not IndexError."""
    main = _function(_parse(path), "main")
    assert main is not None, f"{path.name} has no main()"
    checks_len = any(
        isinstance(node, ast.Call)
        and isinstance(node.func, ast.Name)
        and node.func.id == "len"
        and node.args
        and isinstance(node.args[0], ast.Attribute)
        and node.args[0].attr == "argv"
        for node in ast.walk(main)
    )
    assert checks_len, f"{path.name}'s main() indexes sys.argv without a len(sys.argv) check"


@pytest.mark.parametrize("path", CLAUDE_TOOLS, ids=lambda p: p.name)
def test_claude_tool_documents_windows_only(path):
    """These bind ctypes.windll at module scope; the docstring must say so."""
    doc = ast.get_docstring(_parse(path))
    assert doc is not None, f"{path.name} has no module docstring"
    assert "Windows-only" in doc, f"{path.name}'s docstring does not flag it as Windows-only"


@pytest.mark.parametrize(
    "template,generated", PROPERTIES_TEMPLATES, ids=lambda p: str(p).replace("\\", "/").rsplit("games/", 1)[-1]
)
def test_properties_template_carries_pragma_once(template, generated):
    """configure_file writes the template over the tracked header in the source
    tree, so a template without `#pragma once` silently reverts upstream #178
    on every configure and leaves the working tree dirty.

    The whole file is checked, not just the pragma: the invariant is that the
    committed header is exactly what `configure_file(... @ONLY)` would emit, so
    a configure is a no-op on a clean tree. Each `@VAR@` is compared as a
    wildcard because its expansion is a build-time value this test cannot
    resolve -- which means literal text deleted from directly beside a `@VAR@`
    is absorbed by that wildcard and slips through. Added, removed and
    reordered lines, and literal drift anywhere else, all fail here."""
    template_lines = template.read_text(encoding="utf-8").splitlines()
    generated_lines = generated.read_text(encoding="utf-8").splitlines()
    assert template_lines and template_lines[0] == "#pragma once", f"{template} is missing #pragma once"
    assert len(template_lines) == len(generated_lines), (
        f"{template} has {len(template_lines)} lines but {generated.name} has "
        f"{len(generated_lines)}; the next CMake configure would rewrite it"
    )
    for lineno, (raw, emitted) in enumerate(zip(template_lines, generated_lines), start=1):
        pattern = "".join(
            ".*" if part.startswith("@") and part.endswith("@") else re.escape(part)
            for part in re.split(r"(@[A-Za-z0-9_]+@)", raw)
        )
        assert re.fullmatch(pattern, emitted), (
            f"{template}:{lineno} does not produce {generated.name}:{lineno}; a "
            f"CMake configure would dirty the working tree\n  template:  {raw}\n  generated: {emitted}"
        )


def test_mm_stubs_defines_no_unprefixed_frame_interpolation():
    """mm_stubs.c compiles without RSBS_SINGLE_EXECUTABLE, so nothing rebinds
    FrameInterpolation_* to MM_FrameInterpolation_* here. MM's call sites are
    all rebound and SoH declares neither of the two names, so any definition in
    this file is a dead stub free to drift out of shape (#379)."""
    text = MM_STUBS.read_text(encoding="utf-8")
    stripped = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    offenders = re.findall(r"^\s*\w[\w\s*]*\bFrameInterpolation_\w+\s*\(", stripped, flags=re.MULTILINE)
    assert not offenders, f"mm_stubs.c defines unprefixed FrameInterpolation stubs: {offenders}"


def _titlesetup_transitions():
    """Every `SET_NEXT_GAMESTATE(..., MM_TitleSetup_Init, ...)` under games/mm/src.

    Yields (repo-relative posix path, 1-based line number, the file's lines).
    """
    for path in sorted(MM_SRC.rglob("*.c")):
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        rel = path.relative_to(REPO_ROOT).as_posix()
        for lineno, line in enumerate(lines, start=1):
            if "SET_NEXT_GAMESTATE" in line and "MM_TitleSetup_Init" in line:
                yield rel, lineno, lines


def _strip_comments(text):
    """Blank out // and /* */ comments, preserving line structure.

    Load-bearing for the guard check below, NOT cosmetic. The window search is
    a substring test, so without this a bare mention of the guard's name in a
    comment above an UNGUARDED transition satisfies it and the lock passes
    vacuously — verified: adding the single line `// see
    MM_Combo_OwlSaveExitToOoT` above an unwrapped transition in z_play.c turned
    a red run green while #532's mechanism was fully re-armed. That is not a
    contrived edit; the natural way to document a transition you left alone is
    to name the guard the other ones use.

    The exempt marker deliberately keeps searching the RAW window — it is a
    comment by design. Only the guard has to be real code.
    """
    out = []
    in_block = False
    for line in text.splitlines():
        buf = []
        i = 0
        while i < len(line):
            if in_block:
                end = line.find("*/", i)
                if end == -1:
                    i = len(line)
                else:
                    in_block = False
                    i = end + 2
            elif line.startswith("//", i):
                break
            elif line.startswith("/*", i):
                in_block = True
                i += 2
            else:
                buf.append(line[i])
                i += 1
        out.append("".join(buf))
    return "\n".join(out)


def _guard_is_wired_above(lines, lineno):
    """True when a real CALL to one of the exit guards sits just above `lineno`.

    Comments are stripped (see _strip_comments) and the `extern` prototype is
    excluded, so neither a doc mention nor a forward declaration that happens to
    drift within the lookback can stand in for the call site itself.
    """
    window = lines[max(0, lineno - 1 - TITLESETUP_LOOKBACK) : lineno]
    code = _strip_comments("\n".join(window))
    return any(
        any(guard in stripped for guard in TITLESETUP_GUARDS)
        and not stripped.lstrip().startswith("extern")
        for stripped in code.splitlines()
    )


def _exempt_marker_sites():
    """Every (repo-relative path, 1-based line) carrying the exempt marker."""
    for path in sorted(MM_SRC.rglob("*.c")):
        rel = path.relative_to(REPO_ROOT).as_posix()
        for lineno, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), start=1):
            if TITLESETUP_EXEMPT_MARKER in line:
                yield rel, lineno


def test_titlesetup_transitions_are_guarded_or_marked_exempt():
    """#532: entering MM's TitleSetup from a LIVE cross-game session destroys it.

    TitleSetup_SetupTitleScreen calls MM_Sram_InitNewSave, so a fresh vanilla
    file becomes the live gSaveContext while gComboCtx and OoT's frozen blob
    stay resident (MM has no twin of Context_InvalidateSessionOnReturnToTitle).
    The next hop back to OoT freezes that bootstrap into the MM shadow, and
    OoT's next save serializes it into the .redsave's Tier-3 -- overwriting the
    MM half the owl save persisted seconds earlier.

    Nothing in the C build can fail on a NEW ungated transition: the fix is a
    call-site decision, so a future edit that adds another
    `SET_NEXT_GAMESTATE(..., MM_TitleSetup_Init, ...)` -- or that deletes the
    guard from an existing one -- compiles, links, and passes every ctest row.
    That is precisely why the lock lives here as a source-text assertion.

    Three outcomes are allowed, and each must be spelled out in the source:
      * guarded by one of TITLESETUP_GUARDS (#532's save exits, #590's death
        exit);
      * marked RSBS-TITLESETUP-EXEMPT at a site pre-registered in
        TITLESETUP_EXEMPT_SITES -- today only z_play.c's boot-chain bail;
      * inside MM's own front end (title screen / file select), where there is
        no live session to destroy.

    The exempt branch is deliberately NOT "any file with the marker" any more
    (#590). While it was, a transition could ship a KNOWN HOLE comment and this
    lock reported green around it for as long as anyone cared to leave it --
    which is exactly what happened to the game-over "don't continue" path
    between #543 and #590. A marker now only silences a site the allowlist
    above already named, and test_titlesetup_exempt_marker_is_pinned fails on a
    marker anywhere else.

    Counterfactual: revert the `if (!MM_Combo_OwlSaveExitToOoT())` wrapper in
    games/mm/src/code/z_play.c and this test fails, naming the file and line.
    Same for #590's `if (!MM_Combo_GameOverExitToOoT())` in
    z_kaleido_scope_NES.c -- and, because that file is not in
    TITLESETUP_EXEMPT_SITES, re-adding the old marker there does NOT buy it
    back. A third counterfactual the guard-in-comments bypass used to pass:
    putting a guard's NAME in a comment above an unwrapped transition no longer
    satisfies this, because the guard is matched against comment-stripped code.
    """
    offenders = []
    for rel, lineno, lines in _titlesetup_transitions():
        if rel in TITLESETUP_FRONT_END:
            continue
        raw_window = "\n".join(lines[max(0, lineno - 1 - TITLESETUP_LOOKBACK) : lineno])
        exempt = rel in TITLESETUP_EXEMPT_SITES and TITLESETUP_EXEMPT_MARKER in raw_window
        if _guard_is_wired_above(lines, lineno) or exempt:
            continue
        offenders.append(f"{rel}:{lineno}")
    assert not offenders, (
        "MM_TitleSetup_Init transitions with neither a "
        f"{' / '.join(TITLESETUP_GUARDS)} guard nor an allowlisted "
        f"{TITLESETUP_EXEMPT_MARKER} marker "
        f"(#532/#590 -- these author a vanilla bootstrap over a live cross-game "
        f"session): {offenders}"
    )


def test_titlesetup_exempt_marker_is_pinned():
    """The exemption is a closed allowlist, not a comment anyone may write.

    Before #590 the marker was an open escape hatch: the lock above accepted it
    in any file under games/mm/src, so "document why you left it" and "silence
    the invariant" were the same act. The game-over transition used that for a
    full release cycle while its own comment said KNOWN HOLE.

    Now the marker only means anything at a site TITLESETUP_EXEMPT_SITES names,
    and a marker outside that set is a hard failure here -- so re-opening a hole
    costs an allowlist edit that a reviewer sees, rather than a comment nobody
    diffs.
    """
    stray = [
        f"{rel}:{lineno}"
        for rel, lineno in _exempt_marker_sites()
        if rel not in TITLESETUP_EXEMPT_SITES
    ]
    assert not stray, (
        f"{TITLESETUP_EXEMPT_MARKER} appears outside TITLESETUP_EXEMPT_SITES: {stray}. "
        "The marker is not a way to silence the TitleSetup invariant -- guard the "
        "transition, or register the site (with the reachability argument) in "
        "TITLESETUP_EXEMPT_SITES."
    )

    # The allowlist must not rot into a set of names nothing matches: an entry
    # whose marker is gone is a stale exemption still granting cover.
    marked_files = {rel for rel, _ in _exempt_marker_sites()}
    stale = sorted(TITLESETUP_EXEMPT_SITES - marked_files)
    assert not stale, f"TITLESETUP_EXEMPT_SITES names files that no longer carry the marker: {stale}"


def test_titlesetup_game_over_exit_is_guarded():
    """#590: the death exit is CLOSED, and this test says so by name.

    The game-over "don't continue" prompt carried #532's mechanism on a path
    reached by ordinary play rather than by a deliberate save action: it entered
    TitleSetup, MM_Sram_InitNewSave authored a vanilla bootstrap over the live
    cross-game session, and the next OoT commit serialized that bootstrap into
    Tier-3 as MM's half. #568's write latch and #569's choke point do not
    contain it -- an ordinary session's slot is ARMED, so the commit is
    permitted; #569 only makes the destroying write atomic and stamps it as the
    newest generation.

    This lock is the positive twin of the exemption that used to stand here. It
    asserts the guarantee rather than the hole: the kaleido file must contain a
    guarded game-over transition and must carry NO exempt marker, so a revert to
    "leave it, but document it" fails here as well as above.
    """
    lines_by_site = {
        (rel, lineno): lines
        for rel, lineno, lines in _titlesetup_transitions()
        if rel == TITLESETUP_GAME_OVER_FILE
    }
    assert lines_by_site, f"the TitleSetup transition scanner no longer sees {TITLESETUP_GAME_OVER_FILE}"

    unguarded = [
        f"{rel}:{lineno}"
        for (rel, lineno), lines in lines_by_site.items()
        if not _guard_is_wired_above(lines, lineno)
    ]
    assert not unguarded, (
        f"#590: every MM_TitleSetup_Init transition in {TITLESETUP_GAME_OVER_FILE} must be "
        f"guarded (the game-over 'don't continue' exit authors a vanilla bootstrap over the "
        f"live cross-game session): {unguarded}"
    )

    marked = [f"{rel}:{lineno}" for rel, lineno in _exempt_marker_sites() if rel == TITLESETUP_GAME_OVER_FILE]
    assert not marked, (
        f"#590 retired the {TITLESETUP_EXEMPT_MARKER} marker from {TITLESETUP_GAME_OVER_FILE}; "
        f"it must not come back: {marked}"
    )


def test_titlesetup_invariant_sees_the_known_transitions():
    """Guard against the guard: a scanner that finds nothing passes vacuously.

    If a rename or a macro change made `_titlesetup_transitions` stop matching,
    the assertion above would go green while the invariant it claims to enforce
    was no longer enforced at all. Pin the call sites the fixes own.
    """
    found = {rel for rel, _, _ in _titlesetup_transitions()}
    for required in (
        "games/mm/src/code/z_play.c",
        TITLESETUP_GAME_OVER_FILE,
    ):
        assert required in found, f"the TitleSetup transition scanner no longer sees {required}"

    guarded = [
        f"{rel}:{lineno}"
        for rel, lineno, lines in _titlesetup_transitions()
        if _guard_is_wired_above(lines, lineno)
    ]
    # Three: the owl-save exit (z_play.c), the pause save-and-quit
    # (z_kaleido_scope_NES.c, #532/#543), and the game-over "don't continue"
    # exit (z_kaleido_scope_NES.c, #590).
    assert len(guarded) >= 3, f"expected at least the three guarded transitions, found {guarded}"


def test_titlesetup_guard_cannot_be_satisfied_by_a_comment():
    """The guard check must read CODE, not prose.

    `_guard_is_wired_above` is a substring search over a window; the whole
    reason it strips comments is that a doc mention of the guard's name would
    otherwise stand in for the call. Pin that directly, so a future
    simplification back to a raw-text search fails here instead of silently
    re-opening #532's mechanism to anything with a helpful comment.
    """
    real_call = [
        "if (!MM_Combo_OwlSaveExitToOoT()) {",
        "    STOP_GAMESTATE(&this->state);",
        "    SET_NEXT_GAMESTATE(&this->state, MM_TitleSetup_Init, sizeof(TitleSetupState));",
        "}",
    ]
    assert _guard_is_wired_above(real_call, 3), "a real guarded call site must count as guarded"

    line_comment = [
        "// see MM_Combo_OwlSaveExitToOoT",
        "STOP_GAMESTATE(&this->state);",
        "SET_NEXT_GAMESTATE(&this->state, MM_TitleSetup_Init, sizeof(TitleSetupState));",
    ]
    assert not _guard_is_wired_above(line_comment, 3), "a // mention of the guard must NOT count as guarded"

    block_comment = [
        "/* unlike the owl exit, which uses MM_Combo_OwlSaveExitToOoT,",
        " * this one is different. */",
        "SET_NEXT_GAMESTATE(&this->state, MM_TitleSetup_Init, sizeof(TitleSetupState));",
    ]
    assert not _guard_is_wired_above(block_comment, 3), "a /* */ mention of the guard must NOT count as guarded"

    prototype = [
        "extern int MM_Combo_OwlSaveExitToOoT(void);",
        "SET_NEXT_GAMESTATE(&this->state, MM_TitleSetup_Init, sizeof(TitleSetupState));",
    ]
    assert not _guard_is_wired_above(prototype, 2), "the extern prototype must NOT count as guarded"

    # Every guard in the set has to satisfy the matcher, not just the first one
    # it was written for: a #590-style addition that the matcher silently
    # ignored would leave its call site reading as UNGUARDED and the invariant
    # would fail for a reason that has nothing to do with the code.
    for guard in TITLESETUP_GUARDS:
        guarded_call = [
            f"if (!{guard}()) {{",
            "    STOP_GAMESTATE(&play->state);",
            "    SET_NEXT_GAMESTATE(&play->state, MM_TitleSetup_Init, sizeof(TitleSetupState));",
            "}",
        ]
        assert _guard_is_wired_above(guarded_call, 3), f"{guard} must count as a real guard"


def test_mm_2s2h_glob_is_configure_depends():
    """Without CONFIGURE_DEPENDS an existing build dir never re-globs, so a pull
    that adds a 2s2h/ TU links the stale file list and fails with an
    unresolved-symbol error that names the symbol rather than the missing TU."""
    for line in MM_CMAKELISTS.read_text(encoding="utf-8").splitlines():
        if line.startswith("file(GLOB_RECURSE ship__ "):
            assert "CONFIGURE_DEPENDS" in line, "the 2s2h/ source glob lost CONFIGURE_DEPENDS"
            return
    pytest.fail("could not find the `file(GLOB_RECURSE ship__ ...)` call in games/mm/CMakeLists.txt")
