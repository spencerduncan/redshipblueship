@echo off
rem Sets WAVE_PY to a Python 3 command line for the other wave wrappers.
rem Order: %WAVE_PYTHON% (a full path to python.exe), the py launcher, python.
rem Called with `call`, so the variable lands in the caller's environment.
if defined WAVE_PYTHON (
    set WAVE_PY="%WAVE_PYTHON%"
    exit /b 0
)
where py >nul 2>nul
if not errorlevel 1 (
    set "WAVE_PY=py -3"
    exit /b 0
)
where python >nul 2>nul
if not errorlevel 1 (
    set "WAVE_PY=python"
    exit /b 0
)
echo FAIL wave: no Python 3 on PATH; set WAVE_PYTHON to python.exe 1>&2
exit /b 9
