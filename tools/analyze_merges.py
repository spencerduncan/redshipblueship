#!/usr/bin/env python3
"""
Analyze symbol collisions to determine merge candidates.

This tool analyzes symbols that collide between OoT and MM codebases to
determine which can be merged (identical or nearly identical implementations)
vs which are different and require separate implementations.

Usage:
    # Analyze all symbols from a file
    python tools/analyze_merges.py --symbols symbol_collisions.txt --output merge_report.json

    # Analyze a single symbol with verbose output
    python tools/analyze_merges.py --symbol __d_to_ll --verbose

    # List available symbols
    python tools/analyze_merges.py --list-symbols
"""

import argparse
import json
import sys
from difflib import SequenceMatcher
from pathlib import Path
from typing import Dict, List, Optional, Tuple

# Add tools/lib to path for imports
sys.path.insert(0, str(Path(__file__).parent / "lib"))

from code_extractor import extract_function, extract_global
from normalizer import normalize
from symbol_locator import SymbolLocator


# Match threshold for "near identical" (95%)
NEAR_IDENTICAL_THRESHOLD = 0.95


def compute_similarity(code1: str, code2: str) -> float:
    """
    Compute the similarity ratio between two code strings.

    Uses SequenceMatcher to compute a ratio from 0.0 (completely different)
    to 1.0 (identical).

    Args:
        code1: First code string
        code2: Second code string

    Returns:
        Similarity ratio between 0.0 and 1.0
    """
    if not code1 or not code2:
        return 0.0
    return SequenceMatcher(None, code1, code2).ratio()


def categorize_match(similarity: float) -> str:
    """
    Categorize a match based on similarity score.

    Args:
        similarity: Similarity ratio from 0.0 to 1.0

    Returns:
        Category string: "identical", "near_identical", or "different"
    """
    if similarity == 1.0:
        return "identical"
    elif similarity >= NEAR_IDENTICAL_THRESHOLD:
        return "near_identical"
    else:
        return "different"


def extract_code(file_path: Path, symbol: str) -> Optional[str]:
    """
    Extract code for a symbol from a file.

    Tries function extraction first, then global variable extraction.

    Args:
        file_path: Path to the source file
        symbol: Symbol name to extract

    Returns:
        Extracted code string, or None if not found
    """
    # Try extracting as a function first
    code = extract_function(file_path, symbol)
    if code is not None:
        return code

    # Try extracting as a global variable
    code = extract_global(file_path, symbol)
    return code


def analyze_symbol(
    symbol: str,
    locator: SymbolLocator,
    verbose: bool = False,
) -> Dict:
    """
    Analyze a single symbol to determine if it can be merged.

    Args:
        symbol: The symbol name to analyze
        locator: SymbolLocator instance for finding files
        verbose: If True, include additional details in output

    Returns:
        Dictionary with analysis results
    """
    result = {
        "name": symbol,
        "category": "not_found",
        "oot_file": None,
        "mm_file": None,
        "normalized_match": 0.0,
    }

    # Locate the symbol in both codebases
    oot_file, mm_file = locator.locate(symbol)

    if oot_file:
        result["oot_file"] = str(oot_file)
    if mm_file:
        result["mm_file"] = str(mm_file)

    # If we can't find the symbol in both codebases, mark as not found
    if oot_file is None or mm_file is None:
        if verbose:
            if oot_file is None and mm_file is None:
                result["reason"] = "Symbol not found in either codebase"
            elif oot_file is None:
                result["reason"] = "Symbol not found in OoT"
            else:
                result["reason"] = "Symbol not found in MM"
        return result

    # Try to find the prefixed symbol names
    oot_symbol = f"OoT_{symbol}"
    mm_symbol = f"MM_{symbol}"

    # Extract code from both files
    oot_code = extract_code(oot_file, oot_symbol)
    if oot_code is None:
        oot_code = extract_code(oot_file, symbol)

    mm_code = extract_code(mm_file, mm_symbol)
    if mm_code is None:
        mm_code = extract_code(mm_file, symbol)

    if oot_code is None or mm_code is None:
        if verbose:
            if oot_code is None and mm_code is None:
                result["reason"] = "Could not extract code from either file"
            elif oot_code is None:
                result["reason"] = "Could not extract code from OoT file"
            else:
                result["reason"] = "Could not extract code from MM file"
        return result

    # Normalize both code snippets
    oot_normalized = normalize(oot_code)
    mm_normalized = normalize(mm_code)

    # Compute similarity
    similarity = compute_similarity(oot_normalized, mm_normalized)
    result["normalized_match"] = round(similarity, 4)
    result["category"] = categorize_match(similarity)

    if verbose:
        result["oot_code"] = oot_code
        result["mm_code"] = mm_code
        result["oot_normalized"] = oot_normalized
        result["mm_normalized"] = mm_normalized

    return result


def load_symbols(filepath: str) -> List[str]:
    """
    Load symbol list from a file.

    Ignores lines starting with # (comments) and empty lines.

    Args:
        filepath: Path to the symbol list file

    Returns:
        List of symbol names
    """
    symbols = []
    with open(filepath, "r") as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith("#"):
                symbols.append(line)
    return symbols


def analyze_all_symbols(
    symbols: List[str],
    locator: SymbolLocator,
    verbose: bool = False,
) -> Dict:
    """
    Analyze all symbols and generate a report.

    Args:
        symbols: List of symbol names to analyze
        locator: SymbolLocator instance
        verbose: If True, print progress

    Returns:
        Dictionary with summary and per-symbol results
    """
    results = []
    summary = {
        "total": len(symbols),
        "identical": 0,
        "near_identical": 0,
        "different": 0,
        "not_found": 0,
    }

    for i, symbol in enumerate(symbols):
        if verbose:
            print(f"Analyzing {i + 1}/{len(symbols)}: {symbol}", file=sys.stderr)

        result = analyze_symbol(symbol, locator, verbose=False)
        results.append(result)

        # Update summary
        category = result["category"]
        summary[category] = summary.get(category, 0) + 1

    return {
        "summary": summary,
        "symbols": results,
    }


def list_available_symbols(locator: SymbolLocator) -> None:
    """
    List all symbols available in both codebases.

    Prints symbols that exist in both OoT and MM.
    """
    oot_symbols = set(locator.get_oot_symbols().keys())
    mm_symbols = set(locator.get_mm_symbols().keys())

    # Find symbols that exist in both (potential collisions)
    # We need to strip prefixes for comparison
    oot_bare = set()
    mm_bare = set()

    for sym in oot_symbols:
        if sym.startswith("OoT_"):
            oot_bare.add(sym[4:])
        else:
            oot_bare.add(sym)

    for sym in mm_symbols:
        if sym.startswith("MM_"):
            mm_bare.add(sym[3:])
        else:
            mm_bare.add(sym)

    # Find common symbols
    common = sorted(oot_bare & mm_bare)

    print(f"# Found {len(common)} symbols in both codebases")
    for sym in common:
        print(sym)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Analyze symbol collisions for merge candidates"
    )

    # Mode selection (mutually exclusive)
    mode_group = parser.add_mutually_exclusive_group(required=True)
    mode_group.add_argument(
        "--symbols",
        metavar="FILE",
        help="Analyze all symbols from a file (one symbol per line)",
    )
    mode_group.add_argument(
        "--symbol",
        metavar="NAME",
        help="Analyze a single symbol",
    )
    mode_group.add_argument(
        "--list-symbols",
        action="store_true",
        help="List all available symbols from both codebases",
    )

    # Options
    parser.add_argument(
        "--output",
        "-o",
        metavar="FILE",
        help="Output file for JSON report (default: stdout)",
    )
    parser.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="Show detailed output",
    )
    parser.add_argument(
        "--oot-dir",
        default="games/oot/src",
        help="Path to OoT source directory",
    )
    parser.add_argument(
        "--mm-dir",
        default="games/mm/src",
        help="Path to MM source directory",
    )
    parser.add_argument(
        "--oot-include-dir",
        default="games/oot/include",
        help="Path to OoT include directory",
    )
    parser.add_argument(
        "--mm-include-dir",
        default="games/mm/include",
        help="Path to MM include directory",
    )

    args = parser.parse_args()

    # Create the symbol locator
    locator = SymbolLocator(
        oot_dir=args.oot_dir,
        mm_dir=args.mm_dir,
        oot_include_dir=args.oot_include_dir,
        mm_include_dir=args.mm_include_dir,
    )

    # Handle list-symbols mode
    if args.list_symbols:
        list_available_symbols(locator)
        return 0

    # Handle single symbol mode
    if args.symbol:
        result = analyze_symbol(args.symbol, locator, verbose=args.verbose)

        if args.verbose:
            print(f"Symbol: {result['name']}")
            print(f"Category: {result['category']}")
            print(f"Normalized match: {result['normalized_match']:.2%}")
            print(f"OoT file: {result.get('oot_file', 'Not found')}")
            print(f"MM file: {result.get('mm_file', 'Not found')}")

            if "reason" in result:
                print(f"Reason: {result['reason']}")

            if "oot_code" in result:
                print("\n--- OoT Code ---")
                print(result["oot_code"])

            if "mm_code" in result:
                print("\n--- MM Code ---")
                print(result["mm_code"])

            if "oot_normalized" in result:
                print("\n--- OoT Normalized ---")
                print(result["oot_normalized"])

            if "mm_normalized" in result:
                print("\n--- MM Normalized ---")
                print(result["mm_normalized"])
        else:
            # Output as JSON for single symbol mode
            output = json.dumps(result, indent=2)
            if args.output:
                with open(args.output, "w") as f:
                    f.write(output)
            else:
                print(output)

        return 0

    # Handle batch mode (--symbols)
    if args.symbols:
        if not Path(args.symbols).exists():
            print(f"Error: Symbol file not found: {args.symbols}", file=sys.stderr)
            return 1

        symbols = load_symbols(args.symbols)
        if not symbols:
            print("Error: No symbols found in file", file=sys.stderr)
            return 1

        print(f"Analyzing {len(symbols)} symbols...", file=sys.stderr)
        report = analyze_all_symbols(symbols, locator, verbose=args.verbose)

        # Print summary
        summary = report["summary"]
        print(f"\nSummary:", file=sys.stderr)
        print(f"  Total:          {summary['total']}", file=sys.stderr)
        print(f"  Identical:      {summary['identical']}", file=sys.stderr)
        print(f"  Near-identical: {summary['near_identical']}", file=sys.stderr)
        print(f"  Different:      {summary['different']}", file=sys.stderr)
        print(f"  Not found:      {summary['not_found']}", file=sys.stderr)

        # Output report
        output = json.dumps(report, indent=2)
        if args.output:
            with open(args.output, "w") as f:
                f.write(output)
            print(f"\nReport written to: {args.output}", file=sys.stderr)
        else:
            print(output)

        return 0

    return 1


if __name__ == "__main__":
    sys.exit(main())
