@echo off
rem verify.cmd <branch> <tag> [flags] [--dry-run] [--keep-going]
rem   flags: u = ui tier + parity lint, i = integration + soak, a = regenerate archives
rem Merges origin/<branch> onto origin/main in the main checkout (%WAVE_MAIN%,
rem default C:/Users/penpen/src/redshipblueship), builds build-cmake, runs the
rem tiers with SDL_WINDOW_NO_ACTIVATION_WHEN_SHOWN=1 and writes
rem <state>/logs/verify-<tag>.log. Needs the gate lock held by <tag>.
rem verify.py loads vcvars64 itself (WAVE_VCVARS overrides the path), so this
rem wrapper only finds Python. Exit 0 pass, 1 fail, 2 usage/lock. See README.md.
setlocal
set "SDL_WINDOW_NO_ACTIVATION_WHEN_SHOWN=1"
call "%~dp0_python.cmd" || exit /b 9
%WAVE_PY% "%~dp0verify.py" %*
exit /b %ERRORLEVEL%
