#!/usr/bin/env python3
"""Static monotonicity probe over both logic graphs (ADR 0010 answer O6).

Why this exists
---------------
ADR 0010 §2.3: the combo fill's guarantee is a least fixed point of a MONOTONE
operator. Assumed (reverse) fill and the trailing no-logic dump are sound only
if reachability never shrinks when the player gains something. OoTMM enforces
that by construction (its expression builder throws on `!` over has()/event());
redship's conditions are C++ lambdas in two dialects, so answer O6 makes the
ban three mechanisms, all of them required:

  1. the REVIEW RULE (.claude/worker-prompts.md, docs/monotonicity.md);
  2. this STATIC PROBE, which reads the condition sources; and
  3. the CI GROW-CHECK (`combo-logic-monotonicity`, rando tier), which drives
     both real engines, grants items one copy at a time and asserts the
     reached check and region sets never shrink.

The probe is the cheap, early one: it runs without a build and names file:line.
It is a HEURISTIC over source text, not a C++ parser, and it says so (the
limits are listed in docs/monotonicity.md). The grow-check is the semantic
backstop for everything this cannot see.

What it scans
-------------
  OoT: games/oot/soh/Enhancements/randomizer/location_access/**  (the region
       files: LOCATION / Entrance / EventAccess lambdas)
  MM:  games/mm/2s2h/Rando/Logic/Regions/**  (CHECK / CONNECTION / EXIT /
       EVENT / STAY lambdas) and games/mm/2s2h/Rando/Logic/Logic.h (the helper
       vocabulary those lambdas call)

What it reports
---------------
Every NEGATION site (`!term` or `not term`, never `!=`; also `term ^ true`,
`term != true`, and a boolean-literal ternary `term ? false : x` /
`term ? x : true`) and every RELATIONAL comparison over a player-state
quantity, classified by what the negated/compared term reads:

  PLAYER-STATE  inventory, events, flags, counters, age/time-of-day context,
                region access (`Here`, `AnyAgeTime`, `CAN_*`, `HAS_*`, ...).
                A negation of one of these is a MONOTONICITY VIOLATION: gaining
                the item/event makes the condition false.
  SETTING       frozen settings, tricks, options, generation-time facts
                (`ctx->GetOption`, `GetTrickOption`, `RANDO_SAVE_OPTIONS`,
                `MM_TRICK`, `SettingClocks`, check prices, ...). Negating one is
                LEGAL: it is constant for the whole fill.

Site classes (the first four fail the gate unless the baseline lists them):

  VIOLATION-NEGATION  `!` over a player-state term.
  VIOLATION-COMPARE   a "downward" comparison over player state (`state < k`,
                      `state <= k`, `state == 0`, `state == *_NONE`): true only
                      while the player has LITTLE.
  VIOLATION-GUARD     a player-state term that FORCES false: `if (HAS_X)
                      return false;` — the mirror image of a negation.
  NEEDS-READING       a negation or `==`/`!=` the vocabulary cannot classify
                      (an unknown term, or `state == k` with k > 0, which is
                      monotone only if k is the quantity's maximum), an `^`
                      over state, or a player-state guard that returns a
                      NON-CONSTANT (`if (HAS_X) return EXPR;` is
                      `HAS_X ? EXPR : rest`, monotone only if EXPR implies
                      rest). A human reading is required; the baseline records
                      it.
  LEGAL-SETTING       `!` over settings only.                (shown with --list)
  LEGAL-GUARD         `if (!STATE) return false;` — "requires STATE", which is
                      monotone (MM Logic.h's enemy-soul guard is the shape).

The gate
--------
New VIOLATION-* / NEEDS-READING sites fail against a committed baseline
(.github/monotonicity-negation-baseline.txt). Each baseline entry carries the
READING that justifies it, and every accepted entry is PRINTED on every run —
the baseline records a reading, it does not silence one. A baseline entry that
no longer matches any site is STALE and also fails: when a site is fixed its
entry must go with it, so the file cannot rot into a list of nothing.

Anti-vacuity: the scan refuses to pass (exit 2) if either tree yields
implausibly few files or condition sites, or if the classifier finds almost none
of the settings negations the OoT region files are known to carry — a reader
that silently sees nothing must fail loud, not pass empty. --self-test plants
violations in fixture trees and asserts each is flagged and fails the gate, that
comments/strings are ignored, that legal shapes pass, that a baselined site
passes only with a reading, that a stale entry fails, and that vacuous input is
refused.

Usage:
  check-monotonicity-negations.py [repo-root]     gate (exit 0 clean, 1 new/stale hit, 2 error)
  check-monotonicity-negations.py --list [root]   every classified site, legal ones included
  check-monotonicity-negations.py --self-test     fixture-based red/green validation
"""

import os
import re
import sys
import tempfile

OOT_ROOT = os.path.join("games", "oot", "soh", "Enhancements", "randomizer", "location_access")
MM_REGIONS = os.path.join("games", "mm", "2s2h", "Rando", "Logic", "Regions")
MM_LOGIC_H = os.path.join("games", "mm", "2s2h", "Rando", "Logic", "Logic.h")
BASELINE_REL = os.path.join(".github", "monotonicity-negation-baseline.txt")

SRC_EXT = (".cpp", ".c", ".h", ".hpp")

# Anti-vacuity floors. The real trees are far above these (OoT: ~37 files,
# 2661 LOCATION( and 2394 Entrance(; MM: 17 region files + Logic.h, 2422
# CHECK(). A scan under a floor means the layout moved or the reader broke.
MIN_OOT_FILES = 20
MIN_MM_FILES = 10
MIN_OOT_SITES = 1000  # LOCATION( + Entrance( + EventAccess(
MIN_MM_SITES = 1000  # CHECK( + CONNECTION( + EXIT( + EVENT( + STAY(
# The OoT region files carry dozens of `!ctx->GetOption(...)` settings
# negations. If the classifier sees almost none, it is not looking at what it
# thinks it is looking at.
MIN_OOT_LEGAL_SETTING_NEGATIONS = 10

VIOLATION_CLASSES = ("VIOLATION-NEGATION", "VIOLATION-COMPARE", "VIOLATION-GUARD", "NEEDS-READING")

# ---------------------------------------------------------------------------
# Vocabulary
# ---------------------------------------------------------------------------

# OoT. `logic->X` is PLAYER-STATE unless X is in this allowlist of Logic
# members that read settings only. Every entry names its evidence; adding one is
# a review decision, never a way to make a hit go away.
OOT_LOGIC_SETTING_MEMBERS = {
    # logic.cpp Logic::IsFireLoopLocked: ctx->GetOption(RSK_KEYSANITY).Is(...) || ... — settings only.
    "IsFireLoopLocked": "reads RSK_KEYSANITY only (logic.cpp Logic::IsFireLoopLocked)",
}
OOT_STATE_FREE_FUNCS = {
    "Here",
    "AnyAgeTime",
    "SpiritShared",
    "SpiritCertainAccess",
    "CanPlantBean",
    "BothAges",
    "ChildCanAccess",
    "AdultCanAccess",
    "GetWalletCapacity",
    "RegionTable",
}
OOT_STATE_PREFIXES = ("RG_", "LOGIC_", "RR_")
OOT_SETTING_TOKENS = {"ctx", "GetOption", "GetTrickOption", "GetLocationOption", "GetCheckPrice", "IsNot", "Is"}
OOT_SETTING_PREFIXES = ("RSK_", "RT_", "RO_", "CVarGet")

# MM.
MM_STATE_TOKENS = {
    "INV_CONTENT",
    "KEY_COUNT",
    "CUR_UPG_VALUE",
    "GET_CUR_UPG_VALUE",
    "GET_CUR_EQUIP_VALUE",
    "RANDO_EVENTS",
    "HaveEnemySoul",
    "CanKillEnemy",
    "CanDefeatMajora",
    "MoonMaskCount",
    "RemainsCount",
    "MeetsMoonRequirements",
    "ClockCount",
    "OwnsClockHalfDay",
    "OwnsHalfDayForMode",
    "IsTimeSliceOwned",
    "HasAnyOwnedTime",
    "ClockFilter",
    "FoundOcarinaButtons",
    "canPlaySong",
    "CanAccessDungeon",
    "RawAt",
    "RawBefore",
    "RawAfter",
    "RawBetween",
    "gCurrentRegionTime",
    "gSaveContext",
    "GET_PLAYER_FORM",
    "AT",
    "BEFORE",
    "AFTER",
    "BETWEEN",
    "IS_DAY",
    "IS_NIGHT",
    "FIRST_DAY",
    "SECOND_DAY",
    "FINAL_DAY",
    "MIDNIGHT",
    "FOUND_ALL_FROGS",
    "CHECK_MAX_HP",
    "Inventory_GetSkullTokenCount",
}
MM_STATE_PREFIXES = (
    "HAS_",
    "CAN_",
    "CHECK_QUEST_ITEM",
    "CHECK_WEEKEVENTREG",
    "CHECK_OWL",
    "Flags_Get",
    "ITEM_",
    "RI_",
    "RE_",
    "RANDO_INF_",
    "QUEST_",
    "IS_DEKU",
    "IS_ZORA",
    "IS_GORON",
    "IS_HUMAN",
    "IS_DEITY",
    "GBT_",
)
MM_SETTING_TOKENS = {"RANDO_SAVE_OPTIONS", "MM_TRICK", "SettingClocks"}
MM_SETTING_PREFIXES = ("RO_", "MMRT_", "CVarGet")

IDENT_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
NUM_RE = re.compile(r"-?[0-9][0-9a-fA-FxXuUlL]*")


def classify_tokens(game, text):
    """('STATE' | 'SETTING' | 'UNKNOWN', state terms, setting terms) for an expression."""
    state, setting = [], []
    if game == "oot":
        # logic->Member first, so `logic` itself is not double counted.
        for m in re.finditer(r"logic\s*->\s*([A-Za-z_][A-Za-z0-9_]*)", text):
            member = m.group(1)
            (setting if member in OOT_LOGIC_SETTING_MEMBERS else state).append("logic->" + member)
        stripped = re.sub(r"logic\s*->\s*[A-Za-z_][A-Za-z0-9_]*", " ", text)
        for tok in IDENT_RE.findall(stripped):
            if tok in OOT_STATE_FREE_FUNCS or tok.startswith(OOT_STATE_PREFIXES):
                state.append(tok)
            elif tok in OOT_SETTING_TOKENS or tok.startswith(OOT_SETTING_PREFIXES):
                setting.append(tok)
    else:
        # RANDO_SAVE_CHECKS[x].price / .shuffled / .randoItemId / .skipped are
        # generation facts frozen at creation; any other field (obtained,
        # eligible, ...) is player state.
        field_re = r"RANDO_SAVE_CHECKS\s*\[[^\]]*\]\s*\.\s*([A-Za-z_]+)"
        for m in re.finditer(field_re, text):
            frozen = m.group(1) in ("price", "shuffled", "randoItemId", "skipped")
            (setting if frozen else state).append("RANDO_SAVE_CHECKS." + m.group(1))
        stripped = re.sub(field_re, " ", text)
        for tok in IDENT_RE.findall(stripped):
            if tok in MM_STATE_TOKENS or tok.startswith(MM_STATE_PREFIXES):
                state.append(tok)
            elif tok in MM_SETTING_TOKENS or tok.startswith(MM_SETTING_PREFIXES):
                setting.append(tok)
    if state:
        return "STATE", state, setting
    if setting:
        return "SETTING", state, setting
    return "UNKNOWN", state, setting


# ---------------------------------------------------------------------------
# Source reading
# ---------------------------------------------------------------------------


def strip_comments_and_strings(src):
    """Blank out comments and string/char literals, preserving every newline and
    every other character's offset, so an offset maps back to file:line."""
    out = list(src)
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        nxt = src[i + 1] if i + 1 < n else ""
        if c == "/" and nxt == "/":
            j = src.find("\n", i)
            j = n if j < 0 else j
            for k in range(i, j):
                out[k] = " "
            i = j
        elif c == "/" and nxt == "*":
            j = src.find("*/", i + 2)
            j = n if j < 0 else j + 2
            for k in range(i, j):
                if src[k] != "\n":
                    out[k] = " "
            i = j
        elif c in "\"'":
            j = i + 1
            while j < n and src[j] != c and src[j] != "\n":
                if src[j] == "\\":
                    j += 1
                j += 1
            for k in range(i + 1, min(j, n)):
                if src[k] != "\n":
                    out[k] = " "
            i = j + 1
        else:
            i += 1
    return "".join(out)


def match_group(text, i, open_ch, close_ch):
    """Index one past the group opened at text[i] == open_ch, or len(text)."""
    depth, n = 0, len(text)
    while i < n:
        if text[i] == open_ch:
            depth += 1
        elif text[i] == close_ch:
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    return n


def operand_after(text, i):
    """The expression a prefix operator ending at text[i] applies to: a
    parenthesised group, or an identifier chain with calls, subscripts and
    member accesses."""
    n = len(text)
    while i < n and text[i] in " \t\r\n":
        i += 1
    start = i
    if i < n and text[i] == "(":
        return start, match_group(text, i, "(", ")")
    if i < n and text[i] == "!":  # !!x — take the inner operand
        return operand_after(text, i + 1)
    m = IDENT_RE.match(text, i)
    if not m:
        return start, start
    i = m.end()
    while i < n:
        j = i
        while j < n and text[j] in " \t":
            j += 1
        if j < n and text[j] == "(":
            i = match_group(text, j, "(", ")")
        elif j < n and text[j] == "[":
            i = match_group(text, j, "[", "]")
        elif text.startswith("->", j) or text.startswith("::", j) or (j < n and text[j] == "."):
            j += 1 if text[j] == "." else 2
            while j < n and text[j] in " \t":
                j += 1
            m = IDENT_RE.match(text, j)
            if not m:
                break
            i = m.end()
        else:
            break
    return start, i


def operand_before(text, i):
    """The postfix expression ending just before text[i] (for `x <op> k`)."""
    j = i - 1
    while j >= 0 and text[j] in " \t\r\n":
        j -= 1
    end = j + 1
    while j >= 0:
        c = text[j]
        if c in ")]":
            open_ch = "(" if c == ")" else "["
            depth = 0
            while j >= 0:
                if text[j] == c:
                    depth += 1
                elif text[j] == open_ch:
                    depth -= 1
                    if depth == 0:
                        break
                j -= 1
            j -= 1
            continue
        if c.isalnum() or c == "_":
            j -= 1
            continue
        if c == ">" and j > 0 and text[j - 1] == "-":
            j -= 2
            continue
        if c == ".":
            j -= 1
            continue
        if c == ":" and j > 0 and text[j - 1] == ":":
            j -= 2
            continue
        break
    return j + 1, end


def operand_after_rel(text, i):
    """The primary expression starting at text[i], right side of a comparison."""
    n = len(text)
    while i < n and text[i] in " \t\r\n":
        i += 1
    if i < n and text[i] == "(":
        return i, match_group(text, i, "(", ")")
    m = NUM_RE.match(text, i)
    if m:
        return i, m.end()
    return operand_after(text, i)


def ternary_condition_before(text, q):
    """The condition span [start, q) of the ternary whose `?` is at text[q]:
    back to the nearest depth-0 `(`, `[`, `{`, `,`, `;`, `?`, single `:`,
    assignment `=`, or `return`."""
    j, depth = q - 1, 0
    while j >= 0:
        c = text[j]
        if c in ")]}":
            depth += 1
        elif c in "([{":
            if depth == 0:
                break
            depth -= 1
        elif depth == 0:
            if c in ",;?":
                break
            if c == ":" and not (j > 0 and text[j - 1] == ":") and not (j + 1 < len(text) and text[j + 1] == ":"):
                break
            if c == "=" and not (j > 0 and text[j - 1] in "=!<>") and not (j + 1 < len(text) and text[j + 1] == "="):
                break
            if (c == "n" and text.startswith("return", j - 5)
                    and (j - 6 < 0 or not (text[j - 6].isalnum() or text[j - 6] == "_"))):
                return j + 1, q
        j -= 1
    return j + 1, q


def ternary_colon_after(text, q):
    """Index of the `:` that closes the true branch of the ternary at text[q],
    or -1."""
    i, n, depth, nested = q + 1, len(text), 0, 0
    while i < n:
        c = text[i]
        if c in "([{":
            depth += 1
        elif c in ")]}":
            if depth == 0:
                return -1
            depth -= 1
        elif depth == 0 and c == ";":
            return -1
        elif depth == 0 and c == "?":
            nested += 1
        elif depth == 0 and c == ":":
            if text.startswith("::", i):
                i += 2
                continue
            if nested == 0:
                return i
            nested -= 1
        i += 1
    return -1


def return_expr_guards(text):
    """(condition span, returned expression) of every `if (C) return EXPR;`
    whose EXPR is not a boolean literal."""
    out = []
    for m in re.finditer(r"\bif\s*\(", text):
        open_i = m.end() - 1
        close_i = match_group(text, open_i, "(", ")")
        r = re.match(r"\s*\{?\s*return\s+([^;{}]*);", text[close_i : close_i + 400])
        if r and r.group(1).strip() not in ("true", "false"):
            out.append(((open_i + 1, close_i - 1), r.group(1).strip()))
    return out


def return_false_guards(text):
    """Spans [start, end) of the condition of every `if (C) return false;` —
    inside such a condition the polarity of every term is FLIPPED: a negated
    term there REQUIRES the thing, a positive one FORBIDS it."""
    spans = []
    for m in re.finditer(r"\bif\s*\(", text):
        open_i = m.end() - 1
        close_i = match_group(text, open_i, "(", ")")
        if re.match(r"\s*\{?\s*return\s+false\s*;", text[close_i : close_i + 64]):
            spans.append((open_i + 1, close_i - 1))
    return spans


def line_of(text, offset):
    return text.count("\n", 0, offset) + 1


def norm(s):
    return " ".join(s.split())


# `<`/`>` only when not part of `<<`, `>>`, `->`, `<=`, `>=`.
REL_RE = re.compile(r"(<=|>=|==|!=|(?<![<\-])<(?![<=])|(?<![>\-])>(?![>=]))")
NONE_LITERAL_RE = re.compile(r"^(0|0[xX]0+|[A-Z0-9_]*NONE|false|nullptr)$")
POS_LITERAL_RE = re.compile(r"^-?[0-9][0-9a-fA-FxXuUlL]*$")
FLIP = {"<": ">", "<=": ">=", ">": "<", ">=": "<=", "==": "==", "!=": "!="}


TEMPLATE_ARGS_RE = re.compile(r"\b(?:\w+_cast|std\s*::\s*\w+)\s*<[^<>;{}()]*>")


def blank_template_args(text):
    """Blank the angle brackets of `static_cast<T>` / `std::set<T>`-style
    template argument lists (offsets preserved), so they never read as `<`/`>`."""

    def repl(m):
        s = m.group(0)
        i = s.index("<")
        return s[:i] + " " + s[i + 1 : -1] + " "

    return TEMPLATE_ARGS_RE.sub(repl, text)


def scan_text(game, relpath, raw):
    """Classify every negation and comparison site in one file."""
    text = blank_template_args(strip_comments_and_strings(raw))
    raw_lines = raw.split("\n")
    guards = return_false_guards(text)
    sites = []

    def flipped(off):
        return any(a <= off < b for a, b in guards)

    def add(cls, off, term, detail):
        ln = line_of(text, off)
        sites.append({"game": game, "path": relpath, "line": ln, "class": cls, "term": norm(term), "detail": detail})

    negation_spans = []
    # --- negations: `!` not followed by `=`.
    # `not` is C++'s alternative token for `!` (never an identifier).
    for m in re.finditer(r"!(?!=)|\bnot\b", text):
        s, e = operand_after(text, m.end())
        if e <= s:
            continue
        operand = text[s:e]
        op_txt = "!" if m.group(0) == "!" else "not "
        negation_spans.append((m.start(), e))
        kind, st, se = classify_tokens(game, operand)
        if kind == "STATE":
            if flipped(m.start()):
                add("LEGAL-GUARD", m.start(), op_txt + operand,
                    "negated state inside `if (...) return false;` = REQUIRES " + ", ".join(sorted(set(st))))
            else:
                add("VIOLATION-NEGATION", m.start(), op_txt + operand,
                    "negates player state: " + ", ".join(sorted(set(st))))
        elif kind == "SETTING":
            add("LEGAL-SETTING", m.start(), op_txt + operand, "negates settings only: " + ", ".join(sorted(set(se))))
        else:
            add("NEEDS-READING", m.start(), op_txt + operand, "negation of a term the vocabulary does not know")

    # --- negation by other spellings: a boolean-literal ternary over state
    #     (`S ? false : x` is `!S && x`; `S ? x : true` is `!S || x`) and `S ^ ...`.
    for m in re.finditer(r"\?", text):
        q = m.start()
        colon = ternary_colon_after(text, q)
        if colon < 0:
            continue
        cs, ce = ternary_condition_before(text, q)
        cond = text[cs:ce].strip()
        if not cond:
            continue
        kind, st, _ = classify_tokens(game, cond)
        if kind != "STATE":
            continue
        true_branch = text[q + 1 : colon].strip()
        else_true = re.match(r"\s*true\b\s*(?:[),;}]|$)", text[colon + 1 : colon + 64])
        if true_branch == "false" or else_true:
            shape = "? false : ..." if true_branch == "false" else "? ... : true"
            term = cond + " " + shape
            if flipped(q):
                add("NEEDS-READING", q, term, "boolean-literal ternary over player state inside a flipped guard")
            else:
                add("VIOLATION-NEGATION", q, term,
                    "a `" + shape + "` ternary negates player state: " + ", ".join(sorted(set(st))))
    for m in re.finditer(r"\^(?!=)", text):
        ls, le = operand_before(text, m.start())
        rs, rend = operand_after_rel(text, m.end())
        left, right = text[ls:le].strip(), text[rs:rend].strip()
        lk, st1, _ = classify_tokens(game, left)
        rk, st2, _ = classify_tokens(game, right)
        if lk != "STATE" and rk != "STATE":
            continue
        other = right if lk == "STATE" else left
        term = left + " ^ " + right
        if other in ("true", "1"):
            add("VIOLATION-NEGATION", m.start(), term,
                "`^ true` negates player state: " + ", ".join(sorted(set(st1 + st2))))
        else:
            add("NEEDS-READING", m.start(), term, "exclusive-or over player state is not monotone in general")

    # --- comparisons over player state.
    for m in REL_RE.finditer(text):
        op = m.group(1)
        ln = line_of(text, m.start())
        line_txt = raw_lines[ln - 1] if ln - 1 < len(raw_lines) else ""
        stripped_line = line_txt.lstrip()
        if stripped_line.startswith("#include") or "template" in line_txt or "static_assert" in line_txt:
            continue
        ls, le = operand_before(text, m.start())
        rs, rend = operand_after_rel(text, m.end())
        left, right = text[ls:le].strip(), text[rs:rend].strip()
        if not left or not right:
            continue
        lk, _, _ = classify_tokens(game, left)
        rk, _, _ = classify_tokens(game, right)
        if lk != "STATE" and rk != "STATE":
            continue
        guard = flipped(m.start())
        if lk == "STATE" and rk == "STATE":
            if op in ("==", "!="):
                continue  # `INV_CONTENT(x) == x`-style identity tests (HAS_ITEM's own definition)
            add("NEEDS-READING", m.start(), left + " " + op + " " + right, "comparison between two player-state terms")
            continue
        if lk == "STATE":
            state_side, other, sop = left, right, op
        else:
            state_side, other, sop = right, left, FLIP[op]
        term = state_side + " " + sop + " " + other
        if sop in (">", ">="):
            downward = False
        elif sop in ("<", "<="):
            downward = True
        elif sop == "==" and other == "true":
            downward = False
        elif sop == "!=" and other == "true":
            downward = True  # `S != true` is `!S`
        elif sop == "==":
            if NONE_LITERAL_RE.match(other):
                downward = True
            elif POS_LITERAL_RE.match(other):
                add("NEEDS-READING", m.start(), term,
                    "equality on a player-state quantity: monotone only if " + other + " is its maximum")
                continue
            else:
                continue  # state == some-identifier: an identity/enum test, not a count bound
        else:  # !=
            if NONE_LITERAL_RE.match(other):
                downward = False  # "has anything" — monotone
            elif POS_LITERAL_RE.match(other):
                add("NEEDS-READING", m.start(), term, "inequality on a player-state quantity")
                continue
            else:
                continue
        if downward != guard:
            add("VIOLATION-COMPARE", m.start(), term,
                "an 'at least' test that FORCES false" if guard else "true only while the player has LITTLE")

    # --- a player-state guard returning a NON-CONSTANT: `if (S) return E;` is
    #     `S ? E : rest`, monotone only if E implies rest.
    for (a, b), expr in return_expr_guards(text):
        kind, st, _ = classify_tokens(game, text[a:b])
        if kind == "STATE":
            add("NEEDS-READING", a, "if (" + norm(text[a:b]) + ") return " + norm(expr) + ";",
                "a player-state guard returning a non-constant (" + ", ".join(sorted(set(st))) + ")")

    # --- positive state terms that force false: `if (STATE) return false;`.
    for a, b in guards:
        masked = list(text[a:b])
        for ns, ne in negation_spans:
            for k in range(max(ns, a), min(ne, b)):
                masked[k - a] = " "
        masked = "".join(masked)
        # Comparisons were classified above; take them out before looking.
        masked = re.sub(r"[A-Za-z_0-9\]\)]\s*(<=|>=|==|!=|<|>)\s*[-A-Za-z_0-9(]+", " ", masked)
        kind, st, _ = classify_tokens(game, masked)
        if kind == "STATE":
            add("VIOLATION-GUARD", a, "if (" + norm(text[a:b]) + ") return false;",
                "player state FORCES false: " + ", ".join(sorted(set(st))))
    return sites


def count_sites(game, text):
    if game == "oot":
        return len(re.findall(r"\b(LOCATION|Entrance|EventAccess)\s*\(", text))
    return len(re.findall(r"\b(CHECK|CONNECTION|EXIT|EVENT|STAY)\s*\(", text))


def collect(root):
    stats = {"oot_files": 0, "mm_files": 0, "oot_sites": 0, "mm_sites": 0}
    sites = []
    targets = []
    for base, game in ((os.path.join(root, OOT_ROOT), "oot"), (os.path.join(root, MM_REGIONS), "mm")):
        if not os.path.isdir(base):
            continue
        for dirpath, _, files in os.walk(base):
            for f in files:
                if f.endswith(SRC_EXT):
                    targets.append((game, os.path.join(dirpath, f)))
    mm_h = os.path.join(root, MM_LOGIC_H)
    if os.path.isfile(mm_h):
        targets.append(("mm", mm_h))
    for game, path in sorted(targets, key=lambda t: t[1].replace(os.sep, "/")):
        with open(path, encoding="utf-8", errors="replace") as fh:
            raw = fh.read()
        rel = os.path.relpath(path, root).replace(os.sep, "/")
        stats[game + "_files"] += 1
        stats[game + "_sites"] += count_sites(game, strip_comments_and_strings(raw))
        sites.extend(scan_text(game, rel, raw))
    return sites, stats


DEFAULT_FLOORS = {
    "oot_files": MIN_OOT_FILES,
    "mm_files": MIN_MM_FILES,
    "oot_sites": MIN_OOT_SITES,
    "mm_sites": MIN_MM_SITES,
    "oot_legal": MIN_OOT_LEGAL_SETTING_NEGATIONS,
}


def vacuity_errors(sites, stats, floors):
    errs = []
    if stats["oot_files"] < floors["oot_files"]:
        errs.append("OoT: %d condition files under %s (floor %d)" % (stats["oot_files"], OOT_ROOT, floors["oot_files"]))
    if stats["mm_files"] < floors["mm_files"]:
        errs.append("MM: %d condition files (floor %d)" % (stats["mm_files"], floors["mm_files"]))
    if stats["oot_sites"] < floors["oot_sites"]:
        errs.append("OoT: %d LOCATION/Entrance/EventAccess sites (floor %d)" % (stats["oot_sites"], floors["oot_sites"]))
    if stats["mm_sites"] < floors["mm_sites"]:
        errs.append("MM: %d CHECK/CONNECTION/EXIT/EVENT/STAY sites (floor %d)" % (stats["mm_sites"], floors["mm_sites"]))
    legal = sum(1 for s in sites if s["game"] == "oot" and s["class"] == "LEGAL-SETTING")
    if legal < floors["oot_legal"]:
        errs.append("OoT: the classifier found %d settings negations (floor %d) - it is not reading the region "
                    "files it thinks it is" % (legal, floors["oot_legal"]))
    return errs


# ---------------------------------------------------------------------------
# Baseline: one entry per line, `path | class | term | reading`, keyed on
# (path, class, whitespace-normalised term) and NOT on line numbers, so an
# unrelated edit above a site does not churn the file. `#` lines are comments.
# ---------------------------------------------------------------------------


def load_baseline(path):
    entries = []
    if not os.path.isfile(path):
        return entries
    with open(path, encoding="utf-8") as fh:
        for n, line in enumerate(fh, 1):
            s = line.strip()
            if not s or s.startswith("#"):
                continue
            parts = [p.strip() for p in s.split(" | ")]
            if len(parts) != 4 or not parts[3]:
                raise ValueError("%s:%d: a baseline entry is `path | class | term | reading`, and the reading may "
                                 "not be empty" % (path, n))
            entries.append({"path": parts[0], "class": parts[1], "term": norm(parts[2]), "reading": parts[3]})
    return entries


def gate(sites, baseline):
    """(new hits, accepted [(site, entry)], stale entries). One entry accepts
    every site with the same (path, class, term) — a repeated expression is one
    reading."""
    hits = [s for s in sites if s["class"] in VIOLATION_CLASSES]
    used = [False] * len(baseline)
    new, accepted = [], []
    for s in hits:
        match = None
        for i, e in enumerate(baseline):
            if e["path"] == s["path"] and e["class"] == s["class"] and e["term"] == s["term"]:
                match = i
                break
        if match is None:
            new.append(s)
        else:
            used[match] = True
            accepted.append((s, baseline[match]))
    stale = [e for i, e in enumerate(baseline) if not used[i]]
    return new, accepted, stale


def fmt(s):
    return "%s:%d: %s [%s] %s -- %s" % (s["path"], s["line"], s["class"], s["game"], s["term"], s["detail"])


def run(root, list_all=False, floors=None, quiet=False):
    floors = floors or DEFAULT_FLOORS
    say = (lambda *_: None) if quiet else print
    sites, stats = collect(root)
    errs = vacuity_errors(sites, stats, floors)
    if errs:
        for e in errs:
            say("monotonicity-probe: VACUOUS SCAN: " + e)
        return 2
    try:
        baseline = load_baseline(os.path.join(root, BASELINE_REL))
    except ValueError as exc:
        say("monotonicity-probe: " + str(exc))
        return 2
    new, accepted, stale = gate(sites, baseline)
    counts = {}
    for s in sites:
        counts[s["class"]] = counts.get(s["class"], 0) + 1
    say("monotonicity-probe: scanned OoT %d files / %d condition sites, MM %d files / %d condition sites"
        % (stats["oot_files"], stats["oot_sites"], stats["mm_files"], stats["mm_sites"]))
    say("monotonicity-probe: classified: " + ", ".join("%s=%d" % (k, counts[k]) for k in sorted(counts)))
    if list_all:
        for s in sites:
            say("  " + fmt(s))
    for s, e in accepted:
        say("monotonicity-probe: ACCEPTED (baseline) " + fmt(s))
        say("    reading: " + e["reading"])
    for s in new:
        say("monotonicity-probe: NEW HIT " + fmt(s))
    for e in stale:
        say("monotonicity-probe: STALE BASELINE ENTRY (matches no site any more; delete it): %s | %s | %s"
            % (e["path"], e["class"], e["term"]))
    if new or stale:
        say("monotonicity-probe: FAIL - %d new hit(s), %d stale baseline entr(y/ies). A player-state negation breaks "
            "ADR 0010 section 2.3's monotone operator: write the condition positively (require the thing, never its "
            "absence), or confine the negation to a settings/trick term. Extending the baseline needs a reading that "
            "proves the site monotone - see docs/monotonicity.md." % (len(new), len(stale)))
        return 1
    say("monotonicity-probe: OK - no unaccepted player-state negation in either graph (%d site(s) accepted with a "
        "reading)" % len(accepted))
    return 0


# ---------------------------------------------------------------------------
# Self-test
# ---------------------------------------------------------------------------

FIX_OOT_CLEAN = r"""
void RegionTable_Init_Fixture() {
    areaTable[RR_A] = Region("A", SCENE_ID_MAX, {}, {
        LOCATION(RC_A1, logic->HasItem(RG_BOW) && !ctx->GetOption(RSK_FOREST).Is(RO_CLOSED_FOREST_ON)),
        LOCATION(RC_A2, !logic->IsFireLoopLocked() || logic->Hearts() >= 3),
        LOCATION(RC_A3, GetCheckPrice() <= GetWalletCapacity()),
        // LOCATION(RC_A4, !logic->HasItem(RG_HOOKSHOT)),  a comment is not code
        LOCATION(RC_A5, logic->CanUse(RG_HOOKSHOT) && ctx->GetTrickOption(RT_X).Get() != 0),
        LOCATION(RC_A7, logic->CanUse(logic->IsAdult ? RG_HOOKSHOT : RG_LONGSHOT)),
        LOCATION(RC_A8, ctx->GetTrickOption(RT_X) ? false : logic->HasItem(RG_BOW)),
        LOCATION(RC_A9, logic->HasItem(RG_BOW) ? logic->IsAdult : false),
        LOCATION(RC_A10, logic->HasItem(RG_BOW) == true),
        /* LOCATION(RC_A6, logic->Hearts() < 3), */
    }, {
        Entrance(RR_B, []{return !ctx->GetOption(RSK_X) && logic->IsAdult;}),
    });
    const char* s = "!logic->HasItem(RG_BOW) && logic->Hearts() < 2";
}
"""

FIX_MM_CLEAN = r"""
static void Register() {
    Regions[RR_A] = RandoRegion{ .checks = {
        CHECK(RC_A, HAS_ITEM(ITEM_BOW) && !RANDO_SAVE_OPTIONS[RO_X]),
        CHECK(RC_B, !MM_TRICK(MMRT_X) || CAN_BE_ZORA),
        CHECK(RC_C, KEY_COUNT(WOODFALL) >= 1),
        CHECK(RC_D, RANDO_SAVE_CHECKS[RC_D].price < 100 || CUR_UPG_VALUE(UPG_WALLET) >= 1),
    }, .timeStayRestrictions = { STAY(TIME_NIGHT1_PM_08_00, HAS_ITEM(ITEM_ROOM_KEY)) } };
}
inline bool Helper(int e) {
    if (RANDO_SAVE_OPTIONS[RO_SOULS] && !HaveEnemySoul(e)) {
        return false;
    }
    if (!SettingClocks())
        return true;
    RandoInf f = static_cast<RandoInf>(RANDO_INF_OBTAINED_CLOCK_DAY_1 + e);
    std::set<RandoCheckId> seen;
    return INV_CONTENT(ITEM_BOTTLE) != ITEM_NONE;
}
"""

# Each planted violation: where it goes, the class it must produce, a substring of the term.
PLANTS = [
    ("oot", "LOCATION(RC_P1, !logic->HasItem(RG_HOOKSHOT)),", "VIOLATION-NEGATION", "!logic->HasItem(RG_HOOKSHOT)"),
    ("oot", "Entrance(RR_P, []{return !Here(RR_X, []{return logic->IsAdult;});}),", "VIOLATION-NEGATION", "!Here("),
    ("oot", "LOCATION(RC_P2, logic->Hearts() < 3),", "VIOLATION-COMPARE", "logic->Hearts() < 3"),
    ("oot", "LOCATION(RC_P2b, 3 > logic->Hearts()),", "VIOLATION-COMPARE", "logic->Hearts() < 3"),
    ("oot", "EventAccess(LOGIC_P, []{return !logic->Get(LOGIC_DRAIN_WELL);}),", "VIOLATION-NEGATION",
     "!logic->Get(LOGIC_DRAIN_WELL)"),
    ("oot", "LOCATION(RC_P3, logic->StoneCount() == 3),", "NEEDS-READING", "logic->StoneCount() == 3"),
    ("oot", "LOCATION(RC_P4, !logic->IsAdult),", "VIOLATION-NEGATION", "!logic->IsAdult"),
    ("mm", "CHECK(RC_P, !HAS_ITEM(ITEM_BOW)),", "VIOLATION-NEGATION", "!HAS_ITEM(ITEM_BOW)"),
    ("mm", "CHECK(RC_Q, !Flags_GetRandoInf(RANDO_INF_OBTAINED_ROOM_KEY)),", "VIOLATION-NEGATION",
     "!Flags_GetRandoInf"),
    ("mm", "CHECK(RC_R, KEY_COUNT(WOODFALL) == 0),", "VIOLATION-COMPARE", "KEY_COUNT(WOODFALL) == 0"),
    ("mm", "CHECK(RC_S, !RANDO_EVENTS[RE_X]),", "VIOLATION-NEGATION", "!RANDO_EVENTS[RE_X]"),
    ("mm", "CONNECTION(RR_T, !BEFORE(TIME_DAY1_AM_06_00)),", "VIOLATION-NEGATION", "!BEFORE("),
    ("mm", "STAY(TIME_NIGHT1_PM_08_00, !HAS_ROOM_KEY),", "VIOLATION-NEGATION", "!HAS_ROOM_KEY"),
    ("mm", "CHECK(RC_U, !(HAS_ITEM(ITEM_HOOKSHOT) || RANDO_SAVE_OPTIONS[RO_X])),", "VIOLATION-NEGATION",
     "!(HAS_ITEM(ITEM_HOOKSHOT)"),
    ("mm_h", "inline bool Planted() { if (HAS_ITEM(ITEM_HOOKSHOT)) return false; return true; }", "VIOLATION-GUARD",
     "HAS_ITEM(ITEM_HOOKSHOT)"),
    ("mm_h", "inline bool Planted2() { if (KEY_COUNT(X) >= 2) { return false; } return true; }", "VIOLATION-COMPARE",
     "KEY_COUNT(X) >= 2"),
    # The shapes review of #734 found unflagged: other spellings of a negation.
    ("oot", "LOCATION(RC_P5, not logic->HasItem(RG_BOW)),", "VIOLATION-NEGATION", "not logic->HasItem(RG_BOW)"),
    ("oot", "LOCATION(RC_P6, logic->HasItem(RG_BOW) ? false : true),", "VIOLATION-NEGATION",
     "logic->HasItem(RG_BOW) ? false"),
    ("oot", "LOCATION(RC_P7, logic->HasItem(RG_BOW) ? logic->IsAdult : true),", "VIOLATION-NEGATION",
     "logic->HasItem(RG_BOW) ? ... : true"),
    ("oot", "LOCATION(RC_P8, logic->HasItem(RG_BOW) ^ true),", "VIOLATION-NEGATION", "logic->HasItem(RG_BOW) ^ true"),
    ("oot", "LOCATION(RC_P9, logic->HasItem(RG_BOW) != true),", "VIOLATION-COMPARE", "logic->HasItem(RG_BOW) != true"),
    ("oot", "Entrance(RR_P2, []{if (logic->CanUse(RG_HOOKSHOT)) return logic->IsAdult && false; return true;}),",
     "NEEDS-READING", "if (logic->CanUse(RG_HOOKSHOT)) return"),
    ("mm", "CHECK(RC_V, HAS_ITEM(ITEM_BOW) ? false : true),", "VIOLATION-NEGATION", "HAS_ITEM(ITEM_BOW) ? false"),
    ("mm_h", "inline bool Planted3() { if (HAS_ITEM(ITEM_HOOKSHOT)) return IS_DEKU && false; return true; }",
     "NEEDS-READING", "if (HAS_ITEM(ITEM_HOOKSHOT)) return"),
]


def _write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(text)


def _fixture_tree(tmp, oot_extra="", mm_extra="", mm_h_extra="", nfiles=3):
    for i in range(nfiles):
        _write(os.path.join(tmp, OOT_ROOT, "overworld", "f%d.cpp" % i), FIX_OOT_CLEAN + (oot_extra if i == 0 else ""))
        _write(os.path.join(tmp, MM_REGIONS, "r%d.cpp" % i), FIX_MM_CLEAN + (mm_extra if i == 0 else ""))
    _write(os.path.join(tmp, MM_LOGIC_H), FIX_MM_CLEAN + mm_h_extra)


FIXTURE_FLOORS = {"oot_files": 3, "mm_files": 3, "oot_sites": 3, "mm_sites": 3, "oot_legal": 3}


def self_test():
    failures = []

    def check(cond, msg):
        print("self-test: %s %s" % ("ok:  " if cond else "FAIL:", msg))
        if not cond:
            failures.append(msg)

    # (1) The clean fixture passes, and its legal shapes are classified legal.
    with tempfile.TemporaryDirectory() as tmp:
        _fixture_tree(tmp)
        sites, _ = collect(tmp)
        bad = [s for s in sites if s["class"] in VIOLATION_CLASSES]
        check(not bad, "the clean fixture has no violation (got: %s)" % "; ".join(fmt(s) for s in bad))
        check(any(s["class"] == "LEGAL-SETTING" and "GetOption" in s["term"] for s in sites),
              "`!ctx->GetOption(...)` is LEGAL-SETTING")
        check(any(s["class"] == "LEGAL-SETTING" and "IsFireLoopLocked" in s["term"] for s in sites),
              "the allowlisted settings member `!logic->IsFireLoopLocked()` is LEGAL-SETTING")
        check(any(s["class"] == "LEGAL-SETTING" and "MM_TRICK" in s["term"] for s in sites),
              "`!MM_TRICK(...)` is LEGAL-SETTING")
        check(any(s["class"] == "LEGAL-GUARD" and "HaveEnemySoul" in s["term"] for s in sites),
              "`if (... && !HaveEnemySoul(e)) return false;` is LEGAL-GUARD (it REQUIRES the soul)")
        check(not any("RG_HOOKSHOT" in s["term"] and s["term"].startswith("!") for s in sites),
              "a negation inside a // comment is ignored")
        check(not any("Hearts() < 3" in s["term"] for s in sites), "a comparison inside a /* */ comment is ignored")
        check(not any("RG_BOW" in s["term"] and s["term"].startswith("!") for s in sites),
              "a negation inside a string literal is ignored")
        check(run(tmp, floors=FIXTURE_FLOORS, quiet=True) == 0, "the gate passes the clean fixture")

    # (2) Every planted violation is flagged with the right class and fails the
    #     gate; baselined with a reading, it passes.
    for where, line, want_class, want_term in PLANTS:
        with tempfile.TemporaryDirectory() as tmp:
            _fixture_tree(tmp, oot_extra=("\n" + line + "\n") if where == "oot" else "",
                          mm_extra=("\n" + line + "\n") if where == "mm" else "",
                          mm_h_extra=("\n" + line + "\n") if where == "mm_h" else "")
            sites, _ = collect(tmp)
            hit = [s for s in sites if s["class"] == want_class and want_term in s["term"]]
            check(bool(hit), "planted `%s` is %s" % (line.strip(), want_class))
            check(run(tmp, floors=FIXTURE_FLOORS, quiet=True) == 1, "the gate FAILS on planted `%s`" % line.strip())
            if hit:
                hits = [s for s in sites if s["class"] in VIOLATION_CLASSES]
                body = "".join("%s | %s | %s | fixture reading\n" % (s["path"], s["class"], s["term"]) for s in hits)
                _write(os.path.join(tmp, BASELINE_REL), "# fixture\n" + body)
                check(run(tmp, floors=FIXTURE_FLOORS, quiet=True) == 0,
                      "baselined with a reading, `%s` passes" % line.strip())

    # (3) A stale baseline entry fails.
    with tempfile.TemporaryDirectory() as tmp:
        _fixture_tree(tmp)
        _write(os.path.join(tmp, BASELINE_REL),
               "games/x.cpp | VIOLATION-NEGATION | !logic->HasItem(RG_GONE) | fixed long ago\n")
        check(run(tmp, floors=FIXTURE_FLOORS, quiet=True) == 1, "a stale baseline entry fails the gate")

    # (4) A baseline entry without a reading is refused.
    with tempfile.TemporaryDirectory() as tmp:
        _fixture_tree(tmp)
        _write(os.path.join(tmp, BASELINE_REL), "games/x.cpp | VIOLATION-NEGATION | !x | \n")
        check(run(tmp, floors=FIXTURE_FLOORS, quiet=True) == 2, "a baseline entry with an empty reading is refused")

    # (5) Vacuous input is refused, not passed.
    with tempfile.TemporaryDirectory() as tmp:
        check(run(tmp, quiet=True) == 2, "an empty tree is refused as vacuous (exit 2)")
    with tempfile.TemporaryDirectory() as tmp:
        _fixture_tree(tmp)
        check(run(tmp, quiet=True) == 2, "a fixture under the real floors is refused as vacuous (exit 2)")

    print("self-test: %s (%d failure(s))" % ("PASS" if not failures else "FAIL", len(failures)))
    return 0 if not failures else 1


def main(argv):
    args = [a for a in argv[1:] if not a.startswith("--")]
    flags = [a for a in argv[1:] if a.startswith("--")]
    if "--self-test" in flags:
        return self_test()
    root = args[0] if args else os.getcwd()
    return run(root, list_all="--list" in flags)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
