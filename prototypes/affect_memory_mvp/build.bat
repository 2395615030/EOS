@echo off
echo ====================================
echo Building EmotionOS Phase 1 MVP
echo ====================================

echo [1/2] Checking for GCC...
where g++ >nul 2>&1
if %errorlevel% equ 0 (
    echo Using GCC...
    g++ -std=c++20 -O2 -o emotionos_mvp_gcc.exe src\main.cpp
    if %errorlevel% equ 0 (
        echo GCC build successful! emotionos_mvp_gcc.exe
    ) else (
        echo GCC build failed.
    )
) else (
    echo GCC not found, skipping.
)

echo.
echo [2/2] Checking for MSVC...
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" >nul 2>&1
if %errorlevel% equ 0 (
    echo Using MSVC...
    cl /std:c++20 /O2 /EHsc /Fe"emotionos_mvp.exe" src\main.cpp >nul 2>&1
    if %errorlevel% equ 0 (
        echo MSVC build successful! emotionos_mvp.exe
    ) else (
        echo MSVC build failed.
    )
) else (
    echo MSVC not found, skipping.
)

echo.
echo Done.
pause
