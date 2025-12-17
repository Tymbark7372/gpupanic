@echo off
echo.
echo  +------------------------------------------+
echo  ^|       gpupanic build script              ^|
echo  +------------------------------------------+
echo.

where cl >nul 2>&1
if %errorlevel% neq 0 (
    echo  [!] MSVC not found in PATH
    echo  [*] Run from "Developer Command Prompt for VS"
    echo      or "x64 Native Tools Command Prompt"
    exit /b 1
)

echo  [*] Compiling gpupanic.c...

cl /nologo /O2 /W3 gpupanic.c /Fe:gpupanic.exe /link d3d11.lib dxgi.lib dxguid.lib d3dcompiler.lib user32.lib advapi32.lib

if %errorlevel% equ 0 (
    echo.
    echo  [+] Build successful: gpupanic.exe
    echo.
    echo  Usage:
    echo    gpupanic.exe --list       List GPUs
    echo    gpupanic.exe --safe       Safe test mode
    echo    gpupanic.exe --medium     Medium hang
    echo    gpupanic.exe --nuclear    Full GPU panic
    echo.
    del /q *.obj 2>nul
) else (
    echo.
    echo  [!] Build failed
)

