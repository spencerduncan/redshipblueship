#!/usr/bin/env python3
"""
Symbol locator for OoT and MM codebases.

Maps symbol names to their source file locations in both game codebases.
Handles both bare names (Actor_Init) and prefixed names (OoT_Actor_Init).

Usage:
    from tools.lib.symbol_locator import SymbolLocator

    locator = SymbolLocator()
    oot_file, mm_file = locator.locate("PreRender_Init")
"""

import re
from pathlib import Path
from typing import Dict, Optional, Tuple


class SymbolLocator:
    """
    Locates symbols in OoT and MM source directories.

    Builds an index of function definitions and maps symbol names
    to their source file locations.
    """

    # Prefixes used for game-specific symbol namespacing
    OOT_PREFIX = "OoT_"
    MM_PREFIX = "MM_"

    def __init__(
        self,
        oot_dir: str = "games/oot/src",
        mm_dir: str = "games/mm/src",
    ):
        """
        Initialize the symbol locator.

        Args:
            oot_dir: Path to OoT source directory
            mm_dir: Path to MM source directory
        """
        self.oot_dir = Path(oot_dir)
        self.mm_dir = Path(mm_dir)

        # Symbol -> file path mappings
        self._oot_symbols: Dict[str, Path] = {}
        self._mm_symbols: Dict[str, Path] = {}

        # Build the index
        self._index_built = False

    def _build_index(self) -> None:
        """Build the symbol index from source files."""
        if self._index_built:
            return

        if self.oot_dir.exists():
            self._oot_symbols = self._extract_symbols(self.oot_dir)
        if self.mm_dir.exists():
            self._mm_symbols = self._extract_symbols(self.mm_dir)

        self._index_built = True

    def _extract_symbols(self, src_dir: Path) -> Dict[str, Path]:
        """
        Extract function names from C source files.

        Args:
            src_dir: Source directory to scan

        Returns:
            Dictionary mapping symbol names to file paths
        """
        symbols: Dict[str, Path] = {}

        # Skip common false positives (control flow keywords, macros)
        skip_patterns = {
            "if", "for", "while", "switch", "return", "sizeof", "typeof",
            "NULL", "TRUE", "FALSE", "true", "false",
            "ARRAY_COUNT", "ARRAY_COUNTU", "ABS", "CLAMP", "MIN", "MAX",
        }

        # Match function definitions: name(params) {
        # Based on detect_collisions.py pattern
        func_pattern = r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\([^)]*\)\s*\{"

        for path in src_dir.rglob("*.c"):
            try:
                content = path.read_text(errors="ignore")
            except Exception:
                continue

            # Remove comments to avoid false positives
            content = re.sub(r"//.*?$", "", content, flags=re.MULTILINE)
            content = re.sub(r"/\*.*?\*/", "", content, flags=re.DOTALL)

            # Remove string literals
            content = re.sub(r'"[^"\\]*(?:\\.[^"\\]*)*"', '""', content)

            for match in re.finditer(func_pattern, content):
                name = match.group(1)
                if name not in skip_patterns and not name.startswith("_"):
                    # Store first occurrence (definition location)
                    if name not in symbols:
                        symbols[name] = path

        return symbols

    def locate(self, symbol: str) -> Tuple[Optional[Path], Optional[Path]]:
        """
        Locate a symbol in OoT and MM source directories.

        Handles both bare names (Actor_Init) and prefixed names (OoT_Actor_Init).

        Args:
            symbol: The symbol name to locate

        Returns:
            Tuple of (oot_file, mm_file) where each is the Path to the file
            containing the symbol definition, or None if not found.
        """
        self._build_index()

        oot_file: Optional[Path] = None
        mm_file: Optional[Path] = None

        # Check if symbol has a game prefix
        if symbol.startswith(self.OOT_PREFIX):
            # OoT-prefixed symbol - look for exact match in OoT
            oot_file = self._oot_symbols.get(symbol)
        elif symbol.startswith(self.MM_PREFIX):
            # MM-prefixed symbol - look for exact match in MM
            mm_file = self._mm_symbols.get(symbol)
        else:
            # Bare name - look for prefixed versions in each codebase
            # Also check for bare name itself (some symbols aren't prefixed)
            oot_prefixed = f"{self.OOT_PREFIX}{symbol}"
            mm_prefixed = f"{self.MM_PREFIX}{symbol}"

            # Check OoT: prefixed first, then bare
            oot_file = self._oot_symbols.get(oot_prefixed)
            if oot_file is None:
                oot_file = self._oot_symbols.get(symbol)

            # Check MM: prefixed first, then bare
            mm_file = self._mm_symbols.get(mm_prefixed)
            if mm_file is None:
                mm_file = self._mm_symbols.get(symbol)

        return (oot_file, mm_file)

    def get_oot_symbols(self) -> Dict[str, Path]:
        """Get all indexed OoT symbols."""
        self._build_index()
        return dict(self._oot_symbols)

    def get_mm_symbols(self) -> Dict[str, Path]:
        """Get all indexed MM symbols."""
        self._build_index()
        return dict(self._mm_symbols)
