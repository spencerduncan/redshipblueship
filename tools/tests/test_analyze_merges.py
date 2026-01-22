#!/usr/bin/env python3
"""
Unit tests for analyze_merges.py

Tests cover:
- Similarity computation
- Category classification
- Symbol analysis (identical, near-identical, different, not found)
- Batch analysis
- Symbol list loading
"""

import json
import sys
import tempfile
import unittest
from pathlib import Path

# Add parent directory for direct imports
sys.path.insert(0, str(Path(__file__).parent.parent))

from analyze_merges import (
    NEAR_IDENTICAL_THRESHOLD,
    analyze_all_symbols,
    analyze_symbol,
    categorize_match,
    compute_similarity,
    load_symbols,
)
from lib.symbol_locator import SymbolLocator


class TestComputeSimilarity(unittest.TestCase):
    """Tests for the compute_similarity function."""

    def test_identical_strings(self):
        """Test that identical strings return 1.0."""
        code = "void func(void) { return; }"
        self.assertEqual(compute_similarity(code, code), 1.0)

    def test_empty_strings(self):
        """Test that empty strings return 0.0."""
        self.assertEqual(compute_similarity("", ""), 0.0)
        self.assertEqual(compute_similarity("code", ""), 0.0)
        self.assertEqual(compute_similarity("", "code"), 0.0)

    def test_completely_different(self):
        """Test that completely different strings return low similarity."""
        code1 = "aaaaaaaaaa"
        code2 = "bbbbbbbbbb"
        self.assertLess(compute_similarity(code1, code2), 0.5)

    def test_similar_strings(self):
        """Test that similar strings return high similarity."""
        code1 = "void func(int a) { return a; }"
        code2 = "void func(int b) { return b; }"
        similarity = compute_similarity(code1, code2)
        self.assertGreater(similarity, 0.8)


class TestCategorizeMatch(unittest.TestCase):
    """Tests for the categorize_match function."""

    def test_identical(self):
        """Test that 1.0 similarity is categorized as identical."""
        self.assertEqual(categorize_match(1.0), "identical")

    def test_near_identical(self):
        """Test that >95% similarity is categorized as near_identical."""
        self.assertEqual(categorize_match(0.99), "near_identical")
        self.assertEqual(categorize_match(0.96), "near_identical")
        self.assertEqual(categorize_match(NEAR_IDENTICAL_THRESHOLD), "near_identical")

    def test_different(self):
        """Test that <95% similarity is categorized as different."""
        self.assertEqual(categorize_match(0.94), "different")
        self.assertEqual(categorize_match(0.5), "different")
        self.assertEqual(categorize_match(0.0), "different")


class TestLoadSymbols(unittest.TestCase):
    """Tests for the load_symbols function."""

    def setUp(self):
        """Create a temporary directory for test files."""
        self.temp_dir = tempfile.TemporaryDirectory()

    def tearDown(self):
        """Clean up temporary directory (exception-safe)."""
        self.temp_dir.cleanup()

    def _write_symbols_file(self, content: str) -> str:
        """Helper to write a symbols file and return its path."""
        path = Path(self.temp_dir.name) / "symbols.txt"
        path.write_text(content)
        return str(path)

    def test_load_simple_list(self):
        """Test loading a simple symbol list."""
        path = self._write_symbols_file("Symbol1\nSymbol2\nSymbol3\n")
        symbols = load_symbols(path)
        self.assertEqual(symbols, ["Symbol1", "Symbol2", "Symbol3"])

    def test_skip_comments(self):
        """Test that comment lines are skipped."""
        path = self._write_symbols_file(
            "# This is a comment\nSymbol1\n# Another comment\nSymbol2\n"
        )
        symbols = load_symbols(path)
        self.assertEqual(symbols, ["Symbol1", "Symbol2"])

    def test_skip_empty_lines(self):
        """Test that empty lines are skipped."""
        path = self._write_symbols_file("Symbol1\n\n  \nSymbol2\n")
        symbols = load_symbols(path)
        self.assertEqual(symbols, ["Symbol1", "Symbol2"])

    def test_strip_whitespace(self):
        """Test that whitespace is stripped from symbol names."""
        path = self._write_symbols_file("  Symbol1  \nSymbol2\t\n")
        symbols = load_symbols(path)
        self.assertEqual(symbols, ["Symbol1", "Symbol2"])


class TestAnalyzeSymbol(unittest.TestCase):
    """Tests for the analyze_symbol function."""

    def setUp(self):
        """Create temporary directories for test files."""
        self.temp_dir = tempfile.TemporaryDirectory()
        self.oot_dir = Path(self.temp_dir.name) / "oot" / "src"
        self.mm_dir = Path(self.temp_dir.name) / "mm" / "src"
        self.oot_dir.mkdir(parents=True)
        self.mm_dir.mkdir(parents=True)

    def tearDown(self):
        """Clean up temporary directories."""
        self.temp_dir.cleanup()

    def write_oot_file(self, name: str, content: str) -> Path:
        """Helper to write a C file to the OoT directory."""
        path = self.oot_dir / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
        return path

    def write_mm_file(self, name: str, content: str) -> Path:
        """Helper to write a C file to the MM directory."""
        path = self.mm_dir / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
        return path

    def create_locator(self) -> SymbolLocator:
        """Create a SymbolLocator with test directories."""
        return SymbolLocator(
            oot_dir=str(self.oot_dir),
            mm_dir=str(self.mm_dir),
        )

    def test_identical_functions(self):
        """Test analysis of identical functions."""
        self.write_oot_file(
            "test.c",
            """
void OoT_TestFunc(int x) {
    return x + 1;
}
""",
        )
        self.write_mm_file(
            "test.c",
            """
void MM_TestFunc(int x) {
    return x + 1;
}
""",
        )

        locator = self.create_locator()
        result = analyze_symbol("TestFunc", locator)

        self.assertEqual(result["name"], "TestFunc")
        self.assertEqual(result["category"], "identical")
        self.assertEqual(result["normalized_match"], 1.0)
        self.assertIsNotNone(result["oot_file"])
        self.assertIsNotNone(result["mm_file"])

    def test_different_functions(self):
        """Test analysis of different functions."""
        self.write_oot_file(
            "test.c",
            """
int OoT_Calculate(int x) {
    return x * 2;
}
""",
        )
        self.write_mm_file(
            "test.c",
            """
int MM_Calculate(int x) {
    return x + x + x + x + x;
}
""",
        )

        locator = self.create_locator()
        result = analyze_symbol("Calculate", locator)

        self.assertEqual(result["name"], "Calculate")
        self.assertEqual(result["category"], "different")
        self.assertLess(result["normalized_match"], NEAR_IDENTICAL_THRESHOLD)

    def test_not_found_in_either(self):
        """Test analysis when symbol not found in either codebase."""
        self.write_oot_file("test.c", "void OoT_Other(void) { }")
        self.write_mm_file("test.c", "void MM_Other(void) { }")

        locator = self.create_locator()
        result = analyze_symbol("NonExistent", locator)

        self.assertEqual(result["name"], "NonExistent")
        self.assertEqual(result["category"], "not_found")
        self.assertIsNone(result["oot_file"])
        self.assertIsNone(result["mm_file"])

    def test_not_found_in_oot(self):
        """Test analysis when symbol only in MM."""
        self.write_oot_file("test.c", "void OoT_Other(void) { }")
        self.write_mm_file("test.c", "void MM_OnlyInMM(void) { }")

        locator = self.create_locator()
        result = analyze_symbol("OnlyInMM", locator)

        self.assertEqual(result["category"], "not_found")
        self.assertIsNone(result["oot_file"])
        self.assertIsNotNone(result["mm_file"])

    def test_not_found_in_mm(self):
        """Test analysis when symbol only in OoT."""
        self.write_oot_file("test.c", "void OoT_OnlyInOoT(void) { }")
        self.write_mm_file("test.c", "void MM_Other(void) { }")

        locator = self.create_locator()
        result = analyze_symbol("OnlyInOoT", locator)

        self.assertEqual(result["category"], "not_found")
        self.assertIsNotNone(result["oot_file"])
        self.assertIsNone(result["mm_file"])

    def test_verbose_includes_code(self):
        """Test that verbose mode includes code snippets."""
        self.write_oot_file(
            "test.c",
            """
void OoT_Verbose(void) {
    return;
}
""",
        )
        self.write_mm_file(
            "test.c",
            """
void MM_Verbose(void) {
    return;
}
""",
        )

        locator = self.create_locator()
        result = analyze_symbol("Verbose", locator, verbose=True)

        self.assertIn("oot_code", result)
        self.assertIn("mm_code", result)
        self.assertIn("oot_normalized", result)
        self.assertIn("mm_normalized", result)

    def test_typedef_normalization(self):
        """Test that typedefs are normalized (u32 -> unsigned int)."""
        self.write_oot_file(
            "test.c",
            """
u32 OoT_GetValue(void) {
    return 42;
}
""",
        )
        self.write_mm_file(
            "test.c",
            """
unsigned int MM_GetValue(void) {
    return 42;
}
""",
        )

        locator = self.create_locator()
        result = analyze_symbol("GetValue", locator)

        self.assertEqual(result["category"], "identical")
        self.assertEqual(result["normalized_match"], 1.0)

    def test_comment_normalization(self):
        """Test that comments don't affect matching."""
        self.write_oot_file(
            "test.c",
            """
void OoT_Commented(void) {
    // OoT comment
    return;
}
""",
        )
        self.write_mm_file(
            "test.c",
            """
void MM_Commented(void) {
    /* MM comment */
    return;
}
""",
        )

        locator = self.create_locator()
        result = analyze_symbol("Commented", locator)

        self.assertEqual(result["category"], "identical")


class TestAnalyzeAllSymbols(unittest.TestCase):
    """Tests for the analyze_all_symbols function."""

    def setUp(self):
        """Create temporary directories for test files."""
        self.temp_dir = tempfile.TemporaryDirectory()
        self.oot_dir = Path(self.temp_dir.name) / "oot" / "src"
        self.mm_dir = Path(self.temp_dir.name) / "mm" / "src"
        self.oot_dir.mkdir(parents=True)
        self.mm_dir.mkdir(parents=True)

    def tearDown(self):
        """Clean up temporary directories."""
        self.temp_dir.cleanup()

    def write_oot_file(self, name: str, content: str) -> Path:
        """Helper to write a C file to the OoT directory."""
        path = self.oot_dir / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
        return path

    def write_mm_file(self, name: str, content: str) -> Path:
        """Helper to write a C file to the MM directory."""
        path = self.mm_dir / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
        return path

    def create_locator(self) -> SymbolLocator:
        """Create a SymbolLocator with test directories."""
        return SymbolLocator(
            oot_dir=str(self.oot_dir),
            mm_dir=str(self.mm_dir),
        )

    def test_batch_analysis(self):
        """Test batch analysis of multiple symbols."""
        self.write_oot_file(
            "funcs.c",
            """
void OoT_Identical(void) { return; }
void OoT_Different(void) { return 1; }
void OoT_OnlyOoT(void) { return; }
""",
        )
        self.write_mm_file(
            "funcs.c",
            """
void MM_Identical(void) { return; }
void MM_Different(void) { return 99999; }
void MM_OnlyMM(void) { return; }
""",
        )

        locator = self.create_locator()
        symbols = ["Identical", "Different", "NotFound"]
        report = analyze_all_symbols(symbols, locator)

        # Check summary
        self.assertEqual(report["summary"]["total"], 3)
        self.assertGreaterEqual(report["summary"]["identical"], 1)
        self.assertGreaterEqual(report["summary"]["not_found"], 1)

        # Check that we have results for each symbol
        self.assertEqual(len(report["symbols"]), 3)

        # Verify symbol names are in results
        names = [s["name"] for s in report["symbols"]]
        self.assertIn("Identical", names)
        self.assertIn("Different", names)
        self.assertIn("NotFound", names)

    def test_output_format(self):
        """Test that output matches expected JSON format."""
        self.write_oot_file("test.c", "void OoT_Test(void) { }")
        self.write_mm_file("test.c", "void MM_Test(void) { }")

        locator = self.create_locator()
        report = analyze_all_symbols(["Test"], locator)

        # Verify structure
        self.assertIn("summary", report)
        self.assertIn("symbols", report)

        # Verify summary fields
        summary = report["summary"]
        self.assertIn("total", summary)
        self.assertIn("identical", summary)
        self.assertIn("near_identical", summary)
        self.assertIn("different", summary)
        self.assertIn("not_found", summary)

        # Verify symbol fields
        sym = report["symbols"][0]
        self.assertIn("name", sym)
        self.assertIn("category", sym)
        self.assertIn("oot_file", sym)
        self.assertIn("mm_file", sym)
        self.assertIn("normalized_match", sym)


class TestIntegrationWithRealCodebase(unittest.TestCase):
    """
    Integration tests using the real OoT/MM codebase.

    These tests are skipped if the game directories don't exist.
    """

    @classmethod
    def setUpClass(cls):
        """Check if real codebase exists."""
        cls.oot_dir = Path("games/oot/src")
        cls.mm_dir = Path("games/mm/src")
        cls.has_codebase = cls.oot_dir.exists() and cls.mm_dir.exists()

    def test_analyze_real_symbol(self):
        """Test analyzing a real symbol from the codebase."""
        if not self.has_codebase:
            self.skipTest("OoT/MM codebase not available")

        locator = SymbolLocator()
        result = analyze_symbol("Actor_Init", locator)

        # Should find something (either match or not_found)
        self.assertIn("name", result)
        self.assertIn("category", result)


if __name__ == "__main__":
    unittest.main()
