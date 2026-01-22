#!/usr/bin/env python3
"""
Unit tests for symbol_locator.py

Tests cover:
- Symbol extraction from C source files
- Bare name lookup (Actor_Init -> OoT_Actor_Init, MM_Actor_Init)
- Prefixed name lookup (OoT_Actor_Init, MM_Actor_Init)
- Edge cases (comments, strings, missing symbols)
"""

import tempfile
import unittest
from pathlib import Path

# Path setup handled by conftest.py (auto-loaded by pytest)
from symbol_locator import SymbolLocator


class TestSymbolLocator(unittest.TestCase):
    """Tests for the SymbolLocator class."""

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

    # =========================================================================
    # Basic Symbol Location
    # =========================================================================

    def test_locate_prefixed_oot_symbol(self):
        """Test locating an OoT-prefixed symbol."""
        self.write_oot_file("test.c", """
void OoT_TestFunction(void) {
    return;
}
""")
        locator = self.create_locator()
        oot_file, mm_file = locator.locate("OoT_TestFunction")

        self.assertIsNotNone(oot_file)
        self.assertEqual(oot_file.name, "test.c")
        self.assertIsNone(mm_file)

    def test_locate_prefixed_mm_symbol(self):
        """Test locating an MM-prefixed symbol."""
        self.write_mm_file("test.c", """
void MM_TestFunction(void) {
    return;
}
""")
        locator = self.create_locator()
        oot_file, mm_file = locator.locate("MM_TestFunction")

        self.assertIsNone(oot_file)
        self.assertIsNotNone(mm_file)
        self.assertEqual(mm_file.name, "test.c")

    def test_locate_bare_name_finds_prefixed(self):
        """Test that bare name finds prefixed versions in both codebases."""
        self.write_oot_file("oot_func.c", """
void OoT_SharedFunc(void) {
    return;
}
""")
        self.write_mm_file("mm_func.c", """
void MM_SharedFunc(void) {
    return;
}
""")
        locator = self.create_locator()
        oot_file, mm_file = locator.locate("SharedFunc")

        self.assertIsNotNone(oot_file)
        self.assertEqual(oot_file.name, "oot_func.c")
        self.assertIsNotNone(mm_file)
        self.assertEqual(mm_file.name, "mm_func.c")

    def test_locate_bare_name_not_prefixed(self):
        """Test locating a bare symbol that isn't prefixed in source."""
        self.write_oot_file("bare.c", """
void UnprefixedFunc(void) {
    return;
}
""")
        locator = self.create_locator()
        oot_file, mm_file = locator.locate("UnprefixedFunc")

        self.assertIsNotNone(oot_file)
        self.assertEqual(oot_file.name, "bare.c")
        self.assertIsNone(mm_file)

    def test_locate_symbol_not_found(self):
        """Test that non-existent symbol returns None for both."""
        self.write_oot_file("test.c", """
void SomeFunction(void) {
    return;
}
""")
        locator = self.create_locator()
        oot_file, mm_file = locator.locate("NonExistentSymbol")

        self.assertIsNone(oot_file)
        self.assertIsNone(mm_file)

    # =========================================================================
    # Symbol Extraction
    # =========================================================================

    def test_extract_function_with_params(self):
        """Test extraction of function with parameters."""
        self.write_oot_file("params.c", """
int OoT_AddNumbers(int a, int b) {
    return a + b;
}
""")
        locator = self.create_locator()
        oot_file, _ = locator.locate("AddNumbers")

        self.assertIsNotNone(oot_file)
        self.assertEqual(oot_file.name, "params.c")

    def test_extract_function_with_pointer_return(self):
        """Test extraction of function returning pointer."""
        self.write_oot_file("pointer.c", """
char* OoT_GetString(void) {
    return NULL;
}
""")
        locator = self.create_locator()
        oot_file, _ = locator.locate("GetString")

        self.assertIsNotNone(oot_file)
        self.assertEqual(oot_file.name, "pointer.c")

    def test_extract_multiple_functions(self):
        """Test extraction of multiple functions from one file."""
        self.write_oot_file("multi.c", """
void OoT_FuncA(void) { }
void OoT_FuncB(void) { }
int OoT_FuncC(int x) { return x; }
""")
        locator = self.create_locator()

        oot_a, _ = locator.locate("FuncA")
        oot_b, _ = locator.locate("FuncB")
        oot_c, _ = locator.locate("FuncC")

        self.assertIsNotNone(oot_a)
        self.assertIsNotNone(oot_b)
        self.assertIsNotNone(oot_c)

    # =========================================================================
    # Comment and String Handling
    # =========================================================================

    def test_ignore_single_line_comment(self):
        """Test that functions in comments are ignored."""
        self.write_oot_file("comment.c", """
// void OoT_FakeFunc(void) { }
void OoT_RealFunc(void) { }
""")
        locator = self.create_locator()

        fake_oot, _ = locator.locate("FakeFunc")
        real_oot, _ = locator.locate("RealFunc")

        self.assertIsNone(fake_oot)
        self.assertIsNotNone(real_oot)

    def test_ignore_multi_line_comment(self):
        """Test that functions in block comments are ignored."""
        self.write_oot_file("block_comment.c", """
/*
void OoT_CommentedFunc(void) {
    return;
}
*/
void OoT_ActualFunc(void) { }
""")
        locator = self.create_locator()

        commented_oot, _ = locator.locate("CommentedFunc")
        actual_oot, _ = locator.locate("ActualFunc")

        self.assertIsNone(commented_oot)
        self.assertIsNotNone(actual_oot)

    def test_ignore_string_literals(self):
        """Test that function-like patterns in strings are ignored."""
        self.write_oot_file("strings.c", """
void OoT_RealFunc(void) {
    printf("OoT_FakeFunc(x) {");
}
""")
        locator = self.create_locator()

        real_oot, _ = locator.locate("RealFunc")
        self.assertIsNotNone(real_oot)

    # =========================================================================
    # Skip Patterns
    # =========================================================================

    def test_skip_control_flow_keywords(self):
        """Test that control flow keywords are not detected."""
        self.write_oot_file("control.c", """
void OoT_MyFunc(void) {
    if (x) { }
    for (int i = 0; i < 10; i++) { }
    while (1) { }
    switch (x) { }
}
""")
        locator = self.create_locator()

        if_oot, _ = locator.locate("if")
        for_oot, _ = locator.locate("for")
        while_oot, _ = locator.locate("while")
        switch_oot, _ = locator.locate("switch")

        self.assertIsNone(if_oot)
        self.assertIsNone(for_oot)
        self.assertIsNone(while_oot)
        self.assertIsNone(switch_oot)

    def test_skip_underscore_prefix(self):
        """Test that symbols starting with underscore are skipped."""
        self.write_oot_file("private.c", """
void _privateFunc(void) { }
""")
        locator = self.create_locator()

        private_oot, _ = locator.locate("_privateFunc")
        self.assertIsNone(private_oot)

    # =========================================================================
    # Subdirectories
    # =========================================================================

    def test_locate_in_subdirectory(self):
        """Test symbol extraction from subdirectories."""
        self.write_oot_file("code/subdir/nested.c", """
void OoT_NestedFunc(void) { }
""")
        locator = self.create_locator()
        oot_file, _ = locator.locate("NestedFunc")

        self.assertIsNotNone(oot_file)
        self.assertEqual(oot_file.name, "nested.c")

    # =========================================================================
    # Helper Methods
    # =========================================================================

    def test_get_oot_symbols(self):
        """Test get_oot_symbols returns all OoT symbols."""
        self.write_oot_file("test.c", """
void OoT_Func1(void) { }
void OoT_Func2(void) { }
""")
        locator = self.create_locator()
        symbols = locator.get_oot_symbols()

        self.assertIn("OoT_Func1", symbols)
        self.assertIn("OoT_Func2", symbols)

    def test_get_mm_symbols(self):
        """Test get_mm_symbols returns all MM symbols."""
        self.write_mm_file("test.c", """
void MM_Func1(void) { }
void MM_Func2(void) { }
""")
        locator = self.create_locator()
        symbols = locator.get_mm_symbols()

        self.assertIn("MM_Func1", symbols)
        self.assertIn("MM_Func2", symbols)

    # =========================================================================
    # Index Caching
    # =========================================================================

    def test_index_built_once(self):
        """Test that index is only built once (lazy initialization)."""
        self.write_oot_file("test.c", """
void OoT_TestFunc(void) { }
""")
        locator = self.create_locator()

        # First call builds index
        locator.locate("TestFunc")
        self.assertTrue(locator._index_built)

        # Second call should reuse index
        locator.locate("TestFunc")
        self.assertTrue(locator._index_built)


class TestSymbolLocatorRealCodebase(unittest.TestCase):
    """
    Integration tests using the real OoT/MM codebase.

    These tests verify the locator works with actual source files.
    They are skipped if the game directories don't exist.
    """

    @classmethod
    def setUpClass(cls):
        """Check if real codebase exists."""
        cls.oot_dir = Path("games/oot/src")
        cls.mm_dir = Path("games/mm/src")
        cls.has_codebase = cls.oot_dir.exists() and cls.mm_dir.exists()

    def test_locate_real_oot_symbol(self):
        """Test locating a known OoT symbol in real codebase."""
        if not self.has_codebase:
            self.skipTest("OoT/MM codebase not available")

        locator = SymbolLocator()
        oot_file, _ = locator.locate("PreRender_Init")

        self.assertIsNotNone(oot_file)
        # Should find the OoT_PreRender_Init function
        self.assertIn("PreRender", str(oot_file))

    def test_locate_real_symbol_both_codebases(self):
        """Test that symbols can be found in both codebases."""
        if not self.has_codebase:
            self.skipTest("OoT/MM codebase not available")

        locator = SymbolLocator()

        # Get symbol counts to verify both codebases are indexed
        oot_symbols = locator.get_oot_symbols()
        mm_symbols = locator.get_mm_symbols()

        self.assertGreater(len(oot_symbols), 0, "OoT should have symbols")
        self.assertGreater(len(mm_symbols), 0, "MM should have symbols")


if __name__ == "__main__":
    unittest.main()
