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
