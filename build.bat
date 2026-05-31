@echo off
chcp 65001 >nul 2>nul
setlocal enabledelayedexpansion

:: NOTE: Run this from cmd.exe, not from Git Bash / MSYS2 terminal.
:: In Git Bash use: ./build.sh instead.

set ARCH=x64
set DEBUG=0
set SRCDIR=src
set BUILDDIR=build
set OBJDIR=build\obj

for %%A in (%*) do (
    if /i "%%A"=="x86"   set ARCH=x86
    if /i "%%A"=="x64"   set ARCH=x64
    if /i "%%A"=="debug" set DEBUG=1
    if /i "%%A"=="clean" goto :clean
    if /i "%%A"=="help"  goto :help
    if /i "%%A"=="/?"    goto :help
)

if not exist "%SRCDIR%\tcmg.c" (
    echo.
    echo  ERROR: Run this script from the project root ^(where src\ is^).
    echo.
    pause & exit /b 1
)

set TCMG_VERSION=unknown
for /f "tokens=3 delims= " %%V in ('findstr /c:"#define TCMG_VERSION" "%SRCDIR%\tcmg-globals.h" 2^>nul') do set TCMG_VERSION=%%~V

echo.
echo ==================================================
echo   tcmg v%TCMG_VERSION%  ^|  Windows %ARCH% build
echo ==================================================
echo.

:: ---- Compiler discovery (sequential checks, no loops) ------------------
set CC=
set STRIP=

:: 1. Cross-compile triple in PATH
if "%ARCH%"=="x86" (
    where i686-w64-mingw32-gcc >nul 2>nul
    if !errorlevel! equ 0 set CC=i686-w64-mingw32-gcc& set STRIP=i686-w64-mingw32-strip
) else (
    where x86_64-w64-mingw32-gcc >nul 2>nul
    if !errorlevel! equ 0 set CC=x86_64-w64-mingw32-gcc& set STRIP=x86_64-w64-mingw32-strip
)

:: 2. Plain gcc in PATH
if not defined CC (
    where gcc >nul 2>nul
    if !errorlevel! equ 0 set CC=gcc& set STRIP=strip
)

:: 3. MSYS2 ucrt64
if not defined CC if exist "C:\msys64\ucrt64\bin\gcc.exe" (
    set CC=C:\msys64\ucrt64\bin\gcc.exe
    set STRIP=C:\msys64\ucrt64\bin\strip.exe
    set PATH=C:\msys64\ucrt64\bin;!PATH!
)

:: 4. MSYS2 mingw64
if not defined CC if exist "C:\msys64\mingw64\bin\gcc.exe" (
    set CC=C:\msys64\mingw64\bin\gcc.exe
    set STRIP=C:\msys64\mingw64\bin\strip.exe
    set PATH=C:\msys64\mingw64\bin;!PATH!
)

:: 5. msys2 alt path
if not defined CC if exist "C:\msys2\ucrt64\bin\gcc.exe" (
    set CC=C:\msys2\ucrt64\bin\gcc.exe
    set STRIP=C:\msys2\ucrt64\bin\strip.exe
    set PATH=C:\msys2\ucrt64\bin;!PATH!
)

:: 6. WinLibs / manual unzip to C:\mingw64
if not defined CC if exist "C:\mingw64\bin\gcc.exe" (
    set CC=C:\mingw64\bin\gcc.exe
    set STRIP=C:\mingw64\bin\strip.exe
    set PATH=C:\mingw64\bin;!PATH!
)

:: 7. WinLibs / manual unzip to C:\mingw32
if not defined CC if exist "C:\mingw32\bin\gcc.exe" (
    set CC=C:\mingw32\bin\gcc.exe
    set STRIP=C:\mingw32\bin\strip.exe
    set PATH=C:\mingw32\bin;!PATH!
)

:: 8. Old MinGW
if not defined CC if exist "C:\MinGW\bin\gcc.exe" (
    set CC=C:\MinGW\bin\gcc.exe
    set STRIP=C:\MinGW\bin\strip.exe
    set PATH=C:\MinGW\bin;!PATH!
)

:: 9. TDM-GCC
if not defined CC if exist "C:\TDM-GCC-64\bin\gcc.exe" (
    set CC=C:\TDM-GCC-64\bin\gcc.exe
    set STRIP=C:\TDM-GCC-64\bin\strip.exe
    set PATH=C:\TDM-GCC-64\bin;!PATH!
)

:: 10. Scoop
if not defined CC if exist "%USERPROFILE%\scoop\apps\mingw\current\bin\gcc.exe" (
    set CC=%USERPROFILE%\scoop\apps\mingw\current\bin\gcc.exe
    set STRIP=%USERPROFILE%\scoop\apps\mingw\current\bin\strip.exe
    set PATH=%USERPROFILE%\scoop\apps\mingw\current\bin;!PATH!
)

:: 11. Chocolatey
if not defined CC if exist "C:\ProgramData\chocolatey\lib\mingw\tools\install\mingw64\bin\gcc.exe" (
    set CC=C:\ProgramData\chocolatey\lib\mingw\tools\install\mingw64\bin\gcc.exe
    set STRIP=C:\ProgramData\chocolatey\lib\mingw\tools\install\mingw64\bin\strip.exe
    set PATH=C:\ProgramData\chocolatey\lib\mingw\tools\install\mingw64\bin;!PATH!
)

:: 12. Git for Windows bundled MinGW
if not defined CC if exist "C:\Program Files\Git\mingw64\bin\gcc.exe" (
    set CC=C:\Program Files\Git\mingw64\bin\gcc.exe
    set STRIP=C:\Program Files\Git\mingw64\bin\strip.exe
    set PATH=C:\Program Files\Git\mingw64\bin;!PATH!
)

:: ---- Not found ----------------------------------------------------------
if not defined CC (
    echo  ERROR: MinGW-w64 gcc not found.
    echo.
    echo  Quickest fix: download WinLibs from https://winlibs.com
    echo  Unzip to C:\mingw64  then re-run this script.
    echo.
    echo  Alternatives:
    echo    MSYS2 : https://www.msys2.org
    echo            then: pacman -S mingw-w64-ucrt-x86_64-gcc
    echo    Scoop : scoop install mingw
    echo    Choco : choco install mingw
    echo.
    pause & exit /b 1
)

echo  Compiler : !CC!
"!CC!" --version 2>nul | findstr /i "gcc"
echo.

:: ---- Source files -------------------------------------------------------
set SRCS=tcmg.c tcmg-log.c tcmg-crypto.c tcmg-net.c tcmg-ban.c tcmg-conf.c tcmg-emu.c tcmg-srvid2.c tcmg-webif.c tcmg-webif-common.c tcmg-webif-layout.c tcmg-webif-pages.c tcmg-webif-tvcas.c

:: ---- Flags --------------------------------------------------------------
if "%ARCH%"=="x86" (set ARCH_FLAGS=-march=i686 -m32) else (set ARCH_FLAGS=-march=x86-64 -mtune=generic)

if "%DEBUG%"=="1" (
    set OPT_FLAGS=-Og -g
    set LD_EXTRA=
    echo  Mode: DEBUG
) else (
    set OPT_FLAGS=-Os -ffunction-sections -fdata-sections -fmerge-all-constants -fno-ident -fstack-protector-strong
    set LD_EXTRA=-Wl,--gc-sections -Wl,--strip-all -Wl,--build-id=none
    echo  Mode: RELEASE
)
echo.

set CFLAGS=-std=c11 %OPT_FLAGS% %ARCH_FLAGS% -I%SRCDIR% -DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0601 -D_FORTIFY_SOURCE=2 -Wall -Wextra -Wno-unused-parameter -Wmissing-prototypes -Wstrict-prototypes
set LDFLAGS=-lws2_32 -ladvapi32 -lbcrypt -static -static-libgcc -lpthread %LD_EXTRA%

:: ---- Create dirs --------------------------------------------------------
if not exist "%BUILDDIR%"       mkdir "%BUILDDIR%"
if not exist "%OBJDIR%\%ARCH%" mkdir "%OBJDIR%\%ARCH%"

:: ---- Compile ------------------------------------------------------------
echo --- Compiling ---
echo.
set OBJS=

for %%F in (%SRCS%) do (
    set OBJ=%OBJDIR%\%ARCH%\%%~nF.o
    echo  CC   %SRCDIR%\%%F
    "!CC!" %CFLAGS% -c "%SRCDIR%\%%F" -o "!OBJ!"
    if !errorlevel! neq 0 (
        echo.
        echo  ERROR: Compile failed: %SRCDIR%\%%F
        pause & exit /b 1
    )
    set OBJS=!OBJS! "!OBJ!"
)

:: ---- Link ---------------------------------------------------------------
echo.
echo --- Linking ---
echo.
set OUT=%BUILDDIR%\tcmg_%ARCH%.exe

"!CC!" %CFLAGS% %OBJS% -o "%OUT%" %LDFLAGS%
if !errorlevel! neq 0 (
    echo.
    echo  ERROR: Link failed
    pause & exit /b 1
)

:: ---- Strip --------------------------------------------------------------
if "%DEBUG%"=="0" if defined STRIP (
    if exist "!STRIP!" "!STRIP!" --strip-all "%OUT%" 2>nul
)

:: ---- Done ---------------------------------------------------------------
echo.
echo ==================================================
echo   BUILD SUCCESS
echo ==================================================
echo.
for %%F in ("%OUT%") do (
    set /a KB=%%~zF/1024
    echo   Binary : %OUT%  ^(!KB! KB^)
)
echo.
echo   Usage:  %OUT% -c C:\tcmg\config
echo.
pause
exit /b 0

:clean
echo  Cleaning %BUILDDIR%\...
if exist "%BUILDDIR%" rmdir /s /q "%BUILDDIR%"
echo  Done.
goto :eof

:help
echo.
echo  Usage: build.bat [x64^|x86] [debug] [clean] [help]
echo.
echo  IMPORTANT: Run from cmd.exe, not from Git Bash / MSYS2.
echo  In Git Bash use:  bash build.sh
echo.
echo  Auto-detects gcc in PATH and common locations ^(MSYS2, WinLibs, Scoop...^)
echo  Fastest install: https://winlibs.com  ^> unzip to C:\mingw64
echo.
goto :eof
