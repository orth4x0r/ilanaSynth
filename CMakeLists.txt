cmake_minimum_required(VERSION 3.22)

project(ilanaSynth VERSION 1.2.0 LANGUAGES C CXX)

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
    src/dsp/WavetableDoc.cpp
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

# M7.5: ilanaSynth FX, the same engine as an effect that takes audio input.
juce_add_plugin(ilanaSynthFX
    COMPANY_NAME "Ilana Audio"
    BUNDLE_ID "com.ilanaaudio.ilanasynthfx"
    PLUGIN_MANUFACTURER_CODE Ilan
    PLUGIN_CODE Ilnf
    FORMATS VST3 Standalone
    PRODUCT_NAME "ilanaSynth FX"
    DESCRIPTION "ilanaSynth as an effect: live input into bodies, strings, grains and filters"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx
)

target_sources(ilanaSynthFX PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/dsp/Wavetable.cpp
    src/dsp/WavetableDoc.cpp
    src/dsp/TableFactory.cpp
    src/dsp/Voice.cpp
)

target_compile_definitions(ilanaSynthFX PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    ILANA_FX=1
)

target_link_libraries(ilanaSynthFX
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
    tests/SamplingProfiler.cpp
    src/PluginProcessor.cpp
    src/dsp/Wavetable.cpp
    src/dsp/WavetableDoc.cpp
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

# Symbols for the optimised test build, so ILANA_PROFILE can name lines.
if (MSVC)
    target_compile_options(ilanaTableTest PRIVATE /Zi)
    target_link_options(ilanaTableTest PRIVATE /DEBUG /OPT:REF /OPT:ICF)
    target_link_libraries(ilanaTableTest PRIVATE dbghelp winmm)
endif()

add_test(NAME table_tests COMMAND ilanaTableTest)

# Dev tool: offscreen PNG snapshots of every editor tab (not shipped).
juce_add_console_app(ilanaSnapshot PRODUCT_NAME "ilanaSnapshot")

target_sources(ilanaSnapshot PRIVATE
    tools/Snapshot.cpp
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/dsp/Wavetable.cpp
    src/dsp/WavetableDoc.cpp
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
    src/dsp/WavetableDoc.cpp
    src/dsp/TableFactory.cpp
    src/dsp/Voice.cpp
)

target_include_directories(ilanaFingerprint PRIVATE src)

target_compile_definitions(ilanaFingerprint PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    ILANA_TEST_BUILD=1
    ILANA_FINGERPRINT_BUILD=1
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

# Dev tool: renders reference notes from an installed VST3's default sound,
# for fitting modelled sounds (not shipped).
juce_add_console_app(ilanaRefHost PRODUCT_NAME "ilanaRefHost")

target_sources(ilanaRefHost PRIVATE tools/RefHost.cpp)

target_compile_definitions(ilanaRefHost PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_PLUGINHOST_VST3=1
)

target_link_libraries(ilanaRefHost
    PRIVATE
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
)

# M7.5: ilanaSynth FX's tests, on the engine built as the effect.
juce_add_console_app(ilanaFxTest PRODUCT_NAME "ilanaFxTest")

target_sources(ilanaFxTest PRIVATE
    tests/FxTest.cpp
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/dsp/Wavetable.cpp
    src/dsp/WavetableDoc.cpp
    src/dsp/TableFactory.cpp
    src/dsp/Voice.cpp
)

target_include_directories(ilanaFxTest PRIVATE src)

target_compile_definitions(ilanaFxTest PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    ILANA_FX=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JucePlugin_Name="ilanaSynth FX"
)

target_link_libraries(ilanaFxTest
    PRIVATE
        ilanaFonts
        juce::juce_dsp
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
)

if (MSVC)
    target_compile_options(ilanaFxTest PRIVATE /Zi)
    target_link_options(ilanaFxTest PRIVATE /DEBUG /OPT:REF /OPT:ICF)
    target_link_libraries(ilanaFxTest PRIVATE dbghelp)
endif()

add_test(NAME fx_tests COMMAND ilanaFxTest)
