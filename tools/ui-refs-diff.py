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

Exit 0: every reference capture that passed in the base passed again and hashes
the same (pixels and text). Exit 1: at least one moved, OR was LOST -- it passed
in the base and is now absent, failed (a throw, a failed oracle, a blank page) or
skipped; a reference that stops drawing is the largest possible move. Revert it,
or state the minor wording change in the PR as rule 0 requires. Exit 2: the runs
are not comparable (different profile, backend or ROM mode), which would make any
verdict meaningless.

Volatile pages (Settings/General shows the build version and commit) have their
hashes listed but not gated; losing one is still gated.

WHERE THIS GUARD RUNS: only on a ROM-staged workstation. Hosted CI is ROM-free
(both CI manifests say romFree true), where SoH's own menu is never populated and
only Dev Tools/General registers, and CI has no base run to compare against. The
same holds for R8 (soh-names.txt), which the harness only writes ROM-rich. So the
original-page guard is part of the iteration procedure (docs/ui-style-guide.md
section 12), not a CI check.

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


def refs(manifest, passing_only=True):
    out = {}
    for page in manifest.get("pages", []):
        if page.get("origin") != "SOH_REFERENCE":
            continue
        if passing_only and page.get("status") != "pass":
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
    every_new = refs(new, passing_only=False)
    moved, same, volatile = [], 0, []
    lost = []
    for key in sorted(set(b) - set(n)):
        page = every_new.get(key)
        if page is None:
            lost.append((key, "absent from the new run"))
        else:
            reason = page.get("reason", "")
            lost.append((key, f"status {page.get('status')!r}" + (f": {reason}" if reason else "")))
    for key in sorted(set(b) & set(n)):
        pb, pn = b[key], n[key]
        changed = [f for f in ("rgbaFnv1a64", "textFnv1a64") if pb.get(f) != pn.get(f)]
        if not changed:
            same += 1
        elif key[0] in VOLATILE:
            volatile.append((key, changed))
        else:
            moved.append((key, changed))
    for key, why in lost:
        print(f"LOST: {key[0]}@{key[1]} passed in the base and is now {why}", file=out)
    for key in sorted(set(n) - set(b)):
        print(f"new capture (no baseline): {key[0]}@{key[1]}", file=out)
    for key, fields in volatile:
        print(f"volatile, not gated: {key[0]}@{key[1]} ({', '.join(fields)})", file=out)
    for key, fields in moved:
        print(f"MOVED: {key[0]}@{key[1]} ({', '.join(fields)} changed)", file=out)
    print(f"{same} SoH reference capture(s) unchanged, {len(moved)} moved, {len(lost)} lost, "
          f"{len(volatile)} volatile", file=out)
    return 1 if moved or lost else 0


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
    # A reference that stops drawing: throws, fails its oracle, goes blank.
    for status in ("fail", "skip"):
        new = manifest("a")
        new["pages"][0]["status"] = status
        new["pages"][0]["reason"] = "an exception escaped the frame"
        if compare(manifest("a"), new, sink) != 1:
            failures.append(f"a reference that passed in the base and is now {status!r} passed")
    new = manifest("a")
    del new["pages"][0]
    if compare(manifest("a"), new, sink) != 1:
        failures.append("a reference absent from the new run passed")
    new = manifest("a")
    new["pages"][1]["status"] = "fail"
    if compare(manifest("a"), new, sink) != 1:
        failures.append("a VOLATILE reference that stopped drawing passed")
    base = manifest("a")
    base["pages"][0]["status"] = "fail"
    if compare(base, manifest("a"), sink) != 0:
        failures.append("a reference that was already failing in the base was gated")
    with tempfile.TemporaryDirectory() as d:
        pa, pb = os.path.join(d, "a.json"), os.path.join(d, "b.json")
        json.dump(manifest("a"), open(pa, "w"))
        json.dump(manifest("b"), open(pb, "w"))
        if compare(load(pa), load(pb), sink) != 1:
            failures.append("the file round trip lost the verdict")
    for f in failures:
        print("SELF-TEST FAIL:", f)
    if not failures:
        print("self-test: OK (unchanged, moved pixels, moved text, volatile/ours not gated, incomparable refused, "
              "lost references gated: failed, skipped, absent, volatile)")
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
