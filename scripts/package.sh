#!/bin/bash
# Builds a signed, notarized installer .pkg (VST3 + AU + Standalone) from the stapled bundles.
#
#   scripts/package.sh "Developer ID Installer: Your Name (TEAMID)" <notary-keychain-profile> [version]
#
# Run scripts/sign_notarize.sh first so the bundles inside the package are already signed and stapled.
set -euo pipefail
INSTALLER_ID="${1:?usage: $0 \"Developer ID Installer: Name (TEAMID)\" <notary-profile> [version]}"
PROFILE="${2:?missing notarytool keychain profile name}"
VERSION="${3:-0.1.0}"
cd "$(dirname "$0")/.."

ART="build/MelodicSequencer_artefacts/Release"
ROOT="$(mktemp -d)"; trap 'rm -rf "$ROOT"' EXIT
mkdir -p "$ROOT/VST3" "$ROOT/Components" "$ROOT/Apps" dist

ditto "$ART/VST3/Melodic Sequencer.vst3"        "$ROOT/VST3/Melodic Sequencer.vst3"
ditto "$ART/AU/Melodic Sequencer.component"      "$ROOT/Components/Melodic Sequencer.component"
ditto "$ART/Standalone/Melodic Sequencer.app"    "$ROOT/Apps/Melodic Sequencer.app"

pkgbuild --root "$ROOT/VST3"       --install-location "/Library/Audio/Plug-Ins/VST3"       --identifier com.yourcompany.melodicsequencer.vst3 --version "$VERSION" "$ROOT/vst3.pkg"
pkgbuild --root "$ROOT/Components" --install-location "/Library/Audio/Plug-Ins/Components" --identifier com.yourcompany.melodicsequencer.au   --version "$VERSION" "$ROOT/au.pkg"
pkgbuild --root "$ROOT/Apps"       --install-location "/Applications"                      --identifier com.yourcompany.melodicsequencer.app  --version "$VERSION" "$ROOT/app.pkg"

productbuild --package "$ROOT/vst3.pkg" --package "$ROOT/au.pkg" --package "$ROOT/app.pkg" \
             --sign "$INSTALLER_ID" "dist/MelodicSequencer-$VERSION.pkg"

xcrun notarytool submit "dist/MelodicSequencer-$VERSION.pkg" --keychain-profile "$PROFILE" --wait
xcrun stapler staple "dist/MelodicSequencer-$VERSION.pkg"
spctl --assess --type install -vv "dist/MelodicSequencer-$VERSION.pkg"
echo "Wrote dist/MelodicSequencer-$VERSION.pkg"
