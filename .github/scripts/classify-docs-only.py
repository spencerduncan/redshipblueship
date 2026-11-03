#!/usr/bin/env python3
"""Decide whether a pull request is provably docs-only (#1373).

generate-builds.yml's `docs-only` job pipes in the paths that the PR's merge
commit changes relative to its base parent:

    git diff -z --name-only --no-renames HEAD^1 HEAD | python3 classify-docs-only.py -z

It writes `docs_only=true` only when that list is NON-EMPTY and EVERY path
matches one of DOCS_PATTERNS. Anything else, an empty list included, is
`docs_only=false`, and the builds run.

This file decides when the required checks build-linux, build-windows and
build-macos may pass without a build, so keep it narrow and fail closed. A path
that git quotes, a rename (shown as a delete plus an add by --no-renames) or a
submodule pointer is never docs.

DOCS_PATTERNS mirror generate-builds.yml's `on.push.paths-ignore`, written in
GitHub's filter-pattern syntax. tools/tests/test_required_checks.py keeps the
two lists equal and runs this classifier against a table of change sets.
"""

import argparse
import re
import sys

# GitHub filter patterns: `*` matches any characters except `/`, `**` matches
# any characters including `/`. Only these two wildcards are supported here;
# a pattern using another one is refused rather than guessed at.
DOCS_PATTERNS = ("**.md", "docs/**", "LICENSE*", ".gitignore")


def pattern_to_regex(pattern):
    """Translate a GitHub filter pattern that uses only `*` and `**`."""
    if re.search(r"[?+\[\]!]", pattern):
        raise ValueError(f"unsupported filter pattern syntax in {pattern!r}")
    out = []
    i = 0
    while i < len(pattern):
        if pattern.startswith("**", i):
            out.append(".*")
            i += 2
        elif pattern[i] == "*":
            out.append("[^/]*")
            i += 1
        else:
            out.append(re.escape(pattern[i]))
            i += 1
    return re.compile("".join(out) + r"\Z")


DOCS_REGEXES = tuple(pattern_to_regex(p) for p in DOCS_PATTERNS)


def is_docs_path(path):
    return any(rx.match(path) for rx in DOCS_REGEXES)


def classify(paths):
    """True only for a non-empty change set made entirely of docs paths."""
    paths = [p for p in paths if p]
    return bool(paths) and all(is_docs_path(p) for p in paths)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n", 1)[0])
    parser.add_argument("-z", action="store_true", help="paths on stdin are NUL-separated (git diff -z)")
    parser.add_argument("--github-output", help="append docs_only=<true|false> to this file ($GITHUB_OUTPUT)")
    args = parser.parse_args(argv)

    data = sys.stdin.read()
    paths = [p for p in (data.split("\0") if args.z else data.splitlines()) if p]

    print(f"{len(paths)} changed path(s):")
    for p in paths:
        print(f"  {'docs ' if is_docs_path(p) else 'CODE '} {p}")
    docs_only = classify(paths)
    if not paths:
        print("Empty change set: not provably docs-only, so the builds run.")
    line = f"docs_only={'true' if docs_only else 'false'}"
    print(line)
    if args.github_output:
        with open(args.github_output, "a", encoding="utf-8") as fh:
            fh.write(line + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
