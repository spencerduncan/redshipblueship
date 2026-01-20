#!/usr/bin/env python3
"""
Extract function and global definitions from C source files.

This module provides utilities for extracting complete function bodies
and global variable definitions from C source files using brace-matching.

Features:
- Handles nested braces correctly
- Ignores braces inside comments (// and /* */)
- Ignores braces inside string literals
- Supports array initializers for globals

Usage:
    from tools.lib.code_extractor import extract_function, extract_global

    func_body = extract_function(Path("file.c"), "myFunction")
    global_def = extract_global(Path("file.c"), "gMyGlobal")
"""

import re
from pathlib import Path
from typing import Optional, Tuple


def _skip_whitespace(content: str, pos: int) -> int:
    """Skip whitespace characters starting from pos."""
    while pos < len(content) and content[pos] in ' \t\n\r':
        pos += 1
    return pos


def _skip_line_comment(content: str, pos: int) -> int:
    """Skip a line comment starting at pos (assumes // already matched)."""
    while pos < len(content) and content[pos] != '\n':
        pos += 1
    return pos


def _skip_block_comment(content: str, pos: int) -> int:
    """Skip a block comment starting at pos (assumes /* already matched)."""
    while pos < len(content) - 1:
        if content[pos] == '*' and content[pos + 1] == '/':
            return pos + 2
        pos += 1
    return len(content)


def _skip_string(content: str, pos: int) -> int:
    """Skip a string literal starting at pos (assumes opening quote matched)."""
    quote_char = content[pos]
    pos += 1
    while pos < len(content):
        if content[pos] == '\\' and pos + 1 < len(content):
            pos += 2  # Skip escaped character
        elif content[pos] == quote_char:
            return pos + 1
        else:
            pos += 1
    return pos


def _find_matching_brace(content: str, start: int) -> int:
    """
    Find the position of the closing brace that matches the opening brace at start.

    Args:
        content: The source code string
        start: Position of the opening brace '{'

    Returns:
        Position of the matching closing brace, or -1 if not found
    """
    if start >= len(content) or content[start] != '{':
        return -1

    depth = 1
    pos = start + 1

    while pos < len(content) and depth > 0:
        char = content[pos]

        # Check for comments
        if char == '/' and pos + 1 < len(content):
            next_char = content[pos + 1]
            if next_char == '/':
                pos = _skip_line_comment(content, pos + 2)
                continue
            elif next_char == '*':
                pos = _skip_block_comment(content, pos + 2)
                continue

        # Check for string/char literals
        if char in '"\'':
            pos = _skip_string(content, pos)
            continue

        # Track braces
        if char == '{':
            depth += 1
        elif char == '}':
            depth -= 1

        pos += 1

    if depth == 0:
        return pos - 1  # Return position of the closing brace
    return -1


def _find_function_start(content: str, open_brace_pos: int) -> int:
    """
    Find the start of a function definition given the position of its opening brace.

    This searches backwards to find the beginning of the return type,
    handling multi-word types like "unsigned long long" and modifiers like "static".

    Args:
        content: The source code string
        open_brace_pos: Position of the function's opening brace

    Returns:
        Position of the start of the function definition
    """
    # First find the closing paren of the parameter list
    paren_pos = open_brace_pos - 1
    while paren_pos >= 0 and content[paren_pos] in ' \t\n\r':
        paren_pos -= 1

    if paren_pos < 0 or content[paren_pos] != ')':
        return -1

    # Find the matching open paren
    depth = 1
    pos = paren_pos - 1
    while pos >= 0 and depth > 0:
        char = content[pos]
        if char == ')':
            depth += 1
        elif char == '(':
            depth -= 1
        pos -= 1

    if depth != 0:
        return -1

    open_paren_pos = pos + 1

    # Now find the start of the function name and return type
    # Go backwards from the open paren, skipping the function name
    pos = open_paren_pos - 1
    while pos >= 0 and content[pos] in ' \t\n\r':
        pos -= 1

    # Skip the function name (identifier)
    while pos >= 0 and (content[pos].isalnum() or content[pos] == '_'):
        pos -= 1

    # Now we need to find the start of the return type
    # Keep going back to capture things like "static inline unsigned long long"
    while pos >= 0:
        # Skip whitespace
        while pos >= 0 and content[pos] in ' \t\n\r':
            pos -= 1

        if pos < 0:
            break

        # Check if we're at the end of an identifier (return type, modifier, etc.)
        if content[pos].isalnum() or content[pos] == '_' or content[pos] == '*':
            # Skip the identifier/pointer
            while pos >= 0 and (content[pos].isalnum() or content[pos] == '_' or content[pos] == '*'):
                pos -= 1
        else:
            # Not part of a type declaration, we've gone too far
            break

    return pos + 1


def _find_symbol_pattern(content: str, symbol: str) -> re.Pattern:
    """Create a regex pattern to find a symbol as a whole word."""
    return re.compile(r'\b' + re.escape(symbol) + r'\b')


def extract_function(file: Path, symbol: str) -> Optional[str]:
    """
    Extract a complete function definition from a C source file.

    Args:
        file: Path to the C source file
        symbol: Name of the function to extract

    Returns:
        The complete function definition including signature and body,
        or None if the function is not found
    """
    try:
        content = file.read_text()
    except Exception:
        return None

    pattern = _find_symbol_pattern(content, symbol)

    # Find all occurrences of the symbol
    for match in pattern.finditer(content):
        symbol_pos = match.start()

        # Look for an opening brace after the symbol (with possible params in between)
        pos = match.end()

        # Skip whitespace
        pos = _skip_whitespace(content, pos)

        # For a function definition, we expect an opening paren for parameters
        if pos >= len(content) or content[pos] != '(':
            continue

        # Find the closing paren
        depth = 1
        pos += 1
        while pos < len(content) and depth > 0:
            if content[pos] == '(':
                depth += 1
            elif content[pos] == ')':
                depth -= 1
            pos += 1

        if depth != 0:
            continue

        # Skip whitespace after closing paren
        pos = _skip_whitespace(content, pos)

        # Check for opening brace (function body)
        if pos >= len(content) or content[pos] != '{':
            continue

        # Found a function definition
        open_brace_pos = pos
        close_brace_pos = _find_matching_brace(content, open_brace_pos)

        if close_brace_pos < 0:
            continue

        # Find the start of the function (return type)
        func_start = _find_function_start(content, open_brace_pos)

        if func_start < 0:
            func_start = symbol_pos

        return content[func_start:close_brace_pos + 1]

    return None


def _find_global_end(content: str, start: int) -> int:
    """
    Find the end of a global variable definition.

    Handles:
    - Simple definitions: int x;
    - Initialized definitions: int x = 5;
    - Array definitions: int x[] = {1, 2, 3};
    - Struct initializers: struct S s = {.a = 1, .b = 2};

    Args:
        content: The source code string
        start: Position to start searching from

    Returns:
        Position after the semicolon that ends the definition, or -1 if not found
    """
    pos = start

    while pos < len(content):
        char = content[pos]

        # Check for comments
        if char == '/' and pos + 1 < len(content):
            next_char = content[pos + 1]
            if next_char == '/':
                pos = _skip_line_comment(content, pos + 2)
                continue
            elif next_char == '*':
                pos = _skip_block_comment(content, pos + 2)
                continue

        # Check for string/char literals
        if char in '"\'':
            pos = _skip_string(content, pos)
            continue

        # Handle brace initializers
        if char == '{':
            close_pos = _find_matching_brace(content, pos)
            if close_pos < 0:
                return -1
            pos = close_pos + 1
            continue

        # End of definition
        if char == ';':
            return pos + 1

        # Start of function body means this isn't a global
        if char == '(' and pos > start:
            # Check if this could be a function definition
            # Look ahead for ) { pattern
            paren_depth = 1
            check_pos = pos + 1
            while check_pos < len(content) and paren_depth > 0:
                if content[check_pos] == '(':
                    paren_depth += 1
                elif content[check_pos] == ')':
                    paren_depth -= 1
                check_pos += 1

            if paren_depth == 0:
                # Skip whitespace
                while check_pos < len(content) and content[check_pos] in ' \t\n\r':
                    check_pos += 1
                if check_pos < len(content) and content[check_pos] == '{':
                    # This is a function, not a global
                    return -1

        pos += 1

    return -1


def _find_matching_open_brace(content: str, close_pos: int) -> int:
    """
    Find the opening brace that matches a closing brace, searching backwards.

    Args:
        content: The source code string
        close_pos: Position of the closing brace '}'

    Returns:
        Position of the matching opening brace, or -1 if not found
    """
    if close_pos < 0 or content[close_pos] != '}':
        return -1

    depth = 1
    pos = close_pos - 1

    while pos >= 0 and depth > 0:
        char = content[pos]

        # Note: We're going backwards, so we can't easily skip comments/strings
        # in the forward direction. For simplicity, just track braces.
        # This works for most struct/union definitions.
        if char == '}':
            depth += 1
        elif char == '{':
            depth -= 1

        pos -= 1

    if depth == 0:
        return pos + 1
    return -1


def _find_global_start(content: str, symbol_pos: int) -> int:
    """
    Find the start of a global variable definition.

    Searches backwards from the symbol to find the beginning of the type declaration.
    Handles anonymous struct/union definitions like: static struct { ... } varname;

    Args:
        content: The source code string
        symbol_pos: Position of the variable name

    Returns:
        Position of the start of the definition
    """
    pos = symbol_pos - 1

    # Skip whitespace before the symbol
    while pos >= 0 and content[pos] in ' \t\n\r':
        pos -= 1

    # Handle pointer declarations (e.g., int *ptr)
    while pos >= 0 and content[pos] == '*':
        pos -= 1
        while pos >= 0 and content[pos] in ' \t':
            pos -= 1

    # Check if we have an anonymous struct/union (closing brace before variable name)
    if pos >= 0 and content[pos] == '}':
        # Find the matching opening brace
        open_pos = _find_matching_open_brace(content, pos)
        if open_pos < 0:
            return symbol_pos
        pos = open_pos - 1

        # Skip whitespace before the opening brace
        while pos >= 0 and content[pos] in ' \t\n\r':
            pos -= 1

    # Now find the start of the type (struct/union/enum keyword and modifiers)
    while pos >= 0:
        # Skip whitespace
        while pos >= 0 and content[pos] in ' \t\n\r':
            pos -= 1

        if pos < 0:
            break

        # Check for type keywords/identifiers
        if content[pos].isalnum() or content[pos] == '_':
            # Found part of the type
            while pos >= 0 and (content[pos].isalnum() or content[pos] == '_'):
                pos -= 1
        else:
            # Not part of type declaration
            break

    return pos + 1


def extract_global(file: Path, symbol: str) -> Optional[str]:
    """
    Extract a global variable definition from a C source file.

    Args:
        file: Path to the C source file
        symbol: Name of the global variable to extract

    Returns:
        The complete variable definition including type and initializer,
        or None if the variable is not found
    """
    try:
        content = file.read_text()
    except Exception:
        return None

    pattern = _find_symbol_pattern(content, symbol)

    # Find all occurrences of the symbol
    for match in pattern.finditer(content):
        symbol_pos = match.start()

        # Check what comes after the symbol
        pos = match.end()
        pos = _skip_whitespace(content, pos)

        if pos >= len(content):
            continue

        next_char = content[pos]

        # For a global variable, we expect:
        # - A semicolon (simple declaration)
        # - An equals sign (initialization)
        # - An opening bracket (array declaration)
        # - A comma (multiple declarations - take just this one)
        if next_char not in ';=[,':
            # Could be a function call or function definition, skip
            continue

        # Find the start of the definition
        def_start = _find_global_start(content, symbol_pos)

        # Verify this is at global scope (not inside a function)
        # A simple heuristic: check that there's no unmatched '{' before this point
        brace_depth = 0
        for i in range(def_start):
            char = content[i]

            # Skip comments
            if char == '/' and i + 1 < len(content):
                if content[i + 1] == '/':
                    # Skip to end of line
                    while i < len(content) and content[i] != '\n':
                        i += 1
                    continue
                elif content[i + 1] == '*':
                    # Skip to */
                    i += 2
                    while i < len(content) - 1:
                        if content[i] == '*' and content[i + 1] == '/':
                            break
                        i += 1
                    continue

            # Skip strings
            if char in '"\'':
                i += 1
                while i < len(content):
                    if content[i] == '\\' and i + 1 < len(content):
                        i += 2
                    elif content[i] == char:
                        break
                    else:
                        i += 1
                continue

            if char == '{':
                brace_depth += 1
            elif char == '}':
                brace_depth -= 1

        if brace_depth != 0:
            # Inside a function, not a global
            continue

        # Find the end of the definition
        def_end = _find_global_end(content, symbol_pos)

        if def_end < 0:
            continue

        return content[def_start:def_end]

    return None
