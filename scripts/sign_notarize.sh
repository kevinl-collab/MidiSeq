#!/bin/bash
# Codesign (Developer ID, hardened runtime), notarize with notarytool, and staple the built plugin bundles.
#
#   scripts/sign_notarize.sh "Developer ID Application: Your Name (TEAMID)" <notary-keychain-profile>
#
# One-time setup of the notary profile (needs a paid Apple Developer account + an app-specific password):
#   xcrun notarytool store-credentials <notary-keychain-profile> \
#       --apple-id you@example.com --team-id TEAMID --password <app-specific-password>
#
# Signs whatever exists of: VST3, AU component, Standalone app - from the build tree, not the install folders.
set -euo pipefail

IDENTITY="${1:?usage: $0 \"Developer ID Application: Name (TEAMID)\" <notary-profile> [build-dir]}"
PROFILE="${2:?missing notarytool keychain profile name}"
BUILD_DIR="${3:-build}"
cd "$(dirname "$0")/.."

ARTEFACTS="$BUILD_DIR/MelodicSequencer_artefacts/Release"
BUNDLES=()
for b in "$ARTEFACTS/VST3/Melodic Sequencer.vst3" \
         "$ARTEFACTS/AU/Melodic Sequencer.component" \
         "$ARTEFACTS/Standalone/Melodic Sequencer.app"; do
    [ -e "$b" ] && BUNDLES+=("$b")
done
[ ${#BUNDLES[@]} -gt 0 ] || { echo "No built bundles under $ARTEFACTS - run scripts/build.sh first"; exit 1; }

echo "== Signing"
for b in "${BUNDLES[@]}"; do
    codesign --force --options runtime --timestamp --sign "$IDENTITY" "$b"
    codesign --verify --strict --verbose=2 "$b"
done

echo "== Notarizing"
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
for b in "${BUNDLES[@]}"; do ditto "$b" "$STAGE/$(basename "$b")"; done
ditto -c -k --sequesterRsrc "$STAGE" "$STAGE.zip"
xcrun notarytool submit "$STAGE.zip" --keychain-profile "$PROFILE" --wait
rm -f "$STAGE.zip"

echo "== Stapling"
for b in "${BUNDLES[@]}"; do
    xcrun stapler staple "$b"
    xcrun stapler validate "$b"
done

echo "== Gatekeeper check (the app is assessed as an executable; plugin bundles are validated via their ticket)"
for b in "${BUNDLES[@]}"; do
    case "$b" in
        *.app) spctl --assess --type execute -vv "$b" ;;
    esac
done

echo "Done. Re-install the stapled bundles with scripts/install.sh (or package them with scripts/package.sh)."
