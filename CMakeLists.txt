cmake_minimum_required(VERSION 3.22)

project(ilanaSynth VERSION 1.3.0 LANGUAGES C CXX)

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

# M9: the formats. VST3 and the standalone everywhere, AU on macOS, and
# CLAP through clap-juce-extensions (ILANA_CLAP=OFF skips it).
option(ILANA_CLAP "Build CLAP versions of both plugins" ON)
if (APPLE)
    set(ILANA_FORMATS VST3 AU Standalone)
else()
    set(ILANA_FORMATS VST3 Standalone)
endif()

juce_add_plugin(ilanaSynth
    COMPANY_NAME "Ilana Audio"
    BUNDLE_ID "com.ilanaaudio.ilanasynth"
    PLUGIN_MANUFACTURER_CODE Ilan
    PLUGIN_CODE Ilns
    FORMATS ${ILANA_FORMATS}
    PRODUCT_NAME "ilanaSynth"
    DESCRIPTION "Physical hybrid synthesizer"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
)

# IlanaSynthAudioProcessor's member functions, split by area (src/processor).
set(ILANA_PROCESSOR_SOURCES
    src/processor/ParameterLayout.cpp
    src/processor/Modulation.cpp
    src/processor/Effects.cpp
    src/processor/Sequencers.cpp
    src/processor/Presets.cpp
    src/processor/State.cpp
    src/processor/Bounce.cpp
    src/processor/Dx7.cpp
)

juce_add_binary_data(ilanaFonts SOURCES
    resources/fonts/Manrope-400-Regular.ttf
    resources/fonts/Manrope-500-Medium.ttf
    resources/fonts/Manrope-700-Bold.ttf
)

target_sources(ilanaSynth PRIVATE
    src/PluginProcessor.cpp
    ${ILANA_PROCESSOR_SOURCES}
    src/MtsEsp.cpp
    src/PluginEditor.cpp
    src/dsp/Wavetable.cpp
    src/dsp/WavetableDoc.cpp
    src/dsp/TableFactory.cpp
    src/dsp/Voice.cpp
    src/dsp/airwindows/Registry.cpp
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
    FORMATS ${ILANA_FORMATS}
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
    ${ILANA_PROCESSOR_SOURCES}
    src/MtsEsp.cpp
    src/PluginEditor.cpp
    src/dsp/Wavetable.cpp
    src/dsp/WavetableDoc.cpp
    src/dsp/TableFactory.cpp
    src/dsp/Voice.cpp
    src/dsp/airwindows/Registry.cpp
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

if (ILANA_CLAP)
    FetchContent_Declare(clap-juce-extensions
        GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
        GIT_TAG 55525c9858d4b25687be7759a5e0f70eccef218e
        GIT_SUBMODULES_RECURSE TRUE
    )
    FetchContent_MakeAvailable(clap-juce-extensions)
    clap_juce_extensions_plugin(TARGET ilanaSynth
        CLAP_ID "com.ilanaaudio.ilanasynth"
        CLAP_FEATURES instrument synthesizer stereo)
    clap_juce_extensions_plugin(TARGET ilanaSynthFX
        CLAP_ID "com.ilanaaudio.ilanasynthfx"
        CLAP_FEATURES audio-effect stereo)
endif()

enable_testing()

juce_add_console_app(ilanaTableTest PRODUCT_NAME "ilanaTableTest")

target_sources(ilanaTableTest PRIVATE
    tests/TableTest.cpp
    tests/SamplingProfiler.cpp
    src/PluginProcessor.cpp
    ${ILANA_PROCESSOR_SOURCES}
    src/MtsEsp.cpp
    src/dsp/Wavetable.cpp
    src/dsp/WavetableDoc.cpp
    src/dsp/TableFactory.cpp
    src/dsp/Voice.cpp
    src/dsp/airwindows/Registry.cpp
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
    # 16 MB stack: the tests keep whole processors on the stack, which outgrew
    # MSVC's 1 MB default (Linux gives 8 MB).
    target_link_options(ilanaTableTest PRIVATE /DEBUG /OPT:REF /OPT:ICF /STACK:16777216)
    target_link_libraries(ilanaTableTest PRIVATE dbghelp winmm)
endif()

add_test(NAME table_tests COMMAND ilanaTableTest)

# The same stack for the dev tools below, which also build processors on the stack.
function (ilana_big_stack target)
    if (MSVC)
        target_link_options(${target} PRIVATE /STACK:16777216)
    endif()
endfunction()

# Dev tool: offscreen PNG snapshots of every editor tab (not shipped).
juce_add_console_app(ilanaSnapshot PRODUCT_NAME "ilanaSnapshot")

target_sources(ilanaSnapshot PRIVATE
    tools/Snapshot.cpp
    src/PluginProcessor.cpp
    ${ILANA_PROCESSOR_SOURCES}
    src/MtsEsp.cpp
    src/PluginEditor.cpp
    src/dsp/Wavetable.cpp
    src/dsp/WavetableDoc.cpp
    src/dsp/TableFactory.cpp
    src/dsp/Voice.cpp
    src/dsp/airwindows/Registry.cpp
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

# Link-time optimisation for the plugins (and the snapshot tool, which
# measures their CPU). Off: with gcc it measured 3-5% slower (2026-10-08),
# and the link takes much more memory. ILANA_LTO=ON to try it elsewhere.
option(ILANA_LTO "Link-time optimisation for the plugins" OFF)
if (ILANA_LTO)
    foreach(target ilanaSynth ilanaSynthFX ilanaSnapshot)
        target_link_libraries(${target} PRIVATE juce::juce_recommended_lto_flags)
    endforeach()
endif()

# Profile-guided build of the VST3 with MSVC (tools/pgo-windows.ps1 runs the
# three steps): GEN builds a counting plugin, a training run (ilanaRefHost
# --train) writes .pgc files next to the profile, USE rebuilds optimised from
# them. The profile belongs to the exact sources and compiler it was made
# with, so it lives in the build folder and is never committed.
set(ILANA_PGO "" CACHE STRING "MSVC profile-guided VST3: GEN, USE or empty")
if (MSVC AND ILANA_PGO)
    set(ILANA_PGD "${CMAKE_BINARY_DIR}/pgo/ilanaSynth.pgd")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/pgo")
    target_compile_options(ilanaSynth PRIVATE /GL)
    target_compile_options(ilanaSynth_VST3 PRIVATE /GL)
    set_property(TARGET ilanaSynth APPEND PROPERTY STATIC_LIBRARY_OPTIONS /LTCG)
    if (ILANA_PGO STREQUAL "GEN")
        target_link_options(ilanaSynth_VST3 PRIVATE /LTCG /GENPROFILE:PGD=${ILANA_PGD})
    elseif (ILANA_PGO STREQUAL "USE")
        target_link_options(ilanaSynth_VST3 PRIVATE /LTCG /USEPROFILE:PGD=${ILANA_PGD})
    else()
        message(FATAL_ERROR "ILANA_PGO is GEN, USE or empty, not ${ILANA_PGO}")
    endif()
endif()

# GPU UI on macOS and Linux: the editor paints through an OpenGL context
# (Windows already draws with Direct2D). The snapshot tool links it too, so
# its --fps and --idlecpu probes measure the GPU path.
if (NOT WIN32)
    foreach(target ilanaSynth ilanaSynthFX ilanaSnapshot)
        target_link_libraries(${target} PRIVATE juce::juce_opengl)
    endforeach()
endif()

# Dev tool: per-preset render fingerprints for regression checks (not shipped).
juce_add_console_app(ilanaFingerprint PRODUCT_NAME "ilanaFingerprint")

target_sources(ilanaFingerprint PRIVATE
    tools/PresetFingerprint.cpp
    src/PluginProcessor.cpp
    ${ILANA_PROCESSOR_SOURCES}
    src/MtsEsp.cpp
    src/dsp/Wavetable.cpp
    src/dsp/WavetableDoc.cpp
    src/dsp/TableFactory.cpp
    src/dsp/Voice.cpp
    src/dsp/airwindows/Registry.cpp
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

# Dev tool: the stress phrase (pedal, retrigger, stealing, preset change,
# random block lengths, sample-rate change) through every preset, multi-core
# on vs off (tools/StressTest.cpp; not shipped).
juce_add_console_app(ilanaStress PRODUCT_NAME "ilanaStress")

target_sources(ilanaStress PRIVATE
    tools/StressTest.cpp
    src/PluginProcessor.cpp
    ${ILANA_PROCESSOR_SOURCES}
    src/MtsEsp.cpp
    src/dsp/Wavetable.cpp
    src/dsp/WavetableDoc.cpp
    src/dsp/TableFactory.cpp
    src/dsp/Voice.cpp
    src/dsp/airwindows/Registry.cpp
)

target_include_directories(ilanaStress PRIVATE src)

target_compile_definitions(ilanaStress PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    ILANA_TEST_BUILD=1
    ILANA_FINGERPRINT_BUILD=1
    JucePlugin_Name="ilanaSynth"
)

target_link_libraries(ilanaStress
    PRIVATE
        juce::juce_dsp
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
)

# Dev tool: renders every factory preset's test phrases for the preset critic
# (tools/preset_critic.py; not shipped).
juce_add_console_app(ilanaPresetRender PRODUCT_NAME "ilanaPresetRender")

target_sources(ilanaPresetRender PRIVATE
    tools/PresetRender.cpp
    src/PluginProcessor.cpp
    ${ILANA_PROCESSOR_SOURCES}
    src/MtsEsp.cpp
    src/dsp/Wavetable.cpp
    src/dsp/WavetableDoc.cpp
    src/dsp/TableFactory.cpp
    src/dsp/Voice.cpp
    src/dsp/airwindows/Registry.cpp
)

target_include_directories(ilanaPresetRender PRIVATE src)

target_compile_definitions(ilanaPresetRender PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    ILANA_TEST_BUILD=1
    ILANA_FINGERPRINT_BUILD=1
    JucePlugin_Name="ilanaSynth"
)

target_link_libraries(ilanaPresetRender
    PRIVATE
        juce::juce_dsp
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
)

juce_add_console_app(ilanaKnobAudit PRODUCT_NAME "ilanaKnobAudit")

target_sources(ilanaKnobAudit PRIVATE
    tools/KnobAudit.cpp
    src/PluginProcessor.cpp
    ${ILANA_PROCESSOR_SOURCES}
    src/MtsEsp.cpp
    src/dsp/Wavetable.cpp
    src/dsp/WavetableDoc.cpp
    src/dsp/TableFactory.cpp
    src/dsp/Voice.cpp
    src/dsp/airwindows/Registry.cpp
)

target_include_directories(ilanaKnobAudit PRIVATE src)

target_compile_definitions(ilanaKnobAudit PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    ILANA_TEST_BUILD=1
    ILANA_FINGERPRINT_BUILD=1
    JucePlugin_Name="ilanaSynth"
)

target_link_libraries(ilanaKnobAudit
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
    ${ILANA_PROCESSOR_SOURCES}
    src/MtsEsp.cpp
    src/PluginEditor.cpp
    src/dsp/Wavetable.cpp
    src/dsp/WavetableDoc.cpp
    src/dsp/TableFactory.cpp
    src/dsp/Voice.cpp
    src/dsp/airwindows/Registry.cpp
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

foreach (tool ilanaSnapshot ilanaFingerprint ilanaStress ilanaPresetRender ilanaFxTest)
    ilana_big_stack (${tool})
endforeach()
