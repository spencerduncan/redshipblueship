#!/usr/bin/env python3
"""Cross-port ODR declaration tripwire (source-level, build-free).

Why this exists
---------------
The armed nm gate (check-symbol-collisions.sh, #375) intersects STRONG symbols
of the compiled soh_*/2ship_* archives. It is structurally BLIND to the ODR
class that has bitten this project four times (FlagTable/SaveEditor static-init
crash, ShipInit registry corruption, UIWidgets #383/#434, Ship::Menu+MenuTypes
#446/#452): a type declared in BOTH game trees with identical mangled names and
DIVERGENT layouts/semantics, where one side's implementation TU is excluded
from the single-exe link. The excluded side defines no symbols, the nm
intersection stays empty, CI reports OK — and any TU that later reaches the
excluded side's header silently binds the compiled side's code against the
wrong layout (first-wins archive semantics on Linux, /FORCE:MULTIPLE on
Windows: no diagnostic on either).

Detection therefore has to compare DECLARATIONS across the two source trees,
not linked symbols. This script does that with two scans over
games/oot/soh/** vs games/mm/2s2h/** (the C++ port layers, where all four
incidents lived — the decomp include/ and src/ C layers are out of scope by
design: C types produce no mangled symbols, each game's C TUs see only their
own tree via include paths, and the extern-C function-collision class IS
visible to the nm gate because both sides' C TUs are compiled into archives):

  1. GUARD scan: include guards (#ifndef X / #define X) duplicated between the
     two trees. Identical guards were a 100%-precision proxy for the forked
     divergent headers in every confirmed incident (MENU_H, MENUTYPES_H,
     NOTIFICATION_H, ...). Note the proxy has known false negatives — OoT's
     GameInteractor.h guard differs by CASE (GameInteractor_h vs MM's
     GAME_INTERACTOR_H) — which is why the TYPE scan below exists.

  2. TYPE scan: top-level class/struct/union/enum (incl. typedef'd anonymous
     enums/structs) definitions whose NAME appears in both trees with an
     OVERLAPPING top-level namespace. Namespace tracking is the load-bearing
     part: MM types moved into namespace S2H (the #434/#452 fix recipe) stop
     overlapping and drop out automatically — the fix pattern goes green by
     construction, and UNWRAPPING a fixed type makes it reappear and fail.

New findings fail against a committed baseline of audited names
(.github/odr-declaration-baseline.txt, classified in the audit issue). The
right fix for a new hit is the established recipe — wrap the MM declaration
family in namespace S2H under RSBS_SINGLE_EXECUTABLE with using-declarations
(see games/mm/2s2h/BenGui/MenuTypes.h for the worked example), or MM_-prefix
extern-C surfaces (games/mm/2s2h/ObjectExtension/ActorListIndex.h) — NOT
extending the baseline. Baseline growth requires an audit note proving the
duplicate is layout-identical or unreachable.

Anti-vacuity: the scan refuses to pass if either tree yields implausibly few
headers or type definitions (parser/layout breakage must fail loud, not pass
empty — the same discipline as check-symbol-collisions.sh's zero-symbol
refusal), and --self-test (run in CI before the real scan) plants a divergent
duplicate in fixture trees and asserts the scan still flags it, still passes
the S2H-wrapped fix shape, and still refuses vacuous input.

Usage:
  check-odr-declaration-collisions.py [repo-root]
      Production gate. Exit 0: no unaudited duplicates. Exit 1: new duplicate
      declaration found. Exit 2: collection/self-consistency error.
  check-odr-declaration-collisions.py --write-baseline [repo-root]
      Regenerate the baseline from the current trees. Deliberate refresh only;
      every added name needs an audit-issue entry.
  check-odr-declaration-collisions.py --self-test
      Fixture-based red/green validation of the detection logic itself.
"""

import os
import re
import sys
import tempfile
from collections import defaultdict

OOT_TREE = os.path.join("games", "oot", "soh")
MM_TREE = os.path.join("games", "mm", "2s2h")
BASELINE_REL = os.path.join(".github", "odr-declaration-baseline.txt")

HDR_EXT = {".h", ".hpp", ".hh"}
SRC_EXT = {".c", ".cpp", ".cc"}

# Anti-vacuity floors: real trees are far above these; a scan that comes back
# under them means the tree layout moved or the parser broke. Fixtures in
# --self-test override via _FLOORS.
FLOOR_HEADERS = 40
FLOOR_TYPES = 40

COMMENT_RE = re.compile(r"//[^\n]*|/\*.*?\*/", re.S)
STRING_RE = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')
GUARD_RE = re.compile(r"^\s*#\s*ifndef\s+(\w+)\s*\n\s*#\s*define\s+\1\b", re.M)
NS_OPEN = re.compile(r"\bnamespace\s+((?:\w|::)+)\s*\{")
TYPE_DEF = re.compile(
    r"\b(class|struct|enum\s+class|enum\s+struct|enum|union)\s+"
    r"(?:\[\[[^\]]*\]\]\s*)?(?:__declspec\([^)]*\)\s*|alignas\([^)]*\)\s*)?"
    r"(\w+)\s*(?:final\s*)?(?::\s*[\w:\s,<>]+?)?\s*\{"
)
TYPEDEF_ANON = re.compile(r"\btypedef\s+(?:enum|struct|union)\s*(\w*)\s*\{")


def read_text(path):
    try:
        with open(path, encoding="utf-8", errors="replace") as fh:
            return fh.read()
    except OSError:
        return ""


def strip_noise(text):
    text = COMMENT_RE.sub(lambda m: "\n" * m.group(0).count("\n"), text)
    return STRING_RE.sub('""', text)


def walk(tree, exts):
    for dirpath, dirnames, filenames in os.walk(tree):
        dirnames[:] = [d for d in dirnames if d != ".git"]
        for f in filenames:
            if os.path.splitext(f)[1].lower() in exts:
                yield os.path.join(dirpath, f)


def include_guard(text):
    m = GUARD_RE.search("\n".join(text.split("\n")[:60]))
    return m.group(1) if m else None


def parse_types(text):
    """Yield (name, top_level_namespace) for each top-level type definition,
    tracking namespace nesting with a linear brace walk. Types nested inside
    other types are skipped (their mangled names are qualified by the outer
    type, which the outer type's own entry covers)."""
    text = strip_noise(text)
    events = {}
    for m in NS_OPEN.finditer(text):
        events.setdefault(m.end() - 1, []).append(("ns", m.group(1)))
    for m in TYPE_DEF.finditer(text):
        events.setdefault(m.end() - 1, []).append(("type", m.group(2)))
    # typedef struct { ... } Name; - the name follows the matching close brace.
    for m in TYPEDEF_ANON.finditer(text):
        depth = 0
        j = m.end() - 1
        n = len(text)
        while j < n:
            if text[j] == "{":
                depth += 1
            elif text[j] == "}":
                depth -= 1
                if depth == 0:
                    break
            j += 1
        nm = re.match(r"\s*([A-Za-z_]\w*)\s*;", text[j + 1 : j + 200])
        if nm:
            # Record at the OPEN brace so the namespace stack is current there;
            # mark as typedef so it does not push a type scope of its own
            # (TYPE_DEF may also match tagged typedefs; dedupe below).
            events.setdefault(m.end() - 1, []).append(("typedef", nm.group(1)))

    out = set()
    stack = []  # entries: ("ns", name) | ("type",) | ("b",)
    for i, ch in enumerate(text):
        evs = events.get(i, [])
        consumed_brace = False
        for kind, name in [(e[0], e[1] if len(e) > 1 else None) for e in evs]:
            if kind == "ns":
                stack.append(("ns", name))
                consumed_brace = True
            elif kind in ("type", "typedef"):
                if not any(s[0] == "type" for s in stack):
                    ns = [s[1] for s in stack if s[0] == "ns"]
                    top = ns[0].split("::")[0] if ns else ""
                    out.add((name, top))
                if kind == "type":
                    stack.append(("type", name))
                    consumed_brace = True
                # typedef-anon: the '{' is handled as a plain brace below
        if ch == "{":
            if not consumed_brace:
                stack.append(("b", None))
        elif ch == "}":
            if stack:
                stack.pop()
    return out


def scan_tree(tree):
    guards = {}
    types = defaultdict(set)  # name -> set of top-level namespaces
    where = defaultdict(set)  # name -> files (diagnostics)
    n_headers = 0
    for p in walk(tree, HDR_EXT | SRC_EXT):
        text = read_text(p)
        rel = os.path.relpath(p).replace("\\", "/")
        if os.path.splitext(p)[1].lower() in HDR_EXT:
            n_headers += 1
            g = include_guard(text)
            if g:
                guards.setdefault(g, set()).add(rel)
        for name, ns in parse_types(text):
            types[name].add(ns)
            where[name].add(rel)
    return guards, types, where, n_headers


def compute_findings(oot, mm, floors=(FLOOR_HEADERS, FLOOR_TYPES)):
    """Return (findings, error). findings is a sorted list of baseline keys
    with human context; error is an exit-2 message or None."""
    og, ot, ow, ohn = oot
    mg, mt, mw, mhn = mm
    fh, ft = floors
    if ohn < fh or mhn < fh:
        return None, (f"vacuous scan: header counts oot={ohn} mm={mhn} below "
                      f"floor {fh} — tree layout moved or parser broke; fix "
                      "the scan rather than losing the gate")
    if len(ot) < ft or len(mt) < ft:
        return None, (f"vacuous scan: type counts oot={len(ot)} mm={len(mt)} "
                      f"below floor {ft} — refusing to pass on empty data")

    findings = []
    for g in sorted(set(og) & set(mg)):
        findings.append((f"GUARD {g}",
                         f"  {g}: {';'.join(sorted(og[g]))} <-> {';'.join(sorted(mg[g]))}"))
    for name in sorted(set(ot) & set(mt)):
        common = ot[name] & mt[name]
        if not common:
            continue  # disjoint namespaces (e.g. MM side wrapped in S2H)
        nss = ",".join(sorted(ns if ns else "<global>" for ns in common))
        findings.append((f"TYPE {name}",
                         f"  {name} [ns: {nss}]: "
                         f"{';'.join(sorted(ow[name])[:3])} <-> {';'.join(sorted(mw[name])[:3])}"))
    return findings, None


def load_baseline(path):
    entries = set()
    if os.path.exists(path):
        for line in read_text(path).splitlines():
            line = line.strip()
            if line and not line.startswith("#"):
                entries.add(line)
    return entries


def run_check(root, write_baseline=False):
    os.chdir(root)
    for tree in (OOT_TREE, MM_TREE):
        if not os.path.isdir(tree):
            print(f"error: missing tree {tree} — run from the repo root; "
                  "refusing to pass vacuously", file=sys.stderr)
            return 2
    findings, err = compute_findings(scan_tree(OOT_TREE), scan_tree(MM_TREE))
    if err:
        print(f"error: {err}", file=sys.stderr)
        return 2

    if write_baseline:
        with open(BASELINE_REL, "w", encoding="utf-8", newline="\n") as fh:
            fh.write("# Audited cross-port duplicate declarations "
                     "(check-odr-declaration-collisions.py).\n"
                     "# Every entry is classified in the ODR divergence audit "
                     "issue. Do not add entries\n"
                     "# without an audit note; fix new duplicates with the "
                     "namespace-S2H recipe instead.\n")
            for key, _ in findings:
                fh.write(key + "\n")
        print(f"baseline written: {BASELINE_REL} ({len(findings)} entries)")
        return 0

    baseline = load_baseline(BASELINE_REL)
    if not baseline:
        print(f"error: baseline {BASELINE_REL} missing or empty — a deleted "
              "baseline must not disarm the gate", file=sys.stderr)
        return 2

    current = {key for key, _ in findings}
    new = current - baseline
    stale = baseline - current
    for key in sorted(stale):
        print(f"note: baseline entry no longer found (fixed?): {key} — prune "
              "with --write-baseline on a deliberate refresh")
    if new:
        print("FAIL: unaudited duplicate declaration(s) across the game "
              "trees — the excluded-side ODR class the nm gate cannot see:",
              file=sys.stderr)
        for key, ctx in findings:
            if key in new:
                print(ctx, file=sys.stderr)
        print("\nFix by moving the MM declaration family into namespace S2H "
              "under RSBS_SINGLE_EXECUTABLE\n(worked examples: "
              "games/mm/2s2h/BenGui/MenuTypes.h, 2s2h/ShipInit.hpp) or "
              "MM_-prefixing\nextern-C surfaces "
              "(2s2h/ObjectExtension/ActorListIndex.h). Extend the baseline "
              "only with\nan audit note proving identical layout or "
              "unreachability.", file=sys.stderr)
        return 1
    print(f"OK: {len(current)} audited duplicate declarations, no new ones "
          f"({len(stale)} stale baseline entr{'y' if len(stale)==1 else 'ies'}).")
    return 0


# ---------------------------------------------------------------------------
# Self-test: red/green validation of the detection logic on fixture trees.
# ---------------------------------------------------------------------------

FIX_OOT_HDR = """#ifndef WIDGET_STUFF_H
#define WIDGET_STUFF_H
typedef enum { W_A, W_B } WidgetKind;
struct WidgetInfo { int x; };
namespace Ship { class Menu { int a; }; }
#endif
"""

FIX_MM_DUP = """#ifndef WIDGET_STUFF_H
#define WIDGET_STUFF_H
typedef enum { W_A, W_OTHER, W_B } WidgetKind;
struct WidgetInfo { long long y; };
namespace Ship { class Menu { void* p[4]; }; }
#endif
"""

FIX_MM_FIXED = """#ifndef S2H_WIDGET_STUFF_H
#define S2H_WIDGET_STUFF_H
namespace S2H {
typedef enum { W_A, W_OTHER, W_B } WidgetKind;
struct WidgetInfo { long long y; };
namespace Ship { class Menu { void* p[4]; }; }
} // namespace S2H
#endif
"""


def _fixture_tree(base, rel, files):
    tree = os.path.join(base, rel)
    os.makedirs(tree, exist_ok=True)
    for name, content in files.items():
        with open(os.path.join(tree, name), "w", encoding="utf-8") as fh:
            fh.write(content)
    return tree


def self_test():
    failures = []

    def expect(label, cond):
        print(f"self-test: {label}: {'ok' if cond else 'FAIL'}")
        if not cond:
            failures.append(label)

    with tempfile.TemporaryDirectory() as tmp:
        floors = (1, 1)
        oot = _fixture_tree(tmp, "oot", {"WidgetStuff.h": FIX_OOT_HDR})

        # RED: divergent duplicate at overlapping scope must be flagged.
        mm = _fixture_tree(tmp, "mm_dup", {"WidgetStuff.h": FIX_MM_DUP})
        f, err = compute_findings(scan_tree(oot), scan_tree(mm), floors)
        keys = {k for k, _ in (f or [])}
        expect("flags duplicated include guard", err is None and "GUARD WIDGET_STUFF_H" in keys)
        expect("flags duplicated global typedef enum", "TYPE WidgetKind" in keys)
        expect("flags duplicated global struct", "TYPE WidgetInfo" in keys)
        expect("flags duplicated Ship-namespace class", "TYPE Menu" in keys)

        # GREEN: the S2H-wrapped fix shape must pass clean.
        mmf = _fixture_tree(tmp, "mm_fixed", {"WidgetStuff.h": FIX_MM_FIXED})
        f, err = compute_findings(scan_tree(oot), scan_tree(mmf), floors)
        keys = {k for k, _ in (f or [])}
        expect("S2H-wrapped twin produces no findings", err is None and not keys)

        # VACUOUS: an empty tree must refuse to pass, not pass empty.
        empty = _fixture_tree(tmp, "mm_empty", {})
        f, err = compute_findings(scan_tree(oot), scan_tree(empty), floors)
        expect("empty tree refuses vacuous pass", err is not None)

    if failures:
        print(f"self-test FAILED: {failures}", file=sys.stderr)
        return 2
    print("self-test passed")
    return 0


def main():
    args = [a for a in sys.argv[1:]]
    if "--self-test" in args:
        return self_test()
    write = "--write-baseline" in args
    rest = [a for a in args if a != "--write-baseline"]
    root = rest[0] if rest else "."
    return run_check(root, write_baseline=write)


if __name__ == "__main__":
    sys.exit(main())
