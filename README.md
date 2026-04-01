Overview
This is a JUCE project designed for real-time guitar amplifier simulation. The repository contains the source code and configuration files required to build and run the application as a VST3 audio plug-in.

Technical Details
Project Type: JUCE Project (.jucer)

Target Format: VST3

Language: C++

Build System: CMake / Projucer

Processing: Real-time digital signal processing (DSP) for guitar amplification.

Project Structure
Source/: Contains the core C++ source files for audio processing and GUI.

NewProject.jucer: The primary JUCE project file for Projucer configuration.

CMakeLists.txt: Build configuration for CMake-based workflows.

Build Instructions
Prerequisites
JUCE

C++ Compiler (MSVC, Clang, or GCC)

CMake (optional, for CMake builds)

Compilation
Open NewProject.jucer in Projucer.

Save and open the project in your preferred IDE (Visual Studio, Xcode, etc.).

Build the solution to generate the VST3 binary.
