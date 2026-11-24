# Changelog

All notable changes to the Nebula Engine are documented here.
Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) · Versioning: semver.

## [1.1.0] — 2026-11-24 · Professional tooling & release APK

### Added
- **GitHub Actions CI** (`.github/workflows/android.yml`): native host test suite
  + `assembleDebug` / `assembleRelease` on every push/PR/tag, APKs uploaded as
  workflow artifacts (30-day retention). Tag pushes (`v*`) build release APKs.
- **Release signing support**: optional `keystore.properties` or `NEBULA_*`
  environment variables; signing applies automatically when found, otherwise the
  release APK builds unsigned. Keystores/credentials are git-ignored.
- **R8 release build**: `minifyEnabled` + `shrinkResources` with a curated
  `proguard-rules.pro` (JNI bridge, GLSurfaceView renderer, RecyclerView
  ViewHolder kept; everything else shrinkable).
- **`docs/BUILDING.md`**: three supported build routes (CI artifact, Android
  Studio, headless command line), release signing walkthrough for local + CI,
  and a troubleshooting table.
- **`scripts/build_apk.sh`**: one-command `debug`/`release` APK builder with
  Gradle wrapper bootstrap.
- Version bump: `versionCode 2`, `versionName 1.1.0`.

### Changed
- README build section rewritten to document CI route and local signing; native
  verification instructions unchanged.

### Verified
- Native C++ engine suite: **24/24 tests pass** (`./scripts/run_native_tests.sh`),
  ASan/UBSan clean (`./scripts/run_sanitizer_tests.sh`).
- **android-ci run 45475094827: SUCCESS** — both `:app:assembleDebug` and
  `:app:assembleRelease` build on the hosted runner; APKs published as the
  `nebula-engine-apks` artifact and attached to tagged GitHub Releases.
- On-device behavior has not been machine-verified yet — first physical run
  may need minor fixes.

## [1.0.0] — 2026-11-24 · Initial release

First public release: Nebula Run demo game, on-device 2D scene editor
(hierarchy / gizmos / inspector / save-load / sandbox playtest), and the
dependency-free C++17 engine core (ECS, physics, GLES batcher, particles,
scene serialization). See git history for details.
