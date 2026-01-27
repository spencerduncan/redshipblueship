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

Performance: Uses a single compiled regex pattern for all symbols, enabling
O(files) instead of O(files * symbols) regex operations.
"""

import argparse
import re
from pathlib import Path
from typing import Optional, Set


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
        replacement_count = 0

        def replacer(match: re.Match) -> str:
            nonlocal replacement_count
            replacement_count += 1
            return f"{self.prefix}_{match.group(1)}"

        content = pattern.sub(replacer, content)

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
    args = parser.parse_args()

    # Determine prefix and source directories
    prefix = "OoT" if args.game == "oot" else "MM"
    game_dir = Path(f"games/{args.game}")

    # Enhancement layer directory differs by game
    enhancement_dir = "soh" if args.game == "oot" else "2s2h"

    if args.dir:
        # Single directory mode
        src_dirs = [game_dir / "src" / args.dir]
    else:
        # Process both src/ and enhancement layer
        src_dirs = [
            game_dir / "src",
            game_dir / enhancement_dir,
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
