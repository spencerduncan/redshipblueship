"""
Python wrapper for PMD CPD (Copy/Paste Detector) clone detection.

PMD CPD is an industry-standard tool that detects duplicate code across files.
This wrapper runs CPD on given directories and parses the CSV output to return
structured clone detection results.
"""

import csv
import subprocess
import shutil
from dataclasses import dataclass
from io import StringIO
from typing import List, Optional


@dataclass
class ClonePair:
    """Represents a detected clone pair between two files."""
    file1: str
    line1: int
    file2: str
    line2: int
    lines: int
    tokens: int


class CPDError(Exception):
    """Exception raised when CPD execution fails."""
    pass


class CPDNotFoundError(CPDError):
    """Exception raised when PMD CPD is not installed."""
    pass


def _find_pmd_executable() -> str:
    """Find the PMD executable on the system.

    Returns:
        Path to the PMD executable.

    Raises:
        CPDNotFoundError: If PMD is not found on the system.
    """
    # Check if 'pmd' is in PATH
    pmd_path = shutil.which("pmd")
    if pmd_path:
        return pmd_path

    # Common installation locations
    common_paths = [
        "/usr/local/bin/pmd",
        "/opt/pmd/bin/pmd",
        "/opt/pmd-bin/bin/pmd",
    ]

    for path in common_paths:
        if shutil.which(path):
            return path

    raise CPDNotFoundError(
        "PMD CPD not found. Please install PMD from https://pmd.github.io/ "
        "and ensure 'pmd' is in your PATH."
    )


def _parse_cpd_csv(csv_output: str) -> List[ClonePair]:
    """Parse CPD CSV output into ClonePair objects.

    CPD CSV format (each line after header):
    lines,tokens,occurrences,file1,startline1,file2,startline2[,file3,startline3,...]

    For pairs (occurrences=2):
    15,89,2,games/oot/src/file.c,10,games/mm/src/file.c,10

    Args:
        csv_output: Raw CSV output from CPD.

    Returns:
        List of ClonePair objects representing detected clones.
    """
    clones = []

    # Skip empty output
    if not csv_output.strip():
        return clones

    reader = csv.reader(StringIO(csv_output))

    for row in reader:
        # Skip header row
        if not row or row[0] == "lines":
            continue

        # Skip malformed rows
        if len(row) < 7:
            continue

        try:
            lines = int(row[0])
            tokens = int(row[1])
            occurrences = int(row[2])

            # For each pair of occurrences, create a ClonePair
            # CSV format: lines,tokens,occurrences,file1,line1,file2,line2,...
            # Starting at index 3, each occurrence is (file, line) pair
            occurrence_data = []
            idx = 3
            while idx + 1 < len(row):
                file_path = row[idx]
                line_num = int(row[idx + 1])
                occurrence_data.append((file_path, line_num))
                idx += 2

            # Create ClonePair for each unique pair of occurrences
            for i in range(len(occurrence_data)):
                for j in range(i + 1, len(occurrence_data)):
                    file1, line1 = occurrence_data[i]
                    file2, line2 = occurrence_data[j]

                    clones.append(ClonePair(
                        file1=file1,
                        line1=line1,
                        file2=file2,
                        line2=line2,
                        lines=lines,
                        tokens=tokens,
                    ))

        except (ValueError, IndexError):
            # Skip rows that can't be parsed
            continue

    return clones


def run_cpd(
    directories: List[str],
    language: str = "cpp",
    min_tokens: int = 50,
    ignore_identifiers: bool = True,
    ignore_literals: bool = True,
) -> List[ClonePair]:
    """Run PMD CPD on given directories and return detected clones.

    Args:
        directories: List of directory paths to scan for duplicates.
        language: Programming language (default: "cpp" for C/C++).
        min_tokens: Minimum token count for a duplicate (default: 50).
        ignore_identifiers: Ignore identifier names when comparing (default: True).
        ignore_literals: Ignore literal values when comparing (default: True).

    Returns:
        List of ClonePair objects representing detected duplicate code.

    Raises:
        CPDNotFoundError: If PMD CPD is not installed.
        CPDError: If CPD execution fails.
    """
    pmd_path = _find_pmd_executable()

    # Build command
    cmd = [
        pmd_path, "cpd",
        "--language", language,
        "--minimum-tokens", str(min_tokens),
        "--format", "csv",
    ]

    if ignore_identifiers:
        cmd.append("--ignore-identifiers")

    if ignore_literals:
        cmd.append("--ignore-literals")

    # Add directories
    for directory in directories:
        cmd.extend(["--dir", directory])

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=300,  # 5 minute timeout
        )

        # CPD returns exit code 4 when duplicates are found (which is expected)
        # Exit code 0 means no duplicates found
        # Other exit codes indicate errors
        if result.returncode not in (0, 4):
            error_msg = result.stderr.strip() if result.stderr else "Unknown error"
            raise CPDError(f"CPD failed with exit code {result.returncode}: {error_msg}")

        return _parse_cpd_csv(result.stdout)

    except subprocess.TimeoutExpired:
        raise CPDError("CPD execution timed out after 5 minutes")
    except FileNotFoundError:
        raise CPDNotFoundError(
            "PMD CPD not found. Please install PMD from https://pmd.github.io/ "
            "and ensure 'pmd' is in your PATH."
        )
