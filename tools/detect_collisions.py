#!/usr/bin/env python3
"""
Detect symbol collisions between OoT and MM codebases.

Outputs a list of functions and global variables that exist in both codebases
and would cause linker conflicts in a unified build.

Usage:
    python tools/detect_collisions.py [--oot-dir DIR] [--mm-dir DIR] [--output FILE]
"""

import argparse
import re
from pathlib import Path
from typing import Set, Tuple


def extract_symbols(src_dir: Path) -> Tuple[Set[str], Set[str]]:
    """
    Extract function and global variable names from C source files.

    Returns:
        Tuple of (functions, globals)
    """
    functions = set()
    globals_ = set()

    # Skip common false positives
    skip_patterns = {
        'if', 'for', 'while', 'switch', 'return', 'sizeof', 'typeof',
        'NULL', 'TRUE', 'FALSE', 'true', 'false',
        # Common macros
        'ARRAY_COUNT', 'ARRAY_COUNTU', 'ABS', 'CLAMP', 'MIN', 'MAX',
    }

    for path in src_dir.rglob("*.c"):
        try:
            content = path.read_text(errors='ignore')
        except Exception as e:
            print(f"Warning: Could not read {path}: {e}")
            continue

        # Remove comments to avoid false positives
        content = re.sub(r'//.*?$', '', content, flags=re.MULTILINE)
        content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)

        # Remove string literals
        content = re.sub(r'"[^"\\]*(?:\\.[^"\\]*)*"', '""', content)

        # Match function definitions: return_type name(params) {
        # This pattern looks for identifier followed by ( and then {
        func_pattern = r'\b([A-Za-z_][A-Za-z0-9_]*)\s*\([^)]*\)\s*\{'
        for match in re.finditer(func_pattern, content):
            name = match.group(1)
            if name not in skip_patterns and not name.startswith('_'):
                functions.add(name)

        # Match global variable definitions at file scope
        # Look for: type name = ... or type name; at start of line (file scope)
        # This is a simplified heuristic
        global_pattern = r'^(?:static\s+)?(?:extern\s+)?(?:const\s+)?(?:volatile\s+)?[A-Za-z_][A-Za-z0-9_]*(?:\s*\*)*\s+([A-Za-z_][A-Za-z0-9_]*)\s*(?:=|;|\[)'
        for match in re.finditer(global_pattern, content, re.MULTILINE):
            name = match.group(1)
            if name not in skip_patterns and not name.startswith('_'):
                globals_.add(name)

    # Also check header files for extern declarations
    for path in src_dir.rglob("*.h"):
        try:
            content = path.read_text(errors='ignore')
        except Exception:
            continue

        # Remove comments
        content = re.sub(r'//.*?$', '', content, flags=re.MULTILINE)
        content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)

        # Match extern declarations
        extern_pattern = r'extern\s+[A-Za-z_][A-Za-z0-9_]*(?:\s*\*)*\s+([A-Za-z_][A-Za-z0-9_]*)\s*(?:;|\[)'
        for match in re.finditer(extern_pattern, content):
            name = match.group(1)
            if name not in skip_patterns:
                globals_.add(name)

    return functions, globals_


def main():
    parser = argparse.ArgumentParser(
        description="Detect symbol collisions between OoT and MM codebases"
    )
    parser.add_argument(
        "--oot-dir",
        default="games/oot/src",
        help="Path to OoT source directory (default: games/oot/src)"
    )
    parser.add_argument(
        "--mm-dir",
        default="games/mm/src",
        help="Path to MM source directory (default: games/mm/src)"
    )
    parser.add_argument(
        "--output",
        default="symbol_collisions.txt",
        help="Output file for collision list (default: symbol_collisions.txt)"
    )
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Show detailed output"
    )
    args = parser.parse_args()

    oot_dir = Path(args.oot_dir)
    mm_dir = Path(args.mm_dir)

    if not oot_dir.exists():
        print(f"Error: OoT directory not found: {oot_dir}")
        return 1
    if not mm_dir.exists():
        print(f"Error: MM directory not found: {mm_dir}")
        return 1

    print(f"Scanning OoT sources in {oot_dir}...")
    oot_funcs, oot_globals = extract_symbols(oot_dir)
    oot_symbols = oot_funcs | oot_globals

    print(f"Scanning MM sources in {mm_dir}...")
    mm_funcs, mm_globals = extract_symbols(mm_dir)
    mm_symbols = mm_funcs | mm_globals

    # Find collisions
    func_collisions = oot_funcs & mm_funcs
    global_collisions = oot_globals & mm_globals
    all_collisions = oot_symbols & mm_symbols

    print()
    print(f"OoT symbols: {len(oot_symbols)} ({len(oot_funcs)} functions, {len(oot_globals)} globals)")
    print(f"MM symbols:  {len(mm_symbols)} ({len(mm_funcs)} functions, {len(mm_globals)} globals)")
    print()
    print(f"Function collisions: {len(func_collisions)}")
    print(f"Global collisions:   {len(global_collisions)}")
    print(f"Total collisions:    {len(all_collisions)}")

    # Write output
    output_path = Path(args.output)
    with output_path.open('w') as f:
        f.write(f"# Symbol collisions between OoT and MM\n")
        f.write(f"# Generated by detect_collisions.py\n")
        f.write(f"# Functions: {len(func_collisions)}, Globals: {len(global_collisions)}\n")
        f.write(f"#\n")
        for sym in sorted(all_collisions):
            f.write(f"{sym}\n")

    print(f"\nCollision list written to: {output_path}")

    if args.verbose and all_collisions:
        print("\nSample collisions (first 20):")
        for sym in sorted(all_collisions)[:20]:
            print(f"  {sym}")
        if len(all_collisions) > 20:
            print(f"  ... and {len(all_collisions) - 20} more")

    return 0


if __name__ == "__main__":
    exit(main())
