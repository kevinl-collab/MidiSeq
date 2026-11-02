# Build, test, sign, ship (macOS)

Prerequisites: Xcode (with command-line tools), CMake, Git.

```bash
brew install cmake git
chmod +x scripts/*.sh
```

## Configure, build, install

```bash
bash scripts/build.sh
```

That is `cmake -S . -B build -G Xcode` followed by a Release build of the VST3, AU and Standalone targets
(universal arm64 + x86_64, macOS 12+). JUCE 8 is fetched by CMake on first configure. Each build copies the
plugins into `~/Library/Audio/Plug-Ins/VST3` and `~/Library/Audio/Plug-Ins/Components`.

## Tests

Engine tests (Catch2, no JUCE needed, fast):

```bash
cmake -S . -B build-tests -DMS_ENGINE_ONLY=ON && cmake --build build-tests -j && ctest --test-dir build-tests --output-on-failure
```

Session save/restore + MIDI export tests (JUCE console app, part of the normal build):

```bash
cmake --build build --config Release --target SerializerTests && ctest --test-dir build -C Release -R SerializerTests --output-on-failure
```

## Validate

```bash
bash scripts/validate.sh
```

Runs pluginval at strictness 10 on the VST3 and `auval` on the AU. Also load the plugin in JUCE's
AudioPluginHost (build it from the JUCE checkout in `build/_deps/juce-src/extras/AudioPluginHost`).

## Sign and notarize (needs a paid Apple Developer account)

One-time: create a "Developer ID Application" certificate in Xcode, and store notary credentials:

```bash
xcrun notarytool store-credentials melodic-notary --apple-id you@example.com --team-id TEAMID --password <app-specific-password>
```

Then, after a Release build:

```bash
bash scripts/sign_notarize.sh "Developer ID Application: Your Name (TEAMID)" melodic-notary
bash scripts/install.sh
```

`sign_notarize.sh` signs with the hardened runtime and a secure timestamp, submits all bundles to Apple, waits,
and staples the tickets. For a single installer to hand to other people (also needs a "Developer ID Installer" certificate):

```bash
bash scripts/package.sh "Developer ID Installer: Your Name (TEAMID)" melodic-notary 0.1.0
```

Before shipping, replace the placeholder `COMPANY_NAME`, `BUNDLE_ID`, `PLUGIN_MANUFACTURER_CODE` ("Ykco") and
`PLUGIN_CODE` ("Msq1") in `CMakeLists.txt` (and the matching ids in `scripts/package.sh` and `scripts/validate.sh`).
The manufacturer code must contain at least one uppercase letter.

## Where things live

| What | Where |
| --- | --- |
| User presets (`*.mseq`) | `~/Library/Application Support/<Company>/Melodic Sequencer/Presets` |
| Custom scales (shared by all instances) | `~/Library/Application Support/<Company>/Melodic Sequencer/UserScales.xml` |
| Session state | inside the DAW project (`getStateInformation`), format version `stateVersion` = 2 |

Older sessions (version 1: parameters only) still load; their pattern starts from the default project.
A future format change adds one `if (version < N)` block to `ser::migrate` in `Source/ProjectSerializer.cpp`.

## DAW test checklist

1. Load in Reaper, Ableton Live, Bitwig, Cubase on Apple Silicon; route the plugin's MIDI to a synth track.
2. Play / stop / loop / jump the playhead / change tempo: notes stay on the grid, nothing hangs.
3. Save the project, close, reopen: grid, scenes, scale, preset name and parameters are restored.
4. Export MIDI (single scene and all scenes) and drag the `.mid` handle onto a DAW track.
5. Save a user preset, restart the DAW, load it; save a custom scale and see it in a second instance.
