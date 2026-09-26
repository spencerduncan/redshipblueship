#!/usr/bin/env python3
"""The SoH-references gate for UI work: did a change move any ORIGINAL SoH page?

Rule 0 of docs/ui-style-guide.md is that the vanilla Ship of Harkinian menus stay
as shipped. `redship --test ui-snapshot` records a content hash for every capture
in its manifest; this compares two runs' manifests -- a baseline taken from main
and the branch under review, same machine, same profile, same backend -- and
reports every SOH_REFERENCE capture whose pixels or text changed.

It is a separate report and not a ctest row because a ctest row has no baseline
run to compare against. The iteration procedure runs it after every change:

    tools/ui-refs-diff.py <base>/manifest.json <new>/manifest.json

Exit 0: every reference capture present in both runs hashes the same (pixels and
text). Exit 1: at least one moved -- revert it, or state the minor wording change
in the PR as rule 0 requires. Exit 2: the runs are not comparable (different
profile, backend or ROM mode), which would make any verdict meaningless.

Volatile pages (Settings/General shows the build version and commit) are listed
but never gated.

  --self-test   synthetic manifests, both verdicts and the refusal
"""
import json
import sys
import tempfile
import os

VOLATILE = {"Settings/General"}
COMPARABLE = ("profile", "backend", "readback", "romFree", "platform")


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def refs(manifest):
    out = {}
    for page in manifest.get("pages", []):
        if page.get("origin") != "SOH_REFERENCE" or page.get("status") != "pass":
            continue
        out[(page["id"], page.get("variant", ""))] = page
    return out


def compare(base, new, out=sys.stdout):
    mismatch = [k for k in COMPARABLE if base.get("run", {}).get(k) != new.get("run", {}).get(k)]
    if mismatch:
        for k in mismatch:
            print(f"NOT COMPARABLE: run.{k} is {base['run'].get(k)!r} in the base and {new['run'].get(k)!r} now",
                  file=out)
        return 2
    b, n = refs(base), refs(new)
    moved, same, volatile = [], 0, []
    for key in sorted(set(b) & set(n)):
        pb, pn = b[key], n[key]
        changed = [f for f in ("rgbaFnv1a64", "textFnv1a64") if pb.get(f) != pn.get(f)]
        if not changed:
            same += 1
        elif key[0] in VOLATILE:
            volatile.append((key, changed))
        else:
            moved.append((key, changed))
    for key in sorted(set(b) - set(n)):
        print(f"missing now: {key[0]}@{key[1]}", file=out)
    for key in sorted(set(n) - set(b)):
        print(f"new capture (no baseline): {key[0]}@{key[1]}", file=out)
    for key, fields in volatile:
        print(f"volatile, not gated: {key[0]}@{key[1]} ({', '.join(fields)})", file=out)
    for key, fields in moved:
        print(f"MOVED: {key[0]}@{key[1]} ({', '.join(fields)} changed)", file=out)
    print(f"{same} SoH reference capture(s) unchanged, {len(moved)} moved, {len(volatile)} volatile", file=out)
    return 1 if moved else 0


def self_test():
    def manifest(ref_hash, text_hash="t0", profile="desk-1280x800"):
        return {"schema": 1,
                "run": {"profile": profile, "backend": "OpenGL", "readback": "rgba8-gl", "romFree": False,
                        "platform": "win32"},
                "pages": [
                    {"id": "Randomizer/General", "variant": "scroll0", "origin": "SOH_REFERENCE", "status": "pass",
                     "rgbaFnv1a64": ref_hash, "textFnv1a64": text_hash},
                    {"id": "Settings/General", "variant": "scroll0", "origin": "SOH_REFERENCE", "status": "pass",
                     "rgbaFnv1a64": ref_hash + "v", "textFnv1a64": "x"},
                    {"id": "Combo/Cross-Game Rules", "variant": "unpaired@scroll0", "origin": "RSBS",
                     "status": "pass", "rgbaFnv1a64": "ours", "textFnv1a64": "ours"},
                ]}

    sink = open(os.devnull, "w")
    failures = []
    if compare(manifest("a"), manifest("a"), sink) != 0:
        failures.append("identical references did not pass")
    if compare(manifest("a"), manifest("b"), sink) != 1:
        failures.append("a moved reference passed")
    if compare(manifest("a"), manifest("a", text_hash="t1"), sink) != 1:
        failures.append("a reference whose TEXT changed passed")
    base = manifest("a")
    new = manifest("a")
    new["pages"][1]["rgbaFnv1a64"] = "different"
    new["pages"][2]["rgbaFnv1a64"] = "ours-changed"
    if compare(base, new, sink) != 0:
        failures.append("a volatile page or one of ours was gated")
    if compare(manifest("a"), manifest("a", profile="small-960x704"), sink) != 2:
        failures.append("runs at different profiles were compared")
    with tempfile.TemporaryDirectory() as d:
        pa, pb = os.path.join(d, "a.json"), os.path.join(d, "b.json")
        json.dump(manifest("a"), open(pa, "w"))
        json.dump(manifest("b"), open(pb, "w"))
        if compare(load(pa), load(pb), sink) != 1:
            failures.append("the file round trip lost the verdict")
    for f in failures:
        print("SELF-TEST FAIL:", f)
    if not failures:
        print("self-test: OK (unchanged, moved pixels, moved text, volatile/ours not gated, incomparable refused)")
    return 1 if failures else 0


def main(argv):
    if argv == ["--self-test"]:
        return self_test()
    if len(argv) != 2:
        print(__doc__)
        return 2
    return compare(load(argv[0]), load(argv[1]))


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
