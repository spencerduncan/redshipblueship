"""Shared fixtures for the wave toolkit tests: import paths, dead pids and
throwaway git repositories (a bare "origin" plus a clone standing in for the
main checkout). Nothing here touches the real main checkout or build tree."""

from __future__ import annotations

import importlib.util
import os
import shutil
import stat
import subprocess
import sys
import tempfile
from pathlib import Path
from unittest import mock

WAVE_DIR = Path(__file__).resolve().parents[1]
if str(WAVE_DIR) not in sys.path:
    sys.path.insert(0, str(WAVE_DIR))

import wavelib  # noqa: E402,F401  (re-exported for the tests)


def load_script(name: str):
    """Import a hyphenated CLI script (lane-manifest.py) as a module."""
    module_name = "wave_" + name.replace("-", "_").removesuffix(".py")
    if module_name in sys.modules:
        return sys.modules[module_name]
    spec = importlib.util.spec_from_file_location(module_name, WAVE_DIR / name)
    module = importlib.util.module_from_spec(spec)
    # dataclasses resolve string annotations through sys.modules[__module__].
    sys.modules[module_name] = module
    spec.loader.exec_module(module)
    return module


def dead_pid() -> int:
    """The pid of a process that has already exited."""
    proc = subprocess.Popen([sys.executable, "-c", "pass"])
    proc.wait()
    return proc.pid


# Applied to os.environ for the life of a GitFixture, so wavelib's own git
# calls see the same settings as the fixture's: an identity for merges, LF on
# disk regardless of the host's core.autocrlf, and no system config.
GIT_ENV = {
    "GIT_AUTHOR_NAME": "wave-test",
    "GIT_AUTHOR_EMAIL": "wave-test@localhost",
    "GIT_COMMITTER_NAME": "wave-test",
    "GIT_COMMITTER_EMAIL": "wave-test@localhost",
    "GIT_CONFIG_NOSYSTEM": "1",
    "GIT_CONFIG_COUNT": "2",
    "GIT_CONFIG_KEY_0": "core.autocrlf",
    "GIT_CONFIG_VALUE_0": "false",
    "GIT_CONFIG_KEY_1": "init.defaultBranch",
    "GIT_CONFIG_VALUE_1": "main",
}


def run_git(repo: Path, *args: str) -> str:
    proc = subprocess.run(["git", "-C", str(repo), *args], capture_output=True, text=True)
    if proc.returncode != 0:
        raise AssertionError(f"git {' '.join(args)} failed: {proc.stderr}")
    return proc.stdout.strip()


def commit(repo: Path, files: dict, message: str) -> str:
    for rel, text in files.items():
        path = repo / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8", newline="\n")
        run_git(repo, "add", "--", rel)
    run_git(repo, "commit", "-q", "-m", message)
    return run_git(repo, "rev-parse", "HEAD")


class GitFixture:
    """origin.git (bare) and main/ (its clone, on main), seeded with the four
    build stamps so the restore logic sees the same paths as the real tree.
    `other/` is a second clone used to move origin/main underneath main/."""

    def __init__(self):
        self._env = mock.patch.dict(os.environ, GIT_ENV)
        self._env.start()
        root = Path(tempfile.mkdtemp(prefix="wave-git-"))
        self.root = root
        self.origin = root / "origin.git"
        self.main = root / "main"
        self.other = root / "other"
        run_git(root, "init", "-q", "--bare", str(self.origin))
        run_git(root, "clone", "-q", str(self.origin), str(self.main))
        run_git(self.main, "checkout", "-q", "-B", "main")
        seed = {stamp: f"/* {stamp} */\n" for stamp in wavelib.BUILD_STAMPS}
        seed["README.md"] = "seed\n"
        self.base = commit(self.main, seed, "seed")
        run_git(self.main, "push", "-q", "-u", "origin", "main")
        run_git(root, "clone", "-q", str(self.origin), str(self.other))

    def advance_origin(self, files: dict, message: str) -> str:
        """A commit pushed to origin/main from the second clone."""
        run_git(self.other, "pull", "-q", "--ff-only")
        sha = commit(self.other, files, message)
        run_git(self.other, "push", "-q", "origin", "main")
        return sha

    def cleanup(self):
        try:
            rmtree(self.root)
        finally:
            self._env.stop()


def rmtree(path: Path) -> None:
    """shutil.rmtree that clears the read-only bit git sets on Windows objects."""

    def clear_and_retry(func, target, *_):
        os.chmod(target, stat.S_IWRITE)
        func(target)

    if sys.version_info >= (3, 12):
        shutil.rmtree(path, onexc=clear_and_retry)
    else:
        shutil.rmtree(path, onerror=clear_and_retry)
