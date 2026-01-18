#!/usr/bin/env python3
"""
Unit tests for namespace_symbols.py

Tests cover:
- Symbol renaming with prefixes
- Idempotency (running twice produces same result)
- Edge cases (already prefixed symbols, substrings)
- File loading
"""

import tempfile
import unittest
from pathlib import Path

# Path setup handled by conftest.py (auto-loaded by pytest)
from namespace_symbols import namespace_file, load_symbols


class TestNamespaceFile(unittest.TestCase):
    """Tests for the namespace_file function."""

    def setUp(self):
        """Create a temporary directory for test files."""
        self.temp_dir = tempfile.TemporaryDirectory()
        self.temp_path = Path(self.temp_dir.name)

    def tearDown(self):
        """Clean up temporary directory."""
        self.temp_dir.cleanup()

    def write_file(self, name: str, content: str) -> Path:
        """Helper to write a file to the temp directory."""
        path = self.temp_path / name
        path.write_text(content)
        return path

    # =========================================================================
    # Basic Renaming
    # =========================================================================

    def test_simple_rename(self):
        """Test simple symbol renaming."""
        path = self.write_file("test.c", "void MyFunc(void) { }")
        symbols = {"MyFunc"}

        namespace_file(path, symbols, "OoT", dry_run=False)

        result = path.read_text()
        self.assertIn("OoT_MyFunc", result)
        self.assertNotIn("void MyFunc", result)

    def test_multiple_occurrences(self):
        """Test renaming multiple occurrences of same symbol."""
        path = self.write_file("test.c", """
void MyFunc(void);
void MyFunc(void) {
    MyFunc();  // recursive call
}
""")
        symbols = {"MyFunc"}

        count = namespace_file(path, symbols, "OoT", dry_run=False)

        result = path.read_text()
        self.assertEqual(result.count("OoT_MyFunc"), 3)
        self.assertEqual(result.count("MyFunc"), 3)  # All should be prefixed
        self.assertGreater(count, 0)

    def test_multiple_symbols(self):
        """Test renaming multiple different symbols."""
        path = self.write_file("test.c", """
void FuncA(void) { }
void FuncB(void) { }
int gVarA = 1;
""")
        symbols = {"FuncA", "FuncB", "gVarA"}

        namespace_file(path, symbols, "MM", dry_run=False)

        result = path.read_text()
        self.assertIn("MM_FuncA", result)
        self.assertIn("MM_FuncB", result)
        self.assertIn("MM_gVarA", result)

    # =========================================================================
    # Idempotency
    # =========================================================================

    def test_idempotency(self):
        """Test that running twice produces same result (no double-prefixing)."""
        original = "void MyFunc(void) { MyFunc(); }"
        path = self.write_file("test.c", original)
        symbols = {"MyFunc"}

        # Run once
        namespace_file(path, symbols, "OoT", dry_run=False)
        after_first = path.read_text()

        # Run again
        namespace_file(path, symbols, "OoT", dry_run=False)
        after_second = path.read_text()

        # Results should be identical
        self.assertEqual(after_first, after_second)
        # Should have exactly 2 occurrences of the prefixed name
        self.assertEqual(after_second.count("OoT_MyFunc"), 2)
        # No double-prefixing
        self.assertNotIn("OoT_OoT_", after_second)

    def test_already_prefixed_oot(self):
        """Test that OoT_ prefixed symbols are not modified."""
        path = self.write_file("test.c", "void OoT_MyFunc(void) { }")
        symbols = {"MyFunc"}

        namespace_file(path, symbols, "OoT", dry_run=False)

        result = path.read_text()
        self.assertEqual(result.count("OoT_MyFunc"), 1)
        self.assertNotIn("OoT_OoT_", result)

    def test_already_prefixed_mm(self):
        """Test that MM_ prefixed symbols are not modified."""
        path = self.write_file("test.c", "void MM_MyFunc(void) { }")
        symbols = {"MyFunc"}

        namespace_file(path, symbols, "MM", dry_run=False)

        result = path.read_text()
        self.assertEqual(result.count("MM_MyFunc"), 1)
        self.assertNotIn("MM_MM_", result)

    def test_cross_prefix_preserved(self):
        """Test that OoT_ prefix is preserved when running MM namespacing."""
        path = self.write_file("test.c", """
void OoT_MyFunc(void) { }
void OtherFunc(void) { }
""")
        symbols = {"MyFunc", "OtherFunc"}

        namespace_file(path, symbols, "MM", dry_run=False)

        result = path.read_text()
        # OoT_MyFunc should be unchanged
        self.assertIn("OoT_MyFunc", result)
        self.assertNotIn("MM_OoT_MyFunc", result)
        # OtherFunc should be prefixed
        self.assertIn("MM_OtherFunc", result)

    # =========================================================================
    # Word Boundary Matching
    # =========================================================================

    def test_word_boundary(self):
        """Test that only whole words are matched."""
        path = self.write_file("test.c", """
void PlayInit(void) { }
void Play(void) { }
void PlayerInit(void) { }
""")
        symbols = {"Play"}

        namespace_file(path, symbols, "OoT", dry_run=False)

        result = path.read_text()
        # Only "Play" should be prefixed, not PlayInit or PlayerInit
        self.assertIn("OoT_Play", result)
        self.assertIn("PlayInit", result)  # Not prefixed
        self.assertNotIn("OoT_PlayInit", result)
        self.assertIn("PlayerInit", result)  # Not prefixed
        self.assertNotIn("OoT_PlayerInit", result)

    def test_substring_not_matched(self):
        """Test that symbol substrings are not matched."""
        path = self.write_file("test.c", """
int gSaveContext = 1;
int gSaveContextSize = sizeof(gSaveContext);
int ContextSave = 2;
""")
        symbols = {"gSaveContext"}

        namespace_file(path, symbols, "OoT", dry_run=False)

        result = path.read_text()
        # gSaveContext should be prefixed
        self.assertIn("OoT_gSaveContext", result)
        # gSaveContextSize should NOT have the prefix injected into it
        self.assertNotIn("OoT_gSaveContextSize", result)
        # ContextSave should be unchanged
        self.assertIn("ContextSave", result)
        self.assertNotIn("OoT_ContextSave", result)

    # =========================================================================
    # Edge Cases
    # =========================================================================

    def test_symbol_containing_oot_substring(self):
        """Test symbol that contains 'OoT' as substring (not prefix)."""
        path = self.write_file("test.c", """
void BootOoTGame(void) { }
void xOoT_weird(void) { }
""")
        # These symbols should be renamed since they don't START with OoT_
        symbols = {"BootOoTGame", "xOoT_weird"}

        namespace_file(path, symbols, "OoT", dry_run=False)

        result = path.read_text()
        # Both should be prefixed since neither STARTS with "OoT_"
        self.assertIn("OoT_BootOoTGame", result)
        # xOoT_weird is a weird case - negative lookbehind checks for OoT_ immediately before
        # Since "x" is before "OoT_weird", this WILL match and get prefixed
        # This documents current behavior

    def test_empty_symbols_set(self):
        """Test with empty symbols set."""
        original = "void MyFunc(void) { }"
        path = self.write_file("test.c", original)
        symbols = set()

        count = namespace_file(path, symbols, "OoT", dry_run=False)

        result = path.read_text()
        self.assertEqual(result, original)
        self.assertEqual(count, 0)

    def test_no_matching_symbols(self):
        """Test when file has no matching symbols."""
        original = "void UnrelatedFunc(void) { }"
        path = self.write_file("test.c", original)
        symbols = {"OtherFunc", "DifferentFunc"}

        count = namespace_file(path, symbols, "OoT", dry_run=False)

        result = path.read_text()
        self.assertEqual(result, original)
        self.assertEqual(count, 0)

    # =========================================================================
    # Dry Run
    # =========================================================================

    def test_dry_run_no_modification(self):
        """Test that dry run doesn't modify the file."""
        original = "void MyFunc(void) { }"
        path = self.write_file("test.c", original)
        symbols = {"MyFunc"}

        namespace_file(path, symbols, "OoT", dry_run=True)

        result = path.read_text()
        self.assertEqual(result, original)


class TestLoadSymbols(unittest.TestCase):
    """Tests for the load_symbols function."""

    def setUp(self):
        """Create a temporary directory for test files."""
        self.temp_dir = tempfile.TemporaryDirectory()
        self.temp_path = Path(self.temp_dir.name)

    def tearDown(self):
        """Clean up temporary directory."""
        self.temp_dir.cleanup()

    def test_load_simple(self):
        """Test loading a simple symbol list."""
        path = self.temp_path / "symbols.txt"
        path.write_text("FuncA\nFuncB\nFuncC\n")

        symbols = load_symbols(path)

        self.assertEqual(symbols, {"FuncA", "FuncB", "FuncC"})

    def test_skip_comments(self):
        """Test that comment lines are skipped."""
        path = self.temp_path / "symbols.txt"
        path.write_text("""# This is a comment
FuncA
# Another comment
FuncB
""")
        symbols = load_symbols(path)

        self.assertEqual(symbols, {"FuncA", "FuncB"})

    def test_skip_empty_lines(self):
        """Test that empty lines are skipped."""
        path = self.temp_path / "symbols.txt"
        path.write_text("""FuncA

FuncB

FuncC
""")
        symbols = load_symbols(path)

        self.assertEqual(symbols, {"FuncA", "FuncB", "FuncC"})

    def test_strip_whitespace(self):
        """Test that whitespace is stripped from lines."""
        path = self.temp_path / "symbols.txt"
        path.write_text("  FuncA  \n\tFuncB\t\n")

        symbols = load_symbols(path)

        self.assertEqual(symbols, {"FuncA", "FuncB"})

    def test_real_format(self):
        """Test loading a file in the format detect_collisions.py outputs."""
        path = self.temp_path / "symbols.txt"
        path.write_text("""# Symbol collisions between OoT and MM
# Generated by detect_collisions.py
# Functions: 3120, Globals: 487
#
Actor_Init
gSaveContext
Play_Init
""")
        symbols = load_symbols(path)

        self.assertEqual(symbols, {"Actor_Init", "gSaveContext", "Play_Init"})


if __name__ == "__main__":
    unittest.main()
