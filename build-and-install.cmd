@echo off
setlocal EnableExtensions EnableDelayedExpansion
title ilanaSynth - build and install

rem ==========================================================================
rem  Builds the ilanaSynth VST3 + standalone (Release) and installs them:
rem    VST3       -> C:\Program Files\Common Files\VST3\ilanaSynth.vst3
rem    Standalone -> C:\Program Files\ilanaSynth\ilanaSynth.exe
rem
rem  Needs: Visual Studio 2022 (or Build Tools) with "Desktop development
rem  with C++", Git, and internet the first time (JUCE is downloaded).
rem  CMake is used from PATH, or the copy bundled with Visual Studio.
rem ==========================================================================

rem --- Run as administrator (installing into Program Files needs it) ---------
net session >nul 2>&1
if errorlevel 1 (
    echo Asking for administrator rights to install the plugin...
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

cd /d "%~dp0"

rem --- Find Visual Studio ----------------------------------------------------
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSPATH="
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
)
if not defined VSPATH (
    echo.
    echo ERROR: Visual Studio 2022 with the C++ tools was not found.
    echo Install "Visual Studio 2022 Community" or "Build Tools for Visual Studio 2022"
    echo and tick "Desktop development with C++":
    echo     https://visualstudio.microsoft.com/downloads/
    goto :fail
)
echo Visual Studio: %VSPATH%

rem --- Find CMake ------------------------------------------------------------
set "CMAKE=cmake"
where cmake >nul 2>&1
if errorlevel 1 (
    set "CMAKE=%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if not exist "!CMAKE!" (
        echo.
        echo ERROR: CMake was not found. Install it from https://cmake.org/download/
        echo or add the "C++ CMake tools for Windows" component in Visual Studio.
        goto :fail
    )
)
echo CMake: %CMAKE%

rem --- Git (JUCE is fetched with it) -----------------------------------------
where git >nul 2>&1
if errorlevel 1 (
    echo.
    echo ERROR: Git was not found. Install it from https://git-scm.com/download/win
    goto :fail
)

rem --- Configure and build ---------------------------------------------------
echo.
echo === Configuring (the first run downloads JUCE, this can take a while) ===
"%CMAKE%" -B build -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    echo.
    echo Configure failed. If you changed Visual Studio versions, delete the
    echo "build" folder next to this script and run it again.
    goto :fail
)

echo.
echo === Building the VST3 and standalone (Release) ===
"%CMAKE%" --build build --config Release --target ilanaSynth_VST3 ilanaSynth_Standalone --parallel
if errorlevel 1 goto :fail

set "VST3_SRC=%~dp0build\ilanaSynth_artefacts\Release\VST3\ilanaSynth.vst3"
set "APP_SRC=%~dp0build\ilanaSynth_artefacts\Release\Standalone\ilanaSynth.exe"
if not exist "%VST3_SRC%" (
    echo ERROR: build finished but "%VST3_SRC%" is missing.
    goto :fail
)

rem --- Install ---------------------------------------------------------------
set "COMMON=%CommonProgramW6432%"
if not defined COMMON set "COMMON=%CommonProgramFiles%"
set "PROGRAMS=%ProgramW6432%"
if not defined PROGRAMS set "PROGRAMS=%ProgramFiles%"
set "VST3_DST=%COMMON%\VST3\ilanaSynth.vst3"
set "APP_DST=%PROGRAMS%\ilanaSynth"

echo.
echo === Installing the VST3 to "%VST3_DST%" ===
robocopy "%VST3_SRC%" "%VST3_DST%" /MIR /NFL /NDL /NJH /NJS /NP >nul
if errorlevel 8 (
    echo ERROR: could not copy the VST3. Close your DAW (it may be holding the
    echo plugin open^) and run this script again.
    goto :fail
)

if exist "%APP_SRC%" (
    echo === Installing the standalone app to "%APP_DST%" ===
    if not exist "%APP_DST%" mkdir "%APP_DST%"
    copy /y "%APP_SRC%" "%APP_DST%\ilanaSynth.exe" >nul
    if errorlevel 1 (
        echo WARNING: could not copy the standalone app ^(is it running?^).
    ) else (
        powershell -NoProfile -Command "$s=(New-Object -ComObject WScript.Shell).CreateShortcut([Environment]::GetFolderPath('CommonDesktopDirectory')+'\ilanaSynth.lnk'); $s.TargetPath='%APP_DST%\ilanaSynth.exe'; $s.Save()" >nul 2>&1
    )
)

echo.
echo ==========================================================================
echo  Done. ilanaSynth is installed.
echo    VST3:       %VST3_DST%
echo    Standalone: %APP_DST%\ilanaSynth.exe  (shortcut on the desktop)
echo  Rescan plugins in your DAW to pick it up.
echo ==========================================================================
echo.
pause
exit /b 0

:fail
echo.
echo Build/install did not finish. Scroll up for the error.
pause
exit /b 1
