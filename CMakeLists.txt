cmake_minimum_required(VERSION 3.22)

project(RockGlueArchitecture VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

include(FetchContent)

set(JUCE_VERSION 8.0.4 CACHE STRING "JUCE version to fetch")
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        ${JUCE_VERSION}
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

set(RG_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND RG_FORMATS AU)
endif()

juce_add_plugin(RockGlue
    COMPANY_NAME              "HumHouse"
    BUNDLE_ID                 "com.humhouse.rockglue"
    PLUGIN_MANUFACTURER_CODE  Hhm1
    PLUGIN_CODE               HhRg
    FORMATS                   ${RG_FORMATS}
    PRODUCT_NAME              "RockGlue Architecture"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    VST3_CATEGORIES           "Fx" "Dynamics" "Mastering"
    AU_MAIN_TYPE              "kAudioUnitType_Effect"
)

juce_generate_juce_header(RockGlue)

set(RG_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Parameters.h
    Source/DSP/Biquad.h
    Source/DSP/FetCompressor.h
    Source/DSP/VcaBusCompressor.h
    Source/DSP/BassAnchor.h
    Source/DSP/PocketEq.h
    Source/DSP/RockGlueEngine.h
)

target_sources(RockGlue PRIVATE ${RG_SOURCES})

target_compile_definitions(RockGlue PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(RockGlue PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_plugin_client
    juce::juce_dsp
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# --- Tests -------------------------------------------------------------------
# The DSP core is header-only and JUCE-free, so the latency guarantee and the
# per-node behaviour test as a plain executable.
enable_testing()

add_executable(RockGlueEngineTests tests/EngineTests.cpp)
target_compile_features(RockGlueEngineTests PRIVATE cxx_std_17)
target_include_directories(RockGlueEngineTests PRIVATE Source)
add_test(NAME RockGlueEngineTests COMMAND RockGlueEngineTests)

# The processor test exercises the real plug-in: bus layouts, parameter
# state round-trip and the reported latency.
juce_add_console_app(RockGlueProcessorTests PRODUCT_NAME "RockGlueProcessorTests")
juce_generate_juce_header(RockGlueProcessorTests)
target_sources(RockGlueProcessorTests PRIVATE
    tests/ProcessorTests.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)
target_compile_definitions(RockGlueProcessorTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STANDALONE_APPLICATION=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JucePlugin_Name="RockGlue Architecture"
    JucePlugin_IsSynth=0
    JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0
    JucePlugin_IsMidiEffect=0
    JucePlugin_VersionString="${PROJECT_VERSION}"
    JucePlugin_ManufacturerCode=0x48686d31
    JucePlugin_PluginCode=0x48685267)
target_link_libraries(RockGlueProcessorTests PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
add_test(NAME RockGlueProcessorTests COMMAND RockGlueProcessorTests)
