# TCMG

A lightweight C11 server daemon with a built-in WebIF dashboard, Android app, and CI/CD pipeline.
Supports Linux x86-64, Windows x64 (MinGW), and Android (arm64-v8a / armeabi-v7a / x86_64).

![Build & Release](https://github.com/didou07/tcmg/actions/workflows/build.yml/badge.svg)

---

## Repository Layout

```
tcmg/
├── src/                               ← C sources — shared by all platforms
│   ├── tcmg.c                         ← Entry point / main loop
│   ├── tcmg-globals.h                 ← TCMG_VERSION, shared structs
│   ├── tcmg-conf.c / tcmg-conf.h      ← Config file parser
│   ├── tcmg-net.c                     ← Network / socket layer
│   ├── tcmg-log.c                     ← Logging subsystem
│   ├── tcmg-ban.c                     ← Fail-ban engine
│   ├── tcmg-emu.c                     ← Emulator core
│   ├── tcmg-srvid2.c                  ← Server identity (srvid2)
│   ├── tcmg-crypto.c / tcmg-crypto.h  ← Crypto helpers
│   ├── tcmg-webif.c                   ← WebIF HTTP server
│   ├── tcmg-webif-layout.c            ← CSS + HTML shell (VOID theme)
│   ├── tcmg-webif-pages.c             ← Dashboard, log, config pages
│   ├── tcmg-webif-tvcas.c             ← TVCAS tool page
│   └── tcmg-webif-internal.h          ← WebIF shared types
│
├── build/                             ← Build output (git-ignored)
│   ├── obj/                           ← Compiled object files
│   ├── tcmg                           ← Linux binary
│   └── tcmg.exe                       ← Windows binary
│
├── android/                           ← Android Studio project
│   ├── app/
│   │   ├── build.gradle               ← Signing config (env vars or keystore.properties)
│   │   └── src/main/
│   │       ├── java/com/tcmg/app/     ← Java sources
│   │       ├── res/                   ← Resources (VOID theme)
│   │       └── jni/
│   │           ├── CMakeLists.txt     ← NDK build — links all src/*.c
│   │           └── tcmg_jni_bridge.c  ← JNI glue
│   └── gradlew / gradlew.bat
│
├── Makefile                           ← Linux & Windows (MinGW) builds
├── build.sh                           ← Linux/Windows build helper
├── build.bat                          ← Windows build helper (MinGW)
├── .gitignore
└── .github/
    └── workflows/
        └── build.yml                  ← CI: Linux · Windows · Android · Release
```

> **Never committed:** `android/tcmg-key.jks`, `android/keystore.properties`, and `build/`
> are in `.gitignore`. Signing credentials live only in GitHub Secrets and on your
> local machine.

---

## Building

### Linux

```bash
# Debug build
make

# Optimised + stripped release binary
make RELEASE=1 -j$(nproc)
```

Output is placed in `build/tcmg`.

Or use the helper:

```bash
./build.sh              # Linux x64 release → build/tcmg
./build.sh windows      # Windows x64 release → build/tcmg.exe (requires mingw-w64)
./build.sh all          # both platforms
./build.sh clean        # remove build/
```

### Windows (MinGW-w64)

```bat
build.bat          :: x64 release → build\tcmg_x64.exe
build.bat debug    :: x64 debug
build.bat clean    :: remove build\
```

Requires MinGW-w64 in `PATH` — download from <https://winlibs.com>

### Android

The recommended way is GitHub Actions (push to `main` → signed APK is produced automatically).

For a local debug build:

```bash
cd android
./gradlew assembleDebug      # debug APK — no keystore required
```

For a local release build you need `android/keystore.properties` (see below).

---

## Android APK Signing

### Step 1 — Generate a keystore (one-time setup)

```bash
keytool -genkeypair \
  -keystore android/tcmg-key.jks \
  -alias    tcmg-key \
  -keyalg RSA -keysize 2048 -validity 10000 \
  -storepass YOUR_STORE_PASSWORD \
  -keypass   YOUR_KEY_PASSWORD \
  -dname "CN=Your Name, O=Your Org, C=DZ"
```

> **Keep `tcmg-key.jks` safe and backed up.** Losing it means you can never publish
> an update to the same Play Store listing.

### Step 2 — Add GitHub Secrets (CI signing)

Go to: **Repo → Settings → Secrets and variables → Actions → New repository secret**

| Secret name        | Value                              |
|--------------------|------------------------------------|
| `KEYSTORE_BASE64`  | `base64 -w0 android/tcmg-key.jks` |
| `KEYSTORE_PASSWORD`| Your keystore store password       |
| `KEY_ALIAS`        | `tcmg-key`                         |
| `KEY_PASSWORD`     | Your key password                  |

The workflow decodes the keystore before building and wipes it from disk after,
regardless of success or failure.

### Step 3 — Local release builds (optional)

Create `android/keystore.properties` (already in `.gitignore` — **never commit this file**):

```properties
storeFile=../tcmg-key.jks
storePassword=YOUR_STORE_PASSWORD
keyAlias=tcmg-key
keyPassword=YOUR_KEY_PASSWORD
```

Then run `./gradlew assembleRelease` from the `android/` directory.

---

## CI / GitHub Actions

Every push to `main` / `master` runs **build.yml** with four jobs:

| Job       | Runner           | Output                                          |
|-----------|------------------|-------------------------------------------------|
| `linux`   | `ubuntu-latest`  | `tcmg-VERSION-linux-x86_64.tar.gz` + `.sha256` |
| `windows` | `windows-latest` | `tcmg-VERSION-windows-x64.zip` + `.sha256`     |
| `android` | `ubuntu-latest`  | `app-release.apk`                               |
| `release` | `ubuntu-latest`  | GitHub Release (tag + all three artifacts)      |

The `release` job runs only on push (not pull requests). It reads the version from
`#define TCMG_VERSION` in `src/tcmg-globals.h`, creates a git tag `vX.Y`, and
publishes a GitHub Release with all artifacts attached.

> **To release a new version:** bump `TCMG_VERSION` in `src/tcmg-globals.h`, then
> push to `main`. The CI handles everything else.

---

## WebIF

The built-in web interface (VOID theme — Deep Navy + Electric Blue) is served by
the daemon itself. Open `http://<device-ip>:<port>` in any browser.

| Path        | Description                          |
|-------------|--------------------------------------|
| `/status`   | Dashboard — live stats, clients      |
| `/livelog`  | Real-time server log                 |
| `/users`    | User account management              |
| `/failban`  | Fail-ban IP list                     |
| `/config`   | Configuration file editor            |
| `/tvcas`    | TVCAS tool                           |
| `/restart`  | Restart server                       |
| `/shutdown` | Shutdown server                      |

---

## Android App

Minimum API 24 (Android 7.0). Wraps the C daemon via JNI.

- **Control tab** — start/stop server, uptime counter, network addresses, WebIF quick-launch
- **Log tab** — real-time log with filter and search
- **Config Editor tab** — edit `tcmg.conf` and `tcmg.srvid2` on-device
- **Theme** — VOID (Deep Navy `#090d14` + Electric Blue `#3b82f6`), matches the WebIF

---

## Versioning

Version is defined in one place:

```c
// src/tcmg-globals.h
#define TCMG_VERSION  "4.4"
```

The CI reads this value to name artifacts and create the git tag.
To release `4.5`: change the string, commit, push.

---

## License

See [LICENSE](LICENSE).
