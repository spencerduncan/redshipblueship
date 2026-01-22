#!/usr/bin/env python3
"""
Detect and compare data tables in C source code.

This module provides utilities for identifying data tables (arrays of values,
structs, lookup tables) in C code and comparing them for similarity.

Features:
- Detects array declarations with initializers
- Handles const and static modifiers
- Supports struct arrays and simple value arrays
- Computes content hashes for comparison
- Provides similarity ratio between tables

Usage:
    from tools.lib.data_table_analyzer import detect_table, compare_tables, TableInfo

    code = 'const s16 sWalkSpeedTable[4] = { 10, 20, 30, 40 };'
    table = detect_table(code)
    if table:
        print(f"Found table: {table.name}, size={table.size}")
"""

import hashlib
import re
from dataclasses import dataclass
from typing import List, Optional


@dataclass
class TableInfo:
    """Information about a detected data table."""
    name: str
    element_type: str
    size: int
    is_const: bool
    values_hash: str  # Hash of actual values for comparison


def _extract_initializer_content(code: str, start_pos: int) -> Optional[str]:
    """
    Extract the content between matching braces starting at start_pos.

    Args:
        code: The source code string
        start_pos: Position of the opening brace '{'

    Returns:
        The content between braces (excluding the braces), or None if not found
    """
    if start_pos >= len(code) or code[start_pos] != '{':
        return None

    depth = 1
    pos = start_pos + 1
    content_start = pos

    while pos < len(code) and depth > 0:
        char = code[pos]

        # Skip string literals
        if char in '"\'':
            quote = char
            pos += 1
            while pos < len(code):
                if code[pos] == '\\' and pos + 1 < len(code):
                    pos += 2
                elif code[pos] == quote:
                    pos += 1
                    break
                else:
                    pos += 1
            continue

        # Skip comments
        if char == '/' and pos + 1 < len(code):
            if code[pos + 1] == '/':
                # Line comment
                while pos < len(code) and code[pos] != '\n':
                    pos += 1
                continue
            elif code[pos + 1] == '*':
                # Block comment
                pos += 2
                while pos < len(code) - 1:
                    if code[pos] == '*' and code[pos + 1] == '/':
                        pos += 2
                        break
                    pos += 1
                continue

        if char == '{':
            depth += 1
        elif char == '}':
            depth -= 1

        pos += 1

    if depth == 0:
        return code[content_start:pos - 1]
    return None


def _normalize_values(content: str) -> str:
    """
    Normalize initializer values for hashing.

    Removes whitespace and comments to create a canonical representation.
    """
    # Remove comments
    result = re.sub(r'//[^\n]*', '', content)
    result = re.sub(r'/\*.*?\*/', '', result, flags=re.DOTALL)
    # Normalize whitespace
    result = re.sub(r'\s+', ' ', result)
    result = result.strip()
    return result


def _compute_values_hash(content: str) -> str:
    """Compute a hash of the normalized initializer values."""
    normalized = _normalize_values(content)
    return hashlib.sha256(normalized.encode('utf-8')).hexdigest()[:16]


def _parse_array_size(size_str: str) -> int:
    """
    Parse array size from bracket content.

    Handles numeric literals and returns -1 for symbolic sizes.
    """
    size_str = size_str.strip()
    if not size_str:
        return -1  # Unsized array

    # Try to parse as integer (decimal or hex)
    try:
        if size_str.startswith('0x') or size_str.startswith('0X'):
            return int(size_str, 16)
        return int(size_str)
    except ValueError:
        return -1  # Symbolic size (e.g., ENTR_MAX)


def detect_table(code: str) -> Optional[TableInfo]:
    """
    Detect if code defines a data table.

    Looks for array declarations with initializers, including:
    - Simple arrays: s16 table[4] = { 1, 2, 3, 4 };
    - Const arrays: const u8 data[10] = { ... };
    - Static arrays: static s32 values[] = { ... };
    - Struct arrays: StructType table[N] = { {...}, {...} };

    Args:
        code: C source code potentially containing a table definition

    Returns:
        TableInfo if a table is detected, None otherwise
    """
    # Pattern to match array declarations with initializers
    # Captures: [const/static modifiers] [type] [name] [size] = { ... }
    pattern = re.compile(
        r'''
        ^\s*                           # Start of line/string, optional whitespace
        (?P<modifiers>                 # Optional modifiers group
            (?:const\s+|static\s+)*    # const and/or static
        )
        (?P<type>                      # Type specification
            (?:struct\s+)?             # Optional struct keyword
            [A-Za-z_][A-Za-z0-9_]*     # Type name
            \s*\*?                     # Optional pointer
        )
        \s+
        (?P<name>[A-Za-z_][A-Za-z0-9_]*)  # Variable name
        \s*
        \[                             # Opening bracket
        (?P<size>[^\]]*?)              # Array size (can be empty, number, or symbol)
        \]                             # Closing bracket
        (?:\s*\[[^\]]*\])*             # Optional additional dimensions
        \s*=\s*                        # Assignment
        (?P<init>\{)                   # Opening brace of initializer
        ''',
        re.VERBOSE | re.MULTILINE
    )

    match = pattern.search(code)
    if not match:
        return None

    modifiers = match.group('modifiers')
    type_str = match.group('type').strip()
    name = match.group('name')
    size_str = match.group('size')
    init_start = match.start('init')

    # Extract initializer content
    init_content = _extract_initializer_content(code, init_start)
    if init_content is None:
        return None

    # Determine if const
    is_const = 'const' in modifiers

    # Parse array size
    size = _parse_array_size(size_str)

    # If size is unspecified, try to count elements
    if size == -1:
        # Count top-level commas plus one (rough element count)
        # This handles simple cases like { 1, 2, 3, 4 }
        depth = 0
        count = 1
        for char in init_content:
            if char == '{':
                depth += 1
            elif char == '}':
                depth -= 1
            elif char == ',' and depth == 0:
                count += 1
        # Only use counted size for empty bracket declarations
        if not size_str:
            size = count

    # Compute hash of values
    values_hash = _compute_values_hash(init_content)

    return TableInfo(
        name=name,
        element_type=type_str,
        size=size,
        is_const=is_const,
        values_hash=values_hash
    )


def compare_tables(table1: TableInfo, table2: TableInfo) -> float:
    """
    Return similarity ratio between two tables (0.0 to 1.0).

    Comparison considers:
    - Identical values (hash match): 1.0
    - Same name and type but different values: 0.5
    - Same type only: 0.25
    - Different type and values: 0.0

    Args:
        table1: First table to compare
        table2: Second table to compare

    Returns:
        Similarity ratio from 0.0 (completely different) to 1.0 (identical)
    """
    # Identical values hash means identical content
    if table1.values_hash == table2.values_hash:
        return 1.0

    # Calculate similarity based on matching attributes
    similarity = 0.0

    # Same name contributes 0.25
    if table1.name == table2.name:
        similarity += 0.25

    # Same element type contributes 0.25
    if table1.element_type == table2.element_type:
        similarity += 0.25

    # Same size contributes 0.25 (if both sizes are known)
    if table1.size > 0 and table2.size > 0:
        if table1.size == table2.size:
            similarity += 0.25
        else:
            # Partial credit for similar sizes
            size_ratio = min(table1.size, table2.size) / max(table1.size, table2.size)
            similarity += 0.25 * size_ratio

    # Same const-ness contributes 0.125
    if table1.is_const == table2.is_const:
        similarity += 0.125

    # Cap at 0.875 since values don't match (would be 1.0 if they did)
    return min(similarity, 0.875)


def detect_tables(code: str) -> List[TableInfo]:
    """
    Detect all data tables in the given code.

    Args:
        code: C source code potentially containing multiple table definitions

    Returns:
        List of TableInfo for all detected tables
    """
    tables = []

    # Split by semicolons to find potential table definitions
    # This is a simple heuristic; more sophisticated parsing could be added
    lines = code.split(';')

    for line in lines:
        # Add back the semicolon for complete statement
        statement = line.strip() + ';'
        table = detect_table(statement)
        if table:
            tables.append(table)

    return tables
