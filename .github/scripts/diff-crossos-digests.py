#!/usr/bin/env python3
"""Fail when the Linux and Windows CI legs produced different determinism digests (#1003).

Both legs check the golden rows against the same committed bytes, which proves they
agree only on what a committed golden pins. This compares the digest files the two
legs actually PRODUCED against each other, so a world no golden pins (the self-diff
rows' run-1 files, the hint and shop digests the redship tier writes) and a world a PR
moves on purpose (both legs red against the old golden) are compared across OSes too.

Usage: diff-crossos-digests.py <dir-a> <label-a> <dir-b> <label-b>

Every regular file directly under either directory is a digest: one `key=value` per
line. The comparison is line by line after stripping CR (the writers use stdio text
mode, so Windows writes CRLF), and it is reported field by field. A file that exists on
one leg only is a failure, and so are two empty directories: a diff of nothing would
pass and prove nothing.
"""

import os
import sys


def read_digest(path):
    with open(path, "rb") as f:
        raw = f.read().decode("utf-8", errors="replace")
    return [line.rstrip("\r") for line in raw.split("\n") if line.rstrip("\r") != ""]


def as_fields(lines):
    fields = {}
    order = []
    for line in lines:
        key, sep, value = line.partition("=")
        if not sep:
            key, value = "(free text)", line
        if key in fields:
            # A repeated key keeps every value, so a reordered repeat still differs.
            fields[key] = fields[key] + "|" + value
        else:
            fields[key] = value
            order.append(key)
    return fields, order


def list_digests(d):
    if not os.path.isdir(d):
        return None
    return sorted(n for n in os.listdir(d) if os.path.isfile(os.path.join(d, n)))


def main(argv):
    if len(argv) != 5:
        print(__doc__)
        return 2
    dir_a, label_a, dir_b, label_b = argv[1:]
    names_a = list_digests(dir_a)
    names_b = list_digests(dir_b)
    failures = 0
    for d, label, names in ((dir_a, label_a, names_a), (dir_b, label_b, names_b)):
        if names is None:
            print(f"FAIL: the {label} leg uploaded no digest artifact ({d} does not exist); "
                  f"its test steps did not run, so nothing can be compared")
            failures += 1
    if failures:
        return 1
    if not names_a and not names_b:
        print("FAIL: neither leg produced a digest file; a diff of nothing proves nothing")
        return 1

    only_a = [n for n in names_a if n not in names_b]
    only_b = [n for n in names_b if n not in names_a]
    for n in only_a:
        print(f"FAIL: {n} was produced on {label_a} only")
    for n in only_b:
        print(f"FAIL: {n} was produced on {label_b} only")
    failures += len(only_a) + len(only_b)

    common = [n for n in names_a if n in names_b]
    for n in common:
        lines_a = read_digest(os.path.join(dir_a, n))
        lines_b = read_digest(os.path.join(dir_b, n))
        if lines_a == lines_b:
            print(f"same: {n} ({len(lines_a)} lines)")
            continue
        failures += 1
        fa, order_a = as_fields(lines_a)
        fb, order_b = as_fields(lines_b)
        print(f"FAIL: {n} differs between {label_a} and {label_b}:")
        for key in order_a + [k for k in order_b if k not in fa]:
            va, vb = fa.get(key), fb.get(key)
            if va == vb:
                continue
            print(f"    {key}: {label_a} {va!r} -> {label_b} {vb!r}")
        if fa == fb:
            print("    (the same fields and values in a different order)")

    print(f"{len(common)} digest file(s) compared, {failures} failure(s)")
    if failures:
        print("The two OSes generated different worlds (or different digests) for the same seed and "
              "profile: see docs/determinism-goldens.md, 'Platform portability'.")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
