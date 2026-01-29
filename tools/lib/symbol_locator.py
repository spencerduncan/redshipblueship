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
        oot_include_dir: str = "games/oot/include",
        mm_include_dir: str = "games/mm/include",
        oot_extra_dirs: Optional[list] = None,
        mm_extra_dirs: Optional[list] = None,
    ):
        """
        Initialize the symbol locator.

        Args:
            oot_dir: Path to OoT source directory
            mm_dir: Path to MM source directory
            oot_include_dir: Path to OoT include directory
            mm_include_dir: Path to MM include directory
            oot_extra_dirs: Additional OoT directories to scan (e.g. enhancement layer)
            mm_extra_dirs: Additional MM directories to scan (e.g. enhancement layer)
        """
        self.oot_dir = Path(oot_dir)
        self.mm_dir = Path(mm_dir)
        self.oot_include_dir = Path(oot_include_dir)
        self.mm_include_dir = Path(mm_include_dir)
        self.oot_extra_dirs = [Path(d) for d in (oot_extra_dirs or ["games/oot/soh"])]
        self.mm_extra_dirs = [Path(d) for d in (mm_extra_dirs or ["games/mm/2s2h"])]

        # Symbol -> file path mappings
        self._oot_symbols: Dict[str, Path] = {}
        self._mm_symbols: Dict[str, Path] = {}

        # Build the index
        self._index_built = False

    def _build_index(self) -> None:
        """Build the symbol index from source and header files."""
        if self._index_built:
            return

        # Scan OoT directories (src first, then include, then extras)
        if self.oot_dir.exists():
            self._oot_symbols = self._extract_symbols(self.oot_dir)
        if self.oot_include_dir.exists():
            include_symbols = self._extract_symbols(self.oot_include_dir)
            for name, path in include_symbols.items():
                if name not in self._oot_symbols:
                    self._oot_symbols[name] = path
        for extra_dir in self.oot_extra_dirs:
            if extra_dir.exists():
                extra_symbols = self._extract_symbols(extra_dir)
                for name, path in extra_symbols.items():
                    if name not in self._oot_symbols:
                        self._oot_symbols[name] = path

        # Scan MM directories (src first, then include, then extras)
        if self.mm_dir.exists():
            self._mm_symbols = self._extract_symbols(self.mm_dir)
        if self.mm_include_dir.exists():
            include_symbols = self._extract_symbols(self.mm_include_dir)
            for name, path in include_symbols.items():
                if name not in self._mm_symbols:
                    self._mm_symbols[name] = path
        for extra_dir in self.mm_extra_dirs:
            if extra_dir.exists():
                extra_symbols = self._extract_symbols(extra_dir)
                for name, path in extra_symbols.items():
                    if name not in self._mm_symbols:
                        self._mm_symbols[name] = path

        self._index_built = True

    def _extract_symbols(self, src_dir: Path) -> Dict[str, Path]:
        """
        Extract function names and global variables from C source and header files.

        Args:
            src_dir: Source directory to scan

        Returns:
            Dictionary mapping symbol names to file paths
        """
        symbols: Dict[str, Path] = {}

        # Skip common false positives (control flow keywords, macros, types)
        skip_patterns = {
            "if", "for", "while", "switch", "return", "sizeof", "typeof",
            "NULL", "TRUE", "FALSE", "true", "false",
            "ARRAY_COUNT", "ARRAY_COUNTU", "ABS", "CLAMP", "MIN", "MAX",
            # Common type keywords that might be matched
            "void", "int", "char", "float", "double", "long", "short",
            "struct", "union", "enum", "typedef", "const", "static",
            "extern", "unsigned", "signed", "volatile", "register",
        }

        # Match function definitions: name(params) {
        # Based on detect_collisions.py pattern
        func_pattern = r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\([^)]*\)\s*\{"

        # Match global variable definitions in .c files:
        # Type varName; or Type varName = value; or Type varName[size];
        # This pattern matches lines that start (after optional whitespace) with
        # an optional storage/type qualifier, then a type, then a variable name
        # Examples:
        #   SaveContext gSaveContext;
        #   u8 gWeatherMode = 0;
        #   const s16 D_8014A6C0[] = { ... };
        #   static f32 sScale;
        #   SaveContext gSaveContext ALIGNED(16);  // with attribute macro
        global_var_pattern = (
            r"^(?:static\s+|const\s+|volatile\s+)*"  # Optional qualifiers
            r"(?:unsigned\s+|signed\s+)?"  # Optional sign specifier
            r"[A-Za-z_][A-Za-z0-9_]*"  # Type name
            r"(?:\s*\*)*"  # Optional pointer stars
            r"\s+([A-Za-z_][A-Za-z0-9_]*)"  # Variable name (captured)
            r"\s*(?:\[[^\]]*\])?"  # Optional array brackets
            r"(?:\s+[A-Z_]+\s*\([^)]*\))?"  # Optional attribute macro like ALIGNED(16)
            r"\s*(?:=|;)"  # Followed by = or ;
        )

        # Match extern declarations in .h files:
        # extern Type varName; or extern Type varName[];
        extern_pattern = (
            r"^extern\s+"  # extern keyword
            r"(?:const\s+|volatile\s+)*"  # Optional qualifiers
            r"(?:unsigned\s+|signed\s+)?"  # Optional sign specifier
            r"[A-Za-z_][A-Za-z0-9_]*"  # Type name
            r"(?:\s*\*)*"  # Optional pointer stars
            r"\s+([A-Za-z_][A-Za-z0-9_]*)"  # Variable name (captured)
            r"\s*(?:\[[^\]]*\])?"  # Optional array brackets
            r"\s*;"  # Followed by semicolon
        )

        # Scan C, C++, header, and include files
        for ext in ["*.c", "*.cpp", "*.h", "*.inc"]:
            for path in src_dir.rglob(ext):
                try:
                    content = path.read_text(errors="ignore")
                except Exception:
                    continue

                # Remove comments to avoid false positives
                clean_content = re.sub(r"//.*?$", "", content, flags=re.MULTILINE)
                clean_content = re.sub(r"/\*.*?\*/", "", clean_content, flags=re.DOTALL)

                # Remove string literals
                clean_content = re.sub(r'"[^"\\]*(?:\\.[^"\\]*)*"', '""', clean_content)

                # Extract function definitions
                for match in re.finditer(func_pattern, clean_content):
                    name = match.group(1)
                    if name not in skip_patterns:
                        # Store first occurrence (definition location)
                        if name not in symbols:
                            symbols[name] = path

                # Extract global variables based on file type
                if path.suffix in (".h",):
                    # In headers, look for extern declarations
                    for match in re.finditer(extern_pattern, clean_content, re.MULTILINE):
                        name = match.group(1)
                        if name not in skip_patterns:
                            if name not in symbols:
                                symbols[name] = path
                else:
                    # In .c files, look for global variable definitions
                    for match in re.finditer(global_var_pattern, clean_content, re.MULTILINE):
                        name = match.group(1)
                        if name not in skip_patterns:
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


# Module-level singleton for convenience function
_default_locator: Optional[SymbolLocator] = None


def locate_symbol(symbol_name: str, game: str) -> Optional[str]:
    """
    Locate a symbol in the specified game's source directory.

    This is a convenience function that uses a module-level SymbolLocator.

    Args:
        symbol_name: The symbol name to locate (function or global variable)
        game: The game to search in ("oot" or "mm")

    Returns:
        File path where symbol is defined, or None if not found.
    """
    global _default_locator
    if _default_locator is None:
        _default_locator = SymbolLocator()

    oot_file, mm_file = _default_locator.locate(symbol_name)

    if game.lower() == "oot":
        return str(oot_file) if oot_file else None
    elif game.lower() == "mm":
        return str(mm_file) if mm_file else None
    else:
        raise ValueError(f"Unknown game: {game}. Expected 'oot' or 'mm'.")
