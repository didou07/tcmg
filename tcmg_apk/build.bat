@echo off
setlocal EnableDelayedExpansion
title TCMG Android Builder

set "PROJECT_DIR=%~dp0"
if "%PROJECT_DIR:~-1%"=="\" set "PROJECT_DIR=%PROJECT_DIR:~0,-1%"
cd /d "%PROJECT_DIR%"

cls
echo.
echo  +===================================================+
echo  ^|         TCMG Android Builder v2.2                ^|
echo  +===================================================+
echo.

:: =====================================================================
:: STEP 1 - FIND JAVA
:: =====================================================================
echo  [1/5] Searching for Java JDK...
echo.

set "JAVA_BIN="

if defined JAVA_HOME (
    if exist "%JAVA_HOME%\bin\java.exe" (
        set "JAVA_BIN=%JAVA_HOME%\bin"
        goto :java_ok
    )
    set "JAVA_HOME="
)

for /d %%D in ("C:\Program Files\Microsoft\jdk-17*") do (
    if exist "%%D\bin\java.exe" ( set "JAVA_HOME=%%D" & set "JAVA_BIN=%%D\bin" & goto :java_ok )
)
for /d %%D in ("C:\Program Files\Microsoft\jdk-21*") do (
    if exist "%%D\bin\java.exe" ( set "JAVA_HOME=%%D" & set "JAVA_BIN=%%D\bin" & goto :java_ok )
)
for /d %%D in ("C:\Program Files\Eclipse Adoptium\jdk-17*") do (
    if exist "%%D\bin\java.exe" ( set "JAVA_HOME=%%D" & set "JAVA_BIN=%%D\bin" & goto :java_ok )
)
for /d %%D in ("C:\Program Files\Java\jdk-17*") do (
    if exist "%%D\bin\java.exe" ( set "JAVA_HOME=%%D" & set "JAVA_BIN=%%D\bin" & goto :java_ok )
)
if exist "C:\Program Files\Android\Android Studio\jbr\bin\java.exe" (
    set "JAVA_HOME=C:\Program Files\Android\Android Studio\jbr"
    set "JAVA_BIN=%JAVA_HOME%\bin"
    goto :java_ok
)
for /f "skip=2 tokens=2*" %%A in ('reg query "HKLM\SOFTWARE\JavaSoft\JDK" /v CurrentVersion 2^>nul') do set "_RV=%%B"
if defined _RV (
    for /f "skip=2 tokens=2*" %%A in ('reg query "HKLM\SOFTWARE\JavaSoft\JDK\%_RV%" /v JavaHome 2^>nul') do (
        if exist "%%B\bin\java.exe" ( set "JAVA_HOME=%%B" & set "JAVA_BIN=%%B\bin" & goto :java_ok )
    )
)
where java >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    for /f "tokens=*" %%P in ('where java 2^>nul') do (
        set "JAVA_BIN=%%~dpP" & set "JAVA_BIN=!JAVA_BIN:~0,-1!" & goto :java_ok
    )
)

echo  [ERROR] Java JDK not found!
echo  Install: https://aka.ms/download-jdk/microsoft-jdk-17-windows-x64.msi
echo.
pause & exit /b 1

:java_ok
set "PATH=%JAVA_BIN%;%PATH%"
set "KEYTOOL=%JAVA_BIN%\keytool.exe"
for /f "tokens=*" %%V in ('"%JAVA_BIN%\java.exe" -version 2^>^&1') do ( echo  [OK] %%V & goto :java_ver_done )
:java_ver_done
echo       %JAVA_BIN%
echo.

:: =====================================================================
:: STEP 2 - FIND ANDROID SDK
:: =====================================================================
echo  [2/5] Searching for Android SDK...
echo.

if defined ANDROID_HOME ( if not exist "%ANDROID_HOME%" set "ANDROID_HOME=" )
if not defined ANDROID_HOME if exist "%LOCALAPPDATA%\Android\Sdk"              set "ANDROID_HOME=%LOCALAPPDATA%\Android\Sdk"
if not defined ANDROID_HOME if exist "%USERPROFILE%\AppData\Local\Android\Sdk" set "ANDROID_HOME=%USERPROFILE%\AppData\Local\Android\Sdk"

if not defined ANDROID_HOME (
    echo  [ERROR] Android SDK not found!
    echo  Install Android Studio: https://developer.android.com/studio
    echo.
    pause & exit /b 1
)
echo  [OK] ANDROID_HOME = %ANDROID_HOME%

set "ANDROID_NDK="
for /d %%D in ("%ANDROID_HOME%\ndk\25.*") do set "ANDROID_NDK=%%D"
if not defined ANDROID_NDK for /d %%D in ("%ANDROID_HOME%\ndk\26.*") do set "ANDROID_NDK=%%D"
if not defined ANDROID_NDK for /d %%D in ("%ANDROID_HOME%\ndk\*")    do set "ANDROID_NDK=%%D"
if defined ANDROID_NDK ( echo  [OK] ANDROID_NDK = %ANDROID_NDK% ) else ( echo  [--] NDK not found - Gradle will download it )
echo.

:: =====================================================================
:: STEP 3 - FIX GRADLE WRAPPER JAR (the most common missing file)
:: =====================================================================
echo  [3/5] Checking Gradle wrapper...
echo.

set "WRAPPER_DIR=%PROJECT_DIR%\gradle\wrapper"
set "WRAPPER_JAR=%WRAPPER_DIR%\gradle-wrapper.jar"
set "WRAPPER_PROPS=%WRAPPER_DIR%\gradle-wrapper.properties"

if not exist "%WRAPPER_DIR%" mkdir "%WRAPPER_DIR%"

:: Check if jar exists and is not empty/corrupt (must be > 10KB)
set "JAR_OK=0"
if exist "%WRAPPER_JAR%" (
    for %%F in ("%WRAPPER_JAR%") do (
        if %%~zF GTR 10000 set "JAR_OK=1"
    )
)

if "%JAR_OK%"=="1" (
    echo  [OK] gradle-wrapper.jar is present
) else (
    echo  [--] gradle-wrapper.jar missing or corrupt. Downloading...
    echo.

    :: Try PowerShell download (built into Windows 7+)
    powershell -NoProfile -Command ^
        "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; (New-Object Net.WebClient).DownloadFile('https://github.com/gradle/gradle/raw/v8.6.0/gradle/wrapper/gradle-wrapper.jar', '%WRAPPER_JAR%')" ^
        2>nul

    :: Verify download succeeded
    set "DL_OK=0"
    if exist "%WRAPPER_JAR%" (
        for %%F in ("%WRAPPER_JAR%") do (
            if %%~zF GTR 10000 set "DL_OK=1"
        )
    )

    if "!DL_OK!"=="0" (
        echo  [--] First URL failed, trying fallback...
        powershell -NoProfile -Command ^
            "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; (New-Object Net.WebClient).DownloadFile('https://raw.githubusercontent.com/gradle/gradle/v8.4.0/gradle/wrapper/gradle-wrapper.jar', '%WRAPPER_JAR%')" ^
            2>nul

        for %%F in ("%WRAPPER_JAR%") do (
            if %%~zF GTR 10000 set "DL_OK=1"
        )
    )

    if "!DL_OK!"=="0" (
        echo.
        echo  [ERROR] Could not download gradle-wrapper.jar
        echo.
        echo  Fix manually - open PowerShell and run:
        echo.
        echo    Invoke-WebRequest -Uri "https://github.com/gradle/gradle/raw/v8.6.0/gradle/wrapper/gradle-wrapper.jar" -OutFile "gradle\wrapper\gradle-wrapper.jar"
        echo.
        echo  Or open this project in Android Studio which handles it automatically.
        echo.
        pause & exit /b 1
    )
    echo  [OK] gradle-wrapper.jar downloaded successfully
)

:: Make sure gradle-wrapper.properties points to a valid distribution
if not exist "%WRAPPER_PROPS%" (
    echo  [--] gradle-wrapper.properties missing. Creating...
    (
        echo distributionBase=GRADLE_USER_HOME
        echo distributionPath=wrapper/dists
        echo distributionUrl=https\://services.gradle.org/distributions/gradle-8.6-bin.zip
        echo zipStoreBase=GRADLE_USER_HOME
        echo zipStorePath=wrapper/dists
    ) > "%WRAPPER_PROPS%"
    echo  [OK] gradle-wrapper.properties created
)
echo.

:: =====================================================================
:: STEP 4 - KEYSTORE
:: =====================================================================
echo  [4/5] Checking release keystore...
echo.

set "KS_FILE=%PROJECT_DIR%\tcmg-release.jks"
set "KS_PROPS=%PROJECT_DIR%\keystore.properties"
set "KS_ALIAS=tcmg-key"
set "KS_PASS=tcmg@Release2024"

if exist "%KS_FILE%" goto :ks_exists

echo  [--] Generating keystore... (only happens once)
echo.
"%KEYTOOL%" -genkeypair -keystore "%KS_FILE%" -alias "%KS_ALIAS%" -keyalg RSA -keysize 2048 -validity 10000 -storepass "%KS_PASS%" -keypass "%KS_PASS%" -dname "CN=TCMG, OU=Android, O=TCMG, L=City, ST=State, C=DZ" -storetype PKCS12
if !ERRORLEVEL! NEQ 0 (
    echo.
    echo  [ERROR] Keystore generation failed!
    pause & exit /b 1
)
echo.
echo  [OK] Keystore created: tcmg-release.jks
goto :ks_done

:ks_exists
echo  [OK] Keystore exists: tcmg-release.jks

:ks_done

(
    echo storeFile=%KS_FILE:\=/%
    echo storePassword=%KS_PASS%
    echo keyAlias=%KS_ALIAS%
    echo keyPassword=%KS_PASS%
) > "%KS_PROPS%"
echo  [OK] keystore.properties updated
echo.

:: =====================================================================
:: STEP 5 - SELECT BUILD TYPE AND BUILD
:: =====================================================================
echo  [5/5] Select build type:
echo.
echo    1  =  Debug APK    (for testing)
echo    2  =  Release APK  (signed, for Play Store)
echo    3  =  Both
echo.
set /p "BUILD_CHOICE=  Choice [1/2/3] (Enter = 1): "
if "!BUILD_CHOICE!"=="" set "BUILD_CHOICE=1"
echo.

set "GRADLE_TASK=assembleDebug"
set "BUILD_DEBUG=1"
set "BUILD_RELEASE=0"
if "!BUILD_CHOICE!"=="2" ( set "GRADLE_TASK=assembleRelease"               & set "BUILD_DEBUG=0" & set "BUILD_RELEASE=1" )
if "!BUILD_CHOICE!"=="3" ( set "GRADLE_TASK=assembleDebug assembleRelease" & set "BUILD_DEBUG=1" & set "BUILD_RELEASE=1" )

echo  Running: gradlew.bat %GRADLE_TASK%
echo  ---------------------------------------------------
echo.

call gradlew.bat %GRADLE_TASK%
set "BUILD_CODE=%ERRORLEVEL%"

echo.
echo  ---------------------------------------------------

if %BUILD_CODE% NEQ 0 (
    echo.
    echo  +===================================================+
    echo  ^|   BUILD FAILED  (exit code: %BUILD_CODE%)
    echo  +===================================================+
    echo.
    echo  Troubleshooting:
    echo    More details:    gradlew.bat %GRADLE_TASK% --info
    echo    Clean and retry: gradlew.bat clean %GRADLE_TASK%
    echo    In Android Studio: Tools ^> SDK Manager
    echo      Install: NDK (Side by side) + CMake 3.22.1
    echo.
    pause & exit /b 1
)

:: =====================================================================
:: SUCCESS
:: =====================================================================
echo.
echo  +===================================================+
echo  ^|   BUILD SUCCESSFUL!                               ^|
echo  +===================================================+
echo.

set "DEBUG_APK=%PROJECT_DIR%\app\build\outputs\apk\debug\app-debug.apk"
set "REL_APK=%PROJECT_DIR%\app\build\outputs\apk\release\app-release.apk"

if %BUILD_DEBUG%==1 if exist "!DEBUG_APK!" (
    echo  [DEBUG APK]
    echo    !DEBUG_APK!
    echo.
)
if %BUILD_RELEASE%==1 if exist "!REL_APK!" (
    echo  [RELEASE APK - signed, ready for Play Store]
    echo    !REL_APK!
    echo.
    echo  +===================================================+
    echo  ^|  BACKUP THESE FILES - do not lose them!           ^|
    echo  ^|    tcmg-release.jks     ^<-- your signing key      ^|
    echo  ^|    keystore.properties  ^<-- key passwords         ^|
    echo  ^|  Without them you cannot update the app!          ^|
    echo  +===================================================+
    echo.
)

:: Optional ADB install
where adb >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    adb devices 2>nul | findstr /r "device$" >nul 2>&1
    if !ERRORLEVEL! EQU 0 (
        echo  Android device detected via ADB.
        set /p "DO_INSTALL=  Install APK to device? [y/n]: "
        if /i "!DO_INSTALL!"=="y" (
            if %BUILD_RELEASE%==1 ( adb install -r "!REL_APK!" ) else ( adb install -r "!DEBUG_APK!" )
            if !ERRORLEVEL! EQU 0 ( echo  [OK] Installed. ) else ( echo  [WARN] ADB install failed. )
        )
        echo.
    )
)

echo  Done. Press any key to exit.
echo.
pause
endlocal