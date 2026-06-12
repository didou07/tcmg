@echo off
chcp 65001 >nul 2>nul
setlocal enabledelayedexpansion

set ARCH=x64
set DEBUG=0
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

if not exist "src\main.c" (
    echo.
    echo  ERROR: Run this script from the project root.
    echo.
    pause & exit /b 1
)

set TCMG_VERSION=unknown
for /f "tokens=3 delims= " %%V in ('findstr /c:"#define TCMG_VERSION" "globals.h" 2^>nul') do set TCMG_VERSION=%%~V

echo.
echo ==================================================
echo   tcmg v%TCMG_VERSION%  ^|  Windows %ARCH% build
echo ==================================================
echo.

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
    echo  Quickest fix: https://winlibs.com  unzip to C:\mingw64
    echo  Or: MSYS2 pacman -S mingw-w64-ucrt-x86_64-gcc
    echo.
    pause & exit /b 1
)

echo  Compiler : !CC!
"!CC!" --version 2>nul | findstr /i "gcc"
echo.

set SRCS=
set SRCS=!SRCS! src\core\globals.c
set SRCS=!SRCS! src\main.c
set SRCS=!SRCS! src\client\client.c
set SRCS=!SRCS! src\log\log.c
set SRCS=!SRCS! src\config\config.c
set SRCS=!SRCS! src\security\failban.c
set SRCS=!SRCS! src\emu\emu.c
set SRCS=!SRCS! src\srvid\srvid.c
set SRCS=!SRCS! src\net\net.c
set SRCS=!SRCS! src\cache\cw_cache.c
set SRCS=!SRCS! src\platform\platform.c
set SRCS=!SRCS! src\crypto\crypto.c
set SRCS=!SRCS! src\crypto\sha1.c
set SRCS=!SRCS! src\proto\newcamd.c
set SRCS=!SRCS! src\proto\cccam.c
set SRCS=!SRCS! webif\server.c
set SRCS=!SRCS! webif\layout.c
set SRCS=!SRCS! webif\stats.c
set SRCS=!SRCS! webif\http\request.c
set SRCS=!SRCS! webif\http\response.c
set SRCS=!SRCS! webif\http\auth.c
set SRCS=!SRCS! webif\pages\login.c
set SRCS=!SRCS! webif\pages\status.c
set SRCS=!SRCS! webif\pages\users.c
set SRCS=!SRCS! webif\pages\system.c
set SRCS=!SRCS! webif\pages\config.c
set SRCS=!SRCS! webif\pages\files.c
set SRCS=!SRCS! webif\pages\power.c
set SRCS=!SRCS! webif\pages\tvcas.c
set SRCS=!SRCS! webif\api\status.c
set SRCS=!SRCS! webif\api\users.c
set SRCS=!SRCS! webif\api\config.c
set SRCS=!SRCS! webif\api\system.c

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

set CFLAGS=-std=c11 %OPT_FLAGS% %ARCH_FLAGS% -I. -DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0601 -D_FORTIFY_SOURCE=2 -Wall -Wextra -Wno-unused-parameter -Wno-overlength-strings
set LDFLAGS=-lws2_32 -ladvapi32 -lbcrypt -static -static-libgcc -lpthread %LD_EXTRA%

if not exist "%BUILDDIR%"          mkdir "%BUILDDIR%"
if not exist "%OBJDIR%\%ARCH%"     mkdir "%OBJDIR%\%ARCH%"

echo  Generating webif/assets/webif_assets.h ...
python tools\gen_assets.py
if !errorlevel! neq 0 ( echo  ERROR: gen_assets.py failed & pause & exit /b 1 )
echo.

echo --- Compiling ---
echo.
set OBJS=

for %%F in (%SRCS%) do (
    set FLAT=%%F
    set FLAT=!FLAT:\=__!
    set OBJ=%OBJDIR%\%ARCH%\!FLAT:.c=.o!
    echo  CC   %%F
    "!CC!" %CFLAGS% -c "%%F" -o "!OBJ!"
    if !errorlevel! neq 0 (
        echo.
        echo  ERROR: Compile failed: %%F
        pause & exit /b 1
    )
    set OBJS=!OBJS! "!OBJ!"
)

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

if "%DEBUG%"=="0" if defined STRIP (
    if exist "!STRIP!" "!STRIP!" --strip-all "%OUT%" 2>nul
)

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
pause
exit /b 0

:assets
echo  Regenerating webif/assets/webif_assets.h ...
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
echo  NOTE: Run from cmd.exe, not Git Bash. In Git Bash use: bash build.sh
echo.
goto :eof
