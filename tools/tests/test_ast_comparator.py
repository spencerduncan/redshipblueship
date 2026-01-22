#!/usr/bin/env python3
"""
Unit tests for ast_comparator.py

Tests cover:
- AST parsing of C code
- Identifier normalization (OoT_/MM_ prefix stripping)
- AST comparison of identical functions
- AST comparison of different functions
- Handling of parse errors
- Availability checking
"""

import sys
import unittest
from pathlib import Path

# Add parent directory for direct imports
sys.path.insert(0, str(Path(__file__).parent.parent))
sys.path.insert(0, str(Path(__file__).parent.parent / "lib"))

from lib.ast_comparator import (
    ASTComparisonResult,
    compare_ast,
    is_available,
    parse_code,
    _normalize_identifier,
)


class TestNormalizeIdentifier(unittest.TestCase):
    """Tests for the _normalize_identifier function."""

    def test_oot_prefix(self):
        """Test that OoT_ prefix is removed."""
        self.assertEqual(_normalize_identifier("OoT_TestFunc"), "TestFunc")
        self.assertEqual(_normalize_identifier("OoT_Actor_Init"), "Actor_Init")

    def test_mm_prefix(self):
        """Test that MM_ prefix is removed."""
        self.assertEqual(_normalize_identifier("MM_TestFunc"), "TestFunc")
        self.assertEqual(_normalize_identifier("MM_Actor_Init"), "Actor_Init")

    def test_no_prefix(self):
        """Test that identifiers without prefix are unchanged."""
        self.assertEqual(_normalize_identifier("TestFunc"), "TestFunc")
        self.assertEqual(_normalize_identifier("myVariable"), "myVariable")

    def test_partial_prefix(self):
        """Test that partial prefixes are not removed."""
        self.assertEqual(_normalize_identifier("OoT"), "OoT")
        self.assertEqual(_normalize_identifier("MM"), "MM")
        self.assertEqual(_normalize_identifier("OoThing"), "OoThing")


class TestIsAvailable(unittest.TestCase):
    """Tests for the is_available function."""

    def test_availability_check(self):
        """Test that is_available returns a boolean."""
        result = is_available()
        self.assertIsInstance(result, bool)


@unittest.skipUnless(is_available(), "tree-sitter not available")
class TestParseCode(unittest.TestCase):
    """Tests for the parse_code function."""

    def test_parse_simple_function(self):
        """Test parsing a simple C function."""
        code = "void test(void) { return; }"
        root, valid, error = parse_code(code)
        self.assertTrue(valid)
        self.assertIsNotNone(root)
        self.assertIsNone(error)

    def test_parse_function_with_body(self):
        """Test parsing a function with a body."""
        code = """
        int add(int a, int b) {
            return a + b;
        }
        """
        root, valid, error = parse_code(code)
        self.assertTrue(valid)
        self.assertIsNotNone(root)

    def test_parse_with_types(self):
        """Test parsing code with various types."""
        code = """
        u32 OoT_GetValue(void) {
            u32 x = 42;
            return x;
        }
        """
        root, valid, error = parse_code(code)
        self.assertTrue(valid)
        self.assertIsNotNone(root)


@unittest.skipUnless(is_available(), "tree-sitter not available")
class TestCompareAST(unittest.TestCase):
    """Tests for the compare_ast function."""

    def test_identical_functions(self):
        """Test that identical functions are detected as identical."""
        code1 = "void test(void) { return; }"
        code2 = "void test(void) { return; }"
        result = compare_ast(code1, code2)

        self.assertIsInstance(result, ASTComparisonResult)
        self.assertTrue(result.is_identical)
        self.assertTrue(result.oot_ast_valid)
        self.assertTrue(result.mm_ast_valid)
        self.assertEqual(result.differences, [])

    def test_identical_with_different_prefixes(self):
        """Test that functions differing only in OoT_/MM_ prefix are identical."""
        oot_code = """
        void OoT_TestFunc(int x) {
            return x + 1;
        }
        """
        mm_code = """
        void MM_TestFunc(int x) {
            return x + 1;
        }
        """
        result = compare_ast(oot_code, mm_code)

        self.assertTrue(result.is_identical)
        self.assertEqual(result.differences, [])

    def test_identical_with_calls_to_prefixed_functions(self):
        """Test that functions calling prefixed versions are identical."""
        oot_code = """
        void OoT_Caller(void) {
            OoT_Helper();
            OoT_Process(42);
        }
        """
        mm_code = """
        void MM_Caller(void) {
            MM_Helper();
            MM_Process(42);
        }
        """
        result = compare_ast(oot_code, mm_code)

        self.assertTrue(result.is_identical)

    def test_different_function_bodies(self):
        """Test that functions with different bodies are detected."""
        oot_code = """
        int OoT_Calculate(int x) {
            return x * 2;
        }
        """
        mm_code = """
        int MM_Calculate(int x) {
            return x + x + x;
        }
        """
        result = compare_ast(oot_code, mm_code)

        self.assertFalse(result.is_identical)
        self.assertGreater(len(result.differences), 0)

    def test_different_parameter_count(self):
        """Test that functions with different parameter counts are detected."""
        oot_code = """
        void OoT_Func(int a) {
            return;
        }
        """
        mm_code = """
        void MM_Func(int a, int b) {
            return;
        }
        """
        result = compare_ast(oot_code, mm_code)

        self.assertFalse(result.is_identical)

    def test_different_return_types(self):
        """Test that functions with different return types are detected."""
        oot_code = """
        int OoT_Func(void) {
            return 0;
        }
        """
        mm_code = """
        void MM_Func(void) {
            return;
        }
        """
        result = compare_ast(oot_code, mm_code)

        self.assertFalse(result.is_identical)

    def test_whitespace_and_formatting_ignored(self):
        """Test that whitespace differences don't affect comparison."""
        oot_code = "void OoT_Test(void){return;}"
        mm_code = """
        void MM_Test(void) {
            return;
        }
        """
        result = compare_ast(oot_code, mm_code)

        self.assertTrue(result.is_identical)

    def test_comments_ignored(self):
        """Test that comment differences don't affect comparison."""
        oot_code = """
        // OoT comment
        void OoT_Test(void) {
            /* Another OoT comment */
            return;
        }
        """
        mm_code = """
        /* MM comment */
        void MM_Test(void) {
            // Different MM comment
            return;
        }
        """
        result = compare_ast(oot_code, mm_code)

        self.assertTrue(result.is_identical)

    def test_complex_function_identical(self):
        """Test comparison of more complex functions."""
        oot_code = """
        int OoT_ProcessData(int* data, int size) {
            int sum = 0;
            for (int i = 0; i < size; i++) {
                sum += data[i];
            }
            return sum;
        }
        """
        mm_code = """
        int MM_ProcessData(int* data, int size) {
            int sum = 0;
            for (int i = 0; i < size; i++) {
                sum += data[i];
            }
            return sum;
        }
        """
        result = compare_ast(oot_code, mm_code)

        self.assertTrue(result.is_identical)

    def test_different_variable_names_not_normalized(self):
        """Test that different local variable names are detected as different."""
        oot_code = """
        int OoT_Func(void) {
            int result = 42;
            return result;
        }
        """
        mm_code = """
        int MM_Func(void) {
            int value = 42;
            return value;
        }
        """
        result = compare_ast(oot_code, mm_code)

        # Different variable names should result in different ASTs
        # (we only normalize OoT_/MM_ prefixes)
        self.assertFalse(result.is_identical)


class TestCompareASTWithoutTreeSitter(unittest.TestCase):
    """Tests for compare_ast when tree-sitter is not available."""

    def test_unavailable_returns_error(self):
        """Test that comparison returns error when tree-sitter unavailable."""
        # This test checks the error handling path
        # We can't easily simulate unavailability, but we can verify
        # the result structure is correct
        result = compare_ast("void test(void) {}", "void test(void) {}")

        self.assertIsInstance(result, ASTComparisonResult)
        # Result should either succeed or have an error message
        if not result.oot_ast_valid or not result.mm_ast_valid:
            self.assertIsNotNone(result.error)


@unittest.skipUnless(is_available(), "tree-sitter not available")
class TestEdgeCases(unittest.TestCase):
    """Tests for edge cases in AST comparison."""

    def test_empty_code(self):
        """Test handling of empty code."""
        result = compare_ast("", "")
        # Empty code should still parse (as an empty translation unit)
        self.assertTrue(result.oot_ast_valid)
        self.assertTrue(result.mm_ast_valid)

    def test_multiple_functions(self):
        """Test that first function is compared when multiple exist."""
        oot_code = """
        void OoT_First(void) { return; }
        void OoT_Second(void) { return; }
        """
        mm_code = """
        void MM_First(void) { return; }
        void MM_Second(void) { return; }
        """
        result = compare_ast(oot_code, mm_code)

        # Should compare the entire translation unit
        self.assertTrue(result.is_identical)

    def test_nested_calls_with_prefixes(self):
        """Test nested function calls with prefixes."""
        oot_code = """
        int OoT_Outer(void) {
            return OoT_Inner(OoT_Helper(42));
        }
        """
        mm_code = """
        int MM_Outer(void) {
            return MM_Inner(MM_Helper(42));
        }
        """
        result = compare_ast(oot_code, mm_code)

        self.assertTrue(result.is_identical)

    def test_global_variable_reference(self):
        """Test functions referencing global variables with prefixes."""
        oot_code = """
        void OoT_UseGlobal(void) {
            OoT_gGlobalVar = 42;
        }
        """
        mm_code = """
        void MM_UseGlobal(void) {
            MM_gGlobalVar = 42;
        }
        """
        result = compare_ast(oot_code, mm_code)

        self.assertTrue(result.is_identical)


if __name__ == "__main__":
    unittest.main()
