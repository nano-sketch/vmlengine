@echo off
echo.
echo [1/4] terminating existing engine processes...
taskkill /F /IM vlmengine.exe /T 2>nul

set "MINGW_BIN=C:\Program Files\JetBrains\CLion 2025.3.1.1\bin\mingw\bin"
set PATH=%MINGW_BIN%;%PATH%
if not exist build mkdir build

cd build
cmake -S ../ -B . -G "MinGW Makefiles"
mingw32-make.exe && mingw32-make.exe Shaders

copy /y ..\ul\sdk\bin\*.dll .

if not exist resources mkdir resources
copy /y ..\ul\sdk\resources\* resources\

cd ..
