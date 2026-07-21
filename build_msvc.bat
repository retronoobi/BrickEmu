@echo off
setlocal
pushd "%~dp0"

where cl >nul 2>nul
if errorlevel 1 (
    echo ERROR: MSVC compiler not found.
    echo Run this script from an "x64 Native Tools Command Prompt for VS 2022".
    popd
    exit /b 1
)

if not exist build mkdir build

cl /nologo /std:c++17 /O2 /EHsc /LD /I. /Fo:build\ ^
    libretro.cpp ht943.cpp em73000.cpp spl0x.cpp ks56cx2x.cpp ^
    /link /DEF:brickemu_libretro.def ^
    /OUT:build\brickemu_libretro.dll ^
    /IMPLIB:build\brickemu_libretro.lib

set "BUILD_RESULT=%ERRORLEVEL%"
popd
exit /b %BUILD_RESULT%
