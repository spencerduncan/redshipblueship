#!/usr/bin/env python3
"""UI parity lint: this project's ImGui surfaces against the SoH menu conventions.

docs/ui-style-guide.md is the rulebook; this is its static half (the runtime half
runs inside `redship --test ui-snapshot`, which can see names a PreFunc computes).
Scope is ONLY what this project added: the files and the function slice listed in
.github/scripts/ui-lint-files.txt. The vendored SoH menus are the reference and
are never linted -- they define the conventions.

Rules (level; what it catches):
  S0  error  tripwire: an ImGui-drawing TU under src/common or a *SingleExe*.cpp
             that ui-lint-files.txt does not list (so no new surface escapes)
  S1  error  a raw player-facing ImGui widget outside a
             UIWidgets::PushStyleButton/PopStyleButton bracket
  S2  error  a literal colour: all-literal ImVec4(r,g,b,a), IM_COL32, or a
             literal toast colour array element
  S3  error  ImGui::TextDisabled used for meaning
  S4  error  ImGui::CollapsingHeader on a settings surface (menu files and the
             MM options pane; tracker/spoiler panes are exempt)
  S5  warn   hand spacing (Spacing/Dummy/Indent/Unindent/NewLine, SameLine(x))
  S6  error  raw ImGui::SetTooltip (UIWidgets::Tooltip wraps at 80 columns)
  S7  error  an issue number (#NNN), an ADR reference or a section sign inside a
             player-visible string literal (adjacent literals joined; logging,
             asserts and test sinks skipped; `// ui-lint: internal-id` on the
             line exempts an internal window id)
  S8  hint   ApplyPresentation call sites (the state-in-the-name mechanism);
             printed, never failed
  S9  warn   a literal CVar key in a menu row (.CVar("..."))
  S10 warn   a Reset/Clear/Delete/Erase button row with no RegisterPopup confirm
  S11 error  a WIDGET_WINDOW_BUTTON row not shaped like SoH's: name starts
             "Toggle "/"Popout ", and the chain sets .WindowName( and
             .HideInSearch(true)

Hits are keyed WITHOUT line numbers (rule | path | normalised source text), so
unrelated edits do not churn the baseline. `.github/scripts/ui-lint-baseline.txt`
is a multiset of accepted hits: the check fails on any error/warn hit not in it
AND on any baseline entry no longer hit (the baseline may only shrink; delete the
stale lines). The hits in it today ARE the migration list.

Usage:
  check-ui-parity-lint.py                  check against the baseline
  check-ui-parity-lint.py --report         print every hit, never fail
  check-ui-parity-lint.py --write-baseline regenerate the baseline (review the diff:
                                           it must only lose lines)
  check-ui-parity-lint.py --self-test      synthetic fixtures, both halves of every rule
"""
import argparse
import collections
import contextlib
import io
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
FILES_LIST = ".github/scripts/ui-lint-files.txt"
BASELINE = ".github/scripts/ui-lint-baseline.txt"

LEVEL = {
    "S0": "error", "S1": "error", "S2": "error", "S3": "error", "S4": "error", "S5": "warn", "S6": "error",
    "S7": "error", "S8": "hint", "S9": "warn", "S10": "warn", "S11": "error",
}

# ---------------------------------------------------------------------------
# Source handling
# ---------------------------------------------------------------------------

TOKEN = re.compile(r'//[^\n]*|/\*.*?\*/|\'(?:[^\'\\\n]|\\.)*\'|"(?:[^"\\\n]|\\.)*"', re.S)


def strip_comments(src):
    """Replace every comment with spaces (newlines kept), leaving strings intact,
    so line numbers survive and a rule never fires on prose."""
    out = []
    last = 0
    for m in TOKEN.finditer(src):
        t = m.group(0)
        out.append(src[last:m.start()])
        if t.startswith("//") or t.startswith("/*"):
            out.append(re.sub(r"[^\n]", " ", t))
        else:
            out.append(t)
        last = m.end()
    out.append(src[last:])
    return "".join(out)


def norm(text):
    return re.sub(r"\s+", " ", text).strip()


class Unit:
    """One linted body of text: a whole file, or a function slice of one."""

    def __init__(self, path, text, first_line=1):
        self.path = path
        self.raw = text
        self.code = strip_comments(text)
        self.first_line = first_line
        self.raw_lines = text.split("\n")
        self.code_lines = self.code.split("\n")

    def line_of(self, pos, code=True):
        return self.first_line + (self.code if code else self.raw).count("\n", 0, pos)


def slice_function(path, text, name):
    m = re.search(r"\n([^\n]*\b" + re.escape(name) + r"\s*\([^)]*\)\s*\{.*?\n\})", text, re.S)
    if not m:
        return None
    return Unit(path + "::" + name, m.group(1), text.count("\n", 0, m.start(1)) + 1)


# ---------------------------------------------------------------------------
# Rules
# ---------------------------------------------------------------------------

RAW_WIDGET = re.compile(r"ImGui::(Checkbox|SliderInt|SliderFloat|SliderScalar|DragInt|DragFloat|Combo|BeginCombo|"
                        r"Button|SmallButton|ArrowButton|RadioButton|InputText|InputInt|InputScalar|ColorEdit3|"
                        r"ColorEdit4)\s*\(")
NUM = r"\s*[0-9.]+f?\s*"
LIT_COLOUR = re.compile(r"ImVec4\s*(\(|[A-Za-z_][A-Za-z0-9_]*\s*\()" + NUM + "," + NUM + "," + NUM + "," + NUM +
                        r"\)|IM_COL32\(|(prefix|message|suffix)Color\[[0-3]\]\s*=\s*[0-9]")
TEXT_DISABLED = re.compile(r"ImGui::TextDisabled\(")
COLLAPSING = re.compile(r"ImGui::CollapsingHeader\s*\(")
SPACING = re.compile(r"ImGui::(Spacing|Dummy|Indent|Unindent|NewLine)\s*\(|ImGui::SameLine\(\s*[^) ]")
SET_TOOLTIP = re.compile(r"ImGui::SetTooltip\(")
APPLY_PRESENTATION = re.compile(r"ApplyPresentation\(")
LITERAL_CVAR = re.compile(r"\.CVar\(\s*\"")
REF = re.compile(r"#[0-9]{3,}|ADR ?[0-9]{4}|§")
# Literals handed to a log, an assert or a test macro are not player-visible.
# Word-bounded, so snprintf (which builds menu text) is NOT a sink.
SINK = re.compile(r"\b(SPDLOG_\w+|static_assert|assert|fprintf|printf|puts|Fail|[A-Z]+_CHECK)\s*\([^;{}]*$", re.S)
SUPPRESS = "ui-lint: internal-id"


def line_rule(unit, rule, regex, hits):
    for i, line in enumerate(unit.code_lines):
        if regex.search(line):
            hits.append((rule, unit.path, unit.first_line + i, norm(line)))


def rule_s1(unit, hits):
    bracket = False
    for i, line in enumerate(unit.code_lines):
        if "UIWidgets::PushStyleButton(" in line:
            bracket = True
        if not bracket and RAW_WIDGET.search(line):
            hits.append(("S1", unit.path, unit.first_line + i, norm(line)))
        if "UIWidgets::PopStyleButton(" in line:
            bracket = False


def literals(src):
    """(position, joined text) of each run of adjacent string literals; comments
    and character literals end a run."""
    group, start, last_end = [], None, 0
    for m in TOKEN.finditer(src):
        t = m.group(0)
        if t.startswith('"'):
            if group and src[last_end:m.start()].strip() == "":
                group.append(t[1:-1])
            else:
                if group:
                    yield start, "".join(group)
                group, start = [t[1:-1]], m.start()
            last_end = m.end()
        else:
            if group:
                yield start, "".join(group)
            group = []
    if group:
        yield start, "".join(group)


def rule_s7(unit, hits):
    src = unit.raw
    for pos, text in literals(src):
        if not REF.search(text):
            continue
        if SINK.search(src[max(0, pos - 300):pos]):
            continue
        line_no = src.count("\n", 0, pos)
        lines = unit.raw_lines
        here = lines[line_no] if line_no < len(lines) else ""
        prev = lines[line_no - 1] if line_no > 0 else ""
        if SUPPRESS in here or SUPPRESS in prev:
            continue
        hits.append(("S7", unit.path, unit.first_line + line_no, norm(text)[:100]))


DESTRUCTIVE = re.compile(r"(AddWidget\(\s*path\s*,\s*\"([^\"]*(Reset|Clear|Delete|Erase)[^\"]*)\"\s*,\s*"
                         r"WIDGET_BUTTON\)[^;]*;)", re.S)
WINDOW_BUTTON = re.compile(r"AddWidget\(\s*path\s*,\s*(\"[^\"]*\"|[^,]+?)\s*,\s*WIDGET_WINDOW_BUTTON\)[^;]*;", re.S)


def rule_s10(unit, hits):
    for m in DESTRUCTIVE.finditer(unit.code):
        if "RegisterPopup(" not in m.group(1):
            hits.append(("S10", unit.path, unit.line_of(m.start()), m.group(2)))


def rule_s11(unit, hits):
    for m in WINDOW_BUTTON.finditer(unit.code):
        chain = m.group(0)
        name = m.group(1).strip()
        problems = []
        if name.startswith('"') and not (name.startswith('"Toggle ') or name.startswith('"Popout ')):
            problems.append("name")
        if ".WindowName(" not in chain:
            problems.append("WindowName")
        if not re.search(r"\.HideInSearch\(\s*true\s*\)", chain):
            problems.append("HideInSearch")
        if problems:
            hits.append(("S11", unit.path, unit.line_of(m.start()), name + " missing/bad: " + ",".join(problems)))


# ---------------------------------------------------------------------------
# File sets
# ---------------------------------------------------------------------------


def read_file_sets(root):
    sets = collections.defaultdict(list)
    with open(os.path.join(root, FILES_LIST), encoding="utf-8") as f:
        for raw in f:
            line = raw.split("#", 1)[0].strip()
            if not line:
                continue
            parts = line.split(None, 1)
            if len(parts) != 2:
                raise SystemExit(f"{FILES_LIST}: malformed line: {raw.rstrip()}")
            sets[parts[0]].append(parts[1].split()[0])
    return sets


def load_units(root, entries):
    units = []
    for entry in entries:
        path, _, func = entry.partition("::")
        full = os.path.join(root, path)
        if not os.path.isfile(full):
            raise SystemExit(f"{FILES_LIST} lists '{path}', which does not exist (a stale entry is an error, "
                             "or the lint silently loses coverage)")
        text = open(full, encoding="utf-8", errors="replace").read()
        if func:
            unit = slice_function(path, text, func)
            if unit is None:
                raise SystemExit(f"{FILES_LIST}: function '{func}' not found in {path}")
            units.append(unit)
        else:
            units.append(Unit(path, text))
    return units


def tracked_imgui_files(root):
    """S0's population: src/common/**.{c,cpp} (tests excluded) and *SingleExe*.cpp,
    that contain an ImGui call."""
    try:
        files = subprocess.run(["git", "-C", root, "ls-files"], capture_output=True, text=True,
                               check=True).stdout.split("\n")
    except (OSError, subprocess.CalledProcessError):
        files = []
        for d, _, names in os.walk(root):
            for n in names:
                files.append(os.path.relpath(os.path.join(d, n), root).replace(os.sep, "/"))
    out = []
    call = re.compile(r"ImGui::[A-Z][A-Za-z]*\(")
    for f in files:
        in_common = f.startswith("src/common/") and not f.startswith("src/common/tests/") and \
            (f.endswith(".c") or f.endswith(".cpp"))
        single = f.endswith(".cpp") and "SingleExe" in os.path.basename(f)
        if not (in_common or single):
            continue
        full = os.path.join(root, f)
        if os.path.isfile(full) and call.search(strip_comments(open(full, encoding="utf-8",
                                                                     errors="replace").read())):
            out.append(f)
    return out


def run_rules(root, sets):
    hits = []
    menu = load_units(root, sets.get("MENU", []))
    panes = load_units(root, sets.get("PANES", []))
    toasts = load_units(root, sets.get("TOASTS", []))
    ptr = load_units(root, sets.get("PTR", []))
    strings = load_units(root, sets.get("STRINGS", []))
    settings_panes = [u for u in panes if u.path in sets.get("SETTINGS_PANE", [])]

    listed = set()
    for entries in sets.values():
        for e in entries:
            listed.add(e.partition("::")[0])
    for f in tracked_imgui_files(root):
        if f not in listed:
            hits.append(("S0", f, 1, "draws ImGui but is not listed in " + FILES_LIST))

    for u in menu + panes:
        rule_s1(u, hits)
        line_rule(u, "S3", TEXT_DISABLED, hits)
        line_rule(u, "S5", SPACING, hits)
        line_rule(u, "S6", SET_TOOLTIP, hits)
    for u in menu + panes + toasts:
        line_rule(u, "S2", LIT_COLOUR, hits)
    for u in menu + settings_panes:
        line_rule(u, "S4", COLLAPSING, hits)
    for u in menu:
        line_rule(u, "S8", APPLY_PRESENTATION, hits)
        line_rule(u, "S9", LITERAL_CVAR, hits)
        rule_s10(u, hits)
        rule_s11(u, hits)
    for u in menu + panes + ptr + strings:
        rule_s7(u, hits)
    return hits


def key(hit):
    rule, path, _, text = hit
    return f"{rule} | {path} | {text.strip()}"


def read_baseline(root):
    p = os.path.join(root, BASELINE)
    if not os.path.isfile(p):
        return collections.Counter()
    c = collections.Counter()
    with open(p, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if line.strip() and not line.lstrip().startswith("#"):
                c[line.strip()] += 1
    return c


BASELINE_HEADER = """# UI parity lint baseline (.github/scripts/check-ui-parity-lint.py).
#
# Every line is one ACCEPTED hit: rule | path | normalised source text. These are
# the known deviations of this project's UI from the SoH conventions -- the
# migration list in docs/ui-style-guide.md's companion plan -- recorded so the
# gate can fail on anything NEW. The baseline may only SHRINK: a fix makes a line
# stale, the check then fails until the line is deleted. Never add a line to make
# the check pass; fix the surface instead.
"""


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", default=ROOT)
    ap.add_argument("--report", action="store_true")
    ap.add_argument("--write-baseline", action="store_true")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test()

    sets = read_file_sets(args.root)
    hits = run_rules(args.root, sets)
    hits.sort(key=lambda h: (h[1], h[2], h[0]))
    for h in hits:
        print(f"{h[0]:<4} {LEVEL[h[0]]:<5} {h[1]}:{h[2]}: {h[3]}")
    counts = collections.Counter(h[0] for h in hits)
    print("summary: " + ", ".join(f"{r}={counts[r]}" for r in sorted(counts, key=lambda r: int(r[1:]))))

    gated = [h for h in hits if LEVEL[h[0]] != "hint"]
    if args.write_baseline:
        with open(os.path.join(args.root, BASELINE), "w", encoding="utf-8", newline="\n") as f:
            f.write(BASELINE_HEADER)
            for k in sorted(key(h) for h in gated):
                f.write(k + "\n")
        print(f"wrote {BASELINE} ({len(gated)} entries)")
        return 0
    if args.report:
        return 0

    have = collections.Counter(key(h) for h in gated)
    base = read_baseline(args.root)
    new = have - base
    stale = base - have
    for k, n in sorted(new.items()):
        print(f"NEW   ({n}x) {k}")
    for k, n in sorted(stale.items()):
        print(f"STALE ({n}x) {k}  -- fixed? delete it from {BASELINE}")
    if new or stale:
        print(f"FAIL: {sum(new.values())} new hit(s), {sum(stale.values())} stale baseline entr(y/ies)")
        return 1
    print(f"OK: {sum(have.values())} gated hit(s), all in the baseline")
    return 0


# ---------------------------------------------------------------------------
# Self-test: every rule fires on its red fixture and stays quiet on its green one
# ---------------------------------------------------------------------------

FIX_MENU = r'''
// ImGui::TextDisabled("in a comment, never a hit");
void AddMenuX(SohMenu& m, WidgetPath& path) {
    AddWidget(path, "Toggle Thing", WIDGET_WINDOW_BUTTON).CVar(K).WindowName("Thing").HideInSearch(true);
    AddWidget(path, "Thing Window", WIDGET_WINDOW_BUTTON).CVar(K);
    AddWidget(path, "Reset Stuff", WIDGET_BUTTON).Callback([](WidgetInfo&) { DoIt(); });
    AddWidget(path, "Clear Safe", WIDGET_BUTTON).Callback([](WidgetInfo&) { SohGui::RegisterPopup("a", "b"); });
    AddWidget(path, "Row", WIDGET_CVAR_CHECKBOX).CVar("gLiteral.Key");
    SohMenu::ApplyPresentation(info, base, SOH_MENU_PRESENT_LIVE, nullptr);
    ImGui::CollapsingHeader("Group");
}
'''

FIX_PANE = r'''
void DrawPane() {
    ImGui::Checkbox("raw", &v);
    UIWidgets::PushStyleButton(THEME_COLOR);
    ImGui::Button("bracketed");
    UIWidgets::PopStyleButton();
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "x");
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(r, g, b, 1.0f));
    dl->AddRectFilled(a, b, IM_COL32(0, 0, 0, 170));
    ImGui::TextDisabled("meaning");
    ImGui::Spacing();
    ImGui::SameLine(40.0f);
    ImGui::SameLine();
    ImGui::SetTooltip("x");
    ImGui::CollapsingHeader("Group in a settings pane");
    const char* kId = "Internal window (#582)"; // ui-lint: internal-id
    printf("logged #123 is fine\n");
    const char* shown = "See ADR "
                        "0004 for why";
    const char* ok = "Plain text with no reference";
}
'''

FIX_TOAST = r'''
    prefixColor[0] = 255;
    messageColor[2] = x;
'''


def self_test():
    failures = []

    def expect(cond, what):
        if not cond:
            failures.append(what)

    with tempfile.TemporaryDirectory() as root:
        os.makedirs(os.path.join(root, ".github", "scripts"))
        os.makedirs(os.path.join(root, "src", "common"))
        for name, body in (("menu.cpp", FIX_MENU), ("src/common/Pane.cpp", FIX_PANE), ("toast.cpp", FIX_TOAST),
                           ("src/common/Unlisted.cpp", "void f() { ImGui::Text(\"x\"); }\n"),
                           ("src/common/Quiet.cpp", "// ImGui::Text(\"only a comment\")\n")):
            with open(os.path.join(root, name), "w", encoding="utf-8") as f:
                f.write(body)
        with open(os.path.join(root, FILES_LIST), "w", encoding="utf-8") as f:
            f.write("MENU menu.cpp\nPANES src/common/Pane.cpp\nSETTING_PANE_UNUSED x\n"
                    "SETTINGS_PANE src/common/Pane.cpp\nTOASTS toast.cpp\n")
        sets = read_file_sets(root)
        sets.pop("SETTING_PANE_UNUSED")
        hits = run_rules(root, sets)
        by = collections.defaultdict(list)
        for h in hits:
            by[h[0]].append(h)

        # S0: the unlisted drawing TU is caught; a listed one and a comment-only one are not.
        s0 = [h[1] for h in by["S0"]]
        expect(s0 == ["src/common/Unlisted.cpp"], f"S0 caught {s0}")
        # S1: the raw checkbox fires, the bracketed button does not.
        expect(len(by["S1"]) == 1 and "Checkbox" in by["S1"][0][3], f"S1 {by['S1']}")
        # S2: all-literal ImVec4 and IM_COL32 and the toast array fire; the CVar-alpha ImVec4 does not.
        s2 = [h[3] for h in by["S2"]]
        expect(any("TextColored" in t for t in s2), "S2 missed an all-literal ImVec4")
        expect(any("IM_COL32" in t for t in s2), "S2 missed IM_COL32")
        expect(any("prefixColor" in t for t in s2), "S2 missed a literal toast colour")
        expect(not any("PushStyleColor" in t for t in s2), "S2 flagged a non-literal ImVec4")
        expect(not any("messageColor" in t for t in s2), "S2 flagged a computed toast colour")
        # S3 fires once: the commented-out call in the menu fixture is not code.
        expect(len(by["S3"]) == 1, f"S3 {by['S3']}")
        # S4 fires in the menu file and the settings pane.
        expect(len(by["S4"]) == 2, f"S4 {by['S4']}")
        # S5: Spacing and SameLine(40) fire; a bare SameLine() does not.
        expect(len(by["S5"]) == 2, f"S5 {by['S5']}")
        expect(len(by["S6"]) == 1, f"S6 {by['S6']}")
        # S7: the split "ADR " "0004" literal fires; the suppressed id, the printf and plain text do not.
        s7 = [h[3] for h in by["S7"]]
        expect(len(s7) == 1 and "ADR 0004" in s7[0], f"S7 {s7}")
        expect(len(by["S8"]) == 1, f"S8 {by['S8']}")
        expect(len(by["S9"]) == 1, f"S9 {by['S9']}")
        s10 = [h[3] for h in by["S10"]]
        expect(s10 == ["Reset Stuff"], f"S10 {s10}")
        s11 = [h[3] for h in by["S11"]]
        expect(len(s11) == 1 and "Thing Window" in s11[0], f"S11 {s11}")

        # The gate: a clean baseline passes; an extra hit fails; a stale entry fails.
        gated = sorted(key(h) for h in hits if LEVEL[h[0]] != "hint")
        with open(os.path.join(root, BASELINE), "w", encoding="utf-8") as f:
            f.write("# header\n" + "\n".join(gated) + "\n")
        quiet = io.StringIO()
        with contextlib.redirect_stdout(quiet):
            ok_rc = main(["--root", root])
        expect(ok_rc == 0, "a matching baseline did not pass")
        with open(os.path.join(root, BASELINE), "w", encoding="utf-8") as f:
            f.write("\n".join(gated[1:]) + "\n")
        with contextlib.redirect_stdout(quiet):
            new_rc = main(["--root", root])
        expect(new_rc == 1, "a hit missing from the baseline passed")
        with open(os.path.join(root, BASELINE), "w", encoding="utf-8") as f:
            f.write("\n".join(gated + ["S1 | menu.cpp | long gone"]) + "\n")
        with contextlib.redirect_stdout(quiet):
            stale_rc = main(["--root", root])
        expect(stale_rc == 1, "a stale baseline entry passed")

        # A stale ui-lint-files.txt entry is an error, not a silent skip.
        with open(os.path.join(root, FILES_LIST), "a", encoding="utf-8") as f:
            f.write("MENU does/not/exist.cpp\n")
        try:
            run_rules(root, read_file_sets(root))
            failures.append("a stale ui-lint-files.txt entry was accepted")
        except SystemExit:
            pass

    if failures:
        for f in failures:
            print("SELF-TEST FAIL:", f)
        return 1
    print("self-test: OK (S0-S11 red and green halves, baseline gate, stale-list refusal)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
