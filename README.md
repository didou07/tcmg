# TCMG

A lightweight C11 server daemon with a built-in WebIF dashboard, Android app, and CI/CD pipeline.
Supports Linux x86-64, Windows x64 (MinGW), and Android (arm64-v8a).

![Build & Release](https://github.com/YOUR_USER/YOUR_REPO/actions/workflows/build.yml/badge.svg)

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
├── android/                           ← Android Studio project
│   ├── app/
│   │   ├── build.gradle               ← Signing config (keystore.properties)
│   │   └── src/main/
│   │       ├── java/com/tcmg/app/     ← Kotlin/Java sources
│   │       ├── res/                   ← Resources (VOID theme: colors, drawables)
│   │       └── jni/
│   │           ├── CMakeLists.txt     ← NDK build — links all src/*.c
│   │           └── tcmg_jni_bridge.c  ← JNI glue
│   ├── tcmg-key.jks                   ← Release keystore (git-ignored locally)
│   ├── keystore.properties            ← Signing credentials (git-ignored)
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

---

## Building Locally

### Linux

```bash
# Debug build (default)
make

# Optimised + stripped release binary
make RELEASE=1 -j$(nproc)
```

Or use the helper script:

```bash
./build.sh              # Linux x64 release
./build.sh windows      # Windows x64 release (requires mingw-w64)
./build.sh all          # both platforms
./build.sh clean
```

### Windows (MinGW-w64)

```bat
build.bat          :: x64 release
build.bat debug    :: x64 debug
build.bat clean
```

Requires MinGW-w64 in `PATH` — download from <https://winlibs.com>

### Android

The recommended way is GitHub Actions. For a local build:

```bash
cd android
./gradlew assembleDebug      # debug APK — no keystore required
./gradlew assembleRelease    # signed release APK (keystore required — see below)
```

Requirements: JDK 17, Android SDK, NDK 27.2.12479018, CMake 3.22.1.

---

## Android APK Signing

### Step 1 — Generate a keystore (one-time)

```bash
keytool -genkeypair \
  -keystore tcmg-key.jks \
  -alias    tcmg-key \
  -keyalg RSA -keysize 2048 -validity 10000 \
  -storepass YOUR_STORE_PASSWORD \
  -keypass   YOUR_KEY_PASSWORD \
  -dname "CN=Your Name, O=Your Org, C=DZ"
```

> **Keep `tcmg-key.jks` safe.** Losing the keystore means you can never publish an
> update to the same Play Store listing.

### Step 2 — Local / Android Studio signing

Create `android/keystore.properties` (already in `.gitignore` — **never commit this**):

```properties
storeFile=../tcmg-key.jks
storePassword=YOUR_STORE_PASSWORD
keyAlias=tcmg-key
keyPassword=YOUR_KEY_PASSWORD
```

Then run `./gradlew assembleRelease` — Gradle picks it up automatically.

### Step 3 — GitHub Actions signing (CI)

Go to: **Repo → Settings → Secrets and variables → Actions → New repository secret**

| Secret | Value |
|---|---|
| `KEYSTORE_BASE64` | `base64 -w0 tcmg-key.jks` |
| `KEYSTORE_PASSWORD` | Your keystore password |
| `KEY_ALIAS` | `tcmg-key` |
| `KEY_PASSWORD` | Your key password |

The workflow decodes the keystore before building and deletes it from disk after,
regardless of success or failure.

---

## CI / GitHub Actions

Every push to `main` / `master` runs **build.yml** with four parallel jobs:

| Job | Runner | Output |
|---|---|---|
| `linux` | `ubuntu-latest` | `tcmg-VERSION-linux-x86_64.tar.gz` + `.sha256` |
| `windows` | `windows-latest` | `tcmg-VERSION-windows-x64.zip` + `.sha256` |
| `android` | `ubuntu-latest` | `app-release.apk` |
| `release` | `ubuntu-latest` | GitHub Release (tag + all three artifacts) |

The `release` job runs only on push (not pull requests).
It reads the version from `#define TCMG_VERSION` in `src/tcmg-globals.h`,
creates a git tag `vX.Y`, and publishes a GitHub Release with all artifacts.

> **To release a new version:** bump `TCMG_VERSION` in `src/tcmg-globals.h`,
> then push to `main`. The CI handles everything else.

### Workflow Requirements

- NDK `27.2.12479018` and CMake `3.22.1` are installed by the workflow automatically.
- No auto-version-bump — version is managed manually in `src/tcmg-globals.h`.
- Artifacts are retained for **30 days** (Linux/Windows) and **90 days** (Android APK).

---

## WebIF

The built-in web interface (VOID theme — Deep Navy + Electric Blue) is served by the
daemon itself. Open `http://<device-ip>:<port>` in any browser.

**Pages:**

| Path | Description |
|---|---|
| `/status` | Dashboard — live stats, connected clients |
| `/livelog` | Real-time server log |
| `/users` | User account management |
| `/failban` | Fail-ban IP list |
| `/config` | Configuration file editor |
| `/tvcas` | TVCAS tool |
| `/restart` | Restart server |
| `/shutdown` | Shutdown server |

The sidebar collapses (click the hamburger icon) and persists state across page
navigations via `sessionStorage`.

---

## Android App

The Android app (minimum API 24 — Android 7.0) wraps the C daemon via JNI.

- **Dashboard tab** — start/stop server, uptime counter, network addresses, WebIF quick-launch buttons
- **Log tab** — real-time log with filter and search
- **Config Editor tab** — edit `tcmg.conf` and `tcmg.srvid2` directly on the device
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
