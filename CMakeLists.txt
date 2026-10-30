cmake_minimum_required(VERSION 3.22)
project(ForgeAmp VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(FORGE_BUILD_TESTS "Build the offline test / render harness (ForgeAmpTests)" ON)

# ── Locate JUCE ───────────────────────────────────────────────────────────────
# 1. -DJUCE_DIR=<path> wins.
# 2. Otherwise look in the usual places next to the project (the old layout
#    expected JUCE two folders up, so that still works).
# 3. Otherwise download a pinned JUCE release with FetchContent.
set(JUCE_DIR "" CACHE PATH "Path to a JUCE checkout. Leave empty to auto-detect or download.")
set(FORGE_JUCE_TAG "9.0.4" CACHE STRING "JUCE release fetched when no local checkout is found")

if(NOT JUCE_DIR)
    foreach(_candidate
            "${CMAKE_CURRENT_SOURCE_DIR}/JUCE"
            "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../JUCE")
        if(EXISTS "${_candidate}/CMakeLists.txt")
            set(JUCE_DIR "${_candidate}")
            break()
        endif()
    endforeach()
endif()

if(JUCE_DIR)
    message(STATUS "Using JUCE from ${JUCE_DIR}")
    add_subdirectory("${JUCE_DIR}" JUCE)
else()
    message(STATUS "No local JUCE found - fetching JUCE ${FORGE_JUCE_TAG}")
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        ${FORGE_JUCE_TAG}
        GIT_SHALLOW    TRUE
        GIT_CONFIG     core.longpaths=true)   # JUCE's iOS examples exceed Windows' 260-char path limit
    FetchContent_MakeAvailable(JUCE)
endif()

# ── Plugin ────────────────────────────────────────────────────────────────────
set(FORGE_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND FORGE_FORMATS AU)
endif()

juce_add_plugin(ForgeAmp
    COMPANY_NAME                "kaanceran"
    PRODUCT_NAME                "Forge Amp"
    VERSION                     ${PROJECT_VERSION}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    PLUGIN_MANUFACTURER_CODE    Kcrn
    PLUGIN_CODE                 Frg1
    VST3_CATEGORIES             Fx Distortion
    AU_MAIN_TYPE                kAudioUnitType_Effect
    FORMATS                     ${FORGE_FORMATS})

juce_generate_juce_header(ForgeAmp)

# Everything except the plugin entry point is shared with the test harness.
set(FORGE_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters.cpp
    Source/PresetManager.cpp
    Source/ui/Pixel.cpp
    Source/ui/PixelLookAndFeel.cpp
    Source/ui/Widgets.cpp
    Source/ui/Pages.cpp)

target_sources(ForgeAmp PRIVATE ${FORGE_SOURCES})
target_include_directories(ForgeAmp PRIVATE Source)
target_compile_features(ForgeAmp PRIVATE cxx_std_17)

set(FORGE_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    FORGE_VERSION_STRING="${PROJECT_VERSION}")

target_compile_definitions(ForgeAmp PUBLIC ${FORGE_DEFINITIONS})

target_link_libraries(ForgeAmp
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ── Offline test / render harness ─────────────────────────────────────────────
# Runs the real processor without a host: sanity checks, latency check, CPU
# benchmark, renders every factory preset to WAV and snapshots the editor.
if(FORGE_BUILD_TESTS)
    juce_add_console_app(ForgeAmpTests PRODUCT_NAME "ForgeAmpTests")
    juce_generate_juce_header(ForgeAmpTests)
    target_sources(ForgeAmpTests PRIVATE Tests/AmpTests.cpp ${FORGE_SOURCES})
    target_include_directories(ForgeAmpTests PRIVATE Source)
    target_compile_features(ForgeAmpTests PRIVATE cxx_std_17)
    target_compile_definitions(ForgeAmpTests PRIVATE ${FORGE_DEFINITIONS} FORGE_TEST_BUILD=1 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_PLUGINHOST_VST3=1)
    target_link_libraries(ForgeAmpTests
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()
