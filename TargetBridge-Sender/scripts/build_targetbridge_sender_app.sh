#!/bin/zsh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
REPO_ROOT="$(cd "$ROOT/.." && pwd)"
DERIVED_DATA_DIR="${ROOT}/.build/DerivedData"
CONFIGURATION="${CONFIGURATION:-Debug}"
BUILD_DIR="${DERIVED_DATA_DIR}/Build/Products/${CONFIGURATION}"
SOURCE_APP="${BUILD_DIR}/TargetBridge.app"
DEST_DIR="${REPO_ROOT}/build"
DEST_APP="${DEST_DIR}/TargetBridge.app"

cd "$ROOT"

xcodegen generate

# SENDER_ARCH builds for an architecture other than the host's, e.g. the
# Intel Sender on an Apple Silicon runner: SENDER_ARCH=x86_64.
ARCH_SETTINGS=()
if [[ -n "${SENDER_ARCH:-}" ]]; then
  ARCH_SETTINGS=(ARCHS="$SENDER_ARCH" ONLY_ACTIVE_ARCH=NO)
fi

xcodebuild \
  -scheme TBDisplaySender \
  -configuration "$CONFIGURATION" \
  -derivedDataPath "$DERIVED_DATA_DIR" \
  CODE_SIGN_IDENTITY="" \
  CODE_SIGNING_REQUIRED=NO \
  CODE_SIGNING_ALLOWED=NO \
  "${ARCH_SETTINGS[@]}" \
  build

mkdir -p "$DEST_DIR"
rm -rf "$DEST_APP"
ditto "$SOURCE_APP" "$DEST_APP"
echo "Cleaning extended attributes..."
xattr -cr "$DEST_APP" || true
echo "Signing sender application..."
codesign --force --deep --sign - "$DEST_APP" || true
touch "$DEST_APP"

echo "TargetBridge sender built: $DEST_APP"
echo "Local DerivedData: $DERIVED_DATA_DIR"
