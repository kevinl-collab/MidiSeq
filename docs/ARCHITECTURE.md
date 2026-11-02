# Melodic Sequencer – Architecture

## Data model (Engine/, plain C++, no juce_gui)
```
Project { version, Global, scenes[], customScales[] }
Global  { shuffleType, shuffleAmount, scale, root, range, offset, transpose,
          midiTransposeOn, inChannel, stepLength, stepCount, launchQuantize }
Scene   { name, tracks[] }              // each scene holds a full pattern per track
Track   { outNote, inNote, outChannel, transposeOffset, velScale, gain, mute, solo,
          direction, stepLength, stepCount, shuffle(linked?), chokeGroup,
          followTranspose, euclid{pulses,steps,rotation,live}, steps[] }
Step    { active, velocity, length, shift, probability, reps, interval, shape,
          curve, gain, condition, pitch }
```
Persistence: Project ↔ juce::ValueTree (property `stateVersion`), saved in
get/setStateInformation. Automatable values live in AudioProcessorValueTreeState.

## Thread communication
- UI → audio: edits are written to the ValueTree on the message thread, then a
  **pattern snapshot** (immutable, pre-sized `Project` copy) is published through a
  triple-buffer / atomic pointer swap; the audio thread picks up the newest snapshot
  at block start. Discrete commands (launch scene, audition pad, record arm) go through
  a fixed-size lock-free SPSC FIFO (`juce::AbstractFifo`).
- Audio → UI: playhead positions per track and queued-scene state via atomics; UI
  polls them from a 30–60 Hz timer.
- Audio thread: no allocation, locks, or I/O. Event pools are fixed-capacity arrays
  sized at prepareToPlay.

## Scheduler (stage 3)
Per block: read PositionInfo → compute block PPQ range → per track, step through
step boundaries in range (with shuffle/shift offset) → emit note-on/off at exact sample
offsets; pending note-offs carry across blocks; transport jump/loop/stop/bypass flush
all-notes-off.

## Stages
1. CMake skeleton – **done** (empty VST3/AU/Standalone, silent stereo out, MIDI in/out)
2. Engine + Catch2 tests - **written, not yet compiled** (Engine/, tests/)  3. Host-synced MIDI out - done  4. UI panels - written (Source/UI/)
5. Recording, scenes, MIDI transpose - written (Engine/Sequencer.h, Recording.h, Metronome.h; Source/ProjectModel)  6. Presets, export, signing

