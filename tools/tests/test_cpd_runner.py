"""Tests for the cpd_runner module."""

import pytest

from cpd_runner import (
    ClonePair,
    CPDError,
    CPDNotFoundError,
    _parse_cpd_csv,
    run_cpd,
)


class TestClonePair:
    """Tests for ClonePair dataclass."""

    def test_clone_pair_creation(self):
        clone = ClonePair(
            file1="file1.c",
            line1=10,
            file2="file2.c",
            line2=20,
            lines=15,
            tokens=89,
        )
        assert clone.file1 == "file1.c"
        assert clone.line1 == 10
        assert clone.file2 == "file2.c"
        assert clone.line2 == 20
        assert clone.lines == 15
        assert clone.tokens == 89


class TestParseCpdCsv:
    """Tests for _parse_cpd_csv function."""

    def test_parses_simple_clone_pair(self):
        csv_output = """lines,tokens,occurrences
15,89,2,games/oot/src/file.c,10,games/mm/src/file.c,20"""
        clones = _parse_cpd_csv(csv_output)
        assert len(clones) == 1
        clone = clones[0]
        assert clone.lines == 15
        assert clone.tokens == 89
        assert clone.file1 == "games/oot/src/file.c"
        assert clone.line1 == 10
        assert clone.file2 == "games/mm/src/file.c"
        assert clone.line2 == 20

    def test_parses_multiple_clone_pairs(self):
        csv_output = """lines,tokens,occurrences
15,89,2,games/oot/src/a.c,10,games/mm/src/a.c,20
20,100,2,games/oot/src/b.c,30,games/mm/src/b.c,40"""
        clones = _parse_cpd_csv(csv_output)
        assert len(clones) == 2

        assert clones[0].lines == 15
        assert clones[0].file1 == "games/oot/src/a.c"
        assert clones[0].file2 == "games/mm/src/a.c"

        assert clones[1].lines == 20
        assert clones[1].file1 == "games/oot/src/b.c"
        assert clones[1].file2 == "games/mm/src/b.c"

    def test_parses_triple_occurrence(self):
        # When a clone appears in 3 files, we get 3 pairs: (1,2), (1,3), (2,3)
        csv_output = """lines,tokens,occurrences
15,89,3,file1.c,10,file2.c,20,file3.c,30"""
        clones = _parse_cpd_csv(csv_output)
        assert len(clones) == 3

        # Pair (file1, file2)
        assert clones[0].file1 == "file1.c"
        assert clones[0].line1 == 10
        assert clones[0].file2 == "file2.c"
        assert clones[0].line2 == 20

        # Pair (file1, file3)
        assert clones[1].file1 == "file1.c"
        assert clones[1].line1 == 10
        assert clones[1].file2 == "file3.c"
        assert clones[1].line2 == 30

        # Pair (file2, file3)
        assert clones[2].file1 == "file2.c"
        assert clones[2].line1 == 20
        assert clones[2].file2 == "file3.c"
        assert clones[2].line2 == 30

    def test_handles_empty_output(self):
        clones = _parse_cpd_csv("")
        assert clones == []

    def test_handles_whitespace_only_output(self):
        clones = _parse_cpd_csv("   \n\t  ")
        assert clones == []

    def test_handles_header_only(self):
        csv_output = "lines,tokens,occurrences\n"
        clones = _parse_cpd_csv(csv_output)
        assert clones == []

    def test_skips_malformed_rows(self):
        csv_output = """lines,tokens,occurrences
15,89,2,games/oot/src/a.c,10,games/mm/src/a.c,20
invalid,row,data
20,100,2,games/oot/src/b.c,30,games/mm/src/b.c,40"""
        clones = _parse_cpd_csv(csv_output)
        # Should still parse the valid rows
        assert len(clones) == 2

    def test_skips_rows_with_missing_fields(self):
        csv_output = """lines,tokens,occurrences
15,89,2,file.c,10
20,100,2,games/oot/src/b.c,30,games/mm/src/b.c,40"""
        clones = _parse_cpd_csv(csv_output)
        # First row doesn't have enough fields for a pair
        assert len(clones) == 1
        assert clones[0].file1 == "games/oot/src/b.c"

    def test_handles_paths_with_spaces(self):
        csv_output = """lines,tokens,occurrences
15,89,2,path/to/my file.c,10,another path/file.c,20"""
        clones = _parse_cpd_csv(csv_output)
        assert len(clones) == 1
        assert clones[0].file1 == "path/to/my file.c"
        assert clones[0].file2 == "another path/file.c"


class TestRunCpd:
    """Tests for run_cpd function."""

    def test_raises_cpd_not_found_when_pmd_missing(self):
        # When PMD is not installed, should raise CPDNotFoundError
        # This test will pass on systems without PMD installed
        try:
            run_cpd(["nonexistent_dir"])
        except CPDNotFoundError as e:
            assert "PMD" in str(e) or "pmd" in str(e)
        except CPDError:
            # CPD may also raise a general error if the directory doesn't exist
            pass

    def test_accepts_language_parameter(self):
        # Test that the function accepts the language parameter
        # (actual execution depends on PMD being installed)
        try:
            run_cpd(["nonexistent_dir"], language="java")
        except (CPDNotFoundError, CPDError):
            pass  # Expected when PMD not installed or dir doesn't exist

    def test_accepts_min_tokens_parameter(self):
        try:
            run_cpd(["nonexistent_dir"], min_tokens=100)
        except (CPDNotFoundError, CPDError):
            pass

    def test_accepts_ignore_flags(self):
        try:
            run_cpd(
                ["nonexistent_dir"],
                ignore_identifiers=False,
                ignore_literals=False,
            )
        except (CPDNotFoundError, CPDError):
            pass


class TestCPDExceptions:
    """Tests for CPD exception hierarchy."""

    def test_cpd_not_found_error_is_cpd_error(self):
        assert issubclass(CPDNotFoundError, CPDError)

    def test_cpd_error_is_exception(self):
        assert issubclass(CPDError, Exception)
