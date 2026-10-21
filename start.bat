@echo off
setlocal
title PF-02 - Darm-Akustischer & Ultrametrischer Triaden-Synthesizer

cd /d "%~dp0"

echo ====================================================================
echo   PF-02 · DARM-AKUSTISCHER & ULTRAMETRISCHER TRIADEN-SYNTHESIZER
echo   German Functionalist Laboratory UI & 4-Mode CRT Visualizer
echo   DIN 1451 Mittelschrift · 19" 2U Studio Chassis
echo ====================================================================
echo.

:: 1. Check for Node.js
where node >nul 2>nul
if %errorlevel% equ 0 (
    echo [OK] Node.js runtime detected.
    echo Starting static laboratory server on http://localhost:8080 ...
    echo.
    echo Opening browser to http://localhost:8080 ...
    start "" http://localhost:8080
    echo Press Ctrl+C in this terminal window to shut down the server.
    echo --------------------------------------------------------------------
    node server.js
    goto finished
)

:: 2. Fallback check for Python if Node.js is not present
where python >nul 2>nul
if %errorlevel% equ 0 (
    echo [INFO] Node.js not detected, but Python runtime is available.
    echo Launching Python HTTP server on port 8080 ...
    start "" http://localhost:8080
    if exist "web\index.html" (
        cd web
    )
    python -m http.server 8080
    goto finished
)

:: 3. Neither runtime found
echo [ERROR] Neither Node.js nor Python was found on your system PATH.
echo.
echo To run this synthesizer locally, please install Node.js:
echo    https://nodejs.org/
echo.
pause
exit /b 1

:finished
if %errorlevel% neq 0 (
    echo.
    echo Server process terminated with exit code %errorlevel%.
    pause
)
endlocal
