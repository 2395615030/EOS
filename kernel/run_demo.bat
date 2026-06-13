@echo off
chcp 65001 >nul
cd /d D:\Project\EmotionOS\kernel
echo Running EmotionOS demo (auto-input)...
echo.
(echo help
echo stat
echo horm
echo mem
echo quit) | emotionos.exe
echo.
echo Demo finished.
pause
