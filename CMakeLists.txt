cmake_minimum_required(VERSION 3.24)

# macOS: build one universal binary for Apple Silicon and Intel Macs.
# These must be set before project().
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")

project(ParagonPR7M VERSION 0.6.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(PARAGON_BUILD_TESTS "Build the unit tests" ON)

include(cmake/Dependencies.cmake)

juce_add_plugin(ParagonPR7M
    PRODUCT_NAME             "Paragon PR7-M"
    COMPANY_NAME             "Paragon"
    BUNDLE_ID                "com.paragon.paragonpr7m"
    PLUGIN_MANUFACTURER_CODE Prgn     # the same maker as the rest of the PR7 family
    PLUGIN_CODE              Pr7m     # four characters, exactly one upper case; unique in the family
    FORMATS                  VST3
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    VST3_CATEGORIES          Fx Mastering
    COPY_PLUGIN_AFTER_BUILD  FALSE)

# Inter (SIL Open Font License, see resources/fonts/OFL-LICENSE.txt), built
# into the plugin so it looks the same on every computer.
juce_add_binary_data(ParagonPR7MFonts
    HEADER_NAME ParagonFonts.h
    NAMESPACE   ParagonFonts
    SOURCES
        resources/fonts/Inter-Regular.ttf
        resources/fonts/Inter-SemiBold.ttf
        resources/fonts/InterDisplay-BlackItalic.ttf)

target_sources(ParagonPR7M
    PRIVATE
        source/Parameters.cpp
        source/PluginProcessor.cpp
        source/PluginEditor.cpp
        source/ui/MainView.cpp
        source/ui/VuMeter.cpp
        source/ui/Controls.cpp
        source/ui/LevelMeter.cpp
        source/ui/Logo.cpp
        source/ui/LedDisplay.cpp
        source/ui/LoudnessDisplay.cpp
        source/ui/ParagonLookAndFeel.cpp
        source/ui/Finish.cpp)

target_include_directories(ParagonPR7M PUBLIC source)

target_compile_definitions(ParagonPR7M
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(ParagonPR7M
    PRIVATE
        ParagonPR7MFonts
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_warning_flags   # strict warnings for our code only
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

if(PARAGON_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
