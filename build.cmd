@echo off
REM ============================================
REM EmotionOS Build Script
REM Usage: build [debug|release|clean|run|test]
REM ============================================
setlocal enabledelayedexpansion

set ROOT=%~dp0
set SRC=%ROOT%src
set INC=%ROOT%include
set BUILD=%ROOT%build
set CXX=g++
set CXXFLAGS=-std=c++17 -I%INC% -I%SRC%

if not exist "%BUILD%" mkdir "%BUILD%"

if /i "%1"=="clean" goto :clean
if /i "%1"=="release" goto :release
if /i "%1"=="run" goto :run
if /i "%1"=="test" goto :test
goto :debug

:release
set CXXFLAGS=%CXXFLAGS% -O2 -s
echo [BUILD] Release mode...
%CXX% %CXXFLAGS% %SRC%kernel.cpp %SRC%main.cpp -o %BUILD%emotionos.exe -static
if errorlevel 1 (echo [FAIL] Build failed & exit /b 1)
echo [OK] buildemotionos.exe
goto :eof

:debug
set CXXFLAGS=%CXXFLAGS% -O0 -g
echo [BUILD] Debug mode...
%CXX% %CXXFLAGS% %SRC%kernel.cpp %SRC%main.cpp -o %BUILD%emotionos_dbg.exe -static
if errorlevel 1 (echo [FAIL] Build failed & exit /b 1)
echo [OK] buildemotionos_dbg.exe
goto :eof

:run
if not exist "%BUILD%emotionos.exe" (
  call :release
)
echo [RUN] EmotionOS...
%BUILD%emotionos.exe
goto :eof

:test
call :debug
echo [TEST] Running test suite...
if exist "%ROOT%testsemotionos_test.exe" (
  %ROOT%testsemotionos_test.exe
) else (
  g++ -std=c++17 -O0 -I%INC% -I%SRC% %SRC%kernel.cpp %ROOT%tests	est_suite.cpp -o %BUILD%	est_suite.exe -static
  %BUILD%	est_suite.exe
)
goto :eof

:clean
echo [CLEAN] Removing build artifacts...
if exist "%BUILD%" rmdir /s /q "%BUILD%"
mkdir "%BUILD%"
echo [OK] Clean
goto :eof

:help
echo Usage: build [debug^|release^|clean^|run^|test]
goto :eof
