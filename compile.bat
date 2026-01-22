@echo off
echo Compiling here.exe...
g++ -o here.exe .\main.cpp -lgdi32 -ldwmapi -lshcore -lcomctl32 -luxtheme

if %errorlevel% neq 0 (
    echo Compilation failed!
    pause
    exit /b %errorlevel%
)

echo Compilation successful!
pause
