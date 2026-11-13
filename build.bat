@echo off
rem SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
rem KamoteKeys: one codebase, three firmwares.
rem   build.bat smk37-pro   [--package^|--web]
rem   build.bat smk37-elite [--package^|--web]
rem   build.bat fm1         [--package^|--web]
setlocal
if /i "%~1"=="smk37-pro" (set "DEV=smk37" & set "MODEL=pro" & goto build)
if /i "%~1"=="smk37-elite" (set "DEV=smk37" & set "MODEL=elite" & goto build)
if /i "%~1"=="fm1" (set "DEV=fm1" & set "MODEL=" & goto build)
echo Use: build.bat smk37-pro^|smk37-elite^|fm1 [--package^|--web]
exit /b 2
:build
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\Sync-Core.ps1" -Device %DEV%
if errorlevel 1 exit /b %errorlevel%
rem Run the device build from its own folder: a .bat called by a quoted full
rem path can see the wrong %~dp0 (cmd quirk), so change directory first.
pushd "%~dp0devices\%DEV%" || exit /b 1
call build.bat %MODEL% %2
set "RESULT=%errorlevel%"
popd
exit /b %RESULT%
