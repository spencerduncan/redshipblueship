"""Tests for the normalizer module."""

from pathlib import Path

import pytest

from normalizer import (
    normalize,
    strip_comments,
    strip_includes,
    expand_typedefs,
    strip_prefixes,
    normalize_identifiers,
    normalize_whitespace,
)


class TestStripComments:
    """Tests for strip_comments function."""

    def test_removes_single_line_comment(self):
        code = "int x = 1; // this is a comment\nint y = 2;"
        result = strip_comments(code)
        assert "//" not in result
        assert "this is a comment" not in result
        assert "int x = 1;" in result
        assert "int y = 2;" in result

    def test_removes_multi_line_comment(self):
        code = "int x = 1; /* this is\na multi-line\ncomment */ int y = 2;"
        result = strip_comments(code)
        assert "/*" not in result
        assert "*/" not in result
        assert "multi-line" not in result
        assert "int x = 1;" in result
        assert "int y = 2;" in result

    def test_removes_multiple_comments(self):
        code = "// first\nint x; /* second */ int y; // third"
        result = strip_comments(code)
        assert "first" not in result
        assert "second" not in result
        assert "third" not in result

    def test_preserves_code_without_comments(self):
        code = "int x = 1;\nint y = 2;"
        result = strip_comments(code)
        assert result == code

    def test_handles_empty_string(self):
        assert strip_comments("") == ""


class TestStripIncludes:
    """Tests for strip_includes function."""

    def test_removes_angle_bracket_include(self):
        code = '#include <stdio.h>\nint main() {}'
        result = strip_includes(code)
        assert "#include" not in result
        assert "stdio.h" not in result
        assert "int main()" in result

    def test_removes_quote_include(self):
        code = '#include "global.h"\nint x = 1;'
        result = strip_includes(code)
        assert "#include" not in result
        assert "global.h" not in result
        assert "int x = 1;" in result

    def test_removes_multiple_includes(self):
        code = '#include <stdio.h>\n#include "local.h"\nint x;'
        result = strip_includes(code)
        assert result.count("#include") == 0

    def test_preserves_non_include_preprocessor(self):
        code = '#define MAX 100\nint x;'
        result = strip_includes(code)
        assert "#define MAX 100" in result

    def test_handles_empty_string(self):
        assert strip_includes("") == ""


class TestExpandTypedefs:
    """Tests for expand_typedefs function."""

    def test_expands_s64(self):
        assert "long long" in expand_typedefs("s64 x;")
        assert "s64" not in expand_typedefs("s64 x;")

    def test_expands_s32(self):
        result = expand_typedefs("s32 x;")
        assert "int" in result
        assert "s32" not in result

    def test_expands_s16(self):
        result = expand_typedefs("s16 x;")
        assert "short" in result
        assert "s16" not in result

    def test_expands_s8(self):
        result = expand_typedefs("s8 x;")
        assert "signed char" in result
        assert "s8" not in result

    def test_expands_u64(self):
        result = expand_typedefs("u64 x;")
        assert "unsigned long long" in result
        assert "u64" not in result

    def test_expands_u32(self):
        result = expand_typedefs("u32 x;")
        assert "unsigned int" in result
        assert "u32" not in result

    def test_expands_u16(self):
        result = expand_typedefs("u16 x;")
        assert "unsigned short" in result
        assert "u16" not in result

    def test_expands_u8(self):
        result = expand_typedefs("u8 x;")
        assert "unsigned char" in result
        assert "u8" not in result

    def test_expands_f64(self):
        result = expand_typedefs("f64 x;")
        assert "double" in result
        assert "f64" not in result

    def test_expands_f32(self):
        result = expand_typedefs("f32 x;")
        assert "float" in result
        assert "f32" not in result

    def test_does_not_expand_partial_matches(self):
        # Should not replace 'u32' within 'u32_custom_type'
        result = expand_typedefs("u32_custom_type x;")
        assert "u32_custom_type" in result

    def test_handles_multiple_typedefs(self):
        code = "s64 a; u32 b; f32 c;"
        result = expand_typedefs(code)
        assert "long long" in result
        assert "unsigned int" in result
        assert "float" in result

    def test_handles_empty_string(self):
        assert expand_typedefs("") == ""


class TestStripPrefixes:
    """Tests for strip_prefixes function."""

    def test_removes_oot_prefix(self):
        result = strip_prefixes("OoT_Function();")
        assert result == "Function();"
        assert "OoT_" not in result

    def test_removes_mm_prefix(self):
        result = strip_prefixes("MM_Function();")
        assert result == "Function();"
        assert "MM_" not in result

    def test_removes_multiple_prefixes(self):
        code = "OoT_Func1(); MM_Func2();"
        result = strip_prefixes(code)
        assert "OoT_" not in result
        assert "MM_" not in result
        assert "Func1()" in result
        assert "Func2()" in result

    def test_preserves_similar_identifiers(self):
        # Should not remove prefixes that aren't at word boundaries
        code = "MyOoT_Func(); NotMM_Func();"
        result = strip_prefixes(code)
        # These should remain unchanged since OoT_ and MM_ are not at word boundaries
        assert "MyOoT_Func" in result
        assert "NotMM_Func" in result

    def test_handles_empty_string(self):
        assert strip_prefixes("") == ""


class TestNormalizeIdentifiers:
    """Tests for normalize_identifiers function."""

    def test_normalizes_single_letter_param(self):
        result = normalize_identifiers("int func(int x) { return x; }")
        assert "int func(int _) { return _; }" == result

    def test_normalizes_different_single_letters(self):
        # Different single-letter names should all become _
        result_l = normalize_identifiers("return l;")
        result_s = normalize_identifiers("return s;")
        result_u = normalize_identifiers("return u;")
        assert result_l == result_s == result_u == "return _;"

    def test_preserves_multi_letter_identifiers(self):
        code = "int func(int value) { return value; }"
        result = normalize_identifiers(code)
        assert "value" in result

    def test_preserves_underscored_identifiers(self):
        code = "int __d_to_ll(double d);"
        result = normalize_identifiers(code)
        assert "__d_to_ll" in result
        # The standalone 'd' should become '_'
        assert "(double _)" in result

    def test_handles_empty_string(self):
        assert normalize_identifiers("") == ""


class TestNormalizeWhitespace:
    """Tests for normalize_whitespace function."""

    def test_collapses_multiple_spaces(self):
        result = normalize_whitespace("int    x   =   1;")
        assert result == "int x = 1;"

    def test_collapses_newlines(self):
        result = normalize_whitespace("int x;\n\nint y;")
        assert result == "int x; int y;"

    def test_collapses_tabs(self):
        result = normalize_whitespace("int\tx\t=\t1;")
        assert result == "int x = 1;"

    def test_collapses_mixed_whitespace(self):
        result = normalize_whitespace("int \t\n  x;")
        assert result == "int x;"

    def test_trims_leading_whitespace(self):
        result = normalize_whitespace("  int x;")
        assert result == "int x;"

    def test_trims_trailing_whitespace(self):
        result = normalize_whitespace("int x;  ")
        assert result == "int x;"

    def test_handles_empty_string(self):
        assert normalize_whitespace("") == ""

    def test_handles_whitespace_only(self):
        assert normalize_whitespace("   \n\t  ") == ""


class TestNormalize:
    """Tests for the main normalize function."""

    def test_applies_all_normalizations(self):
        code = '''#include "global.h"
// Comment
s64 OoT_Func(u32   x) {
    /* multi
       line */
    return x;
}'''
        result = normalize(code)
        # Check comments removed
        assert "//" not in result
        assert "/*" not in result
        assert "Comment" not in result
        # Check include removed
        assert "#include" not in result
        # Check typedefs expanded
        assert "s64" not in result
        assert "u32" not in result
        assert "long long" in result
        assert "unsigned int" in result
        # Check prefix removed
        assert "OoT_" not in result
        assert "Func" in result
        # Check whitespace normalized
        assert "  " not in result

    def test_handles_empty_string(self):
        assert normalize("") == ""

    def test_normalization_order_is_correct(self):
        # Ensure normalizations happen in the right order
        # For example, comments should be removed before whitespace normalization
        # Single-letter identifiers are normalized to '_'
        code = "int x; // comment\nint y;"
        result = normalize(code)
        assert result == "int _; int _;"


class TestLlcvtNormalization:
    """Integration test: OoT and MM llcvt.c should be identical after normalization."""

    @pytest.fixture
    def project_root(self):
        """Get the project root directory."""
        return Path(__file__).parent.parent.parent

    @pytest.fixture
    def oot_llcvt(self, project_root):
        """Load OoT's llcvt.c content."""
        path = project_root / "games" / "oot" / "src" / "libultra" / "libc" / "llcvt.c"
        return path.read_text()

    @pytest.fixture
    def mm_llcvt(self, project_root):
        """Load MM's llcvt.c content."""
        path = project_root / "games" / "mm" / "src" / "libultra" / "libc" / "llcvt.c"
        return path.read_text()

    def test_llcvt_files_exist(self, oot_llcvt, mm_llcvt):
        """Verify both llcvt.c files can be loaded."""
        assert len(oot_llcvt) > 0
        assert len(mm_llcvt) > 0

    def test_llcvt_files_differ_before_normalization(self, oot_llcvt, mm_llcvt):
        """Verify the files are different before normalization."""
        assert oot_llcvt != mm_llcvt

    def test_llcvt_files_identical_after_normalization(self, oot_llcvt, mm_llcvt):
        """Key test: OoT and MM llcvt.c should be identical after normalization."""
        oot_normalized = normalize(oot_llcvt)
        mm_normalized = normalize(mm_llcvt)
        assert oot_normalized == mm_normalized, (
            f"Normalized files differ:\n"
            f"OoT: {oot_normalized[:200]}...\n"
            f"MM:  {mm_normalized[:200]}..."
        )
