@echo off
rem gate-lock.cmd tag=<TAG> [pr=<N>] [run=<WORKFLOW RUN ID>] [--wait=<MIN>] [--dry-run]
rem Takes the gate merge lock (an atomic mkdir; owner.txt records tag, pr,
rem run, pid, acquired and heartbeat). Exit 0 locked, 1 held, 2 usage.
rem See README.md in this directory.
setlocal
call "%~dp0_python.cmd" || exit /b 9
%WAVE_PY% "%~dp0gatelock.py" acquire %*
exit /b %ERRORLEVEL%
