#!/usr/bin/env python3
"""
Unit tests for code_extractor.py

Tests cover:
- Function extraction with brace matching
- Global variable extraction
- Edge cases: comments, strings, nested braces
- Real C file extraction
"""

import tempfile
import unittest
from pathlib import Path

# Path setup handled by conftest.py (auto-loaded by pytest)
from code_extractor import extract_function, extract_global


class TestExtractFunction(unittest.TestCase):
    """Tests for the extract_function function."""

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
    # Basic Function Extraction
    # =========================================================================

    def test_simple_function(self):
        """Test extracting a simple function."""
        path = self.write_file("test.c", """
int add(int a, int b) {
    return a + b;
}
""")
        result = extract_function(path, "add")
        self.assertIsNotNone(result)
        self.assertIn("int add(int a, int b)", result)
        self.assertIn("return a + b;", result)
        self.assertTrue(result.strip().endswith("}"))

    def test_function_with_void_return(self):
        """Test extracting a function with void return type."""
        path = self.write_file("test.c", """
void doNothing(void) {
    // nothing
}
""")
        result = extract_function(path, "doNothing")
        self.assertIsNotNone(result)
        self.assertIn("void doNothing(void)", result)

    def test_function_with_static(self):
        """Test extracting a static function."""
        path = self.write_file("test.c", """
static int helper(int x) {
    return x * 2;
}
""")
        result = extract_function(path, "helper")
        self.assertIsNotNone(result)
        self.assertIn("static int helper(int x)", result)

    def test_function_with_inline(self):
        """Test extracting an inline function."""
        path = self.write_file("test.c", """
static inline int16_t clamp16(int32_t v) {
    if (v < -0x8000) {
        return -0x8000;
    }
    return (int16_t)v;
}
""")
        result = extract_function(path, "clamp16")
        self.assertIsNotNone(result)
        self.assertIn("static inline int16_t clamp16", result)

    def test_function_with_long_return_type(self):
        """Test extracting a function with multi-word return type."""
        path = self.write_file("test.c", """
unsigned long long computeValue(int x) {
    return x * 1000ULL;
}
""")
        result = extract_function(path, "computeValue")
        self.assertIsNotNone(result)
        self.assertIn("unsigned long long computeValue", result)

    def test_function_with_pointer_return(self):
        """Test extracting a function returning a pointer."""
        path = self.write_file("test.c", """
char* getString(void) {
    return "hello";
}
""")
        result = extract_function(path, "getString")
        self.assertIsNotNone(result)
        self.assertIn("char* getString", result)

    # =========================================================================
    # Nested Braces
    # =========================================================================

    def test_nested_braces(self):
        """Test extracting a function with nested braces."""
        path = self.write_file("test.c", """
int complex(int x) {
    if (x > 0) {
        while (x > 10) {
            x--;
        }
    } else {
        for (int i = 0; i < 10; i++) {
            x++;
        }
    }
    return x;
}
""")
        result = extract_function(path, "complex")
        self.assertIsNotNone(result)
        self.assertIn("if (x > 0)", result)
        self.assertIn("while (x > 10)", result)
        self.assertIn("for (int i = 0; i < 10; i++)", result)
        self.assertIn("return x;", result)
        # Verify we captured the entire function
        self.assertEqual(result.count('{'), result.count('}'))

    def test_deeply_nested_braces(self):
        """Test extracting a function with deeply nested braces."""
        path = self.write_file("test.c", """
void deep(void) {
    {
        {
            {
                int x = 1;
            }
        }
    }
}
""")
        result = extract_function(path, "deep")
        self.assertIsNotNone(result)
        self.assertEqual(result.count('{'), 4)
        self.assertEqual(result.count('}'), 4)

    # =========================================================================
    # Comments
    # =========================================================================

    def test_line_comment_with_brace(self):
        """Test that braces in line comments are ignored."""
        path = self.write_file("test.c", """
int commented(int x) {
    // This brace { should be ignored
    return x;
    // And this one }
}
""")
        result = extract_function(path, "commented")
        self.assertIsNotNone(result)
        self.assertIn("return x;", result)
        # Function should end with the actual closing brace
        self.assertTrue(result.rstrip().endswith("}"))

    def test_block_comment_with_brace(self):
        """Test that braces in block comments are ignored."""
        path = self.write_file("test.c", """
int blockCommented(int x) {
    /* This brace { should be ignored
       and this one } too */
    return x;
}
""")
        result = extract_function(path, "blockCommented")
        self.assertIsNotNone(result)
        self.assertIn("return x;", result)
        self.assertTrue(result.rstrip().endswith("}"))

    # =========================================================================
    # String Literals
    # =========================================================================

    def test_brace_in_string(self):
        """Test that braces in strings are ignored."""
        path = self.write_file("test.c", """
void printBraces(void) {
    printf("{");
    printf("}");
}
""")
        result = extract_function(path, "printBraces")
        self.assertIsNotNone(result)
        self.assertIn('printf("{");', result)
        self.assertIn('printf("}");', result)
        self.assertEqual(result.count('{'), result.count('}'))

    def test_escaped_quote_in_string(self):
        """Test handling of escaped quotes in strings."""
        path = self.write_file("test.c", """
void escapedQuote(void) {
    char* s = "He said \\"Hello\\"";
    printf("%s", s);
}
""")
        result = extract_function(path, "escapedQuote")
        self.assertIsNotNone(result)
        self.assertIn('He said \\"Hello\\"', result)

    # =========================================================================
    # Multiple Functions
    # =========================================================================

    def test_extract_specific_function(self):
        """Test extracting a specific function from a file with multiple functions."""
        path = self.write_file("test.c", """
int funcA(void) {
    return 1;
}

int funcB(void) {
    return 2;
}

int funcC(void) {
    return 3;
}
""")
        result = extract_function(path, "funcB")
        self.assertIsNotNone(result)
        self.assertIn("funcB", result)
        self.assertIn("return 2;", result)
        self.assertNotIn("funcA", result)
        self.assertNotIn("funcC", result)

    # =========================================================================
    # Edge Cases
    # =========================================================================

    def test_function_not_found(self):
        """Test that None is returned when function is not found."""
        path = self.write_file("test.c", """
int existingFunc(void) {
    return 0;
}
""")
        result = extract_function(path, "nonExistentFunc")
        self.assertIsNone(result)

    def test_function_declaration_not_definition(self):
        """Test that function declarations (prototypes) are not extracted."""
        path = self.write_file("test.c", """
int myFunc(int x);

int otherFunc(void) {
    return myFunc(5);
}
""")
        result = extract_function(path, "myFunc")
        # Should not find a definition, only declaration
        self.assertIsNone(result)

    def test_file_not_found(self):
        """Test that None is returned when file doesn't exist."""
        result = extract_function(Path("/nonexistent/path.c"), "func")
        self.assertIsNone(result)


class TestExtractGlobal(unittest.TestCase):
    """Tests for the extract_global function."""

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
    # Basic Global Extraction
    # =========================================================================

    def test_simple_global(self):
        """Test extracting a simple global variable."""
        path = self.write_file("test.c", """
int gCounter;
""")
        result = extract_global(path, "gCounter")
        self.assertIsNotNone(result)
        self.assertIn("int gCounter;", result)

    def test_initialized_global(self):
        """Test extracting an initialized global variable."""
        path = self.write_file("test.c", """
int gValue = 42;
""")
        result = extract_global(path, "gValue")
        self.assertIsNotNone(result)
        self.assertIn("int gValue = 42;", result)

    def test_static_global(self):
        """Test extracting a static global variable."""
        path = self.write_file("test.c", """
static int sPrivate = 100;
""")
        result = extract_global(path, "sPrivate")
        self.assertIsNotNone(result)
        self.assertIn("static int sPrivate = 100;", result)

    def test_const_global(self):
        """Test extracting a const global variable."""
        path = self.write_file("test.c", """
const int kConstant = 123;
""")
        result = extract_global(path, "kConstant")
        self.assertIsNotNone(result)
        self.assertIn("const int kConstant = 123;", result)

    def test_pointer_global(self):
        """Test extracting a pointer global variable."""
        path = self.write_file("test.c", """
char* gString = "hello";
""")
        result = extract_global(path, "gString")
        self.assertIsNotNone(result)
        self.assertIn("char* gString", result)

    # =========================================================================
    # Array Globals
    # =========================================================================

    def test_simple_array_global(self):
        """Test extracting an array global variable."""
        path = self.write_file("test.c", """
int gArray[10];
""")
        result = extract_global(path, "gArray")
        self.assertIsNotNone(result)
        self.assertIn("int gArray[10];", result)

    def test_initialized_array_global(self):
        """Test extracting an initialized array global."""
        path = self.write_file("test.c", """
int gNumbers[] = {1, 2, 3, 4, 5};
""")
        result = extract_global(path, "gNumbers")
        self.assertIsNotNone(result)
        self.assertIn("{1, 2, 3, 4, 5}", result)

    def test_nested_array_initializer(self):
        """Test extracting an array with nested braces."""
        path = self.write_file("test.c", """
int gMatrix[2][3] = {
    {1, 2, 3},
    {4, 5, 6}
};
""")
        result = extract_global(path, "gMatrix")
        self.assertIsNotNone(result)
        self.assertIn("{1, 2, 3}", result)
        self.assertIn("{4, 5, 6}", result)

    def test_large_array_initializer(self):
        """Test extracting an array with many elements."""
        path = self.write_file("test.c", """
static int16_t resample_table[64][4] = {
    { 0x0c39, 0x66ad, 0x0d46, 0xffdf },
    { 0x0b39, 0x6696, 0x0e5f, 0xffd8 },
    { 0x0a44, 0x6669, 0x0f83, 0xffd0 }
};
""")
        result = extract_global(path, "resample_table")
        self.assertIsNotNone(result)
        self.assertIn("static int16_t resample_table[64][4]", result)
        self.assertIn("0x0c39", result)

    # =========================================================================
    # Struct Globals
    # =========================================================================

    def test_struct_global(self):
        """Test extracting a struct global variable."""
        path = self.write_file("test.c", """
static struct {
    uint16_t in;
    uint16_t out;
} rspa;
""")
        result = extract_global(path, "rspa")
        self.assertIsNotNone(result)
        self.assertIn("static struct", result)
        self.assertIn("uint16_t in;", result)

    def test_struct_with_initializer(self):
        """Test extracting a struct global with initializer."""
        path = self.write_file("test.c", """
struct Point gOrigin = {0, 0};
""")
        result = extract_global(path, "gOrigin")
        self.assertIsNotNone(result)
        self.assertIn("struct Point gOrigin = {0, 0};", result)

    # =========================================================================
    # Edge Cases
    # =========================================================================

    def test_global_not_found(self):
        """Test that None is returned when global is not found."""
        path = self.write_file("test.c", """
int gExisting = 1;
""")
        result = extract_global(path, "gNonExistent")
        self.assertIsNone(result)

    def test_local_variable_not_extracted(self):
        """Test that local variables inside functions are not extracted."""
        path = self.write_file("test.c", """
void someFunc(void) {
    int localVar = 5;
}
""")
        result = extract_global(path, "localVar")
        self.assertIsNone(result)

    def test_file_not_found(self):
        """Test that None is returned when file doesn't exist."""
        result = extract_global(Path("/nonexistent/path.c"), "var")
        self.assertIsNone(result)

    def test_multiple_globals(self):
        """Test extracting a specific global from file with multiple globals."""
        path = self.write_file("test.c", """
int gFirst = 1;
int gSecond = 2;
int gThird = 3;
""")
        result = extract_global(path, "gSecond")
        self.assertIsNotNone(result)
        self.assertIn("int gSecond = 2;", result)


class TestRealFiles(unittest.TestCase):
    """Tests using real C files from the codebase."""

    def test_extract_function_from_llcvt(self):
        """Test extracting a function from the real llcvt.c file."""
        llcvt_path = Path("games/mm/src/libultra/libc/llcvt.c")
        if not llcvt_path.exists():
            self.skipTest("llcvt.c not found")

        result = extract_function(llcvt_path, "__d_to_ll")
        self.assertIsNotNone(result)
        self.assertIn("long long __d_to_ll(double d)", result)
        self.assertIn("return d;", result)

    def test_extract_multiple_functions_from_llcvt(self):
        """Test extracting multiple functions from llcvt.c."""
        llcvt_path = Path("games/mm/src/libultra/libc/llcvt.c")
        if not llcvt_path.exists():
            self.skipTest("llcvt.c not found")

        funcs = ["__d_to_ll", "__f_to_ll", "__ll_to_d", "__ull_to_f"]
        for func_name in funcs:
            result = extract_function(llcvt_path, func_name)
            self.assertIsNotNone(result, f"Failed to extract {func_name}")
            self.assertIn(func_name, result)


if __name__ == "__main__":
    unittest.main()
