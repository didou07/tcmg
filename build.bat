@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

:: ==========================================================================
:: build.bat - tcmg Windows build script (pure C11, MinGW-w64)
::
:: Usage:
::   build.bat             x64 release build (default)
::   build.bat x86         32-bit build
::   build.bat debug       x64 debug build (no optimisation, with symbols)
::   build.bat clean       remove obj/ and bin/
::   build.bat help        show this help
::
:: Requirements: MinGW-w64 installed and gcc.exe in PATH
::   Download: https://winlibs.com  or  https://www.msys2.org
:: ==========================================================================

set BINARY_NAME=tcmg.exe
set ARCH=x64
set DEBUG=0

:: -- Parse arguments -------------------------------------------------------
for %%A in (%*) do (
    if /i "%%A"=="x86"   set ARCH=x86
    if /i "%%A"=="x64"   set ARCH=x64
    if /i "%%A"=="debug" set DEBUG=1
    if /i "%%A"=="clean" goto :clean
    if /i "%%A"=="help"  goto :help
    if /i "%%A"=="/?"    goto :help
)

:: -- Read version from tcmg-globals.h ----------------------------------------
set TCMG_VERSION=unknown
for /f "tokens=3 delims= " %%V in ('findstr /c:"#define TCMG_VERSION" tcmg-globals.h 2^>nul') do (
    set TCMG_VERSION=%%~V
)

:: -- Banner ------------------------------------------------------------------
echo.
echo ==================================================
echo   tcmg v%TCMG_VERSION%  ^|  Windows %ARCH% build
echo ==================================================
echo.

:: -- Detect compiler -------------------------------------------------------
set CC=gcc
if "%ARCH%"=="x86" (
    where i686-w64-mingw32-gcc >nul 2>nul
    if !errorlevel! equ 0 (
        set CC=i686-w64-mingw32-gcc
        set STRIP=i686-w64-mingw32-strip
    )
) else (
    where x86_64-w64-mingw32-gcc >nul 2>nul
    if !errorlevel! equ 0 (
        set CC=x86_64-w64-mingw32-gcc
        set STRIP=x86_64-w64-mingw32-strip
    )
)

where %CC% >nul 2>nul
if !errorlevel! neq 0 (
    echo  ERROR: Compiler "%CC%" not found.
    echo.
    echo  Install MinGW-w64 and add to PATH:
    echo    https://winlibs.com  or  https://www.msys2.org
    echo.
    pause
    exit /b 1
)

echo  Compiler:  %CC%
%CC% --version | findstr "gcc"
echo.

:: -- Source files (pure C11) -----------------------------------------------
set SRCS=tcmg.c tcmg-log.c tcmg-crypto.c tcmg-net.c tcmg-ban.c tcmg-conf.c tcmg-emu.c tcmg-srvid2.c tcmg-webif.c

:: -- Flags -------------------------------------------------------------------
if "%ARCH%"=="x86" (
    set ARCH_FLAGS=-march=i686 -m32
) else (
    set ARCH_FLAGS=-march=x86-64 -mtune=generic
)

if "%DEBUG%"=="1" (
    set OPT_FLAGS=-Og -g
    set LD_EXTRA=
    echo  Mode: DEBUG ^(no optimisation^)
) else (
    set OPT_FLAGS=-Os -ffunction-sections -fdata-sections -fmerge-all-constants -fno-ident -fstack-protector-strong
    set LD_EXTRA=-Wl,--gc-sections -Wl,--strip-all -Wl,--build-id=none
    echo  Mode: RELEASE ^(optimised + stripped^)
)
echo.

set CFLAGS=-std=c11 %OPT_FLAGS% %ARCH_FLAGS% ^
  -DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0601 ^
  -D_FORTIFY_SOURCE=2 ^
  -Wall -Wextra -Wno-unused-parameter ^
  -Wmissing-prototypes -Wstrict-prototypes

set LDFLAGS=-lws2_32 -ladvapi32 -lbcrypt ^
  -static -static-libgcc ^
  -lpthread ^
  %LD_EXTRA%

:: -- Create directories ----------------------------------------------------
if not exist bin mkdir bin
if not exist "obj\%ARCH%" mkdir "obj\%ARCH%"

:: -- Compile -----------------------------------------------------------------
echo --- Compiling -----------------------------------------------
echo.
set OBJS=

for %%F in (%SRCS%) do (
    set OBJ=obj\%ARCH%\%%~nF.o
    echo  CC   %%F
    %CC% %CFLAGS% -c %%F -o !OBJ!
    if !errorlevel! neq 0 (
        echo.
        echo  ERROR: Failed to compile %%F
        pause
        exit /b 1
    )
    set OBJS=!OBJS! !OBJ!
)

:: -- Link -------------------------------------------------------------------
echo.
echo --- Linking -------------------------------------------------
echo.
set OUT=bin\tcmg_%ARCH%.exe

%CC% %CFLAGS% %OBJS% -o %OUT% %LDFLAGS%
if !errorlevel! neq 0 (
    echo.
    echo  ERROR: Link failed
    pause
    exit /b 1
)

:: -- Strip (release only, belt+suspenders after -Wl,--strip-all) ----------
if "%DEBUG%"=="0" (
    if defined STRIP (
        %STRIP% --strip-all %OUT% 2>nul
    ) else (
        strip --strip-all %OUT% 2>nul
    )
)

:: -- Report ------------------------------------------------------------------
echo.
echo ==================================================
echo   BUILD SUCCESS
echo ==================================================
echo.
for %%F in (%OUT%) do (
    set /a size_kb=%%~zF / 1024
    echo   Binary : %OUT%
    echo   Size   : !size_kb! KB  ^(%%~zF bytes^)
)
echo.
echo   Usage:
echo     %OUT% -h
echo     %OUT% -c C:\tcmg\config
echo     %OUT% -d 0x18   ^(client+ecm debug^)
echo.
pause
exit /b 0

:: -- Clean -------------------------------------------------------------------
:clean
echo  Cleaning...
if exist obj rmdir /s /q obj
if exist bin rmdir /s /q bin
echo  Done.
goto :eof

:: -- Help --------------------------------------------------------------------
:help
echo.
echo  Usage: build.bat [x64^|x86] [debug] [clean] [help]
echo.
echo    x64    Build for 64-bit Windows (default)
echo    x86    Build for 32-bit Windows
echo    debug  No optimisation, include debug symbols
echo    clean  Remove obj/ and bin/ directories
echo    help   Show this help
echo.
echo  Requirements:
echo    MinGW-w64 with gcc.exe in PATH
echo    https://winlibs.com  or  https://www.msys2.org
echo.
echo  Examples:
echo    build.bat              ^<^ x64 release
echo    build.bat x86          ^<^ x86 release
echo    build.bat debug        ^<^ x64 debug
echo.
goto :eof
