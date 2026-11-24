# Building the Nebula Engine APK

Three supported ways to produce `app-debug.apk` / `app-release.apk`. All three
compile the *same* source in this repository — nothing is prebuilt or checked in.

| Route | When to use it |
|---|---|
| **A. GitHub Actions CI** (this repo) | Produce a downloadable APK artifact without installing anything locally |
| **B. Android Studio** | Day-to-day development and on-device debugging |
| **C. Command line** | Headless machines, scripted releases |

---

## A. GitHub Actions CI (no local SDK needed)

Every push to `main` / the engine branch, every PR, and every tag matching `v*`
triggers `.github/workflows/android.yml`:

1. `native-tests` — builds the C++17 engine on the host with `-Wall -Wextra` and
   runs the full native test suite (`scripts/run_native_tests.sh`).
2. `apk` — installs Temurin JDK 17 + Android SDK 34 / build-tools 34.0.0 /
   NDK 25.2.9519653 / CMake 3.22.1, generates the Gradle wrapper, then runs
   `:app:assembleDebug` and `:app:assembleRelease`.

The workflow uploads both APKs as the **`nebula-engine-apks`** workflow artifact
(Actions → run → *Artifacts* → download → unzip). Tag releases additionally get
the APK attached to a GitHub Release.

To build on your own fork: push the repo, open the **Actions** tab, select
`android-ci` → *Run workflow*.

## B. Android Studio (recommended for development)

1. Install **Android Studio Iguana or newer** and a **JDK 17** (Studio bundles one).
2. *File → Open…* → select this repository root.
3. Studio will offer to install missing SDK components (platform 34, NDK
   25.2.9519653, CMake 3.22.1). Accept.
4. *Run ▶* on a device/emulator — or build the APK from *Build → Build Bundle(s)/APK(s)*.

Output: `app/build/outputs/apk/debug/app-debug.apk`

> The Gradle wrapper jar is intentionally **not vendored** (binary artifact).
> Studio regenerates it on first sync, or run once:
> `gradle wrapper --gradle-version 8.4 --distribution-type bin`

## C. Command line (headless)

Requirements: JDK 17, Android command-line tools, and:

```bash
sdkmanager --install \
  "platforms;android-34" "build-tools;34.0.0" "platform-tools" \
  "ndk;25.2.9519653" "cmake;3.22.1"
yes | sdkmanager --licenses
```

Point Gradle at the SDK via `local.properties` (git-ignored):

```properties
sdk.dir=/absolute/path/to/Android/Sdk
```

Then:

```bash
./scripts/build_apk.sh           # debug APK
./scripts/build_apk.sh release   # release APK
```

## Signing the release build (optional)

Debug builds are signed automatically with the local debug key.
For a distributable release APK, create an upload keystore **outside the repo**:

```bash
keytool -genkeypair -v -keystore nebula-release.jks \
  -alias nebula -keyalg RSA -keysize 2048 -validity 10000
```

Then either create `keystore.properties` (git-ignored) at the repo root:

```properties
storeFile=/absolute/path/to/nebula-release.jks
storePassword=••••••
keyAlias=nebula
keyPassword=••••••
```

or export `NEBULA_KEYSTORE_PATH`, `NEBULA_KEYSTORE_PASSWORD`, `NEBULA_KEY_ALIAS`,
`NEBULA_KEY_PASSWORD`. The release build type enables **R8** (`minifyEnabled`,
keep rules in `proguard-rules.pro`) and resource shrinking; signing is applied
automatically when a keystore is found. Without one, the release APK is built
**unsigned** (fine for `apksigner`-later / internal testing).

### Signing in CI

Add four repository secrets (`Settings → Secrets and variables → Actions → New repository secret`):

`NEBULA_KEYSTORE_B64` (base64 of the .jks), `NEBULA_KEYSTORE_PASSWORD`,
`NEBULA_KEY_ALIAS`, `NEBULA_KEY_PASSWORD` — then extend the release step with:

```yaml
      - name: Decode keystore
        run: echo "$NEBULA_KEYSTORE_B64" | base64 -d > "$RUNNER_TEMP/nebula-release.jks"
        env: { NEBULA_KEYSTORE_B64: ${{ secrets.NEBULA_KEYSTORE_B64 }} }
```
and pass the four `NEBULA_*` variables to the Gradle step's `env:` so the release
build signs automatically. Secrets must never be committed —
`.gitignore` already excludes `keystore.properties`, `*.jks`, `*.keystore`, `*.p12`.

## Native verification without any Android tooling

```bash
./scripts/run_native_tests.sh     # full C++ engine suite (24 tests)
./scripts/run_sanitizer_tests.sh  # ASan + UBSan build
```

## Troubleshooting

| Symptom | Fix |
|---|---|
| `SDK location not found` | Create `local.properties` with `sdk.dir=…` or export `ANDROID_HOME` |
| `No version of NDK matched` | `sdkmanager --install "ndk;25.2.9519653"` |
| `CMake '3.22.1' not found` | `sdkmanager --install "cmake;3.22.1"` |
| `Licenses not accepted` | `yes \| sdkmanager --licenses` |
| `gradlew: Permission denied` | `chmod +x gradlew` after regenerating the wrapper |
| Kotlin plugin can't resolve | Ensure JDK 17 (`org.gradle.java.home`) and Gradle ≥ 8.4 |
