@echo off
rem gate-unlock.cmd tag=<TAG> | owner=<TAG> | <TAG>  [--force] [--dry-run]
rem Releases the gate merge lock. Refuses a lock held by another tag, or one
rem whose recorded pid (a running verify) is alive, unless --force.
rem Exit 0 released or already free, 1 refused, 2 usage. See README.md.
setlocal
call "%~dp0_python.cmd" || exit /b 9
%WAVE_PY% "%~dp0gatelock.py" release %*
exit /b %ERRORLEVEL%
