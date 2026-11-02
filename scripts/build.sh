#!/bin/bash
# Configure, build and install (COPY_PLUGIN_AFTER_BUILD copies to ~/Library/Audio/Plug-Ins/VST3 and /Components)
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S . -B build -G Xcode
cmake --build build --config Release --target MelodicSequencer_VST3 MelodicSequencer_AU MelodicSequencer_Standalone
echo "Installed to ~/Library/Audio/Plug-Ins/VST3"
