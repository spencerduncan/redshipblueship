"""
Normalize C code for comparison by removing superficial differences.

This module provides functions to normalize C source code, making it easier
to compare code from different sources (e.g., OoT vs MM) by removing
differences that don't affect functionality.
"""

import re


def strip_comments(code: str) -> str:
    """Remove C-style comments (// and /* */) from code."""
    # Remove single-line comments
    code = re.sub(r'//[^\n]*', '', code)
    # Remove multi-line comments (non-greedy match)
    code = re.sub(r'/\*.*?\*/', '', code, flags=re.DOTALL)
    return code


def strip_includes(code: str) -> str:
    """Remove #include lines from code."""
    return re.sub(r'^\s*#include\s+[<"][^>"]*[>"]\s*$', '', code, flags=re.MULTILINE)


def expand_typedefs(code: str) -> str:
    """Expand common typedefs to their standard C equivalents.

    Handles:
    - s64 -> long long
    - s32 -> int
    - s16 -> short
    - s8 -> signed char
    - u64 -> unsigned long long
    - u32 -> unsigned int
    - u16 -> unsigned short
    - u8 -> unsigned char
    - f64 -> double
    - f32 -> float
    """
    # Order matters: longer types first to avoid partial replacements
    # Use word boundaries to avoid replacing parts of identifiers
    typedefs = [
        (r'\bu64\b', 'unsigned long long'),
        (r'\bu32\b', 'unsigned int'),
        (r'\bu16\b', 'unsigned short'),
        (r'\bu8\b', 'unsigned char'),
        (r'\bs64\b', 'long long'),
        (r'\bs32\b', 'int'),
        (r'\bs16\b', 'short'),
        (r'\bs8\b', 'signed char'),
        (r'\bf64\b', 'double'),
        (r'\bf32\b', 'float'),
    ]

    for pattern, replacement in typedefs:
        code = re.sub(pattern, replacement, code)

    return code


def strip_prefixes(code: str) -> str:
    """Remove OoT_ and MM_ prefixes from identifiers."""
    # Match OoT_ or MM_ followed by word characters, replace with just the word characters
    code = re.sub(r'\bOoT_(\w+)', r'\1', code)
    code = re.sub(r'\bMM_(\w+)', r'\1', code)
    return code


def normalize_identifiers(code: str) -> str:
    """Normalize single-letter identifiers to canonical form.

    This replaces single-letter variable/parameter names with a placeholder '_'
    to allow comparison of code with different naming conventions.
    For example, both 'return l;' and 'return s;' become 'return _;'.
    """
    # Replace single-letter identifiers that are standalone words
    # This handles parameter names like (int l), (int s), (int u)
    # and their usage like 'return l;' or 'return s;'
    # Use word boundaries to match standalone single letters
    code = re.sub(r'\b([a-z])\b', '_', code)
    return code


def normalize_whitespace(code: str) -> str:
    """Collapse all whitespace to single spaces and trim lines."""
    # Replace all whitespace sequences (including newlines) with single space
    code = re.sub(r'\s+', ' ', code)
    # Trim leading and trailing whitespace
    return code.strip()


def normalize(code: str) -> str:
    """Apply all normalizations to C code.

    This applies the following normalizations in order:
    1. Strip comments
    2. Strip #include directives
    3. Expand typedefs to standard C types
    4. Strip OoT_/MM_ prefixes
    5. Normalize single-letter identifiers
    6. Normalize whitespace

    Args:
        code: The C source code to normalize.

    Returns:
        The normalized code string.
    """
    code = strip_comments(code)
    code = strip_includes(code)
    code = expand_typedefs(code)
    code = strip_prefixes(code)
    code = normalize_identifiers(code)
    code = normalize_whitespace(code)
    return code
