@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
set "VCPKG_ROOT=C:/Users/penpen/src/redshipblueship/vcpkg"
set "PATH=%PATH%;C:\Users\penpen\AppData\Local\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe"
cd /d "C:\Users\penpen\src\redshipblueship\.claude\worktrees\wf_4221f5cf-c9d-3"
%*
