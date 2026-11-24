#!/usr/bin/env bash
# Nebula Engine — one-command APK builder.
#
#   ./scripts/build_apk.sh            # debug APK (installable on any device with USB debugging)
#   ./scripts/build_apk.sh release    # release APK (signed if keystore.properties/env is set, else unsigned)
#
# Requires: JDK 17, Android SDK (platform-34, build-tools 34, NDK 25.2.9519653,
# cmake 3.22.1). Set ANDROID_HOME or write a local.properties with
# sdk.dir=/path/to/Android/Sdk. See docs/BUILDING.md for a full walkthrough.
set -euo pipefail
cd "$(dirname "$0")/.."

MODE="${1:-debug}"
case "$MODE" in
  debug|release) ;;
  *) echo "usage: $0 [debug|release]" >&2; exit 2 ;;
esac

# Prefer the Gradle wrapper; fall back to a system gradle; otherwise bootstrap one.
if [ ! -f gradle/wrapper/gradle-wrapper.jar ] && command -v gradle >/dev/null 2>&1; then
  if [ ! -x ./gradlew ] || ! grep -q "distributionUrl" gradle/wrapper/gradle-wrapper.properties 2>/dev/null; then
    echo ":: Generating Gradle wrapper (8.4)…"
    gradle wrapper --gradle-version 8.4 --distribution-type bin --no-daemon
  fi
fi

if [ -x ./gradlew ]; then
  GRADLE=./gradlew
else
  GRADLE=gradle
fi

echo ":: Building $MODE APK with $GRADLE"
"$GRADLE" --no-daemon --stacktrace ":app:assemble${MODE^}"

APK_DIR="app/build/outputs/apk/$MODE"
echo
echo "✔ Build finished. Artifacts:"
find "app/build/outputs/apk" -name '*.apk' -exec ls -lh {} \;
