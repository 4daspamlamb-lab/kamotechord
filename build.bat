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
call "%~dp0devices\%DEV%\build.bat" %MODEL% %2
exit /b %errorlevel%
