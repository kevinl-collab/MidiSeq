#!/bin/bash
# Copies the (signed / stapled) bundles from the build tree into the user plug-in folders.
set -euo pipefail
cd "$(dirname "$0")/.."
ART="${1:-build}/MelodicSequencer_artefacts/Release"

mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3" "$HOME/Library/Audio/Plug-Ins/Components"
[ -e "$ART/VST3/Melodic Sequencer.vst3" ] && ditto "$ART/VST3/Melodic Sequencer.vst3" "$HOME/Library/Audio/Plug-Ins/VST3/Melodic Sequencer.vst3"
[ -e "$ART/AU/Melodic Sequencer.component" ] && ditto "$ART/AU/Melodic Sequencer.component" "$HOME/Library/Audio/Plug-Ins/Components/Melodic Sequencer.component"
killall -9 AudioComponentRegistrar 2>/dev/null || true   # make hosts see the new AU
echo "Installed to ~/Library/Audio/Plug-Ins"
