cmake_minimum_required(VERSION 3.22)

project(ilanaSynth VERSION 1.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options(/MP /bigobj)
endif()

include(FetchContent)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(JUCE)
include(cmake/JuceLayerClip.cmake) # greyed-out controls draw through small layers (animation frame rate)
include(cmake/BuildInfo.cmake) # git hash and date for src/BuildInfo.h and build/build-label.txt

# Step 40 follow-up: the TIME menu's high-quality stretchers, rendered in the
# background (src/dsp/stretch). Pinned sources, only downloaded: Signalsmith
# Stretch and its linear library (MIT), Bungee (MPL-2.0) with Eigen (MPL-2.0)
# and pffft (BSD-like), which PGHI and Paulstretch use too.
set(ILANA_STRETCH_DEPS ${CMAKE_BINARY_DIR}/_deps/stretch)
FetchContent_Declare(signalsmith_stretch
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
    GIT_TAG a670068d9aeb64913331d5cc29337b19a457a7df # 1.4.0
    SOURCE_DIR ${ILANA_STRETCH_DEPS}/signalsmith-stretch
    SOURCE_SUBDIR none
)
FetchContent_Declare(signalsmith_linear
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/linear.git
    GIT_TAG 146b26f91b48835d665fa0498d64d9617818d27a # 0.6.5
    SOURCE_DIR ${ILANA_STRETCH_DEPS}/signalsmith-linear
    SOURCE_SUBDIR none
)
FetchContent_Declare(bungee
    GIT_REPOSITORY https://github.com/bungee-audio-stretch/bungee.git
    GIT_TAG 052451eff35165dae16221092447be0ce521dd24 # 2.4.8
    GIT_SUBMODULES ""
    SOURCE_DIR ${ILANA_STRETCH_DEPS}/bungee
    SOURCE_SUBDIR none
)
FetchContent_Declare(eigen
    URL https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
    URL_HASH SHA256=8586084f71f9bde545ee7fa6d00288b264a2b7ac3607b974e54d13e7162c1c72
    SOURCE_DIR ${ILANA_STRETCH_DEPS}/eigen
    SOURCE_SUBDIR none
)
FetchContent_Declare(pffft
    URL https://bitbucket.org/jpommier/pffft/get/0aec0327a691.tar.gz
    URL_HASH SHA256=29b3b0bf83ace5e32ad4fb26a99fe13284240a4d4f1b762970aa19c3821ddb1f
    SOURCE_DIR ${ILANA_STRETCH_DEPS}/pffft
    SOURCE_SUBDIR none
)
FetchContent_MakeAvailable(signalsmith_stretch signalsmith_linear bungee eigen pffft)

file(GLOB ILANA_BUNGEE_SOURCES ${ILANA_STRETCH_DEPS}/bungee/src/*.cpp)
add_library(ilanaStretch STATIC
    src/dsp/stretch/OfflineStretch.cpp
    src/dsp/stretch/StretchSignalsmith.cpp
    src/dsp/stretch/StretchBungee.cpp
    src/dsp/stretch/StretchPghi.cpp
    src/dsp/stretch/StretchPaul.cpp
    src/dsp/stretch/StretchResample.cpp
    ${ILANA_BUNGEE_SOURCES}
    ${ILANA_STRETCH_DEPS}/pffft/pffft.c
    ${ILANA_STRETCH_DEPS}/pffft/fftpack.c
)
target_include_directories(ilanaStretch PRIVATE
    ${ILANA_STRETCH_DEPS}
    ${ILANA_STRETCH_DEPS}/signalsmith-stretch
    ${ILANA_STRETCH_DEPS}/bungee
    ${ILANA_STRETCH_DEPS}/eigen
    ${ILANA_STRETCH_DEPS}/pffft
)
target_compile_definitions(ilanaStretch PRIVATE
    BUNGEE_VERSION="2.4.8" BUNGEE_VISIBILITY= BUNGEE_SELF_TEST=0
    eigen_assert=BUNGEE_ASSERT1 EIGEN_DONT_PARALLELIZE=1 _USE_MATH_DEFINES
)
target_compile_options(ilanaStretch PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-fwrapv>)
set_target_properties(ilanaStretch PROPERTIES POSITION_INDEPENDENT_CODE ON)

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
    # macOS: the Live oscillator mode opens the audio input, and the standalone
    # is killed on first use without this usage string.
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "ilanaSynth plays the audio input through its Live oscillator mode."
    VST3_CATEGORIES Instrument Synth
)

# IlanaSynthAudioProcessor's member functions, split by area (src/processor).
set(ILANA_PROCESSOR_SOURCES
    src/processor/ParameterLayout.cpp
    src/processor/Modulation.cpp
    src/processor/Effects.cpp
    src/processor/FxPatcher.cpp
    src/processor/OscRouting.cpp
    src/processor/OscSlots.cpp
    src/processor/ModuleClipboard.cpp
    src/processor/Sequencers.cpp
    src/processor/Presets.cpp
    src/processor/State.cpp
    src/processor/Migrations.cpp
    src/processor/Bounce.cpp
    src/processor/SamplePortability.cpp
    src/processor/Dx7.cpp
    src/dsp/BuiltInTables.cpp
    src/dsp/ModalPianoTable.cpp
)

# The whole engine (processor, voices, tables, FX), one list for every target
# that compiles it itself (the plugins, ilanaSnapshot, ilanaFxTest).
set(ILANA_ENGINE_SOURCES
    src/PluginProcessor.cpp
    ${ILANA_PROCESSOR_SOURCES}
    src/MtsEsp.cpp
    src/dsp/Wavetable.cpp
    src/dsp/WavetableDoc.cpp
    src/dsp/TableFactory.cpp
    src/dsp/Voice.cpp
    src/dsp/airwindows/Registry.cpp
)
# Absolute, so the engine libraries in cmake/engine can use the same list.
list(TRANSFORM ILANA_ENGINE_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")

# The editor, for every target that builds it (the plugins, ilanaSnapshot,
# ilanaFxTest): PluginEditor.cpp (the shell) and the pages, each in its own
# .cpp (ARCH-8).
set(ILANA_GUI_SOURCES
    src/PluginEditor.cpp
    src/gui/pages/EnvLfoPages.cpp
    src/gui/pages/VectorPage.cpp
    src/gui/pages/FmInputPages.cpp
    src/gui/pages/FxPage.cpp
    src/gui/pages/MatrixPage.cpp
    src/gui/pages/ModSourcePanel.cpp
    src/gui/pages/OscPage.cpp
    src/gui/pages/PatchLfo.cpp
    src/gui/pages/PatchPage.cpp
    src/gui/pages/PlayPage.cpp
    src/gui/pages/SeqPage.cpp
)

juce_add_binary_data(ilanaFonts SOURCES
    resources/fonts/Manrope-400-Regular.ttf
    resources/fonts/Manrope-500-Medium.ttf
    resources/fonts/Manrope-700-Bold.ttf
)

# The human-made wavetables the editor carries (tools/build_builtin_wavetables.py;
# src/dsp/BuiltInTables.cpp reads them), and the Modal Piano's partial table
# (tools/build_modal_piano.py; src/dsp/ModalPianoTable.cpp) and the Modal Guitar's (tools/build_modal_guitar.py). Every target built from the processor
# links them (the foreach at the end).
juce_add_binary_data(ilanaWavetables
    HEADER_NAME WavetableData.h
    NAMESPACE WavetableData
    SOURCES content/wavetables/builtin-wavetables.bin
            content/piano/modal-piano.bin
            content/guitar/modal-guitar.bin
)

target_sources(ilanaSynth PRIVATE
    ${ILANA_ENGINE_SOURCES}
    ${ILANA_GUI_SOURCES}
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
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "ilanaSynth FX processes the audio input through its bodies, strings, grains and filters."
    VST3_CATEGORIES Fx
)

target_sources(ilanaSynthFX PRIVATE
    ${ILANA_ENGINE_SOURCES}
    ${ILANA_GUI_SOURCES}
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

# Sanitizer for the test engine, ilanaTableTest and ilanaTorture only (never the
# plugins): -DILANA_SANITIZE=thread or address. Empty: off (the normal gate).
set(ILANA_SANITIZE "" CACHE STRING "Sanitizer for the test engine and tools: address, thread or empty")
set_property(CACHE ILANA_SANITIZE PROPERTY STRINGS "" address thread)
if (ILANA_SANITIZE AND NOT ILANA_SANITIZE MATCHES "^(address|thread)$")
    message(FATAL_ERROR "ILANA_SANITIZE must be empty, address or thread")
endif()

# The same stack for the dev tools below, which also build processors on the stack.
function (ilana_big_stack target)
    if (MSVC)
        target_link_options(${target} PRIVATE /STACK:16777216)
    endif()
endfunction()


# The engine as static libraries, one per define flavour (cmake/engine), so a
# flavour is compiled once however many tools use it. The plugins, ilanaSnapshot
# and ilanaFxTest need the editor and keep their own compile.
add_subdirectory(cmake/engine)

# A dev tool: its own source file on top of an engine library. EXCLUDE_FROM_ALL
# keeps it out of the default build (tools/verify.sh --all builds everything).
function (ilana_engine_tool name engine source)
    juce_add_console_app(${name} PRODUCT_NAME "${name}")
    target_sources(${name} PRIVATE ${source})
    target_link_libraries(${name} PRIVATE ${engine})
    ilana_big_stack (${name})
    set_target_properties(${name} PROPERTIES EXCLUDE_FROM_ALL TRUE)
endfunction()

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
)

target_link_libraries(ilanaTableTest PRIVATE ilanaEngineTest)
# Where the old-state corpus is (tests/old-states; ILANA_OLDSTATE_DIR overrides it).
target_compile_definitions(ilanaTableTest PRIVATE ILANA_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

# Symbols for the optimised test build, so ILANA_PROFILE can name lines.
if (MSVC)
    target_compile_options(ilanaTableTest PRIVATE /Zi)
    # 16 MB stack: the tests keep whole processors on the stack, which outgrew
    # MSVC's 1 MB default (Linux gives 8 MB).
    target_link_options(ilanaTableTest PRIVATE /DEBUG /OPT:REF /OPT:ICF /STACK:16777216)
    target_link_libraries(ilanaTableTest PRIVATE dbghelp winmm)
endif()

add_test(NAME table_tests COMMAND ilanaTableTest)

# Dev tool: offscreen PNG snapshots of every editor tab (not shipped).
juce_add_console_app(ilanaSnapshot PRODUCT_NAME "ilanaSnapshot")

target_sources(ilanaSnapshot PRIVATE
    tools/Snapshot.cpp
    ${ILANA_ENGINE_SOURCES}
    ${ILANA_GUI_SOURCES}
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

# Vulkan UI on Windows and Linux (src/gpu): the editor's pictures are painted
# by JUCE's rasteriser semantics with Vulkan doing the fills, layers and the
# presenting; Direct2D (Windows) or OpenGL (Linux) stay as the fallback and in
# the settings menu. macOS has no native Vulkan and keeps CoreGraphics. The
# loader is opened at run time (volk), so no Vulkan SDK is needed to build and
# a PC without a Vulkan driver still runs. Shaders are pre-compiled into
# src/gpu/VulkanShaders.h (tools/build_vulkan_shaders.sh regenerates it).
if (WIN32 OR (UNIX AND NOT APPLE))
    set(ILANA_VULKAN_DEFAULT ON)
else()
    set(ILANA_VULKAN_DEFAULT OFF)
endif()
option(ILANA_VULKAN "Vulkan editor renderer" ${ILANA_VULKAN_DEFAULT})

if (ILANA_VULKAN)
    FetchContent_Declare(vulkan_headers
        GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
        GIT_TAG vulkan-sdk-1.3.296.0
        GIT_SHALLOW TRUE
    )
    FetchContent_Declare(volk
        GIT_REPOSITORY https://github.com/zeux/volk.git
        GIT_TAG vulkan-sdk-1.3.296.0
        GIT_SHALLOW TRUE
    )
    # Headers only: volk is compiled inside src/gpu/VulkanDevice.cpp.
    FetchContent_GetProperties(vulkan_headers)
    if (NOT vulkan_headers_POPULATED)
        FetchContent_Populate(vulkan_headers)
    endif()
    FetchContent_GetProperties(volk)
    if (NOT volk_POPULATED)
        FetchContent_Populate(volk)
    endif()

    foreach(target ilanaSynth ilanaSynthFX ilanaSnapshot)
        target_sources(${target} PRIVATE
            src/gpu/VulkanDevice.cpp
            src/gpu/VulkanGraphics.cpp
            src/gpu/VulkanAttachment.cpp
            src/gpu/VulkanSurface.cpp
        )
        target_include_directories(${target} PRIVATE ${vulkan_headers_SOURCE_DIR}/include ${volk_SOURCE_DIR})
        target_compile_definitions(${target} PRIVATE ILANA_VULKAN=1 VK_NO_PROTOTYPES=1)
        if (UNIX AND NOT APPLE)
            target_link_libraries(${target} PRIVATE ${CMAKE_DL_LIBS})
        endif()
    endforeach()
endif()

# Dev tool: per-preset render fingerprints for regression checks (not shipped).
juce_add_console_app(ilanaFingerprint PRODUCT_NAME "ilanaFingerprint")
target_sources(ilanaFingerprint PRIVATE tools/PresetFingerprint.cpp)
target_link_libraries(ilanaFingerprint PRIVATE ilanaEngineFp)

# Dev tools on the same flavour, built only on request (verify.sh --all):
# the stress phrase (pedal, retrigger, stealing, preset change, random block
# lengths, sample-rate change) through every preset, multi-core on vs off
# (tools/StressTest.cpp); the polish repro; the preset critic's renders
# (tools/preset_critic.py); the MIDI-file renderer; the knob audit.
ilana_engine_tool(ilanaStress      ilanaEngineFp tools/StressTest.cpp)
ilana_engine_tool(ilanaPolishRepro ilanaEngineFp tools/PolishRepro.cpp)
ilana_engine_tool(ilanaPresetRender ilanaEngineFp tools/PresetRender.cpp)
ilana_engine_tool(ilanaMidiRender  ilanaEngineFp tools/MidiRender.cpp)
ilana_engine_tool(ilanaKnobAudit   ilanaEngineFp tools/KnobAudit.cpp)
# Concurrency torture (tools/TortureTest.cpp): audio thread against the message
# thread's edits; build with -DILANA_SANITIZE=thread (or address).
ilana_engine_tool(ilanaTorture     ilanaEngineTest tools/TortureTest.cpp)
if (ILANA_SANITIZE)
    foreach (target IN ITEMS ilanaEngineTest ilanaTableTest ilanaTorture)
        target_compile_options(${target} PRIVATE -fsanitize=${ILANA_SANITIZE} -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=${ILANA_SANITIZE})
    endforeach()
endif()

# Dev tool: renders reference notes from an installed VST3's default sound,
# for fitting modelled sounds (not shipped).
juce_add_console_app(ilanaRefHost PRODUCT_NAME "ilanaRefHost")

target_sources(ilanaRefHost PRIVATE tools/RefHost.cpp)
set_target_properties(ilanaRefHost PROPERTIES EXCLUDE_FROM_ALL TRUE)

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
    ${ILANA_ENGINE_SOURCES}
    ${ILANA_GUI_SOURCES}
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

# Dev tool: what the FX rack costs, per effect and per routing (tools/FxCpuBench.cpp; not shipped).
juce_add_console_app(ilanaFxCpuBench PRODUCT_NAME "ilanaFxCpuBench")
target_sources(ilanaFxCpuBench PRIVATE tools/FxCpuBench.cpp)
target_link_libraries(ilanaFxCpuBench PRIVATE ilanaEngineFxBench)
set_target_properties(ilanaFxCpuBench PROPERTIES EXCLUDE_FROM_ALL TRUE)

if (MSVC)
    target_compile_options(ilanaFxCpuBench PRIVATE /Zi)
    target_link_options(ilanaFxCpuBench PRIVATE /DEBUG /OPT:REF /OPT:ICF)
    target_link_libraries(ilanaFxCpuBench PRIVATE dbghelp)
endif()

add_test(NAME fx_tests COMMAND ilanaFxTest)

foreach (tool ilanaSnapshot ilanaFingerprint ilanaFxTest)
    ilana_big_stack (${tool})
endforeach()

# TIME's high-quality stretchers and the built-in wavetables (factory tables),
# in the targets that compile the engine themselves; the engine libraries carry
# them for the rest.
foreach(target ilanaSynth ilanaSynthFX ilanaSnapshot ilanaFxTest)
    target_link_libraries(${target} PRIVATE ilanaStretch ilanaWavetables)
endforeach()

# src/BuildInfo.h reads the hash and date that cmake/BuildInfo.cmake generated.
foreach(target ilanaSynth ilanaSynthFX ilanaTableTest ilanaSnapshot ilanaFxTest ilanaRefHost)
    target_include_directories(${target} PRIVATE "${ILANA_GENERATED_DIR}")
endforeach()

# The dev tools kept out of the default build (EXCLUDE_FROM_ALL), so they
# cannot rot unseen: `cmake --build build --target ilanaTools`, which
# `tools/verify.sh --all` runs after the default build.
add_custom_target(ilanaTools DEPENDS
    ilanaStress ilanaPolishRepro ilanaKnobAudit ilanaMidiRender ilanaPresetRender ilanaTorture
    ilanaRefHost ilanaFxCpuBench)
