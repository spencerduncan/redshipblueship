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
TITLESETUP_GUARD = "MM_Combo_OwlSaveExitToOoT"
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
      * guarded by MM_Combo_OwlSaveExitToOoT (the #532 fix);
      * marked RSBS-TITLESETUP-EXEMPT with a reason (boot-chain bails, and the
        game-over "don't continue" path, which is a known unfixed hole);
      * inside MM's own front end (title screen / file select), where there is
        no live session to destroy.

    Counterfactual: revert the `if (!MM_Combo_OwlSaveExitToOoT())` wrapper in
    games/mm/src/code/z_play.c and this test fails, naming the file and line.
    """
    offenders = []
    for rel, lineno, lines in _titlesetup_transitions():
        if rel in TITLESETUP_FRONT_END:
            continue
        window = "\n".join(lines[max(0, lineno - 1 - TITLESETUP_LOOKBACK) : lineno])
        if TITLESETUP_GUARD in window or TITLESETUP_EXEMPT_MARKER in window:
            continue
        offenders.append(f"{rel}:{lineno}")
    assert not offenders, (
        "MM_TitleSetup_Init transitions with neither a "
        f"{TITLESETUP_GUARD} guard nor a {TITLESETUP_EXEMPT_MARKER} marker "
        f"(#532 -- these author a vanilla bootstrap over a live cross-game "
        f"session): {offenders}"
    )


def test_titlesetup_invariant_sees_the_known_transitions():
    """Guard against the guard: a scanner that finds nothing passes vacuously.

    If a rename or a macro change made `_titlesetup_transitions` stop matching,
    the assertion above would go green while the invariant it claims to enforce
    was no longer enforced at all. Pin the two call sites the #532 fix owns.
    """
    found = {rel for rel, _, _ in _titlesetup_transitions()}
    for required in (
        "games/mm/src/code/z_play.c",
        "games/mm/src/overlays/kaleido_scope/ovl_kaleido_scope/z_kaleido_scope_NES.c",
    ):
        assert required in found, f"the TitleSetup transition scanner no longer sees {required}"

    guarded = [
        f"{rel}:{lineno}"
        for rel, lineno, lines in _titlesetup_transitions()
        if TITLESETUP_GUARD in "\n".join(lines[max(0, lineno - 1 - TITLESETUP_LOOKBACK) : lineno])
    ]
    assert len(guarded) >= 2, f"expected at least the two #532-guarded transitions, found {guarded}"


def test_mm_2s2h_glob_is_configure_depends():
    """Without CONFIGURE_DEPENDS an existing build dir never re-globs, so a pull
    that adds a 2s2h/ TU links the stale file list and fails with an
    unresolved-symbol error that names the symbol rather than the missing TU."""
    for line in MM_CMAKELISTS.read_text(encoding="utf-8").splitlines():
        if line.startswith("file(GLOB_RECURSE ship__ "):
            assert "CONFIGURE_DEPENDS" in line, "the 2s2h/ source glob lost CONFIGURE_DEPENDS"
            return
    pytest.fail("could not find the `file(GLOB_RECURSE ship__ ...)` call in games/mm/CMakeLists.txt")
