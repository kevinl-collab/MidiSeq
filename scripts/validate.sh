#!/bin/bash
# Runs pluginval (strictness 10) on the installed VST3 and auval on the AU.
#   brew install --cask pluginval      (or download from https://github.com/Tracktion/pluginval/releases)
set -euo pipefail

PLUGINVAL="${PLUGINVAL:-/Applications/pluginval.app/Contents/MacOS/pluginval}"
[ -x "$PLUGINVAL" ] || PLUGINVAL="$(command -v pluginval || true)"
[ -n "$PLUGINVAL" ] || { echo "pluginval not found (set PLUGINVAL=/path/to/pluginval)"; exit 1; }

echo "== pluginval, strictness 10, VST3"
"$PLUGINVAL" --strictness-level 10 --validate-in-process \
    "$HOME/Library/Audio/Plug-Ins/VST3/Melodic Sequencer.vst3"

if [ -e "$HOME/Library/Audio/Plug-Ins/Components/Melodic Sequencer.component" ]; then
    echo "== auval (type aumu, subtype Msq1, manufacturer Ykco)"
    auval -v aumu Msq1 Ykco
fi
