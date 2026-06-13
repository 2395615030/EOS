@echo off
echo ====================================
echo GCC Build: EmotionOS Phase 1 MVP
echo ====================================
g++ -std=c++20 -O2 -o emotionos_mvp_gcc.exe src\main.cpp
if %errorlevel% equ 0 (
    echo OK: emotionos_mvp_gcc.exe
) else (
    echo FAILED
    pause
    exit /b 1
)
echo.
emotionos_mvp_gcc.exe
pause
