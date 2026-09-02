@echo off
setlocal\r
echo ============================================================
echo Calypso16/32 - BUILD + FULL VERBOSE SELF-TEST
echo ============================================================\r
echo Building Calypso16/32...
g++ -std=c++17 -Wall -Wextra -pedantic main.cpp decode.cpp disassembler.cpp execute.cpp m_extension.cpp memory.cpp -o calypso.exe
if errorlevel 1 (
    echo.
    echo BUILD FAILED.
    exit /b 1
)\r
echo.
echo Running full verbose self-test suite...
.\\calypso.exe --self-test
set TEST_RESULT=%errorlevel%\r
echo.
echo ============================================================
if "%TEST_RESULT%"=="0" (
    echo ALL SELF-TESTS PASSED.
) else (
    echo SELF-TEST FAILURES DETECTED.
)
echo ============================================================\r
exit /b %TEST_RESULT%
