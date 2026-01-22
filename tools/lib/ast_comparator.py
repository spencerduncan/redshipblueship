#!/usr/bin/env python3
"""
AST-based comparison of C code using tree-sitter.

This module provides functions to parse C code into ASTs, normalize them
by stripping game-specific prefixes (OoT_/MM_), and compare them for
semantic equivalence.

Usage:
    from ast_comparator import compare_ast, ASTComparisonResult

    result = compare_ast(oot_code, mm_code)
    if result.is_identical:
        print("Functions are semantically identical")
"""

from dataclasses import dataclass
from typing import Any, List, Optional, Tuple

try:
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser, Node

    TREE_SITTER_AVAILABLE = True
except ImportError:
    TREE_SITTER_AVAILABLE = False
    Node = Any  # Type hint fallback


@dataclass
class ASTComparisonResult:
    """Result of an AST comparison between two code snippets."""

    is_identical: bool
    """Whether the ASTs are semantically identical after normalization."""

    oot_ast_valid: bool
    """Whether the OoT code was successfully parsed."""

    mm_ast_valid: bool
    """Whether the MM code was successfully parsed."""

    differences: List[str]
    """List of differences found (if any)."""

    error: Optional[str] = None
    """Error message if comparison failed."""


def is_available() -> bool:
    """Check if tree-sitter is available for AST comparison."""
    return TREE_SITTER_AVAILABLE


def _create_parser() -> "Parser":
    """Create a tree-sitter parser for C code."""
    if not TREE_SITTER_AVAILABLE:
        raise RuntimeError("tree-sitter is not available")

    parser = Parser(Language(tsc.language()))
    return parser


def _normalize_identifier(name: str) -> str:
    """
    Normalize an identifier by removing OoT_/MM_ prefixes.

    Args:
        name: The identifier name

    Returns:
        The normalized identifier name
    """
    if name.startswith("OoT_"):
        return name[4:]
    elif name.startswith("MM_"):
        return name[3:]
    return name


def _node_to_normalized_tuple(node: "Node", source: bytes) -> Tuple:
    """
    Convert a tree-sitter node to a normalized tuple representation.

    This recursively converts the AST to a tuple structure where identifiers
    are normalized to remove game-specific prefixes.

    Args:
        node: The tree-sitter node
        source: The source code bytes

    Returns:
        A tuple representation of the node and its children
    """
    node_type = node.type

    # For identifier nodes, normalize the text
    if node_type == "identifier":
        text = source[node.start_byte : node.end_byte].decode("utf-8")
        normalized = _normalize_identifier(text)
        return ("identifier", normalized)

    # For leaf nodes (no children), include the text
    if node.child_count == 0:
        text = source[node.start_byte : node.end_byte].decode("utf-8")
        return (node_type, text)

    # For non-leaf nodes, recursively process children
    # Skip comment nodes as they don't affect semantics
    children = []
    for child in node.children:
        if child.type in ("comment", "preproc_include"):
            continue
        children.append(_node_to_normalized_tuple(child, source))

    return (node_type, tuple(children))


def _extract_function_body(root: "Node", source: bytes) -> Optional["Node"]:
    """
    Extract the function definition node from the AST root.

    Args:
        root: The root node of the parsed AST
        source: The source code bytes

    Returns:
        The function_definition node, or None if not found
    """

    def find_function(node: "Node") -> Optional["Node"]:
        if node.type == "function_definition":
            return node
        for child in node.children:
            result = find_function(child)
            if result is not None:
                return result
        return None

    return find_function(root)


def parse_code(code: str) -> Tuple[Optional["Node"], bool, Optional[str]]:
    """
    Parse C code and return the AST root.

    Args:
        code: The C source code to parse

    Returns:
        Tuple of (root_node, success, error_message)
    """
    if not TREE_SITTER_AVAILABLE:
        return None, False, "tree-sitter is not available"

    try:
        parser = _create_parser()
        tree = parser.parse(bytes(code, "utf-8"))
        root = tree.root_node

        # Check for parse errors
        if root.has_error:
            return root, True, "Parse completed with errors"

        return root, True, None
    except Exception as e:
        return None, False, str(e)


def compare_ast(oot_code: str, mm_code: str) -> ASTComparisonResult:
    """
    Compare two C code snippets using AST comparison.

    This parses both code snippets, normalizes them by removing OoT_/MM_
    prefixes from identifiers, and compares the resulting AST structures.

    Args:
        oot_code: The OoT version of the code
        mm_code: The MM version of the code

    Returns:
        ASTComparisonResult with comparison details
    """
    if not TREE_SITTER_AVAILABLE:
        return ASTComparisonResult(
            is_identical=False,
            oot_ast_valid=False,
            mm_ast_valid=False,
            differences=[],
            error="tree-sitter is not available. Install with: pip install tree-sitter tree-sitter-c",
        )

    # Parse both code snippets
    oot_root, oot_valid, oot_error = parse_code(oot_code)
    mm_root, mm_valid, mm_error = parse_code(mm_code)

    if not oot_valid or not mm_valid:
        errors = []
        if not oot_valid:
            errors.append(f"OoT parse error: {oot_error}")
        if not mm_valid:
            errors.append(f"MM parse error: {mm_error}")
        return ASTComparisonResult(
            is_identical=False,
            oot_ast_valid=oot_valid,
            mm_ast_valid=mm_valid,
            differences=errors,
            error="; ".join(errors),
        )

    # Convert to normalized tuple representation
    oot_bytes = bytes(oot_code, "utf-8")
    mm_bytes = bytes(mm_code, "utf-8")

    # Find function definitions in both
    oot_func = _extract_function_body(oot_root, oot_bytes)
    mm_func = _extract_function_body(mm_root, mm_bytes)

    # If we found function definitions, compare those; otherwise compare entire AST
    if oot_func is not None and mm_func is not None:
        oot_normalized = _node_to_normalized_tuple(oot_func, oot_bytes)
        mm_normalized = _node_to_normalized_tuple(mm_func, mm_bytes)
    else:
        oot_normalized = _node_to_normalized_tuple(oot_root, oot_bytes)
        mm_normalized = _node_to_normalized_tuple(mm_root, mm_bytes)

    # Compare the normalized ASTs
    is_identical = oot_normalized == mm_normalized

    differences = []
    if not is_identical:
        differences = _find_differences(oot_normalized, mm_normalized)

    return ASTComparisonResult(
        is_identical=is_identical,
        oot_ast_valid=True,
        mm_ast_valid=True,
        differences=differences,
    )


def _find_differences(
    oot_tuple: Tuple, mm_tuple: Tuple, path: str = "root"
) -> List[str]:
    """
    Find differences between two normalized AST tuples.

    Args:
        oot_tuple: The OoT normalized AST tuple
        mm_tuple: The MM normalized AST tuple
        path: Current path in the AST (for error messages)

    Returns:
        List of difference descriptions
    """
    differences = []

    if not isinstance(oot_tuple, tuple) or not isinstance(mm_tuple, tuple):
        if oot_tuple != mm_tuple:
            differences.append(f"{path}: {oot_tuple!r} != {mm_tuple!r}")
        return differences

    if len(oot_tuple) == 0 and len(mm_tuple) == 0:
        return differences

    # Compare node types
    if len(oot_tuple) > 0 and len(mm_tuple) > 0:
        if oot_tuple[0] != mm_tuple[0]:
            differences.append(f"{path}: node type {oot_tuple[0]} != {mm_tuple[0]}")
            return differences

    # Compare content/children
    if len(oot_tuple) == 2 and len(mm_tuple) == 2:
        node_type = oot_tuple[0]
        oot_content = oot_tuple[1]
        mm_content = mm_tuple[1]

        if isinstance(oot_content, tuple) and isinstance(mm_content, tuple):
            # Both have children
            if len(oot_content) != len(mm_content):
                differences.append(
                    f"{path}/{node_type}: child count {len(oot_content)} != {len(mm_content)}"
                )
            else:
                for i, (oot_child, mm_child) in enumerate(zip(oot_content, mm_content)):
                    child_diffs = _find_differences(
                        oot_child, mm_child, f"{path}/{node_type}[{i}]"
                    )
                    differences.extend(child_diffs)
        elif oot_content != mm_content:
            differences.append(
                f"{path}/{node_type}: {oot_content!r} != {mm_content!r}"
            )

    return differences[:10]  # Limit to first 10 differences


def get_ast_summary(code: str) -> Optional[str]:
    """
    Get a summary representation of the AST for a code snippet.

    Args:
        code: The C source code

    Returns:
        A string summary of the AST structure, or None if parsing failed
    """
    if not TREE_SITTER_AVAILABLE:
        return None

    root, valid, _ = parse_code(code)
    if not valid or root is None:
        return None

    def summarize_node(node: "Node", depth: int = 0) -> List[str]:
        indent = "  " * depth
        lines = [f"{indent}{node.type}"]
        for child in node.children:
            if child.type not in ("comment",):
                lines.extend(summarize_node(child, depth + 1))
        return lines

    return "\n".join(summarize_node(root))
