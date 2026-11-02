# Header-only, JUCE-free engine. Include as "Engine/Scheduler.h" etc.
add_library(MelodicEngine INTERFACE)
target_include_directories(MelodicEngine INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(MelodicEngine INTERFACE cxx_std_17)
