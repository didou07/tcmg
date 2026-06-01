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
    if /i "%%A"=="assets" goto :assets
)

if not exist "%SRCDIR%\main.c" (
    echo.
    echo  ERROR: Run this script from the project root ^(where src\ is^).
    echo.
    pause & exit /b 1
)

set TCMG_VERSION=unknown
for /f "tokens=3 delims= " %%V in ('findstr /c:"#define TCMG_VERSION" "%SRCDIR%\tcmg.h" 2^>nul') do set TCMG_VERSION=%%~V

echo.
echo ==================================================
echo   tcmg v%TCMG_VERSION%  ^|  Windows %ARCH% build
echo ==================================================
echo.

:: ---- Compiler discovery --------------------------------------------------
set CC=
set STRIP=

where x86_64-w64-mingw32-gcc >nul 2>nul
if !errorlevel! equ 0 set CC=x86_64-w64-mingw32-gcc& set STRIP=x86_64-w64-mingw32-strip

if not defined CC ( where gcc >nul 2>nul
    if !errorlevel! equ 0 set CC=gcc& set STRIP=strip )

if not defined CC if exist "C:\msys64\ucrt64\bin\gcc.exe" (
    set CC=C:\msys64\ucrt64\bin\gcc.exe
    set STRIP=C:\msys64\ucrt64\bin\strip.exe
    set PATH=C:\msys64\ucrt64\bin;!PATH! )

if not defined CC if exist "C:\msys64\mingw64\bin\gcc.exe" (
    set CC=C:\msys64\mingw64\bin\gcc.exe
    set STRIP=C:\msys64\mingw64\bin\strip.exe
    set PATH=C:\msys64\mingw64\bin;!PATH! )

if not defined CC if exist "C:\mingw64\bin\gcc.exe" (
    set CC=C:\mingw64\bin\gcc.exe
    set STRIP=C:\mingw64\bin\strip.exe
    set PATH=C:\mingw64\bin;!PATH! )

if not defined CC if exist "C:\MinGW\bin\gcc.exe" (
    set CC=C:\MinGW\bin\gcc.exe
    set STRIP=C:\MinGW\bin\strip.exe
    set PATH=C:\MinGW\bin;!PATH! )

if not defined CC if exist "C:\TDM-GCC-64\bin\gcc.exe" (
    set CC=C:\TDM-GCC-64\bin\gcc.exe
    set STRIP=C:\TDM-GCC-64\bin\strip.exe
    set PATH=C:\TDM-GCC-64\bin;!PATH! )

if not defined CC if exist "%USERPROFILE%\scoop\apps\mingw\current\bin\gcc.exe" (
    set CC=%USERPROFILE%\scoop\apps\mingw\current\bin\gcc.exe
    set STRIP=%USERPROFILE%\scoop\apps\mingw\current\bin\strip.exe
    set PATH=%USERPROFILE%\scoop\apps\mingw\current\bin;!PATH! )

if not defined CC if exist "C:\ProgramData\chocolatey\lib\mingw\tools\install\mingw64\bin\gcc.exe" (
    set CC=C:\ProgramData\chocolatey\lib\mingw\tools\install\mingw64\bin\gcc.exe
    set STRIP=C:\ProgramData\chocolatey\lib\mingw\tools\install\mingw64\bin\strip.exe
    set PATH=C:\ProgramData\chocolatey\lib\mingw\tools\install\mingw64\bin;!PATH! )

if not defined CC if exist "C:\Program Files\Git\mingw64\bin\gcc.exe" (
    set "CC=C:\Program Files\Git\mingw64\bin\gcc.exe"
    set "STRIP=C:\Program Files\Git\mingw64\bin\strip.exe"
    set "PATH=C:\Program Files\Git\mingw64\bin;!PATH!" )

if not defined CC (
    echo  ERROR: MinGW-w64 gcc not found.
    echo.
    echo  Quickest fix: download WinLibs from https://winlibs.com
    echo  Unzip to C:\mingw64  then re-run this script.
    echo.
    echo  Alternatives:
    echo    MSYS2 : https://www.msys2.org  then: pacman -S mingw-w64-ucrt-x86_64-gcc
    echo    Scoop : scoop install mingw
    echo    Choco : choco install mingw
    echo.
    pause & exit /b 1
)

echo  Compiler : !CC!
"!CC!" --version 2>nul | findstr /i "gcc"
echo.

:: ---- Source files (new layered structure) --------------------------------
:: Core
set SRCS=globals.c main.c client.c
:: Core lib (subdirectory — use subdir\file.c syntax)
set SRCS=%SRCS% core\log.c core\conf.c core\ban.c core\emu.c core\srvid.c
:: Net / Crypto / Platform
set SRCS=%SRCS% net\net.c crypto\crypto.c platform\platform.c
:: WebIF
set SRCS=%SRCS% webif\webif.c webif\webif_common.c webif\webif_layout.c
set SRCS=%SRCS% webif\webif_page_login.c webif\webif_page_status.c
set SRCS=%SRCS% webif\webif_page_users.c webif\webif_page_system.c
set SRCS=%SRCS% webif\webif_api.c webif\webif_tvcas.c

:: ---- Flags ---------------------------------------------------------------
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

set CFLAGS=-std=c11 %OPT_FLAGS% %ARCH_FLAGS% -I%SRCDIR% -DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0601 -D_FORTIFY_SOURCE=2 -Wall -Wextra -Wno-unused-parameter -Wno-overlength-strings -Wno-format
set LDFLAGS=-lws2_32 -ladvapi32 -lbcrypt -static -static-libgcc -lpthread %LD_EXTRA%

:: ---- Create dirs ---------------------------------------------------------
if not exist "%BUILDDIR%"          mkdir "%BUILDDIR%"
if not exist "%OBJDIR%\%ARCH%"     mkdir "%OBJDIR%\%ARCH%"

:: ---- Compile -------------------------------------------------------------
echo --- Compiling ---
echo.
set OBJS=

for %%F in (%SRCS%) do (
    :: Flatten path: core\log.c → core__log.o
    set FLAT=%%F
    set FLAT=!FLAT:\=__!
    set OBJ=%OBJDIR%\%ARCH%\!FLAT:.c=.o!
    echo  CC   %SRCDIR%\%%F
    "!CC!" %CFLAGS% -c "%SRCDIR%\%%F" -o "!OBJ!"
    if !errorlevel! neq 0 (
        echo.
        echo  ERROR: Compile failed: %SRCDIR%\%%F
        pause & exit /b 1
    )
    set OBJS=!OBJS! "!OBJ!"
)

:: ---- Link ----------------------------------------------------------------
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

:: ---- Strip ---------------------------------------------------------------
if "%DEBUG%"=="0" if defined STRIP (
    if exist "!STRIP!" "!STRIP!" --strip-all "%OUT%" 2>nul
)

:: ---- Done ----------------------------------------------------------------
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

:assets
echo  Regenerating webif_assets.h ...
python tools\gen_assets.py
if !errorlevel! neq 0 ( echo  ERROR: gen_assets.py failed & pause & exit /b 1 )
echo  Done. Rebuild required.
goto :eof

:clean
echo  Cleaning %BUILDDIR%\...
if exist "%BUILDDIR%" rmdir /s /q "%BUILDDIR%"
echo  Done.
goto :eof

:help
echo.
echo  Usage: build.bat [x64^|x86] [debug] [clean] [assets] [help]
echo.
echo  IMPORTANT: Run from cmd.exe, not from Git Bash / MSYS2.
echo  In Git Bash use:  bash build.sh
echo.
echo  Auto-detects gcc in PATH and common install locations.
echo  Fastest install: https://winlibs.com  ^> unzip to C:\mingw64
echo.
goto :eof
BATEOF
echo "build.bat done"
