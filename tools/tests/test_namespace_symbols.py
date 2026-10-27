#!/usr/bin/env python3
"""Locks for tools/namespace_symbols.py (#1195).

The January pass (#53) ran its symbol regex over whole files, so it rewrote
identifiers inside string literals and comments as well as code: the Fast64
limb marker "gEmptyDL" became "OoT_gEmptyDL" (#1190, every custom skeleton mod
crashed), the pixel-width key "P" became "OoT_P", and the OoT gfxprint font's
resource path gained a prefix. These tests run the tool's file entry point on
a throwaway file and require that only CODE changes.
"""

import importlib.util
from pathlib import Path

TOOL = Path(__file__).resolve().parents[1] / "namespace_symbols.py"
_spec = importlib.util.spec_from_file_location("namespace_symbols", TOOL)
namespace_symbols = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(namespace_symbols)

SYMBOLS = {"gEmptyDL", "gPlayState", "P", "sGfxPrintFontData", "Actor_Spawn"}


def _run(tmp_path, source, prefix="OoT", suffix=".cpp"):
    path = tmp_path / ("sample" + suffix)
    path.write_bytes(source.encode("utf-8"))
    namespace_symbols.namespace_file(path, SYMBOLS, prefix)
    return path.read_text(encoding="utf-8")


def test_string_literal_survives_while_the_adjacent_identifier_is_rewritten(tmp_path):
    out = _run(tmp_path, 'if (strcmp(dlName, "gEmptyDL") == 0) { return gEmptyDL; }\n')
    assert out == 'if (strcmp(dlName, "gEmptyDL") == 0) { return OoT_gEmptyDL; }\n'


def test_comments_survive(tmp_path):
    src = (
        "// gEmptyDL is Fast64's marker\n"
        "/* gPlayState may be null here:\n   gPlayState */\n"
        "Gfx* dl = gEmptyDL; // not gEmptyDL's twin\n"
    )
    out = _run(tmp_path, src)
    assert out == (
        "// gEmptyDL is Fast64's marker\n"
        "/* gPlayState may be null here:\n   gPlayState */\n"
        "Gfx* dl = OoT_gEmptyDL; // not gEmptyDL's twin\n"
    )


def test_char_literal_and_single_letter_symbol(tmp_path):
    # `P` is a real colliding symbol; the width table keys "P" and the char 'P'
    # must keep their spelling.
    src = "{ \"P\", 7 }, c == 'P' ? P : 0, label = \"P TIME  MAX\";\n"
    out = _run(tmp_path, src)
    assert out == "{ \"P\", 7 }, c == 'P' ? OoT_P : 0, label = \"P TIME  MAX\";\n"


def test_escaped_quotes_prefixed_and_raw_strings(tmp_path):
    src = (
        'const char* a = "say \\"gPlayState\\" " ; int b = gPlayState;\n'
        'const wchar_t* w = L"gEmptyDL"; const char* u = u8"gEmptyDL";\n'
        'const char* r = R"x(gEmptyDL ")" gPlayState)x"; Actor_Spawn();\n'
    )
    out = _run(tmp_path, src)
    assert out == (
        'const char* a = "say \\"gPlayState\\" " ; int b = OoT_gPlayState;\n'
        'const wchar_t* w = L"gEmptyDL"; const char* u = u8"gEmptyDL";\n'
        'const char* r = R"x(gEmptyDL ")" gPlayState)x"; OoT_Actor_Spawn();\n'
    )


def test_resource_path_literal_keeps_its_spelling(tmp_path):
    src = '#define drGfxPrintFontData "__OTR__textures/font/sGfxPrintFontData"\nu8 sGfxPrintFontData[4];\n'
    out = _run(tmp_path, src, suffix=".c")
    assert out == '#define drGfxPrintFontData "__OTR__textures/font/sGfxPrintFontData"\nu8 OoT_sGfxPrintFontData[4];\n'


def test_rewrite_is_idempotent(tmp_path):
    src = 'Gfx* dl = gEmptyDL; puts("gEmptyDL");\n'
    once = _run(tmp_path, src)
    path = tmp_path / "sample.cpp"
    namespace_symbols.namespace_file(path, SYMBOLS, "OoT")
    assert path.read_text(encoding="utf-8") == once == 'Gfx* dl = OoT_gEmptyDL; puts("gEmptyDL");\n'


def test_digit_separator_is_not_a_char_literal(tmp_path):
    # Read as char literals, the two separators would hide gPlayState between them.
    src = "int n = 1'000 + gPlayState->x + 2'000; char c = 'x'; Actor_Spawn();\n"
    out = _run(tmp_path, src, prefix="MM")
    assert out == "int n = 1'000 + MM_gPlayState->x + 2'000; char c = 'x'; MM_Actor_Spawn();\n"


def test_unterminated_quote_stops_at_the_line_end(tmp_path):
    # An apostrophe in a preprocessor message must not swallow the next lines.
    src = "#error can't build\nGfx* dl = gEmptyDL;\n"
    assert _run(tmp_path, src) == "#error can't build\nGfx* dl = OoT_gEmptyDL;\n"


def test_split_source_round_trips_real_sources():
    repo = TOOL.parents[1]
    for rel in (
        "games/oot/soh/resource/importer/SkeletonLimbFactory.cpp",
        "games/oot/soh/Enhancements/custom-message/CustomMessageManager.cpp",
        "games/oot/src/code/gfxprint.c",
    ):
        text = (repo / rel).read_text(encoding="utf-8")
        segments = namespace_symbols.split_source(text)
        assert "".join(seg for _, seg in segments) == text, rel
        assert any(kind == namespace_symbols.STRING for kind, _ in segments), rel


def test_split_source_kinds():
    # The // comment is spliced onto the next line by a backslash before CRLF;
    # the raw string's prefix stays in the code segment.
    text = 'a /*b*/ "c\\"d" \'e\' // f \\\r\n still f\ng R"z(h)" i)z" j'
    kinds = [(k, s) for k, s in namespace_symbols.split_source(text) if k != namespace_symbols.CODE]
    assert kinds == [
        (namespace_symbols.COMMENT, "/*b*/"),
        (namespace_symbols.STRING, '"c\\"d"'),
        (namespace_symbols.CHAR, "'e'"),
        (namespace_symbols.COMMENT, "// f \\\r\n still f"),
        (namespace_symbols.STRING, '"z(h)" i)z"'),
    ]
    spliced = 'x = "ab\\\r\ncd"; y'
    assert [s for k, s in namespace_symbols.split_source(spliced) if k == namespace_symbols.STRING] == [
        '"ab\\\r\ncd"'
    ]


def test_audit_lists_prefixed_literals_and_skips_comments_by_default():
    ns = namespace_symbols.SymbolNamespacer(SYMBOLS, "OoT")
    text = (
        'if (!strcmp(n, "OoT_gEmptyDL")) {}\n'
        "// OoT_gEmptyDL in a comment\n"
        'puts("OoT_gPlayStateX is not a symbol");\n'
        "char c = 'P'; Gfx* g = OoT_gEmptyDL;\n"
    )
    assert ns.audit_literals(text) == [(1, namespace_symbols.STRING, '"OoT_gEmptyDL"')]
    assert ns.audit_literals(text, include_comments=True) == [
        (1, namespace_symbols.STRING, '"OoT_gEmptyDL"'),
        (2, namespace_symbols.COMMENT, "// OoT_gEmptyDL in a comment"),
    ]
