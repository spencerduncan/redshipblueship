#!/usr/bin/env python3
"""Each required status check has one producer, and that producer is never skipped (#1373).

The "basic" ruleset (id 11957440, no bypass actors) requires the GitHub Actions
check runs build-linux, build-windows, build-macos and clang-format. Two GitHub
behaviours make a required check easy to satisfy by accident:

* A job skipped by its `if`, or by a failed or skipped `needs`, reports
  "Success" and "will not prevent a pull request from merging, even if it is a
  required check" (docs: using conditions to control job execution;
  troubleshooting required status checks).
* A check run only exists once its job is queued. A job that `needs` another
  has no check run until that job finishes.

docs-bypass.yml combined the two. Its stub jobs carried the required names and
finished `skipped` on any PR that changed docs AND code. generate-builds.yml
creates the real build-* check runs only after generate-rsbs-otr, so for 20-60 s
every required check read as satisfied, and #1321, #1325 and #1331 merged
before any build had started.

The fix gives each required name exactly ONE producer:

* build-linux, build-windows and build-macos come from generate-builds.yml's
  required-build-* jobs. They have `if: always()`, so they are never skipped.
  Each reports the real build job's result, and passes without a build only on
  a pull_request that the docs-only job proved docs-only.
* clang-format comes from clang-format.yml's job, which has no `if`, no
  `needs` and no path filter, so it always runs the real check.

These tests lock that structure, run the required-check script against every
combination of results, run the docs-only job's step against real merge
commits, and run the classifier against a table of change sets.

They also lock every workflow's concurrency (#1377). generate-builds.yml used
cancel-in-progress on every event, so each merge to main cancelled the running
main build, and the macOS caches that only main saves were never written (a
cold macOS build takes about 4.5 hours). The lock evaluates each concurrency
block for a table of sample runs and fails if any run could cancel, in
progress, a run on main, a release branch or a tag, while a pull request's
newer push must still cancel its older run.
"""

import importlib.util
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

import pytest
import yaml

REPO_ROOT = Path(__file__).resolve().parents[2]
WORKFLOWS = REPO_ROOT / ".github" / "workflows"
GENERATE_BUILDS = WORKFLOWS / "generate-builds.yml"
CLASSIFIER = REPO_ROOT / ".github" / "scripts" / "classify-docs-only.py"

# The "basic" ruleset's required checks, and the only workflow allowed to
# produce each one.
REQUIRED_CHECKS = {
    "build-linux": "generate-builds.yml",
    "build-windows": "generate-builds.yml",
    "build-macos": "generate-builds.yml",
    "clang-format": "clang-format.yml",
}

# For each required build check: the real job it reports, and the runner family
# that job must use (a mistyped job id would otherwise report the wrong build).
BUILD_CHECKS = {
    "build-linux": ("build-linux", "ubuntu"),
    "build-windows": ("build-windows", "windows"),
    "build-macos": ("build-macos", "macos"),
}

DOCS_ONLY_JOB = "docs-only"
JOB_RESULTS = ("success", "failure", "cancelled", "skipped")


def _load(path):
    return yaml.safe_load(path.read_text(encoding="utf-8"))


def _triggers(workflow):
    # PyYAML reads the bare key `on` as the boolean True (YAML 1.1).
    on = workflow.get("on", workflow.get(True))
    if isinstance(on, str):
        return {on: None}
    if isinstance(on, list):
        return {event: None for event in on}
    return dict(on or {})


def _workflow_files():
    files = sorted(p for p in WORKFLOWS.iterdir() if p.suffix in (".yml", ".yaml"))
    assert len(files) >= 5, f"expected the repo's workflows under {WORKFLOWS}, found {files}"
    return files


def _check_name(job_id, job):
    """The name GitHub gives a job's check run: its `name`, else its id."""
    return str(job.get("name", job_id))


def _expr(value):
    """An `if:` value without its optional ${{ }} wrapper."""
    text = str(value).strip()
    m = re.fullmatch(r"\$\{\{\s*(.*?)\s*\}\}", text, re.DOTALL)
    return (m.group(1) if m else text).strip()


def _needs(job):
    needs = job.get("needs", [])
    return [needs] if isinstance(needs, str) else list(needs)


def _jobs(path):
    return _load(path)["jobs"]


def _producers():
    """{check name: [(workflow file name, job id)]} over every workflow."""
    found = {}
    for path in _workflow_files():
        for job_id, job in (_load(path).get("jobs") or {}).items():
            found.setdefault(_check_name(job_id, job), []).append((path.name, job_id))
    return found


def test_docs_bypass_workflow_is_gone():
    assert not (WORKFLOWS / "docs-bypass.yml").exists(), (
        "#1373: docs-bypass.yml's stub jobs carried the required check names and finished `skipped` on mixed "
        "docs+code PRs, which GitHub counts as passing; generate-builds.yml decides docs-only itself now"
    )


@pytest.mark.parametrize("check", sorted(REQUIRED_CHECKS))
def test_each_required_check_has_exactly_one_producer(check):
    producers = _producers().get(check, [])
    assert len(producers) == 1, (
        f"#1373: required check {check!r} must be produced by exactly one job in one workflow, found {producers}. "
        "A second job with that name (a stub, a skipped job) can satisfy the requirement before the real one runs"
    )
    assert producers[0][0] == REQUIRED_CHECKS[check], (
        f"#1373: required check {check!r} must come from {REQUIRED_CHECKS[check]}, found {producers[0]}"
    )


# One ${{ ... }} expression in a job name. An unclosed one runs to the end of
# the name, so it can never shrink what the name could spell.
_NAME_EXPRESSION = re.compile(r"\$\{\{.*?(?:\}\}|\Z)", re.DOTALL)


def _computed_name_pattern(name):
    """Every check-run name a computed job `name` could produce: its literal
    text as written, and each ${{ ... }} as any text at all.

    GitHub appends the matrix values to a matrix job's check-run name only when
    the name does not reference the matrix, so `build-${{ matrix.os }}` over
    [linux, windows, macos] produces exactly build-linux, build-windows and
    build-macos. Reading every expression as `.*` is stricter than evaluating
    it, and no workflow here computes a job name today.
    """
    return re.compile(".*".join(re.escape(part) for part in _NAME_EXPRESSION.split(name)), re.DOTALL)


def _computed_name_violations(workflows):
    """[(workflow file, job id, required check)] for each computed job name
    that could spell a required check."""
    found = []
    for workflow_name, workflow in workflows:
        for job_id, job in ((workflow or {}).get("jobs") or {}).items():
            name = str(job.get("name", ""))
            if "${{" not in name:
                continue
            pattern = _computed_name_pattern(name)
            found.extend(
                (workflow_name, job_id, check) for check in sorted(REQUIRED_CHECKS) if pattern.fullmatch(check)
            )
    return found


def test_no_job_computes_a_required_name():
    # A name built from an expression (matrix, inputs, format()) is invisible
    # to the producer count above, so it must not be able to spell a required
    # name at all.
    violations = _computed_name_violations((path.name, _load(path)) for path in _workflow_files())
    assert not violations, (
        f"#1373: these jobs compute a name that could be a required check: {violations}. A second producer of a "
        "required name (skipped, or finished early) can satisfy the requirement before the real one runs; give the "
        "job a literal name, or literal text that no required name can match"
    )


# A workflow that would sneak a second producer of required names past a lock
# that only looked for the literal names: skipped on every code PR, and named
# from an expression. @JOB_NAME@ is replaced with the job's name.
_COMPUTED_NAME_MUTANT = """\
name: Mutant
on:
  pull_request:
    paths: ['**.md']
jobs:
  build:
    if: false
    name: @JOB_NAME@
    strategy:
      matrix:
        os: [linux, windows, macos]
    runs-on: ubuntu-latest
    steps:
      - run: 'true'
"""


@pytest.mark.parametrize(
    "name, could_spell",
    [
        # The #1373 stub shape: GitHub names these exactly build-linux,
        # build-windows and build-macos.
        ("build-${{ matrix.os }}", ["build-linux", "build-macos", "build-windows"]),
        ("build-${{matrix.os}}", ["build-linux", "build-macos", "build-windows"]),
        ("${{ format('clang-{0}', 'format') }}", sorted(REQUIRED_CHECKS)),
        ("clang-${{ inputs.what }}", ["clang-format"]),
        ("${{ matrix.prefix }}-linux", ["build-linux"]),
        ("build-${{ matrix.os", ["build-linux", "build-macos", "build-windows"]),
        # Literal text no required name can match.
        ("Test ${{ matrix.os }}", []),
        ("build-${{ matrix.os }} (rando)", []),
        ("${{ matrix.os }} build", []),
    ],
)
def test_computed_name_lock_catches_names_that_could_spell_a_required_check(name, could_spell):
    workflow = yaml.safe_load(_COMPUTED_NAME_MUTANT.replace("@JOB_NAME@", json.dumps(name)))
    assert workflow["jobs"]["build"]["name"] == name
    found = [check for _, _, check in _computed_name_violations([("mutant.yml", workflow)])]
    assert found == could_spell, f"job name {name!r} could produce {could_spell}, the lock found {found}"


@pytest.mark.parametrize("workflow", sorted(set(REQUIRED_CHECKS.values())))
def test_required_check_workflows_run_on_every_pull_request(workflow):
    on = _triggers(_load(WORKFLOWS / workflow))
    assert "pull_request" in on, f"#1373: {workflow} produces a required check, so it must run on pull_request"
    config = on["pull_request"]
    assert not config, (
        f"#1373: {workflow}'s pull_request trigger must have no filters (found {config!r}). A workflow skipped by a "
        "path, branch or type filter leaves its required checks 'Expected', and docs-only PRs could never merge; "
        "decide docs-only inside the workflow instead"
    )


@pytest.mark.parametrize("check", sorted(BUILD_CHECKS))
def test_required_build_check_cannot_be_skipped(check):
    jobs = _jobs(GENERATE_BUILDS)
    real_id, runner_family = BUILD_CHECKS[check]
    [(gate_id, gate)] = [(jid, job) for jid, job in jobs.items() if _check_name(jid, job) == check]

    assert _expr(gate.get("if", "")) == "always()", (
        f"#1373: {gate_id} (check {check!r}) must have `if: always()`. Without it a failed or skipped need skips "
        "the job, and GitHub counts a skipped job as passing a required check"
    )
    assert sorted(_needs(gate)) == sorted([DOCS_ONLY_JOB, real_id]), (
        f"#1373: {gate_id} must need exactly [{DOCS_ONLY_JOB}, {real_id}], found {_needs(gate)}"
    )
    assert gate.get("runs-on") == "ubuntu-latest", f"{gate_id} only reads results; keep it on ubuntu-latest"
    assert "continue-on-error" not in gate, f"#1373: {gate_id} must be able to fail"
    assert "strategy" not in gate, f"#1373: a matrix would rename {gate_id}'s check run"
    steps = gate["steps"]
    assert len(steps) == 1 and "uses" not in steps[0], f"#1373: {gate_id} must be one `run` step"
    step = steps[0]
    assert "if" not in step and "continue-on-error" not in step, f"#1373: {gate_id}'s step must always run and fail"
    assert step["env"] == {
        "EVENT_NAME": "${{ github.event_name }}",
        "DOCS_ONLY_RESULT": f"${{{{ needs.{DOCS_ONLY_JOB}.result }}}}",
        "DOCS_ONLY": f"${{{{ needs.{DOCS_ONLY_JOB}.outputs.docs_only }}}}",
        "BUILD_JOB": real_id,
        "BUILD_JOB_NAME": _check_name(real_id, jobs[real_id]),
        "BUILD_RESULT": f"${{{{ needs.{real_id}.result }}}}",
    }, f"#1373: {gate_id} must read the results of {DOCS_ONLY_JOB} and {real_id}"

    real = jobs[real_id]
    assert _check_name(real_id, real) not in REQUIRED_CHECKS, (
        f"#1373: the real job {real_id} must carry a `name` that is not a required check, or its own check run "
        f"(skipped on a docs-only PR) would answer the requirement beside {gate_id}"
    )
    assert str(real.get("runs-on", "")).startswith(runner_family), (
        f"{gate_id} must report the {runner_family} build, but job {real_id} runs on {real.get('runs-on')!r}"
    )
    assert "continue-on-error" not in real, (
        f"#1373: the real job {real_id} must have no job-level `continue-on-error`: a failed job with it can report "
        f"needs.{real_id}.result == 'success', and {gate_id} would pass a failed build"
    )
    assert "strategy" not in real, (
        f"#1373: the real job {real_id} must have no matrix: {gate_id} reports one build, not a set of them"
    )


def test_required_build_checks_run_one_script():
    jobs = _jobs(GENERATE_BUILDS)
    scripts = {
        check: [job for jid, job in jobs.items() if _check_name(jid, job) == check][0]["steps"][0]["run"]
        for check in BUILD_CHECKS
    }
    assert len(set(scripts.values())) == 1, (
        "#1373: the required-build-* jobs must run the same script (only their env differs), "
        "so test_required_build_check_decisions covers all three"
    )


def test_clang_format_check_always_runs_the_real_check():
    [(job_id, job)] = [
        (jid, j) for jid, j in _jobs(WORKFLOWS / "clang-format.yml").items() if _check_name(jid, j) == "clang-format"
    ]
    for key in ("if", "needs", "continue-on-error", "strategy"):
        assert key not in job, f"#1373: the clang-format job must have no `{key}`: it must never be skipped"
    for step in job["steps"]:
        assert "if" not in step and "continue-on-error" not in step, (
            f"#1373: the clang-format job's step {step.get('name', step.get('uses'))!r} must always run"
        )
    assert any("run-clang-format.sh" in str(step.get("run", "")) for step in job["steps"]), (
        "the clang-format job must run ./run-clang-format.sh"
    )


def _ancestors(jobs, job_id):
    seen = set()
    todo = list(_needs(jobs[job_id]))
    while todo:
        need = todo.pop()
        if need not in seen:
            seen.add(need)
            todo.extend(_needs(jobs[need]))
    return seen


def test_docs_only_job_gates_the_heavy_jobs_and_is_never_skipped():
    jobs = _jobs(GENERATE_BUILDS)
    detect = jobs[DOCS_ONLY_JOB]
    # Never skipped: a skipped ancestor skips every job below it, which on a
    # push or a tag would skip the builds and create-release.
    for key in ("if", "needs", "continue-on-error", "strategy"):
        assert key not in detect, f"#1373: the {DOCS_ONLY_JOB} job must have no `{key}`"
    assert detect["outputs"] == {"docs_only": "${{ steps.classify.outputs.docs_only }}"}
    [classify] = [s for s in detect["steps"] if s.get("id") == "classify"]
    assert "classify-docs-only.py" in classify["run"], (
        "the docs-only job must use .github/scripts/classify-docs-only.py"
    )

    otr = jobs["generate-rsbs-otr"]
    assert _needs(otr) == [DOCS_ONLY_JOB], "generate-rsbs-otr must need the docs-only job"
    assert _expr(otr.get("if", "")) == f"needs.{DOCS_ONLY_JOB}.outputs.docs_only != 'true'", (
        "#1373: generate-rsbs-otr must be skipped exactly on a proven docs-only PR. No status function: if the "
        "docs-only job fails, the implicit success() skips the builds and the required-build-* jobs go red"
    )
    for check, (real_id, _) in BUILD_CHECKS.items():
        assert {DOCS_ONLY_JOB, "generate-rsbs-otr"} <= _ancestors(jobs, real_id), (
            f"{real_id} must sit below generate-rsbs-otr and {DOCS_ONLY_JOB}, so a docs-only PR skips it"
        )


# --- the required-check script, run as GitHub runs it --------------------------------------------


def _bash():
    # RSBS_TEST_BASH names a bash to use (Git Bash on Windows); otherwise the
    # steps run only where bash is the system's own, as on the CI runner.
    override = os.environ.get("RSBS_TEST_BASH")
    if override:
        return override
    if os.name == "nt":
        pytest.skip("runs workflow bash steps; set RSBS_TEST_BASH to Git Bash to run it on Windows")
    found = shutil.which("bash")
    if not found:
        pytest.skip("bash not found")
    return found


def _gate_passes(bash, script, event, docs_only_result, docs_only, build_result):
    proc = subprocess.run(
        [bash, "--noprofile", "--norc", "-e", "-c", script],
        env=dict(
            os.environ,
            EVENT_NAME=event,
            DOCS_ONLY_RESULT=docs_only_result,
            DOCS_ONLY=docs_only,
            BUILD_JOB="build-test",
            BUILD_JOB_NAME="Test build",
            BUILD_RESULT=build_result,
        ),
        capture_output=True,
        text=True,
    )
    return proc.returncode == 0


def test_required_build_check_decisions():
    bash = _bash()
    jobs = _jobs(GENERATE_BUILDS)
    script = [job for jid, job in jobs.items() if _check_name(jid, job) == "build-linux"][0]["steps"][0]["run"]
    cases = 0
    for event in ("pull_request", "push", "workflow_dispatch"):
        for docs_only_result in JOB_RESULTS:
            for docs_only in ("true", "false", ""):
                for build_result in JOB_RESULTS:
                    expected = build_result == "success" or (
                        event == "pull_request"
                        and docs_only_result == "success"
                        and docs_only == "true"
                        and build_result == "skipped"
                    )
                    got = _gate_passes(bash, script, event, docs_only_result, docs_only, build_result)
                    assert got == expected, (
                        f"#1373: event={event} docs-only job={docs_only_result} docs_only={docs_only!r} "
                        f"build={build_result}: the required check should {'pass' if expected else 'FAIL'}"
                    )
                    cases += 1
    assert cases == 3 * 4 * 3 * 4


# --- the docs-only job's step, run against real merge commits -------------------------------------


def _git(repo, *args):
    subprocess.run(
        ["git", "-c", "user.name=t", "-c", "user.email=t@t", "-c", "commit.gpgsign=false", *args],
        cwd=repo,
        check=True,
        capture_output=True,
        text=True,
    )


def _head(repo):
    proc = subprocess.run(["git", "rev-parse", "HEAD"], cwd=repo, check=True, capture_output=True, text=True)
    return proc.stdout.strip()


def _write(repo, rel, text):
    path = repo / rel
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def _merge_commit_repo(tmp_path, pr_files):
    """A repo whose HEAD is a refs/pull/N/merge-shaped commit: parent 1 = base tip, parent 2 = PR head."""
    repo = tmp_path / "repo"
    repo.mkdir()
    _git(repo, "init", "-q", "-b", "main")
    _write(repo, "src/common/a.c", "int a;\n")
    _write(repo, "README.md", "readme\n")
    _write(repo, ".github/scripts/classify-docs-only.py", CLASSIFIER.read_text(encoding="utf-8"))
    _git(repo, "add", "-A")
    _git(repo, "commit", "-q", "-m", "base")
    _git(repo, "checkout", "-q", "-b", "pr")
    for rel, text in pr_files.items():
        _write(repo, rel, text)
    _git(repo, "add", "-A")
    _git(repo, "commit", "-q", "-m", "pr")
    pr_head = _head(repo)
    _git(repo, "checkout", "-q", "main")
    # The base moves on with code the PR does not touch: the merge commit's
    # first parent is this tip, so the PR is judged on its own change only.
    _write(repo, "src/common/b.c", "int b;\n")
    _git(repo, "add", "-A")
    _git(repo, "commit", "-q", "-m", "main moves on")
    _git(repo, "merge", "-q", "--no-ff", "-m", "merge", "pr")
    return repo, pr_head


def _run_classify_step(tmp_path, repo, event, pr_head):
    bash = _bash()
    [detect_step] = [s for s in _jobs(GENERATE_BUILDS)[DOCS_ONLY_JOB]["steps"] if s.get("id") == "classify"]
    shim = tmp_path / "bin"
    shim.mkdir(exist_ok=True)
    (shim / "python3").write_text(f'#!/bin/sh\nexec "{Path(sys.executable).as_posix()}" "$@"\n', encoding="utf-8")
    (shim / "python3").chmod(0o755)
    output = tmp_path / "github_output"
    output.write_text("", encoding="utf-8")
    runner_temp = tmp_path / "runner_temp"
    runner_temp.mkdir(exist_ok=True)
    # `shell: bash` runs `bash --noprofile --norc -eo pipefail {0}`. The shim
    # directory goes on PATH through `pwd`, which spells it the way this bash
    # does (Git Bash wants /c/..., not C:/...).
    proc = subprocess.run(
        [
            bash,
            "--noprofile",
            "--norc",
            "-eo",
            "pipefail",
            "-c",
            'PATH="$(cd "$SHIM_DIR" && pwd):$PATH"; export PATH\n' + detect_step["run"],
        ],
        cwd=repo,
        env=dict(
            os.environ,
            SHIM_DIR=shim.as_posix(),
            EVENT_NAME=event,
            PR_HEAD_SHA=pr_head,
            GITHUB_OUTPUT=output.as_posix(),
            RUNNER_TEMP=runner_temp.as_posix(),
        ),
        capture_output=True,
        text=True,
    )
    assert proc.returncode == 0, proc.stdout + proc.stderr
    lines = [line for line in output.read_text(encoding="utf-8").splitlines() if line.startswith("docs_only=")]
    assert len(lines) == 1, f"the step must write docs_only exactly once, wrote {lines}\n{proc.stdout}{proc.stderr}"
    return lines[0].split("=", 1)[1]


def test_docs_only_step_passes_a_docs_only_merge(tmp_path):
    repo, pr_head = _merge_commit_repo(tmp_path, {"README.md": "edited\n", "docs/new.md": "new\n"})
    assert _run_classify_step(tmp_path, repo, "pull_request", pr_head) == "true"


def test_docs_only_step_builds_a_mixed_merge(tmp_path):
    # The #1331 shape: docs AND code.
    repo, pr_head = _merge_commit_repo(tmp_path, {"README.md": "edited\n", "src/common/a.c": "int a2;\n"})
    assert _run_classify_step(tmp_path, repo, "pull_request", pr_head) == "false"


def test_docs_only_step_fails_closed_on_an_unexpected_merge_commit(tmp_path):
    repo, _ = _merge_commit_repo(tmp_path, {"README.md": "edited\n"})
    assert _run_classify_step(tmp_path, repo, "pull_request", "0" * 40) == "false"


def test_docs_only_step_never_skips_builds_outside_pull_requests(tmp_path):
    repo, pr_head = _merge_commit_repo(tmp_path, {"README.md": "edited\n"})
    for event in ("push", "workflow_dispatch"):
        assert _run_classify_step(tmp_path, repo, event, pr_head) == "false"


# --- the classifier ------------------------------------------------------------------------------


def _classifier():
    spec = importlib.util.spec_from_file_location("classify_docs_only", CLASSIFIER)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_docs_patterns_mirror_the_push_paths_ignore():
    push = _triggers(_load(GENERATE_BUILDS))["push"]
    assert sorted(_classifier().DOCS_PATTERNS) == sorted(push["paths-ignore"]), (
        "classify-docs-only.py's DOCS_PATTERNS must equal generate-builds.yml's push paths-ignore: "
        "a docs-only PR and a docs-only push to main must skip the same builds"
    )


@pytest.mark.parametrize(
    "paths, docs_only",
    [
        (["README.md"], True),
        (["docs/BUILDING.md", "docs/supportedHashes.json"], True),
        (["games/oot/README.md", ".claude/worker-prompts.md"], True),
        (["LICENSE"], True),
        (["LICENSE.txt", ".gitignore"], True),
        (["docs.md"], True),
        # Not docs-only.
        ([], False),
        (["README.md", "src/common/a.c"], False),
        (["src/common/a.c"], False),
        ([".github/workflows/generate-builds.yml"], False),
        ([".github/scripts/classify-docs-only.py"], False),
        (["libultraship"], False),
        (["LICENSES/third_party.c"], False),
        (["games/oot/.gitignore"], False),
        (["src/docs/x.c"], False),
        (["src/x.md.c"], False),
        (["README.MD"], False),
        (['"docs/\\303\\251.md"'], False),
    ],
)
def test_classifier_table(paths, docs_only):
    assert _classifier().classify(paths) is docs_only


def test_classifier_refuses_unsupported_pattern_syntax():
    with pytest.raises(ValueError):
        _classifier().pattern_to_regex("docs/?.md")


@pytest.mark.parametrize(
    "stdin, expected",
    [
        ("README.md\0docs/a.md\0", "true"),
        ("README.md\0src/common/a.c\0", "false"),
        ("", "false"),
    ],
)
def test_classifier_cli_writes_github_output(tmp_path, stdin, expected):
    output = tmp_path / "github_output"
    proc = subprocess.run(
        [sys.executable, str(CLASSIFIER), "-z", "--github-output", str(output)],
        input=stdin,
        capture_output=True,
        text=True,
    )
    assert proc.returncode == 0, proc.stdout + proc.stderr
    assert output.read_text(encoding="utf-8") == f"docs_only={expected}\n"


# --- concurrency: no run on main, a release branch or a tag is cancelled in progress (#1377) ----
#
# GitHub cancels by group name, across every workflow in the repository, and it
# reads `cancel-in-progress` from the NEW run: a new run with it true cancels
# the run in progress in its group. So the lock evaluates every concurrency
# block (workflow and job level) for every sample run its workflow fires on, and
# checks every pair of runs both ways.

# Two runs of each kind, A and B, which differ only in sha and run id: a pair of
# one kind asks whether a second such run cancels the first, a pair of two kinds
# asks the same across events and refs.
_RUN_KINDS = (
    ("pull_request", "refs/pull/101/merge"),
    ("pull_request", "refs/pull/102/merge"),
    ("push", "refs/heads/main"),
    ("push", "refs/heads/release/v0.4.4-alpha"),
    ("push", "refs/tags/v0.4.4-alpha"),
    ("push", "refs/heads/claude/lane-a"),
    ("workflow_dispatch", "refs/heads/main"),
    ("workflow_dispatch", "refs/tags/v0.4.4-alpha"),
    ("workflow_dispatch", "refs/heads/claude/lane-a"),
)


def _sample_run(event, ref, copy):
    serial = 2 * _RUN_KINDS.index((event, ref)) + "AB".index(copy) + 1
    pull = re.fullmatch(r"refs/pull/(\d+)/merge", ref)
    ref_name = re.sub(r"^refs/(heads/|tags/|pull/)", "", ref)
    return {
        "label": f"{event} on {ref} (run {copy})",
        # Context names are matched ignoring case, so the keys are lower case.
        "context": {
            "github.event_name": event,
            "github.ref": ref,
            "github.ref_name": ref_name,
            "github.sha": f"{serial:040x}",
            "github.run_id": str(9000 + serial),
            "github.run_number": str(serial),
            "github.head_ref": f"claude/pr-{pull.group(1)}" if pull else "",
            "github.base_ref": "main" if pull else "",
            "github.event.repository.default_branch": "main",
            "github.event.pull_request.number": int(pull.group(1)) if pull else None,
        },
    }


def _sample_runs():
    return [_sample_run(event, ref, copy) for event, ref in _RUN_KINDS for copy in "AB"]


def _protected(run):
    """A run that must never be cancelled in progress: one on main, a release branch or a tag."""
    ref = run["context"]["github.ref"]
    return ref == "refs/heads/main" or ref.startswith("refs/heads/release/") or ref.startswith("refs/tags/")


_EXPR_TOKEN = re.compile(
    r"\s*(?:(?P<string>'(?:[^']|'')*')|(?P<op>==|!=|&&|\|\||[!(),])|(?P<number>\d+)"
    r"|(?P<name>[A-Za-z_][\w-]*(?:\.[A-Za-z_][\w-]*)*))"
)


def _text(value):
    """A value as GitHub splices it into text."""
    if value is None:
        return ""
    if isinstance(value, bool):
        return "true" if value else "false"
    return str(value)


def _truthy(value):
    return value not in (None, False, 0, "")


def _same(left, right):
    if isinstance(left, str) and isinstance(right, str):
        # GitHub compares strings ignoring case.
        return left.lower() == right.lower()
    if type(left) is type(right):
        return left == right
    raise ValueError(f"the concurrency lock does not compare {left!r} with {right!r}")


class _Expression:
    """One GitHub Actions expression, for the subset a concurrency block needs: string, number, true, false and
    null literals, contexts, ==, !=, !, &&, ||, parentheses, startsWith, endsWith and contains. Anything else
    raises ValueError, so the lock never guesses what a run would do."""

    FUNCTIONS = {
        "startswith": lambda text, prefix: _text(text).lower().startswith(_text(prefix).lower()),
        "endswith": lambda text, suffix: _text(text).lower().endswith(_text(suffix).lower()),
        "contains": lambda text, part: _text(part).lower() in _text(text).lower(),
    }

    def __init__(self, text, context):
        self.text = text.strip()
        self.context = context
        self.tokens = []
        pos = 0
        while pos < len(self.text):
            match = _EXPR_TOKEN.match(self.text, pos)
            if not match:
                self._fail()
            self.tokens.append((match.lastgroup, match.group(match.lastgroup)))
            pos = match.end()
        self.pos = 0

    def evaluate(self):
        value = self._or()
        if self.pos != len(self.tokens):
            self._fail()
        return value

    def _fail(self):
        raise ValueError(f"the concurrency lock cannot read ${{{{ {self.text} }}}}")

    def _peek(self):
        return self.tokens[self.pos] if self.pos < len(self.tokens) else (None, None)

    def _take(self, op=None):
        token = self._peek()
        if token[0] is None or (op is not None and token != ("op", op)):
            self._fail()
        self.pos += 1
        return token

    def _or(self):
        value = self._and()
        while self._peek() == ("op", "||"):
            self._take()
            right = self._and()
            value = value if _truthy(value) else right
        return value

    def _and(self):
        value = self._compare()
        while self._peek() == ("op", "&&"):
            self._take()
            right = self._compare()
            value = right if _truthy(value) else value
        return value

    def _compare(self):
        value = self._unary()
        while self._peek() in (("op", "=="), ("op", "!=")):
            op = self._take()[1]
            same = _same(value, self._unary())
            value = same if op == "==" else not same
        return value

    def _unary(self):
        # `!` binds tighter than == and != in GitHub's grammar.
        if self._peek() == ("op", "!"):
            self._take()
            return not _truthy(self._unary())
        return self._primary()

    def _primary(self):
        kind, text = self._take()
        if (kind, text) == ("op", "("):
            value = self._or()
            self._take(")")
            return value
        if kind == "string":
            return text[1:-1].replace("''", "'")
        if kind == "number":
            return int(text)
        if kind != "name":
            self._fail()
        lowered = text.lower()
        if lowered in ("true", "false"):
            return lowered == "true"
        if lowered == "null":
            return None
        if self._peek() == ("op", "("):
            function = self.FUNCTIONS.get(lowered)
            if function is None:
                raise ValueError(f"the concurrency lock does not know the function {text}()")
            self._take("(")
            args = [self._or()]
            while self._peek() == ("op", ","):
                self._take()
                args.append(self._or())
            self._take(")")
            if len(args) != 2:
                self._fail()
            return function(*args)
        if lowered.startswith("inputs."):
            # Dispatch inputs: empty on every other event, and the samples give none.
            return self.context.get(lowered, "")
        if lowered not in self.context:
            raise ValueError(f"the concurrency lock has no sample value for `{text}`; add one to _sample_run")
        return self.context[lowered]


_INTERPOLATION = re.compile(r"\$\{\{(.*?)\}\}", re.DOTALL)


def _evaluate_field(value, context):
    """A concurrency field as GitHub reads it: a YAML scalar as it is, a lone ${{ }} as that expression's value,
    and text with ${{ }} parts as text with each part's value spliced in."""
    if not isinstance(value, str):
        return value
    text = value.strip()
    parts = list(_INTERPOLATION.finditer(text))
    if len(parts) == 1 and parts[0].span() == (0, len(text)):
        return _Expression(parts[0].group(1), context).evaluate()
    spliced = _INTERPOLATION.sub(lambda m: _text(_Expression(m.group(1), context).evaluate()), text)
    if "${{" in spliced:
        raise ValueError(f"the concurrency lock found an unclosed expression in {value!r}")
    return spliced


def _concurrency_of(block, context):
    """(group, cancel-in-progress) of one concurrency block for one run. The group is lower-cased: GitHub matches
    group names ignoring case."""
    if isinstance(block, str):
        block = {"group": block}
    group = _evaluate_field(block["group"], context)
    cancel = _evaluate_field(block.get("cancel-in-progress", False), context)
    if not isinstance(cancel, bool):
        raise ValueError(f"cancel-in-progress {block.get('cancel-in-progress')!r} gave {cancel!r}, not a boolean")
    return _text(group).lower(), cancel


def _filter_regex(pattern):
    """A branch or tag filter pattern as a regex: `*` stays inside one path segment, `**` crosses segments. The
    rest of GitHub's filter syntax is refused rather than guessed at."""
    if any(c in pattern for c in "?+[]!"):
        raise ValueError(f"the concurrency lock does not read the filter pattern {pattern!r}")
    return "".join(
        ".*" if part == "**" else "[^/]*" if part == "*" else re.escape(part)
        for part in re.split(r"(\*\*|\*)", pattern)
    )


def _filter_list(config, key):
    values = config.get(key)
    return [values] if isinstance(values, str) else values


def _push_fires(config, ref):
    """Whether a push trigger with these filters runs for a push of `ref`. Path filters are ignored: the run may
    happen, so it counts."""
    config = config or {}
    for kind, other, prefix in (("branches", "tags", "refs/heads/"), ("tags", "branches", "refs/tags/")):
        if not ref.startswith(prefix):
            continue
        name = ref[len(prefix) :]
        include, ignore = _filter_list(config, kind), _filter_list(config, f"{kind}-ignore")
        if include is not None:
            return any(re.fullmatch(_filter_regex(p), name) for p in include)
        if ignore is not None:
            return not any(re.fullmatch(_filter_regex(p), name) for p in ignore)
        # Filters for the other kind of ref only: GitHub never runs the workflow for this kind.
        return config.get(other) is None and config.get(f"{other}-ignore") is None
    return False


def _fires(workflow, run):
    on = _triggers(workflow)
    event = run["context"]["github.event_name"]
    if event not in on:
        return False
    return _push_fires(on[event], run["context"]["github.ref"]) if event == "push" else True


def _run_context(workflow, file_name, run):
    # github.workflow is the workflow's name, or its path when it has none.
    name = workflow.get("name") or f".github/workflows/{file_name}"
    return dict(run["context"], **{"github.workflow": str(name)})


def _cancellation_violations(workflows):
    """Every way one sample run could cancel, in progress, a run on main, a release branch or a tag, as text."""
    seats = []
    for file_name, workflow in workflows:
        workflow = workflow or {}
        blocks = [(file_name, workflow["concurrency"])] if "concurrency" in workflow else []
        blocks += [
            (f"{file_name} job {job_id}", job["concurrency"])
            for job_id, job in (workflow.get("jobs") or {}).items()
            if "concurrency" in (job or {})
        ]
        for where, block in blocks:
            for run in _sample_runs():
                if _fires(workflow, run):
                    group, cancel = _concurrency_of(block, _run_context(workflow, file_name, run))
                    seats.append((where, run, group, cancel))
    found = []
    for new_where, new_run, group, cancel in seats:
        if not cancel:
            continue
        for old_where, old_run, old_group, _ in seats:
            if (old_where, old_run["label"]) == (new_where, new_run["label"]):
                continue
            if old_group == group and _protected(old_run):
                found.append(
                    f"{new_run['label']} [{new_where}] cancels {old_run['label']} [{old_where}] (group {group!r})"
                )
    return found


def test_no_run_on_main_a_release_branch_or_a_tag_is_cancelled_in_progress():
    violations = _cancellation_violations((path.name, _load(path)) for path in _workflow_files())
    assert not violations, (
        "#1377: these runs would cancel, in progress, a run on main, a release branch or a tag:\n  "
        + "\n  ".join(violations)
        + "\nMain's run is the only one that saves the macOS and Windows build caches, and a cold macOS build takes "
        "about 4.5 hours, so a main run that is cancelled never writes them. Use `cancel-in-progress: "
        "${{ github.event_name == 'pull_request' }}` with a group that names the event and the ref"
    )


def _generate_builds_concurrency(event, ref, copy="A"):
    workflow = _load(GENERATE_BUILDS)
    run = _sample_run(event, ref, copy)
    return _concurrency_of(workflow["concurrency"], _run_context(workflow, GENERATE_BUILDS.name, run))


def test_generate_builds_still_cancels_a_superseded_pull_request_run():
    first = _generate_builds_concurrency("pull_request", "refs/pull/101/merge", "A")
    newer = _generate_builds_concurrency("pull_request", "refs/pull/101/merge", "B")
    other = _generate_builds_concurrency("pull_request", "refs/pull/102/merge", "A")
    assert newer[1] is True, "#1377: a newer push to a pull request must still cancel the older head's run"
    assert newer[0] == first[0], "#1377: two runs of one pull request must share a group, or the cancel finds nothing"
    assert other[0] != first[0], "two pull requests must never share a group: one would cancel the other's run"


def test_generate_builds_queues_main_pushes_and_keeps_dispatches_apart():
    first = _generate_builds_concurrency("push", "refs/heads/main", "A")
    newer = _generate_builds_concurrency("push", "refs/heads/main", "B")
    assert newer == first and newer[1] is False, (
        "#1377: pushes to main must share one group and never cancel: they queue, so main builds one commit at a "
        f"time and the run in progress always finishes and saves its caches. Found {first} then {newer}"
    )
    pushes = {_generate_builds_concurrency(event, ref)[0] for event, ref in _RUN_KINDS if event == "push"}
    for event, ref in _RUN_KINDS:
        if event == "workflow_dispatch":
            group = _generate_builds_concurrency(event, ref)[0]
            assert group not in pushes, (
                f"#1377: a dispatch on {ref} shares group {group!r} with a push. GitHub keeps one pending run per "
                "group, so a merge to main could replace a queued release dispatch"
            )


# A workflow that runs on everything the lock protects, and on pull requests.
_CONCURRENCY_MUTANT = """\
name: Mutant
on:
  push:
    branches: [main, 'release/v*']
    tags: ['v*']
  pull_request:
  workflow_dispatch:
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - run: 'true'
"""


@pytest.mark.parametrize(
    "group, cancel, cancels_a_protected_run",
    [
        # Before #1377: every merge cancelled the running main build.
        ("${{ github.workflow }}-${{ github.ref }}", True, True),
        # One group for every ref: a pull request's push cancels the running main build.
        ("${{ github.workflow }}", "${{ github.event_name == 'pull_request' }}", True),
        # Spares main pushes, but a dispatch or a second push of a tag still cancels.
        ("${{ github.workflow }}-${{ github.ref }}", "${{ github.ref != 'refs/heads/main' }}", True),
        # The fix, and shapes that are just as safe.
        (
            "${{ github.workflow }}-${{ github.event_name }}-${{ github.ref }}",
            "${{ github.event_name == 'pull_request' }}",
            False,
        ),
        ("${{ github.workflow }}-${{ github.ref }}", "${{ github.event_name == 'pull_request' }}", False),
        ("${{ github.workflow }}-${{ github.ref }}", "${{ startsWith(github.ref, 'refs/pull/') }}", False),
        ("${{ github.workflow }}-${{ github.ref }}", False, False),
        ("${{ github.workflow }}-${{ github.event.pull_request.number || github.ref }}", True, True),
    ],
)
def test_cancellation_lock_catches_runs_that_cancel_main_or_a_tag(group, cancel, cancels_a_protected_run):
    workflow = yaml.safe_load(_CONCURRENCY_MUTANT)
    workflow["concurrency"] = {"group": group, "cancel-in-progress": cancel}
    found = _cancellation_violations([("mutant.yml", workflow)])
    assert bool(found) == cancels_a_protected_run, (
        f"group {group!r} with cancel-in-progress {cancel!r}: expected "
        f"{'a cancelled' if cancels_a_protected_run else 'no cancelled'} protected run, the lock found {found}"
    )


def test_cancellation_lock_reads_job_level_concurrency():
    workflow = yaml.safe_load(_CONCURRENCY_MUTANT)
    workflow["jobs"]["build"]["concurrency"] = {"group": "macos-${{ github.ref }}", "cancel-in-progress": True}
    found = _cancellation_violations([("mutant.yml", workflow)])
    expected = "push on refs/heads/main (run B) [mutant.yml job build] cancels push on refs/heads/main (run A)"
    assert any(v.startswith(expected) for v in found), found


def test_cancellation_lock_matches_groups_across_workflows():
    # Groups are repository-wide: a pull-request-only workflow that cancels in
    # a group a main workflow also uses cancels main's run.
    main_workflow = yaml.safe_load(_CONCURRENCY_MUTANT)
    main_workflow["concurrency"] = {"group": "shared", "cancel-in-progress": False}
    # Upper case on purpose: GitHub matches group names ignoring case.
    lint = yaml.safe_load(
        "name: Lint\non:\n  pull_request:\nconcurrency:\n  group: SHARED\n  cancel-in-progress: true\n"
    )
    found = _cancellation_violations([("main.yml", main_workflow), ("lint.yml", lint)])
    expected = (
        "pull_request on refs/pull/101/merge (run A) [lint.yml] cancels push on refs/heads/main (run A) [main.yml]"
    )
    assert any(v.startswith(expected) for v in found), found


@pytest.mark.parametrize(
    "expression, value",
    [
        ("${{ github.event_name == 'PUSH' }}", True),
        ("${{ github.event_name != 'push' }}", False),
        ("${{ !startsWith(github.ref, 'refs/tags/') && github.ref == 'refs/heads/main' }}", True),
        ("${{ !github.head_ref == true }}", True),
        ("${{ github.event.pull_request.number || github.ref }}", "refs/heads/main"),
        ("${{ github.base_ref && 'pr' || 'branch' }}", "branch"),
        ("${{ contains(github.ref, 'HEADS') }}", True),
        ("x-${{ github.ref_name }}-${{ inputs.release_tag }}", "x-main-"),
        ("${{ 'it''s' }}", "it's"),
    ],
)
def test_concurrency_lock_evaluates_expressions(expression, value):
    assert _evaluate_field(expression, _sample_run("push", "refs/heads/main", "A")["context"]) == value


@pytest.mark.parametrize(
    "block",
    [
        {"group": "${{ hashFiles('x') }}"},
        {"group": "${{ matrix.os }}"},
        {"group": "${{ github.ref == 1 }}"},
        {"group": "${{ github.ref ~ 'x' }}"},
        {"group": "${{ github.ref"},
        {"group": "${{ startsWith(github.ref) }}"},
        {"group": "g", "cancel-in-progress": "${{ github.ref }}"},
    ],
)
def test_concurrency_lock_refuses_what_it_cannot_read(block):
    with pytest.raises(ValueError):
        _concurrency_of(block, _sample_run("push", "refs/heads/main", "A")["context"])


def test_cancellation_lock_reads_push_filters_like_github():
    on_branches = {"branches": ["main", "release/v*"]}
    assert _push_fires(on_branches, "refs/heads/main")
    assert _push_fires(on_branches, "refs/heads/release/v0.4.4-alpha")
    assert not _push_fires(on_branches, "refs/heads/release/v1/x"), "`*` must not cross a /"
    assert not _push_fires(on_branches, "refs/tags/v0.4.4-alpha"), "branch filters alone never run for a tag"
    assert _push_fires({"branches": ["claude/**"]}, "refs/heads/claude/a/b")
    assert _push_fires({"tags": ["v*"]}, "refs/tags/v1") and not _push_fires({"tags": ["v*"]}, "refs/heads/main")
    assert _push_fires(None, "refs/heads/anything") and _push_fires(None, "refs/tags/anything")
    assert _push_fires({"paths-ignore": ["docs/**"]}, "refs/heads/main")
    assert not _push_fires({"branches-ignore": ["main"]}, "refs/heads/main")
    with pytest.raises(ValueError):
        _push_fires({"branches": ["!main"]}, "refs/heads/main")
