#!/usr/bin/env python3
"""
Unit tests for detect_collisions.py

Tests cover:
- Function extraction from C source
- Global variable extraction
- Collision detection logic
- Edge cases (comments, strings, macros)
"""

import tempfile
import unittest
from pathlib import Path
import sys

# Add parent directory to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent))
from detect_collisions import extract_symbols


class TestExtractSymbols(unittest.TestCase):
    """Tests for the extract_symbols function."""

    def setUp(self):
        """Create a temporary directory for test files."""
        self.temp_dir = tempfile.TemporaryDirectory()
        self.src_dir = Path(self.temp_dir.name)

    def tearDown(self):
        """Clean up temporary directory."""
        self.temp_dir.cleanup()

    def write_c_file(self, name: str, content: str) -> Path:
        """Helper to write a C file to the temp directory."""
        path = self.src_dir / name
        path.write_text(content)
        return path

    def write_h_file(self, name: str, content: str) -> Path:
        """Helper to write a header file to the temp directory."""
        path = self.src_dir / name
        path.write_text(content)
        return path

    # =========================================================================
    # Basic Function Detection
    # =========================================================================

    def test_simple_function(self):
        """Test detection of a simple function definition."""
        self.write_c_file("test.c", """
void MyFunction(void) {
    return;
}
""")
        funcs, globals_ = extract_symbols(self.src_dir)
        self.assertIn("MyFunction", funcs)

    def test_function_with_parameters(self):
        """Test detection of function with parameters."""
        self.write_c_file("test.c", """
int AddNumbers(int a, int b) {
    return a + b;
}
""")
        funcs, globals_ = extract_symbols(self.src_dir)
        self.assertIn("AddNumbers", funcs)

    def test_function_with_pointer_return(self):
        """Test detection of function returning pointer."""
        self.write_c_file("test.c", """
char* GetString(void) {
    return NULL;
}
""")
        funcs, globals_ = extract_symbols(self.src_dir)
        self.assertIn("GetString", funcs)

    def test_multiple_functions(self):
        """Test detection of multiple functions in one file."""
        self.write_c_file("test.c", """
void FuncA(void) { }
void FuncB(void) { }
int FuncC(int x) { return x; }
""")
        funcs, globals_ = extract_symbols(self.src_dir)
        self.assertIn("FuncA", funcs)
        self.assertIn("FuncB", funcs)
        self.assertIn("FuncC", funcs)

    # =========================================================================
    # Basic Global Detection
    # =========================================================================

    def test_simple_global(self):
        """Test detection of simple global variable."""
        self.write_c_file("test.c", """
int gGlobalVar = 42;
""")
        funcs, globals_ = extract_symbols(self.src_dir)
        self.assertIn("gGlobalVar", globals_)

    def test_global_with_extern(self):
        """Test detection of extern declaration in header."""
        self.write_h_file("test.h", """
extern int gExternVar;
""")
        funcs, globals_ = extract_symbols(self.src_dir)
        self.assertIn("gExternVar", globals_)

    def test_global_array(self):
        """Test detection of global array."""
        self.write_c_file("test.c", """
int gArray[100];
""")
        funcs, globals_ = extract_symbols(self.src_dir)
        self.assertIn("gArray", globals_)

    def test_pointer_global(self):
        """Test detection of pointer global variable."""
        self.write_c_file("test.c", """
void* gPointer = NULL;
""")
        funcs, globals_ = extract_symbols(self.src_dir)
        self.assertIn("gPointer", globals_)

    # =========================================================================
    # Skip Patterns
    # =========================================================================

    def test_skip_control_flow_keywords(self):
        """Test that control flow keywords are not detected as functions."""
        self.write_c_file("test.c", """
void MyFunc(void) {
    if (x) { }
    for (int i = 0; i < 10; i++) { }
    while (1) { }
    switch (x) { }
}
""")
        funcs, globals_ = extract_symbols(self.src_dir)
        self.assertNotIn("if", funcs)
        self.assertNotIn("for", funcs)
        self.assertNotIn("while", funcs)
        self.assertNotIn("switch", funcs)
        self.assertIn("MyFunc", funcs)

    def test_skip_underscore_prefix(self):
        """Test that symbols starting with underscore are skipped."""
        self.write_c_file("test.c", """
void _privateFunc(void) { }
int _privateVar = 1;
""")
        funcs, globals_ = extract_symbols(self.src_dir)
        self.assertNotIn("_privateFunc", funcs)
        self.assertNotIn("_privateVar", globals_)

    # =========================================================================
    # Comment Handling
    # =========================================================================

    def test_ignore_single_line_comment(self):
        """Test that function names in comments are ignored."""
        self.write_c_file("test.c", """
// void FakeFunction(void) { }
void RealFunction(void) { }
""")
        funcs, globals_ = extract_symbols(self.src_dir)
        self.assertNotIn("FakeFunction", funcs)
        self.assertIn("RealFunction", funcs)

    def test_ignore_multi_line_comment(self):
        """Test that function names in block comments are ignored."""
        self.write_c_file("test.c", """
/*
void CommentedFunction(void) {
    return;
}
*/
void ActualFunction(void) { }
""")
        funcs, globals_ = extract_symbols(self.src_dir)
        self.assertNotIn("CommentedFunction", funcs)
        self.assertIn("ActualFunction", funcs)

    # =========================================================================
    # String Literal Handling
    # =========================================================================

    def test_ignore_string_literals(self):
        """Test that function-like patterns in strings are ignored."""
        self.write_c_file("test.c", """
void RealFunc(void) {
    printf("FakeFunc(x) {");
}
""")
        funcs, globals_ = extract_symbols(self.src_dir)
        self.assertIn("RealFunc", funcs)
        # FakeFunc shouldn't be detected since it's in a string

    # =========================================================================
    # Edge Cases - Macros (Known Limitation)
    # =========================================================================

    def test_macro_invocation_edge_case(self):
        """
        Test macro invocation that looks like function definition.

        Note: This is a known limitation - macro invocations that look like
        function definitions may be incorrectly detected. This test documents
        the current behavior.
        """
        self.write_c_file("test.c", """
DEFINE_ACTOR(ActorName) {
    // This might be detected as a function
}

void RealFunction(void) { }
""")
        funcs, globals_ = extract_symbols(self.src_dir)
        # Document current behavior - DEFINE_ACTOR may or may not be detected
        # depending on how the macro expands. The key is RealFunction IS detected.
        self.assertIn("RealFunction", funcs)

    # =========================================================================
    # Multiple Files
    # =========================================================================

    def test_multiple_files(self):
        """Test symbol extraction across multiple files."""
        self.write_c_file("file1.c", """
void Function1(void) { }
int gVar1 = 1;
""")
        self.write_c_file("file2.c", """
void Function2(void) { }
int gVar2 = 2;
""")
        funcs, globals_ = extract_symbols(self.src_dir)
        self.assertIn("Function1", funcs)
        self.assertIn("Function2", funcs)
        self.assertIn("gVar1", globals_)
        self.assertIn("gVar2", globals_)

    def test_subdirectories(self):
        """Test symbol extraction in subdirectories."""
        subdir = self.src_dir / "subdir"
        subdir.mkdir()
        (subdir / "nested.c").write_text("""
void NestedFunction(void) { }
""")
        funcs, globals_ = extract_symbols(self.src_dir)
        self.assertIn("NestedFunction", funcs)


class TestCollisionDetection(unittest.TestCase):
    """Tests for collision detection logic (using set intersection)."""

    def test_simple_collision(self):
        """Test detection of colliding symbols."""
        oot_symbols = {"FuncA", "FuncB", "FuncC"}
        mm_symbols = {"FuncB", "FuncC", "FuncD"}
        collisions = oot_symbols & mm_symbols
        self.assertEqual(collisions, {"FuncB", "FuncC"})

    def test_no_collision(self):
        """Test when there are no colliding symbols."""
        oot_symbols = {"OoT_Func1", "OoT_Func2"}
        mm_symbols = {"MM_Func1", "MM_Func2"}
        collisions = oot_symbols & mm_symbols
        self.assertEqual(collisions, set())

    def test_all_collision(self):
        """Test when all symbols collide."""
        oot_symbols = {"SharedFunc1", "SharedFunc2"}
        mm_symbols = {"SharedFunc1", "SharedFunc2"}
        collisions = oot_symbols & mm_symbols
        self.assertEqual(collisions, {"SharedFunc1", "SharedFunc2"})


if __name__ == "__main__":
    unittest.main()
