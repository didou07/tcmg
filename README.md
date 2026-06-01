# tcmg

A lightweight C11 card-sharing server daemon with a built-in web dashboard, Android companion app, and a full CI/CD pipeline.

![Build & Release](https://github.com/didou07/tcmg/actions/workflows/build.yml/badge.svg)

**Version:** 4.8 &nbsp;|&nbsp; **Platforms:** Linux x86-64, Windows x64, Android (arm64-v8a / armeabi-v7a / x86-64)

---

## Features

- TVCAS MGcamd/Newcamd protocol server with multi-client support
- Built-in HTTP web interface
- ECM emulator core with CAID/SID → channel-name lookup
- Android app with foreground service and live control UI
- Zero runtime dependencies — single static binary on Linux/Windows

---

## Building

### Linux / macOS

```bash
# Debug build
make

# Release build (optimized, stripped)
make RELEASE=1

# Or use the shell script
./build.sh
./build.sh linux
./build.sh all        # Linux + Windows cross-compile
```

Output: `build/tcmg`

### Windows (MinGW / MSYS2)

```bat
build.bat
build.bat release
```

Output: `build/tcmg_x64.exe`

### Android

Open `android/` in Android Studio and build, or run:

```bash
cd android
./gradlew assembleRelease
```

Requires Android NDK 27. See [Android signing](#android-signing) below.

### Regenerate web assets

If you edit `src/webif/assets/tcmg.css` or `tcmg.js`, regenerate the embedded header:

```bash
make assets
# then rebuild
make clean && make
```

---

## Usage

```
tcmg [options]

  -c <dir>    Config directory (default: /usr/local/etc)
  -b          Run in background (daemonize)
  -d <level>  Debug bitmask  1=net 2=client 4=ecm 8=proto 16=conf 32=webif 64=ban
  -v          Show version
  -h          Show help
```

On first run, place `tcmg.conf` in the config directory. The server listens for card-sharing clients on port **15050** and the web interface on port **8080** by default.

---

## Project Layout

```
tcmg/
├── src/
│   ├── main.c                  Entry point and main loop
│   ├── client.c / client.h     Per-client thread handling
│   ├── tcmg.h                  Master header (version, platform, includes)
│   ├── core/                   log, conf, ban, emu, srvid
│   ├── crypto/                 DES / 3DES-EDE2-CBC / MD5 / CSPRNG
│   ├── net/                    Socket I/O and Newcamd frame codec
│   ├── platform/               OS abstraction (signals, daemon, paths)
│   └── webif/                  HTTP server, pages, API, embedded assets
├── android/                    Android Studio project (Java + JNI)
├── tools/
│   └── gen_assets.py           Embeds CSS/JS into webif_assets.h
├── Makefile
├── build.sh                    Shell build script (Linux / macOS / MSYS2)
├── build.bat                   Batch build script (Windows)
└── .github/workflows/
    └── build.yml               CI: Linux, Windows, Android → GitHub Release
```

---

## CI / CD

GitHub Actions builds on every push to `main`/`master`:

| Job | Runner | Output |
|-----|--------|--------|
| Linux | ubuntu-latest / GCC | `tcmg-<version>-linux-x86_64.tar.gz` |
| Windows | MinGW-w64 | `tcmg-<version>-windows-x64.zip` |
| Android | NDK 27 + Gradle | signed release APK |
| Release | on version tag | GitHub Release with all artifacts + SHA-256 |

### Android signing

Add these secrets in **Settings → Secrets → Actions**:

| Secret | Value |
|--------|-------|
| `KEYSTORE_BASE64` | `base64 -w0 tcmg-key.jks` |
| `KEYSTORE_PASSWORD` | keystore store password |
| `KEY_ALIAS` | key alias |
| `KEY_PASSWORD` | key password |

---

## Requirements

| Target | Requirement |
|--------|-------------|
| Linux | GCC, make, python3 |
| Windows | MinGW-w64 (MSYS2 / UCRT64) |
| Android | Android Studio, NDK 27, SDK 35 (minSdk 24) |
| Cross-compile | `x86_64-w64-mingw32-gcc` on Linux |