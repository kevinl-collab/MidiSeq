cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")

project(MelodicSequencer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MS_BUILD_TESTS "Build engine unit tests" ON)
option(MS_ENGINE_ONLY  "Only build the engine + tests (no JUCE)" OFF)

add_subdirectory(Engine)
if(MS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
if(MS_ENGINE_ONLY)
    return()
endif()

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

set(MS_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND MS_FORMATS AU)
endif()

# IS_SYNTH TRUE makes the VST3 register as an Instrument (with a silent stereo
# output bus) so Ableton/Bitwig/Cubase/Reaper offer MIDI routing from it.
# NOTE: IS_MIDI_EFFECT is a single flag for all formats and would strip the audio
# buses from the VST3, so it stays FALSE. Revisit for a dedicated AU 'aumi' build.
juce_add_plugin(MelodicSequencer
    COMPANY_NAME                "YourCompany"
    BUNDLE_ID                   com.yourcompany.melodicsequencer
    PLUGIN_MANUFACTURER_CODE    Ykco
    PLUGIN_CODE                 Msq1
    FORMATS                     ${MS_FORMATS}
    PRODUCT_NAME                "Melodic Sequencer"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Instrument
    COPY_PLUGIN_AFTER_BUILD     TRUE)

target_sources(MelodicSequencer PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ProjectModel.cpp
    Source/ProjectSerializer.cpp
    Source/PresetManager.cpp
    Source/MidiExport.cpp
    Source/UI/GeneratorPanel.cpp
    Source/UI/TopBar.cpp
    Source/UI/TrackArea.cpp
    Source/UI/PadsPanel.cpp
    Source/UI/EditPanel.cpp
    Source/UI/GlobalPanel.cpp)

target_compile_definitions(MelodicSequencer PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MelodicSequencer
    PRIVATE
        MelodicEngine
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# JUCE-side tests (session save/restore, versioning, MIDI export): ctest -C Release  or run the binary directly
if(MS_BUILD_TESTS)
    juce_add_console_app(SerializerTests PRODUCT_NAME "SerializerTests")
    target_sources(SerializerTests PRIVATE
        tests/SerializerTests.cpp
        Source/ProjectSerializer.cpp
        Source/MidiExport.cpp)
    target_compile_definitions(SerializerTests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_UNIT_TESTS=1)
    target_link_libraries(SerializerTests PRIVATE
        MelodicEngine
        juce::juce_core
        juce::juce_events
        juce::juce_data_structures
        juce::juce_audio_basics
        juce::juce_recommended_config_flags)
    add_test(NAME SerializerTests COMMAND SerializerTests)
endif()