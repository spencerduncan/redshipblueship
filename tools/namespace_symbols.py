#!/usr/bin/env python3
"""
Automatically rename symbols in source files to add game prefix.

This tool is part of the Large-Scale Change (LSC) for symbol namespacing.
It adds OoT_ or MM_ prefixes to colliding symbols to enable a unified build.

Usage:
    python tools/namespace_symbols.py --game oot --symbols symbol_collisions.txt
    python tools/namespace_symbols.py --game mm --symbols symbol_collisions.txt --dir src/code
    python tools/namespace_symbols.py --game oot --symbols symbol_collisions.txt --dry-run

The script is idempotent - running it multiple times produces the same result.

Only CODE is rewritten. String literals (plain, prefixed and raw, double- and
single-quoted) and comments are copied through untouched (#1195): the first
pass (#53) rewrote them too, which turned lookup keys such as the Fast64 limb
marker "gEmptyDL" into "OoT_gEmptyDL" and broke every lookup against them
(#1190). `--audit-literals` lists the literals that still carry a prefixed
symbol, read-only, so a sweep can classify them.

Performance: Uses a single compiled regex pattern for all symbols, enabling
O(files) instead of O(files * symbols) regex operations.
"""

import argparse
import re
import sys
from pathlib import Path
from typing import Iterator, List, Optional, Set, Tuple

# Segment kinds produced by split_source().
CODE = "code"
STRING = "string"  # a "..." / R"(...)" literal, any encoding prefix
CHAR = "char"  # a '...' literal
COMMENT = "comment"  # // ... or /* ... */

_RAW_STRING_PREFIXES = ("R", "u8R", "uR", "UR", "LR")
_RAW_DELIM_FORBIDDEN = frozenset(' ()\\"' + "".join(map(chr, range(0, 32))))  # space, parens, \, ", controls
_IDENT_CHARS = frozenset("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")


def _word_before(text: str, pos: int) -> str:
    """The identifier/number run that ends right before text[pos]."""
    start = pos
    while start > 0 and (text[start - 1] in _IDENT_CHARS or text[start - 1] == "."):
        start -= 1
    return text[start:pos]


def _end_of_quoted(text: str, pos: int, quote: str) -> int:
    """End (exclusive) of the quoted span opening at text[pos].

    Backslash escapes are honoured (including a backslash-newline splice). An
    unterminated literal ends at the newline, as a compiler would diagnose it,
    so a stray quote in a preprocessor #error line cannot swallow the file.
    """
    i = pos + 1
    n = len(text)
    while i < n:
        c = text[i]
        if c == "\\":
            # An escape, or a line splice; a CRLF splice is three characters.
            i += 3 if text.startswith("\r\n", i + 1) else 2
            continue
        if c == quote:
            return i + 1
        if c == "\n":
            return i
        i += 1
    return n


def _line_is_spliced(text: str, newline: int) -> bool:
    """True when the newline at text[newline] follows a backslash (CRLF-aware)."""
    k = newline - 1
    if k >= 0 and text[k] == "\r":
        k -= 1
    return k >= 0 and text[k] == "\\"


def split_source(text: str) -> List[Tuple[str, str]]:
    """Split C/C++ source into ordered (kind, segment) pieces.

    Joining the segments reproduces `text` exactly. Kinds are CODE, STRING,
    CHAR and COMMENT. Handles // comments (with backslash-newline
    continuation), /* */ comments, escaped quotes, encoding prefixes (L, u, U,
    u8: the prefix stays in the code segment), raw strings R"delim(...)delim"
    and C++14 digit separators (1'000 is code, not a char literal).
    """
    segments: List[Tuple[str, str]] = []
    n = len(text)
    code_start = 0
    i = 0

    def flush_code(end: int) -> None:
        if end > code_start:
            segments.append((CODE, text[code_start:end]))

    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if c == "/" and nxt == "/":
            j = i + 2
            while j < n:
                if text[j] == "\n" and not _line_is_spliced(text, j):
                    break
                j += 1
            flush_code(i)
            segments.append((COMMENT, text[i:j]))
            i = code_start = j
        elif c == "/" and nxt == "*":
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            flush_code(i)
            segments.append((COMMENT, text[i:j]))
            i = code_start = j
        elif c == '"':
            word = _word_before(text, i)
            if word in _RAW_STRING_PREFIXES:
                # R"delim( ... )delim": the delimiter is at most 16 chars and
                # holds no space, parenthesis, backslash or control character.
                open_paren = text.find("(", i + 1, i + 18)
                delim = text[i + 1 : open_paren] if open_paren >= 0 else None
                if delim is not None and not any(ch in _RAW_DELIM_FORBIDDEN for ch in delim):
                    close = text.find(")" + delim + '"', open_paren + 1)
                    j = n if close < 0 else close + len(delim) + 2
                else:
                    j = _end_of_quoted(text, i, '"')
            else:
                j = _end_of_quoted(text, i, '"')
            flush_code(i)
            segments.append((STRING, text[i:j]))
            i = code_start = j
        elif c == "'":
            word = _word_before(text, i)
            if word and word[0].isdigit():
                i += 1  # digit separator inside a number: 1'000'000
                continue
            j = _end_of_quoted(text, i, "'")
            flush_code(i)
            segments.append((CHAR, text[i:j]))
            i = code_start = j
        else:
            i += 1
    flush_code(n)
    return segments


def iter_literals(text: str) -> Iterator[Tuple[int, str, str]]:
    """Yield (line, kind, segment) for every non-code segment of `text`."""
    line = 1
    for kind, seg in split_source(text):
        if kind != CODE:
            yield line, kind, seg
        line += seg.count("\n")


class SymbolNamespacer:
    """
    Efficiently namespace symbols using a single compiled regex.

    Instead of running 3,607 separate regexes per file, this compiles
    all symbols into one alternation pattern for single-pass replacement.
    """

    def __init__(self, symbols: Set[str], prefix: str):
        self.symbols = symbols
        self.prefix = prefix
        self._pattern: Optional[re.Pattern] = None
        self._audit_pattern: Optional[re.Pattern] = None

    def _get_pattern(self) -> Optional[re.Pattern]:
        """Lazily compile the combined regex pattern."""
        if self._pattern is None and self.symbols:
            # Sort symbols by length (longest first) to ensure longest match wins
            # e.g., "gSaveContext" matches before "Save"
            sorted_symbols = sorted(self.symbols, key=len, reverse=True)

            # Escape each symbol for regex safety
            escaped = [re.escape(sym) for sym in sorted_symbols]

            # Build alternation pattern with word boundaries
            # Negative lookbehind ensures we don't match already-prefixed symbols
            # Pattern: (?<!OoT_)(?<!MM_)\b(symbol1|symbol2|...)\b
            alternation = '|'.join(escaped)
            pattern_str = r'(?<!OoT_)(?<!MM_)\b(' + alternation + r')\b'

            self._pattern = re.compile(pattern_str)

        return self._pattern

    def namespace_text(self, text: str) -> Tuple[str, int]:
        """Prefix every symbol occurrence in the CODE of `text`.

        String/char literals and comments pass through unchanged (#1195).
        Returns (new_text, replacement_count).
        """
        pattern = self._get_pattern()
        if pattern is None:
            return text, 0

        replacement_count = 0

        def replacer(match: re.Match) -> str:
            nonlocal replacement_count
            replacement_count += 1
            return f"{self.prefix}_{match.group(1)}"

        out = []
        for kind, seg in split_source(text):
            out.append(pattern.sub(replacer, seg) if kind == CODE else seg)
        return "".join(out), replacement_count

    def audit_literals(self, text: str, include_comments: bool = False) -> List[Tuple[int, str, str]]:
        """List (line, kind, segment) for literals naming a prefixed symbol.

        A literal such as "OoT_gEmptyDL" is what the pre-#1195 pass produced
        from "gEmptyDL"; each hit is either a damaged lookup key or a message
        that names the prefixed symbol on purpose, and a human classifies it.
        """
        if not self.symbols:
            return []
        if self._audit_pattern is None:
            alternation = "|".join(re.escape(sym) for sym in sorted(self.symbols, key=len, reverse=True))
            self._audit_pattern = re.compile(r"\b" + re.escape(self.prefix) + r"_(?:" + alternation + r")\b")
        hits = []
        for line, kind, seg in iter_literals(text):
            if kind == COMMENT and not include_comments:
                continue
            if self._audit_pattern.search(seg):
                hits.append((line, kind, seg))
        return hits

    def namespace_file(self, path: Path, dry_run: bool = False) -> int:
        """
        Add prefix to all occurrences of symbols in file.

        Returns the number of replacements made.
        """
        pattern = self._get_pattern()
        if pattern is None:
            return 0  # No symbols to match

        try:
            content = path.read_text()
        except Exception as e:
            print(f"Warning: Could not read {path}: {e}")
            return 0

        original = content
        content, replacement_count = self.namespace_text(content)

        if content != original:
            if dry_run:
                print(f"Would modify: {path} ({replacement_count} replacements)")
            else:
                path.write_text(content)
                print(f"Modified: {path} ({replacement_count} replacements)")

        return replacement_count


def namespace_file(path: Path, symbols: Set[str], prefix: str, dry_run: bool = False) -> int:
    """
    Add prefix to all occurrences of symbols in file.

    Returns the number of replacements made.

    This is a convenience wrapper around SymbolNamespacer for single-file use.
    For processing many files, use SymbolNamespacer directly to reuse the
    compiled regex pattern.
    """
    namespacer = SymbolNamespacer(symbols, prefix)
    return namespacer.namespace_file(path, dry_run)


def load_symbols(symbols_file: Path) -> Set[str]:
    """Load symbol list from file, skipping comments and empty lines."""
    symbols = set()
    with symbols_file.open() as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith('#'):
                symbols.add(line)
    return symbols


AUDIT_SUFFIXES = (".c", ".cpp", ".h", ".hpp", ".inc")


def audit_dirs(src_dirs, namespacer: SymbolNamespacer, include_comments: bool = False) -> int:
    """Print every literal under src_dirs that names a prefixed symbol.

    Read-only. Output is one `path:line: kind: literal` row per hit, sorted, so
    two runs diff cleanly. Returns 1 when anything was found, else 0.
    """
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(errors="backslashreplace")
    hits = 0
    for src_dir in src_dirs:
        for path in sorted(p for p in src_dir.rglob("*") if p.suffix in AUDIT_SUFFIXES and p.is_file()):
            text = path.read_text(encoding="utf-8", errors="replace")
            for line, kind, seg in namespacer.audit_literals(text, include_comments):
                hits += 1
                shown = seg if len(seg) <= 160 else seg[:157] + "..."
                print(f"{path.as_posix()}:{line}: {kind}: {shown}")
    print(f"{hits} literal(s) name a {namespacer.prefix}_-prefixed symbol", file=sys.stderr)
    return 1 if hits else 0


def main():
    parser = argparse.ArgumentParser(
        description="Rename symbols with game prefix for unified build"
    )
    parser.add_argument(
        "--game",
        choices=["oot", "mm"],
        required=True,
        help="Which game's sources to modify"
    )
    parser.add_argument(
        "--symbols",
        required=True,
        help="Path to symbol collision list file"
    )
    parser.add_argument(
        "--dir",
        help="Subdirectory to process (for sharded execution). If not specified, processes all sources."
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show what would be changed without making changes"
    )
    parser.add_argument(
        "--include-headers",
        action="store_true",
        default=True,
        help="Also process header files (default: True)"
    )
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Show detailed output"
    )
    parser.add_argument(
        "--path",
        action="append",
        default=[],
        help="Directory to process instead of the game's default ones (repeatable), e.g. games/oot/soh"
    )
    parser.add_argument(
        "--audit-literals",
        action="store_true",
        help="Read-only: list every string/char literal that names a prefixed symbol "
             "(the damage the pre-#1195 pass did); exits 1 when any is found"
    )
    parser.add_argument(
        "--include-comments",
        action="store_true",
        help="With --audit-literals, also list comments that name a prefixed symbol"
    )
    args = parser.parse_args()

    # Determine prefix and source directories
    prefix = "OoT" if args.game == "oot" else "MM"
    game_dir = Path(f"games/{args.game}")

    # Enhancement layer directory differs by game
    enhancement_dir = "soh" if args.game == "oot" else "2s2h"

    if args.path:
        src_dirs = [Path(p) for p in args.path]
    elif args.dir:
        # Single directory mode
        src_dirs = [game_dir / "src" / args.dir]
    else:
        # Process src/, enhancement layer, and include/
        src_dirs = [
            game_dir / "src",
            game_dir / enhancement_dir,
            game_dir / "include",
        ]

    src_dirs = [d for d in src_dirs if d.exists()]
    if not src_dirs:
        print(f"Error: No source directories found in: {game_dir}")
        return 1

    # Load symbols
    symbols_file = Path(args.symbols)
    if not symbols_file.exists():
        print(f"Error: Symbols file not found: {symbols_file}")
        return 1

    symbols = load_symbols(symbols_file)
    print(f"Loaded {len(symbols)} symbols to rename")

    if args.audit_literals:
        return audit_dirs(src_dirs, SymbolNamespacer(symbols, prefix), args.include_comments)

    if args.dry_run:
        print("DRY RUN - no files will be modified")

    # Process C/C++ files from all source directories
    all_files = []
    for src_dir in src_dirs:
        c_files = list(src_dir.rglob("*.c"))
        cpp_files = list(src_dir.rglob("*.cpp"))
        h_files = list(src_dir.rglob("*.h")) if args.include_headers else []
        inc_files = list(src_dir.rglob("*.inc"))
        all_files.extend(c_files + cpp_files + h_files + inc_files)
        print(f"Found {len(c_files)} .c, {len(cpp_files)} .cpp, {len(h_files)} .h, {len(inc_files)} .inc files in {src_dir}")

    # Use a single SymbolNamespacer to reuse the compiled regex pattern
    namespacer = SymbolNamespacer(symbols, prefix)

    # Pre-compile the pattern (this takes a few seconds with 3600+ symbols)
    print("Compiling regex pattern...")
    namespacer._get_pattern()
    print("Pattern compiled.")

    total_files = 0
    total_replacements = 0

    for path in all_files:
        replacements = namespacer.namespace_file(path, args.dry_run)
        if replacements > 0:
            total_files += 1
            total_replacements += replacements

    print()
    print(f"Summary:")
    print(f"  Files modified: {total_files}")
    print(f"  Total replacements: {total_replacements}")
    print(f"  Prefix used: {prefix}_")

    if args.dry_run:
        print("\nRe-run without --dry-run to apply changes")

    return 0


if __name__ == "__main__":
    exit(main())
