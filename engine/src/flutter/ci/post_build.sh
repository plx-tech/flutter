#!/bin/bash
set -e

# Post-build packaging script for Clay engine artifacts
# Must be run from engine/src directory

if [[ ! "$(pwd)" == */engine/src ]]; then
  echo "Error: must run from engine/src directory"
  exit 1
fi

cd flutter
ENGINE_HASH=$(git rev-parse HEAD)
cd ..

DEPLOY_DIR="deploy/${ENGINE_HASH}"

if [ ! -d "$DEPLOY_DIR" ]; then
  echo "Error: deploy directory not found: $DEPLOY_DIR"
  exit 1
fi

echo "Engine hash: ${ENGINE_HASH}"
echo "Deploy dir: ${DEPLOY_DIR}"

# --- Steps 10-11: navigate into deploy artifacts ---
FLUTTER_IO_DIR="${DEPLOY_DIR}/download.flutter.io/io/flutter"

if [ ! -d "$FLUTTER_IO_DIR" ]; then
  echo "Error: download.flutter.io/io/flutter not found in deploy dir"
  exit 1
fi

# --- Steps 12-22: Create x86_debug from x86_64_debug ---
X86_64_DIR="${FLUTTER_IO_DIR}/x86_64_debug"
X86_DIR="${FLUTTER_IO_DIR}/x86_debug"
VERSION_DIR="1.0.0-${ENGINE_HASH}"

if [ -d "$X86_64_DIR" ]; then
  echo "Creating x86_debug from x86_64_debug..."

  # Step 12: copy and rename folder
  rm -rf "$X86_DIR"
  cp -r "$X86_64_DIR" "$X86_DIR"

  WORK_DIR="${X86_DIR}/${VERSION_DIR}"

  if [ ! -d "$WORK_DIR" ]; then
    echo "  Error: version directory not found: $WORK_DIR"
    exit 1
  fi

  # Step 13-14: rename .pom file
  POM_OLD="${WORK_DIR}/x86_64_debug-${VERSION_DIR}.pom"
  POM_NEW="${WORK_DIR}/x86_debug-${VERSION_DIR}.pom"
  if [ -f "$POM_OLD" ]; then
    mv "$POM_OLD" "$POM_NEW"
    # Step 15-16: change artifactId in pom
    sed -i '' 's|<artifactId>x86_64_debug</artifactId>|<artifactId>x86_debug</artifactId>|g' "$POM_NEW"
    echo "  POM renamed and updated"
  else
    echo "  Warning: POM file not found: $POM_OLD"
  fi

  # Step 17: rename .jar to .zip
  JAR_FILE="${WORK_DIR}/x86_64_debug-${VERSION_DIR}.jar"
  ZIP_FILE="${WORK_DIR}/x86_64_debug-${VERSION_DIR}.zip"
  if [ -f "$JAR_FILE" ]; then
    mv "$JAR_FILE" "$ZIP_FILE"

    # Step 18: unzip
    cd "$WORK_DIR"
    unzip -q "x86_64_debug-${VERSION_DIR}.zip"

    # Step 19: rename lib/x86_64 to lib/x86
    if [ -d "lib/x86_64" ]; then
      mv "lib/x86_64" "lib/x86"
    fi

    # Step 20-21: compress and rename to final jar
    zip -rq "x86_debug-${VERSION_DIR}.jar" lib/

    # Step 22: cleanup
    rm -rf lib/
    rm -f "x86_64_debug-${VERSION_DIR}.zip"

    cd - > /dev/null
    echo "  JAR repackaged as x86_debug"
  else
    echo "  Warning: JAR file not found: $JAR_FILE"
  fi
else
  echo "Warning: x86_64_debug directory not found, skipping x86_debug creation"
fi

# --- Steps 23-25: Repackage web SDK ---
WEB_SDK_ZIP="${DEPLOY_DIR}/flutter-web-sdk.zip"

if [ -f "$WEB_SDK_ZIP" ]; then
  echo "Repackaging flutter-web-sdk..."

  cd "$DEPLOY_DIR"

  # Step 24: unzip into a flutter-web-sdk directory
  mkdir -p flutter-web-sdk
  cd flutter-web-sdk
  unzip -qo "../flutter-web-sdk.zip"

  # Step 25: rename canvaskit to engine hash
  if [ -d "canvaskit" ]; then
    mv "canvaskit" "${ENGINE_HASH}"
    echo "  canvaskit renamed to ${ENGINE_HASH}"
  else
    echo "  Warning: canvaskit not found inside flutter-web-sdk.zip"
  fi

  cd - > /dev/null
  cd - > /dev/null
else
  echo "Warning: flutter-web-sdk.zip not found, skipping web SDK repackaging"
fi

echo ""
echo "Post-build packaging complete!"
echo "Engine hash: ${ENGINE_HASH}"
echo "Deploy dir: ${DEPLOY_DIR}"
echo ""
echo "Next steps (manual):"
echo "  1. Upload ${DEPLOY_DIR}/download.flutter.io to GCS bucket clay-flutter-storage/download.flutter.io"
echo "  2. Upload ${DEPLOY_DIR}/flutter-web-sdk/${ENGINE_HASH} to GCS bucket clay-flutter-storage/flutter-canvaskit"
echo "  3. Upload ${DEPLOY_DIR} to GCS bucket clay-flutter-storage/flutter_infra_release/flutter"
echo "  4. Upload flutter_infra_release/flutter/${ENGINE_HASH}/ios-release/Flutter.dSYM.zip to Crashlytics"
