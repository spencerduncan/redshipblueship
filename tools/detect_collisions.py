#!/usr/bin/env python3
"""
Detect symbol collisions between OoT and MM codebases.

Outputs a list of functions and global variables that exist in both codebases
and would cause linker conflicts in a unified build.

Usage:
    python tools/detect_collisions.py [--oot-dir DIR] [--mm-dir DIR] [--output FILE]
    python tools/detect_collisions.py --oot-dir games/oot --mm-dir games/mm
"""

import argparse
import re
from pathlib import Path
from typing import List, Set, Tuple


def extract_symbols(src_dirs: List[Path]) -> Tuple[Set[str], Set[str]]:
    """
    Extract function and global variable names from C/C++ source files.

    Args:
        src_dirs: List of directories to scan

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

    for src_dir in src_dirs:
        if not src_dir.exists():
            continue

        # Scan C, C++, header, and include files
        for ext in ["*.c", "*.cpp", "*.h", "*.inc"]:
            for path in src_dir.rglob(ext):
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
                func_pattern = r'\b([A-Za-z_][A-Za-z0-9_]*)\s*\([^)]*\)\s*\{'
                for match in re.finditer(func_pattern, content):
                    name = match.group(1)
                    if name not in skip_patterns:
                        functions.add(name)

                # Extract global variables based on file type
                if path.suffix == ".h":
                    # Match extern declarations
                    extern_pattern = r'extern\s+[A-Za-z_][A-Za-z0-9_]*(?:\s*\*)*\s+([A-Za-z_][A-Za-z0-9_]*)\s*(?:;|\[)'
                    for match in re.finditer(extern_pattern, content):
                        name = match.group(1)
                        if name not in skip_patterns:
                            globals_.add(name)
                else:
                    # Match global variable definitions at file scope
                    global_pattern = r'^(?:static\s+)?(?:extern\s+)?(?:const\s+)?(?:volatile\s+)?[A-Za-z_][A-Za-z0-9_]*(?:\s*\*)*\s+([A-Za-z_][A-Za-z0-9_]*)\s*(?:=|;|\[)'
                    for match in re.finditer(global_pattern, content, re.MULTILINE):
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
        default="games/oot",
        help="Path to OoT game directory (default: games/oot). Scans src/, soh/, and include/ subdirs."
    )
    parser.add_argument(
        "--mm-dir",
        default="games/mm",
        help="Path to MM game directory (default: games/mm). Scans src/, 2s2h/, and include/ subdirs."
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

    oot_base = Path(args.oot_dir)
    mm_base = Path(args.mm_dir)

    # OoT directories: src, enhancement layer (soh), include
    oot_dirs = [oot_base / "src", oot_base / "soh", oot_base / "include"]
    mm_dirs = [mm_base / "src", mm_base / "2s2h", mm_base / "include"]

    existing_oot = [d for d in oot_dirs if d.exists()]
    existing_mm = [d for d in mm_dirs if d.exists()]

    if not existing_oot:
        print(f"Error: No OoT directories found under: {oot_base}")
        return 1
    if not existing_mm:
        print(f"Error: No MM directories found under: {mm_base}")
        return 1

    print(f"Scanning OoT sources in {', '.join(str(d) for d in existing_oot)}...")
    oot_funcs, oot_globals = extract_symbols(existing_oot)
    oot_symbols = oot_funcs | oot_globals

    print(f"Scanning MM sources in {', '.join(str(d) for d in existing_mm)}...")
    mm_funcs, mm_globals = extract_symbols(existing_mm)
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
