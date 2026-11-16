cmake_minimum_required(VERSION 3.22)

project(VoxTune VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")

# Binaires universels (Intel + Apple Silicon) sur macOS
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
endif()

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

set(VOXTUNE_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND VOXTUNE_FORMATS AU)
endif()

juce_add_plugin(VoxTune
    COMPANY_NAME                "VoxTune"
    BUNDLE_ID                   "com.voxtune.voxtune"
    PLUGIN_MANUFACTURER_CODE    Vxtn
    PLUGIN_CODE                 Vxt1
    FORMATS                     ${VOXTUNE_FORMATS}
    PRODUCT_NAME                "VoxTune"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    VST3_CATEGORIES             Fx Pitch Shift
    AU_MAIN_TYPE                kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD     FALSE)

target_sources(VoxTune PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(VoxTune PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(MSVC)
    target_compile_options(VoxTune PRIVATE /utf-8)
endif()

target_link_libraries(VoxTune
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Test du moteur DSP (indépendant de JUCE)
add_executable(EngineTest tests/engine_test.cpp)
enable_testing()
add_test(NAME EngineTest COMMAND EngineTest)
