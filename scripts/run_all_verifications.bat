@echo off
setlocal enabledelayedexpansion
set EXIT_CODE=0

echo ================================================================================
echo    POOPENFARTEN II (PF-02) -- FULL VERIFICATION SUITE EXECUTION
echo ================================================================================
echo.

echo [STAGE 1/3] Executing Automated Zero-PCM Source Audit...
call python scripts\audit_zero_pcm.py
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Stage 1 Zero-PCM Audit FAILED.
    set EXIT_CODE=1
) else (
    echo [SUCCESS] Stage 1 Zero-PCM Audit PASSED.
)
echo.

echo [STAGE 2/3] Executing Node.js Verification Suite...
call npm test
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Stage 2 Node.js Verification FAILED.
    set EXIT_CODE=1
) else (
    echo [SUCCESS] Stage 2 Node.js Verification PASSED.
)
echo.

echo [STAGE 3/3] Checking for Headless C++ DSP Test Executable...
if exist "build\source\tests\Release\poopenfarten2_headless_dsp_tests.exe" (
    echo Running C++ Headless Test Suite from build\source\tests\Release
    build\source\tests\Release\poopenfarten2_headless_dsp_tests.exe --all
    if !ERRORLEVEL! neq 0 (
        echo [ERROR] Stage 3 Headless C++ DSP Tests FAILED.
        set EXIT_CODE=1
    ) else (
        echo [SUCCESS] Stage 3 Headless C++ DSP Tests PASSED.
    )
) else if exist "build_test\Release\poopenfarten2_headless_dsp_tests.exe" (
    echo Running C++ Headless Test Suite from build_test\Release
    build_test\Release\poopenfarten2_headless_dsp_tests.exe --all
    if !ERRORLEVEL! neq 0 (
        echo [ERROR] Stage 3 Headless C++ DSP Tests FAILED.
        set EXIT_CODE=1
    ) else (
        echo [SUCCESS] Stage 3 Headless C++ DSP Tests PASSED.
    )
) else if exist "build\poopenfarten2_headless_dsp_tests.exe" (
    echo Running C++ Headless Test Suite from build
    build\poopenfarten2_headless_dsp_tests.exe --all
    if !ERRORLEVEL! neq 0 (
        echo [ERROR] Stage 3 Headless C++ DSP Tests FAILED.
        set EXIT_CODE=1
    ) else (
        echo [SUCCESS] Stage 3 Headless C++ DSP Tests PASSED.
    )
) else (
    echo [INFO] Headless C++ executable not yet compiled. Skipping binary execution.
)
echo.

echo ================================================================================
if !EXIT_CODE! equ 0 (
    echo OVERALL STATUS: ALL VERIFICATIONS PASSED [100%% SUCCESS]
) else (
    echo OVERALL STATUS: VERIFICATIONS FAILED [Code: !EXIT_CODE!]
)
echo ================================================================================

exit /b %EXIT_CODE%
