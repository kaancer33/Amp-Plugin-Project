cmake_minimum_required(VERSION 3.22)
project(NewProject VERSION 0.9.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Path to JUCE. Override with -DJUCE_PATH=<dir>; otherwise common locations are searched.
set(JUCE_PATH "" CACHE PATH "Path to the JUCE source tree")
if(NOT JUCE_PATH)
    foreach(_candidate
        "${CMAKE_CURRENT_SOURCE_DIR}/../../JUCE"
        "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE"
        "${CMAKE_CURRENT_SOURCE_DIR}/JUCE"
    )
        if(EXISTS "${_candidate}/CMakeLists.txt")
            set(JUCE_PATH "${_candidate}")
            break()
        endif()
    endforeach()
endif()
if(NOT JUCE_PATH OR NOT EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found. Pass -DJUCE_PATH=<path to JUCE source tree>.")
endif()
message(STATUS "Using JUCE from: ${JUCE_PATH}")
add_subdirectory("${JUCE_PATH}" JUCE)

juce_add_plugin(NewProject
    COMPANY_NAME               "FruGuitar Audio"
    IS_SYNTH                   FALSE
    NEEDS_MIDI_INPUT           FALSE
    NEEDS_MIDI_OUTPUT          FALSE
    IS_MIDI_EFFECT             FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD    FALSE
    PLUGIN_MANUFACTURER_CODE   Frgt
    PLUGIN_CODE                Frga
    FORMATS                    VST3 Standalone
    PRODUCT_NAME               "FruGuitar Audio"
)

# Generate JuceHeader.h for CMake builds
juce_generate_juce_header(NewProject)

# ── NAM Core source files ────────────────────────────────────────────────────
set(NAM_SOURCES
    Source/NAM/activations.cpp
    Source/NAM/container.cpp
    Source/NAM/conv1d.cpp
    Source/NAM/convnet.cpp
    Source/NAM/dsp.cpp
    Source/NAM/get_dsp.cpp
    Source/NAM/lstm.cpp
    Source/NAM/ring_buffer.cpp
    Source/NAM/slimmable_wavenet.cpp
    Source/NAM/util.cpp
    Source/NAM/wavenet.cpp
)

target_sources(NewProject PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PresetManager.cpp
    ${NAM_SOURCES}
)

# C++17 for JUCE, NAM Core uses some C++17/20 features but builds fine with 17
target_compile_features(NewProject PRIVATE cxx_std_17)

# ── Include paths ────────────────────────────────────────────────────────────
# NAM headers are in Source/NAM/, they include "json.hpp" and <Eigen/Dense>
target_include_directories(NewProject PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/eigen
    ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/nlohmann
)

# NAM headers include "json.hpp" — make nlohmann dir available to NAM sources
# Also NAM includes <Eigen/Dense> — Eigen dir needs to be on include path
# The NAM .cpp files include headers like "activations.h" relative to NAM/
target_include_directories(NewProject PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/NAM
)

target_compile_definitions(NewProject
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(NewProject PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
