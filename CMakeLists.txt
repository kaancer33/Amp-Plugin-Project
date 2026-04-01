cmake_minimum_required(VERSION 3.22)
project(NewProject VERSION 0.1.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Auto-incrementing build number ────────────────────────────────────────────
# Increments every time CMake configures (i.e. every build from VS Code,
# or any time CMakeLists.txt is saved).
set(_BUILD_NUM_FILE "${CMAKE_CURRENT_SOURCE_DIR}/build_number.txt")
if(EXISTS "${_BUILD_NUM_FILE}")
    file(READ "${_BUILD_NUM_FILE}" _BUILD_NUM)
    string(STRIP "${_BUILD_NUM}" _BUILD_NUM)
else()
    set(_BUILD_NUM 0)
endif()
math(EXPR _BUILD_NUM "${_BUILD_NUM} + 1")
file(WRITE "${_BUILD_NUM_FILE}" "${_BUILD_NUM}")
message(STATUS "Build number: ${_BUILD_NUM}")

# Path to JUCE (two levels up from this file, then into JUCE folder)
add_subdirectory("../../JUCE" JUCE)

juce_add_plugin(NewProject
    COMPANY_NAME               "YourCompany"
    IS_SYNTH                   FALSE
    NEEDS_MIDI_INPUT           FALSE
    NEEDS_MIDI_OUTPUT          FALSE
    IS_MIDI_EFFECT             FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD    FALSE
    PLUGIN_MANUFACTURER_CODE   Manu
    PLUGIN_CODE                NewP
    FORMATS                    VST3
    PRODUCT_NAME               "NewProject b${_BUILD_NUM}"
)

# Generate JuceHeader.h for CMake builds (replaces Projucer's JuceLibraryCode version)
juce_generate_juce_header(NewProject)

target_sources(NewProject PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PresetManager.cpp
)

target_compile_features(NewProject PRIVATE cxx_std_17)

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
