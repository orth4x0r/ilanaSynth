cmake_minimum_required(VERSION 3.22)

project(ilanaSynth VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options(/MP /bigobj)
endif()

include(FetchContent)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.9
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(ilanaSynth
    COMPANY_NAME "Ilana Audio"
    BUNDLE_ID "com.ilanaaudio.ilanasynth"
    PLUGIN_MANUFACTURER_CODE Ilan
    PLUGIN_CODE Ilns
    FORMATS VST3 Standalone
    PRODUCT_NAME "ilanaSynth"
    DESCRIPTION "Aggressive wavetable synthesizer"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
)

juce_add_binary_data(ilanaFonts SOURCES
    resources/fonts/Jost-400-Book.ttf
    resources/fonts/Jost-500-Medium.ttf
    resources/fonts/Jost-700-Bold.ttf
)

target_sources(ilanaSynth PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/dsp/Wavetable.cpp
    src/dsp/TableFactory.cpp
    src/dsp/Voice.cpp
)

target_compile_definitions(ilanaSynth PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
)

target_link_libraries(ilanaSynth
    PRIVATE
        ilanaFonts
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

enable_testing()

juce_add_console_app(ilanaTableTest PRODUCT_NAME "ilanaTableTest")

target_sources(ilanaTableTest PRIVATE
    tests/TableTest.cpp
    src/PluginProcessor.cpp
    src/dsp/Wavetable.cpp
    src/dsp/TableFactory.cpp
    src/dsp/Voice.cpp
)

target_include_directories(ilanaTableTest PRIVATE src)

target_compile_definitions(ilanaTableTest PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    ILANA_TEST_BUILD=1
    JucePlugin_Name="ilanaSynth"
)

target_link_libraries(ilanaTableTest
    PRIVATE
        juce::juce_dsp
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
)

add_test(NAME table_tests COMMAND ilanaTableTest)

# Dev tool: offscreen PNG snapshots of every editor tab (not shipped).
juce_add_console_app(ilanaSnapshot PRODUCT_NAME "ilanaSnapshot")

target_sources(ilanaSnapshot PRIVATE
    tools/Snapshot.cpp
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/dsp/Wavetable.cpp
    src/dsp/TableFactory.cpp
    src/dsp/Voice.cpp
)

target_include_directories(ilanaSnapshot PRIVATE src)

target_compile_definitions(ilanaSnapshot PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JucePlugin_Name="ilanaSynth"
)

target_link_libraries(ilanaSnapshot
    PRIVATE
        ilanaFonts
        juce::juce_dsp
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
)

add_test(NAME ui_tests COMMAND ilanaSnapshot --uitest)

# Dev tool: per-preset render fingerprints for regression checks (not shipped).
juce_add_console_app(ilanaFingerprint PRODUCT_NAME "ilanaFingerprint")

target_sources(ilanaFingerprint PRIVATE
    tools/PresetFingerprint.cpp
    src/PluginProcessor.cpp
    src/dsp/Wavetable.cpp
    src/dsp/TableFactory.cpp
    src/dsp/Voice.cpp
)

target_include_directories(ilanaFingerprint PRIVATE src)

target_compile_definitions(ilanaFingerprint PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    ILANA_TEST_BUILD=1
    JucePlugin_Name="ilanaSynth"
)

target_link_libraries(ilanaFingerprint
    PRIVATE
        juce::juce_dsp
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
)
