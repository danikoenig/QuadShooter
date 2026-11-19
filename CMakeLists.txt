cmake_minimum_required(VERSION 3.22)

project(QuadShooter VERSION 0.2.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: lokaler Pfad (-DJUCE_PATH=...) oder automatischer Download
if(DEFINED JUCE_PATH)
    add_subdirectory(${JUCE_PATH} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(QuadShooter
    COMPANY_NAME "Dani Koenig"
    BUNDLE_ID "ch.danikoenig.quadshooter"
    PLUGIN_MANUFACTURER_CODE Dkng
    PLUGIN_CODE Qds1
    FORMATS VST3 AU Standalone
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    PRODUCT_NAME "Quad Shooter"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Drum Sampler)

target_sources(QuadShooter PRIVATE
    Source/PluginProcessor.cpp
    Source/PitchDetector.cpp
    Source/PluginEditor.cpp
    Source/SlotView.cpp
    Source/XYPad.cpp
    Source/EditStrip.cpp
    Source/Look.cpp
    Source/SampleLibrary.cpp
    Source/LibraryPanel.cpp)

target_compile_definitions(QuadShooter PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(QuadShooter
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optionaler Test (nur lokal): -DSHOOTER_TEST_SRC=tests/EngineTest.cpp
if(SHOOTER_TEST_SRC)
    add_executable(ShooterTest ${SHOOTER_TEST_SRC})
    target_link_libraries(ShooterTest PRIVATE QuadShooter)
    target_include_directories(ShooterTest PRIVATE $<TARGET_PROPERTY:QuadShooter,INCLUDE_DIRECTORIES>)
    target_compile_definitions(ShooterTest PRIVATE $<TARGET_PROPERTY:QuadShooter,COMPILE_DEFINITIONS>)
endif()
