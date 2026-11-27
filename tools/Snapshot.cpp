// Dev tool: renders the plugin editor offscreen and writes one PNG per tab,
// so the UI can be reviewed without a DAW or a display session.
//   ilanaSnapshot <output dir> [factory preset index]
//   ilanaSnapshot --uitest [--list | --only GROUP[,GROUP]]   (drives the editor and checks the wiring)
//   ilanaSnapshot --idlecpu    (UI cost of each page while nothing moves)
//   ilanaSnapshot --vulkanlive <dir>  (on screen through Vulkan: each page against software)

#if __linux__
 #include <dlfcn.h>
#endif
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <tuple>
#include <algorithm>
#include <iostream>
#include <functional>
#include <typeinfo>
#if defined (__GNUC__)
 #include <cxxabi.h>
#endif
#include <map>
#include <set>
#include <thread>

#include "PluginProcessor.h"
#include "gui/HeaderWidgets.h"
#include "gui/CardTabs.h"
#include "gui/FilterWidgets.h"
#include "gui/VectorPad.h"
#include "dsp/VectorShape.h"
#include "gui/PhysicalView.h"
#if ILANA_VULKAN
 #include "gpu/VulkanCanvas.h"
 #include "gpu/VulkanDevice.h"
#endif
#include "gui/TableBrowser.h"
#include "gui/WavetableEditor.h"
#include "PluginEditor.h"
#include "gui/pages/PlayPage.h"
#include "gui/SectionColourPicker.h"
#include "gui/EnvelopeDisplay.h"
#include "gui/FmWidgets.h"
#include "gui/FmDiagram.h"
#include "gui/OperatorEnvDisplay.h"
#include "gui/OperatorPoolCards.h"
#include "gui/ModRoutes.h"
#include "gui/ModulationCardView.h"
#include "gui/MatrixWidgets.h"
#include "gui/FilterDisplay.h"
#include "gui/OutputView.h"
#include "gui/ParamControls.h"
#include "gui/SubTabBar.h"
#include "gui/TutorialOverlay.h"
#include "gui/WaveDisplay.h"
#include "gui/SampleNameBox.h"
#include "gui/ModHoverPopup.h"
#include "gui/StateTabs.h"
#include "gui/ClipEditor.h"
#include "gui/ConfirmOverlay.h"
#include "Dx7Banks.h"
#include "gui/LfoSimView.h"
#include "gui/LogoComponent.h"
#include "gui/FxCarousel.h"
#include "gui/PatchCableView.h"
#include "gui/FxLibrary.h"
#include "processor/PatchGraphState.h"
#include "gui/LfoDisplay.h"
#include "gui/LfoShapeMenu.h"
#include "gui/SequencerEditors.h"
#include "gui/RemapEditor.h"
#include "gui/FxDisplays.h"
#include "gui/FxLibrary.h"

namespace
{
template <typename Type>
void findAll (juce::Component& parent, std::vector<Type*>& found)
{
    for (auto* child : parent.getChildren())
    {
        if (auto* match = dynamic_cast<Type*> (child))
            found.push_back (match);

        findAll<Type> (*child, found);
    }
}

template <typename Type>
Type* findChild (juce::Component& parent)
{
    for (auto* child : parent.getChildren())
    {
        if (auto* match = dynamic_cast<Type*> (child))
            return match;

        if (auto* nested = findChild<Type> (*child))
            return nested;
    }

    return nullptr;
}

void settle (int milliseconds)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil (milliseconds);
}

// Snapshot mode plays a held note so the live views (scope, modulation
// markers) have something to show; this runs the audio before each capture.
std::function<void()>& beforeSave()
{
    static std::function<void()> callback;
    return callback;
}

// ILANA_SNAPSHOT_RENDERER: who draws the PNGs that save() writes.
//   (unset) / software  createComponentSnapshot, JUCE's software renderer
//   vulkan              ilana::vulkan::renderComponent (the Vulkan renderer,
//                       offscreen); exits non-zero if Vulkan can't run
//   both                <name>.png from software, <name>.vulkan.png from
//                       Vulkan, then <name>.software2.png from software
//                       again: of the same UI state, except what moves on
//                       its own while painting (playhead dots, readouts),
//                       which shows as .png vs .software2.png differences
//                       that tools/vulkan_parity.py leaves out
enum class SnapshotRenderer { software, vulkan, both };

SnapshotRenderer snapshotRenderer()
{
    static const auto renderer = []
    {
        const auto name = juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_RENDERER", "software").trim().toLowerCase();
        if (name.isEmpty() || name == "software")
            return SnapshotRenderer::software;
        if (name == "vulkan" || name == "both")
        {
           #if ILANA_VULKAN
            return name == "vulkan" ? SnapshotRenderer::vulkan : SnapshotRenderer::both;
           #else
            std::cerr << "ILANA_SNAPSHOT_RENDERER=" << name << ": this build has no Vulkan renderer (ILANA_VULKAN is off)" << std::endl;
            std::exit (3);
           #endif
        }
        std::cerr << "ILANA_SNAPSHOT_RENDERER=" << name << ": unknown (software, vulkan or both)" << std::endl;
        std::exit (3);
    }();
    return renderer;
}

void writePng (const juce::Image& image, const juce::File& file)
{
    file.deleteFile();
    juce::FileOutputStream stream (file);
    juce::PNGImageFormat().writeImageToStream (image, stream);
    std::cout << file.getFullPathName() << std::endl;
}

void save (juce::Component& editor, const juce::File& file)
{
    if (beforeSave() != nullptr)
        beforeSave()();

    // ILANA_SNAPSHOT_SCALE: the pixel scale of the PNGs (1.5 by default; 3 for close-ups).
    static const auto scale = (float) juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_SCALE", "1.5").getDoubleValue();
    const auto pixels = scale > 0.1f ? scale : 1.5f;
    const auto renderer = snapshotRenderer();
    // The text is sized for the zoom it draws at, as in the host (a 1.5x PNG of a 100 % editor showed the 100 %
    // floors and drew the small OSC card text bigger than ilana's 175 % window, 2026-10-23).
    const auto zoom = IlanaTheme::uiScaleRef();
    IlanaTheme::uiScaleRef() = zoom * pixels;
    IlanaTheme::notifyUiScaleListeners();
    juce::Image image, gpuImage, image2;
    // In both mode each capture waits 250 ms first (the message loop doesn't
    // run, so nothing else changes): the live views' trackers (PhaseTracker,
    // EnvTracker in gui/AnimationUtils.h) predict from the time since their
    // last paint and restart from the published value after 150 ms, so with
    // the wait every capture shows the same phase instead of one moved on
    // by however long the previous renderer took.
    const auto settleTrackers = [renderer] { if (renderer == SnapshotRenderer::both) juce::Thread::sleep (250); };
    if (renderer != SnapshotRenderer::vulkan)
    {
        settleTrackers();
        image = editor.createComponentSnapshot (editor.getLocalBounds(), true, pixels);
    }
   #if ILANA_VULKAN
    if (renderer != SnapshotRenderer::software)
    {
        settleTrackers();
        gpuImage = ilana::vulkan::renderComponent (editor, editor.getLocalBounds(), pixels);
        if (! gpuImage.isValid())
        {
            std::cerr << "ILANA_SNAPSHOT_RENDERER: the Vulkan renderer is unavailable (" << ilana::vulkan::lastError()
                      << "); no picture written for " << file.getFullPathName() << std::endl;
            std::exit (4);
        }
        if (renderer == SnapshotRenderer::both)
        {
            settleTrackers();
            image2 = editor.createComponentSnapshot (editor.getLocalBounds(), true, pixels);
        }
    }
   #endif
    IlanaTheme::uiScaleRef() = zoom;
    IlanaTheme::notifyUiScaleListeners();
    writePng (renderer == SnapshotRenderer::vulkan ? gpuImage : image, file);
    if (renderer == SnapshotRenderer::both)
    {
        writePng (gpuImage, file.getSiblingFile (file.getFileNameWithoutExtension() + ".vulkan" + file.getFileExtension()));
        writePng (image2, file.getSiblingFile (file.getFileNameWithoutExtension() + ".software2" + file.getFileExtension()));
    }
}
int uiFailures = 0;
int uiChecks = 0;

// isShowing() needs a window on screen; the editor here is offscreen, so
// walk the visibility chain instead.
bool visibleInTree (const juce::Component* component)
{
    // The editor itself is never put on screen here, so stop below it.
    for (; component != nullptr && component->getParentComponent() != nullptr; component = component->getParentComponent())
        if (! component->isVisible())
            return false;

    return true;
}

void expect (bool condition, const juce::String& message)
{
    ++uiChecks;
    std::cout << (condition ? "PASS: " : "FAIL: ") << message << std::endl;

    if (! condition)
        ++uiFailures;
}

// A source's pop-up editor, opened from any dock chip (null when no chip shows), and closed again.
juce::Component* openSourcePopover (IlanaSynthAudioProcessorEditor& editor, Mod::Source source)
{
    std::vector<ModSourceChip*> chips;
    findAll<ModSourceChip> (editor, chips);
    ModSourceChip* anchor = nullptr;
    for (auto* chip : chips)
        if (anchor == nullptr && chip->isVisible())
            anchor = chip;
    if (anchor == nullptr)
        return nullptr;

    auto* popover = editor.getSourcePopover();
    if (popover == nullptr || ! popover->isVisible() || editor.getSourcePopoverSource() != (int) source)
        editor.toggleSourcePopover ((int) source, *anchor);
    settle (400);
    return editor.getSourcePopover();
}

void closeSourcePopover (IlanaSynthAudioProcessorEditor& editor)
{
    std::vector<ModSourceChip*> chips;
    findAll<ModSourceChip> (editor, chips);
    auto* popover = editor.getSourcePopover();
    for (auto* chip : chips)
        if (chip->isVisible() && popover != nullptr && popover->isVisible())
        {
            editor.toggleSourcePopover (editor.getSourcePopoverSource(), *chip);
            break;
        }
    settle (300);
}

#include "ModulationUiTests.h"
#include "FilterFxUiTests.h"
#include "ModulationReview8Tests.h"
#include "LayoutUiTests8.h"
#include "LayoutUiTests9.h"
#include "GlobalUiTests.h"
#include "DropdownUiTests.h"
#include "CodeHealthUiTests.h"
#include "PatchCableUiTests.h"
#include "OscRoutingUiTests.h"
#include "OscSlotUiTests.h"
#include "WorkflowUiTests.h"
#include "FxLfoUiTests.h"
#include "KnobGeometryUiTests.h"
#include "FxCarouselUiTests.h"
#include "LayoutUiTests10.h"
#include "OperatorUiTests.h"
#include "OscTypeUiTests.h"
#include "PlayPageUiTests.h"
#include <set>
#include "HitReachUiTests.h"
#include "OscCardOverlapTests.h"
#include "Review9T2Tests.h"

#include "snapshot/UiReviewTests.inc"
#include "snapshot/UiGroupsCore.inc"
#include "snapshot/UiGroupsPages.inc"
// The wave display draws every knob that changes the cycle's shape, live (2026-11-20, ilana: "spectral warps in
// oscillator are not animated in the illustration. if theres other knobs with modifications to the shape that are
// not visible in the illustration. fix"): the spectral warp at its amount (before, the display showed whatever
// the audio thread's cache had built, so it froze or lagged a step), OSC 4-6's own spectral warp (they showed
// OSC 3's), FRAME between two frames under FRAME BLEND Crossfade / Spectral (it jumped to the nearest frame) and
// SYNC 2 TO 1 on OSC 2. Pictures with ILANA_UITEST_SHOTS.
#include "BlindspotScenes.h"
#include "snapshot/UiGroupsReview.inc"
#include "snapshot/UiGroupsLate.inc"
#include "FxBlindSpotUiTests.h"

struct UiGroup
{
    const char* id;
    void (*run) (UiCtx&);
};

const UiGroup uiGroups[] = {

    { "BASICS", uiGroup_BASICS },
    { "TABS", uiGroup_TABS },
    { "KEYS", uiGroup_KEYS },
    { "MATRIX_ROWS", uiGroup_MATRIX_ROWS },
    { "DROP_SOURCE", uiGroup_DROP_SOURCE },
    { "MACRO_NAMES", uiGroup_MACRO_NAMES },
    { "ENVLFO_PAGES", uiGroup_ENVLFO_PAGES },
    { "MAIN_OSC", uiGroup_MAIN_OSC },
    { "FX_QUICK_ADD", uiGroup_FX_QUICK_ADD },
    { "FX_CARDS", uiGroup_FX_CARDS },
    { "M5_PAGES", uiGroup_M5_PAGES },
    { "NEXT_PRESET", uiGroup_NEXT_PRESET },
    { "UNDO_DRAWN", uiGroup_UNDO_DRAWN },
    { "PIANO_ROLL", uiGroup_PIANO_ROLL },
    { "INIT_MODULES", uiGroup_INIT_MODULES },
    { "INIT_STARTERS", uiGroup_INIT_STARTERS },
    { "GENERATIVE", uiGroup_GENERATIVE },
    { "LFO_CARD", uiGroup_LFO_CARD },
    { "MSEG_CARD", uiGroup_MSEG_CARD },
    { "VECTOR_PAGE", uiGroup_VECTOR_PAGE },
    { "BOUNCE", uiGroup_BOUNCE },
    { "WARP2", uiGroup_WARP2 },
    { "SHAPE_DISPLAY", uiGroup_SHAPE_DISPLAY },
    { "BLINDSPOTS", uiGroup_BLINDSPOTS },
    { "FX_DISPLAY", uiGroup_FX_DISPLAY },
    { "SAMPLE_ZOOM", uiGroup_SAMPLE_ZOOM },
    { "PHYSICAL", uiGroup_PHYSICAL },
    { "FILTER_PICKERS", uiGroup_FILTER_PICKERS },
    { "WEST_CARD", uiGroup_WEST_CARD },
    { "WEST_BODY", uiGroup_WEST_BODY },
    { "FILTER_MARKERS", uiGroup_FILTER_MARKERS },
    { "REVIEW1", uiGroup_REVIEW1 },
    { "MOD_RINGS", uiGroup_MOD_RINGS },
    { "REVIEW4C", uiGroup_REVIEW4C },
    { "REVIEW4E", uiGroup_REVIEW4E },
    { "MOD", uiGroup_MOD },
    { "SMALL", uiGroup_SMALL },
    { "T2", uiGroup_T2 },
    { "PLAYOSC7", uiGroup_PLAYOSC7 },
    { "FF", uiGroup_FF },
    { "MOD8", uiGroup_MOD8 },
    { "L8", uiGroup_L8 },
    { "T1", uiGroup_T1 },
    { "U1", uiGroup_U1 },
    { "OV", uiGroup_OV },
    { "PLAY", uiGroup_PLAY },
    { "VECTOR_FIT", uiGroup_VECTOR_FIT },
    { "PLAY_LFO_DRAG", uiGroup_PLAY_LFO_DRAG },
    { "ECHO_SLIDER", uiGroup_ECHO_SLIDER },
    { "ECHO_GRAPH", uiGroup_ECHO_GRAPH },
    { "HELD_GRAPH", uiGroup_HELD_GRAPH },
    { "MODAL_SNAP", uiGroup_MODAL_SNAP },
    { "R6", uiGroup_R6 },
    { "R31C", uiGroup_R31C },
    { "DD", uiGroup_DD },
    { "CODE_HEALTH", uiGroup_CODE_HEALTH },
    { "PG", uiGroup_PG },
    { "OR", uiGroup_OR },
    { "OS", uiGroup_OS },
    { "HR", uiGroup_HR },
    { "WF", uiGroup_WF },
    { "FL", uiGroup_FL },
    { "FC", uiGroup_FC },
    { "POPUP_STACK", uiGroup_POPUP_STACK },
    { "OP8", uiGroup_OP8 },
    { "OT", uiGroup_OT },
    { "LAF", uiGroup_LAF },
    { "DOCK_CHIP", uiGroup_DOCK_CHIP },
    { "OP_LFO", uiGroup_OP_LFO },
};

// The ids the old ILANA_UITEST_ONLY understood that were more than one group.
juce::StringArray uiExpandAlias (const juce::String& id)
{
    if (id == "OSC")
        return { "OT", "L8", "T1", "U1", "OV", "R6" };
    if (id == "STRIPS")
        return { "MAIN_OSC" };
    return { id };
}

int runUiTests (const juce::StringArray& args)
{
    if (args.contains ("--list"))
    {
        for (const auto& group : uiGroups)
            std::cout << group.id << std::endl;
        return 0;
    }

    juce::StringArray only;
    const auto onlyIndex = args.indexOf ("--only");
    const auto onlyText = onlyIndex >= 0 && onlyIndex + 1 < args.size() ? args[onlyIndex + 1]
                                                                          : juce::SystemStats::getEnvironmentVariable ("ILANA_UITEST_ONLY", {});
    for (const auto& token : juce::StringArray::fromTokens (onlyText, ",", ""))
        if (token.trim().isNotEmpty())
            only.addArray (uiExpandAlias (token.trim()));

    for (const auto& id : only)
    {
        auto known = false;
        for (const auto& group : uiGroups)
            known = known || id == group.id;
        if (! known)
        {
            std::cout << "unknown UI test group " << id << " (--uitest --list shows them)" << std::endl;
            return 2;
        }
    }

    const auto runGroup = [] (const UiGroup& group, UiCtx& ctx)
    {
        const auto failuresBefore = uiFailures;
        const auto checksBefore = uiChecks;
        const auto started = juce::Time::getMillisecondCounterHiRes();
        group.run (ctx);
        std::cout << "GROUP " << group.id << ": " << (uiChecks - checksBefore) << " checks, "
                  << (uiFailures - failuresBefore) << " failed, "
                  << juce::String ((juce::Time::getMillisecondCounterHiRes() - started) / 1000.0, 1) << " s" << std::endl;
    };

    if (only.isEmpty())
    {
        UiCtx ctx;
        for (const auto& group : uiGroups)
            runGroup (group, ctx);

        ctx.closeEditor();
        expect (dynamic_cast<IlanaLookAndFeel*> (&juce::LookAndFeel::getDefaultLookAndFeel()) == nullptr,
                "sweep: with no editor open the default look is JUCE's own again");
    }
    else
    {
        // Each named group on its own processor and editor (--chain: one for all of them, in the order named,
        // to find what an earlier group leaves behind).
        std::unique_ptr<UiCtx> shared;
        for (const auto& id : only)
            for (const auto& group : uiGroups)
                if (id == group.id)
                {
                    if (args.contains ("--chain"))
                    {
                        if (shared == nullptr)
                            shared = std::make_unique<UiCtx>();
                        runGroup (group, *shared);
                    }
                    else
                    {
                        UiCtx ctx;
                        runGroup (group, ctx);
                        ctx.closeEditor();
                    }
                }
        if (shared != nullptr)
            shared->closeEditor();
    }

    std::cout << "UI TESTS " << (uiFailures == 0 ? "PASSED" : "FAILED") << " (" << uiFailures << " failures, " << uiChecks << " checks)" << std::endl;
    return uiFailures == 0 ? 0 : 1;
}
} // namespace

// The editor on screen, idle, on every page: how much of a core the UI
// spends repainting when nothing moves (animations must settle).
#include <ctime>
#if JUCE_WINDOWS
 #define NOMINMAX
 #define WIN32_LEAN_AND_MEAN
 #include <windows.h>
#endif

#include "snapshot/PerfTools.inc"
#include "snapshot/MotionTools.inc"
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    if (argc < 2)
    {
        std::cout << "usage: ilanaSnapshot <output dir> [factory preset index]\n"
                     "       ilanaSnapshot --uitest" << std::endl;
        return 1;
    }

    if (juce::String (argv[1]) == "--uitest")
    {
        juce::StringArray args;
        for (int i = 2; i < argc; ++i)
            args.add (argv[i]);
        return runUiTests (args);
    }
#if ILANA_VULKAN
    if (juce::String (argv[1]) == "--vulkanlive" && argc > 2)
        return runVulkanLive (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
#endif

    if (juce::String (argv[1]) == "--usability3" && argc > 2)
        return runUsability3 (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
    if (juce::String (argv[1]) == "--usability2" && argc > 2)
        return runUsability2 (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
    if (juce::String (argv[1]) == "--usability" && argc > 2)
        return runUsability (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));

    if (juce::String (argv[1]) == "--flair" && argc > 2)
        return runFlair (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));

    if (juce::String (argv[1]) == "--motion" && argc > 2)
        return runMotion (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));

    if (juce::String (argv[1]) == "--envdot" && argc > 2)
        return runEnvDot (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]), argc > 3 ? juce::String (argv[3]).getFloatValue() : 0.0f);

    // (ILANA_MOTION_LESS=1: --idlecpu and --fps measured with Motion: Less, to weigh the flair.)
    if (std::getenv ("ILANA_MOTION_LESS") != nullptr)
        IlanaAnim::fullMotion() = false;

    if (juce::String (argv[1]) == "--idlecpu")
        return runIdleCpu();

    if (juce::String (argv[1]) == "--foldrepro" && argc > 2)
        return runFoldRepro (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));

    if (juce::String (argv[1]) == "--blindspots" && argc > 2)
    {
        IlanaAnim::fullMotion() = false;
        return runBlindspots (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
    }

    if (juce::String (argv[1]) == "--fxsizes" && argc > 2)
        return runFxSizes (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
    if (juce::String (argv[1]) == "--fps")
        return runFps();

    if (juce::String (argv[1]) == "--animcost")
        return runAnimCost();

    // Still pictures are taken with Motion: Less, so a pop-up or a card is
    // drawn where it ends, not partway through growing or sliding in
    // (ILANA_SNAPSHOT_MOTION=1 keeps the motion, for recordings).
    IlanaAnim::fullMotion() = std::getenv ("ILANA_SNAPSHOT_MOTION") != nullptr;

    if (juce::String (argv[1]) == "--loadtime")
        return runLoadTime();

    if (juce::String (argv[1]) == "--loadstate" && argc > 2)
        return runLoadState (juce::File (juce::String (argv[2])));

    if (juce::String (argv[1]) == "--exciters")
        return runExciterLevels();

    if (juce::String (argv[1]) == "--cpurank")
        return runCpuRank (argc > 2 ? juce::File (juce::String (argv[2])) : juce::File());

    if (juce::String (argv[1]) == "--heldcpu" && argc > 2)
        return runHeldCpu (juce::String (argv[2]));

    if (juce::String (argv[1]) == "--looptest")
        return runLoopTest (argc > 2 ? juce::String (argv[2]) : juce::String ("Swarm"), argc > 3);

    const juce::File outDir (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    outDir.createDirectory();

    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    // A factory preset by index or by name.
    if (argc > 2)
    {
        const juce::String preset (argv[2]);
        const auto byName = processor.getFactoryPresetNames().indexOf (preset);
        processor.loadFactoryPreset (byName >= 0 ? byName : preset.getIntValue());
    }

    // ILANA_SNAPSHOT_SET="osc1_mode=1,osc1_excite=9": parameters set (in their own units) after the preset loads.
    for (const auto& assignment : juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_SET", ""), ",", ""))
        if (const auto id = assignment.upToFirstOccurrenceOf ("=", false, false).trim(); id.isNotEmpty())
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (assignment.fromFirstOccurrenceOf ("=", false, false).getFloatValue()));

    // ILANA_SNAPSHOT_COLOURS="0:ffff6fa8,14:ff3fc7e8": section colours picked
    // (keys as in SectionColours.h) after the preset loads.
    for (const auto& pair : juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_COLOURS", ""), ",", ""))
        if (pair.containsChar (':'))
            processor.setSectionColour (pair.upToFirstOccurrenceOf (":", false, false).getIntValue(),
                                        (juce::uint32) pair.fromFirstOccurrenceOf (":", false, false).getHexValue64());

    // Shows the SEQ tab, which is hidden until a step LFO is in use.
    if (auto* shape = processor.apvts.getParameter ("lfo4_shape"))
        shape->setValueNotifyingHost (shape->convertTo0to1 (7.0f));

    // A held note (unless ILANA_SNAPSHOT_SILENT is set), so the scope and the
    // modulation markers are live in the pictures.
    juce::AudioBuffer<float> audio (2, 512);
    bool noteSent = false;
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_SILENT", "").isEmpty())
        beforeSave() = [&]
        {
            for (int block = 0; block < 24; ++block)
            {
                juce::MidiBuffer midi;
                if (! noteSent)
                {
                    midi.addEvent (juce::MidiMessage::noteOn (1, 48, 0.8f), 0);
                    noteSent = true;
                }
                audio.clear();
                processor.processBlock (audio, midi);
            }
            settle (60);
        };

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    // ILANA_SNAPSHOT_WIDTH renders at another zoom (795 is 75%, 2120 is 200%).
    const auto width = juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_WIDTH", "1060").getIntValue();
    editor->setSize (width, width * 720 / 1060);
    // ILANA_SNAPSHOT_SKIN=0..4: the pictures in that skin (the settings' themeIndex is left alone).
    if (const auto skin = juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_SKIN", ""); skin.isNotEmpty())
        if (auto* look = dynamic_cast<IlanaLookAndFeel*> (&editor->getLookAndFeel()))
        {
            const auto index = juce::jlimit (0, IlanaTheme::numPalettes - 1, skin.getIntValue());
            look->setAccent (juce::Colour (IlanaTheme::palette[index]), juce::Colour (IlanaTheme::paletteSecond[index]));
            editor->sendLookAndFeelChange();
        }
    settle (400);

    // The intro only shows until it has been seen once; capture it anyway.
    if (auto* tutorial = findChild<TutorialOverlay> (*editor))
    {
        tutorial->setVisible (true);
        tutorial->toFront (false);
        settle (200);
        // (ILANA_SNAPSHOT_TOUR_NEW: with WHAT'S NEW open.)
        if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_TOUR_NEW", "").isNotEmpty())
        {
            tutorial->setWhatsNewOpen (true);
            settle (200);
        }
        save (*editor, outDir.getChildFile ("00-tutorial.png"));
        tutorial->setVisible (false);
    }

    // ILANA_SNAPSHOT_CONFIRM: the question a load over an edited patch asks.
    if (auto* confirm = findChild<ConfirmOverlay> (*editor);
        confirm != nullptr && juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_CONFIRM", "").isNotEmpty())
    {
        ConfirmOverlay::Choices choices;
        choices.confirmText = "Load anyway";
        choices.alternativeText = "Save and load";
        choices.onAlternative = [] {};
        confirm->ask ("Replace your edits?",
                      "'" + processor.getCurrentPresetName() + "' has changes that aren't saved. Loading 'Init' replaces them.",
                      choices, [] (bool, bool) {});
        settle (100);
        save (*editor, outDir.getChildFile ("00-confirm.png"));
        confirm->finish (false);
    }

    auto* pages = dynamic_cast<IlanaSynthAudioProcessorEditor*> (editor.get());

    if (pages == nullptr)
        return 1;

    // ILANA_SNAPSHOT_LAYOUT: every page (and the ENV / LFO pop-ups) as
    // layout-<page>.png plus layout-<page>.json, each visible component's
    // class, words and bounds in editor pixels, clipped to what shows. The
    // layout arranger page is built from these; then stop.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_LAYOUT", "").isNotEmpty())
    {
        const auto dump = [&] (const juce::String& name)
        {
            save (*editor, outDir.getChildFile ("layout-" + name + ".png"));
            juce::Array<juce::var> items;
            std::function<void (juce::Component&, juce::Rectangle<int>, int)> walk = [&] (juce::Component& c, juce::Rectangle<int> clip, int depth)
            {
                for (auto* child : c.getChildren())
                {
                    if (! child->isVisible() || child->getWidth() <= 0 || child->getHeight() <= 0)
                        continue;
                    const auto area = editor->getLocalArea (child, child->getLocalBounds());
                    const auto shown = area.getIntersection (clip);
                    if (shown.isEmpty())
                        continue;
                    juce::DynamicObject::Ptr item = new juce::DynamicObject();
                   #if defined (__GNUC__)
                    int status = 0;
                    auto* demangled = abi::__cxa_demangle (typeid (*child).name(), nullptr, nullptr, &status);
                    item->setProperty ("cls", juce::String (status == 0 && demangled != nullptr ? demangled : typeid (*child).name()));
                    std::free (demangled);
                   #else
                    item->setProperty ("cls", juce::String (typeid (*child).name()));
                   #endif
                    item->setProperty ("d", depth);
                    item->setProperty ("x", area.getX());
                    item->setProperty ("y", area.getY());
                    item->setProperty ("w", area.getWidth());
                    item->setProperty ("h", area.getHeight());
                    if (shown != area)
                        item->setProperty ("clip", juce::Array<juce::var> { shown.getX(), shown.getY(), shown.getWidth(), shown.getHeight() });
                    if (child->getName().isNotEmpty())
                        item->setProperty ("name", child->getName());
                    if (auto* knob = dynamic_cast<KnobControl*> (child))
                        item->setProperty ("text", knob->getLabelText()), item->setProperty ("param", knob->getParameterId());
                    else if (auto* combo = dynamic_cast<ComboControl*> (child))
                        item->setProperty ("text", combo->getLabelText()), item->setProperty ("param", combo->getParameterId());
                    else if (auto* toggle = dynamic_cast<ToggleControl*> (child))
                        item->setProperty ("param", toggle->getParameterId());
                    else if (auto* label = dynamic_cast<juce::Label*> (child))
                        item->setProperty ("text", label->getText());
                    else if (auto* button = dynamic_cast<juce::Button*> (child))
                        item->setProperty ("text", button->getButtonText());
                    if (const auto cards = child->getProperties()["playCards"].toString(); cards.isNotEmpty())
                        item->setProperty ("cards", cards);
                    {
                        // The card bodies this component paints (not its children's).
                        juce::Image scratch (juce::Image::ARGB, juce::jmax (1, child->getWidth()), juce::jmax (1, child->getHeight()), true);
                        FillRecorder recorder (scratch);
                        juce::Graphics g (recorder);
                        child->paint (g);
                        juce::Array<juce::var> fills;
                        for (const auto& f : recorder.fills)
                        {
                            const auto r = editor->getLocalArea (child, f.area.getSmallestIntegerContainer());
                            fills.add (juce::Array<juce::var> { r.getX(), r.getY(), r.getWidth(), r.getHeight(), f.colour.toDisplayString (false), f.rounded });
                        }
                        if (! fills.isEmpty())
                            item->setProperty ("fills", fills);
                    }
                    items.add (juce::var (item.get()));
                    walk (*child, shown, depth + 1);
                }
            };
            walk (*editor, editor->getLocalBounds(), 0);
            juce::DynamicObject::Ptr root = new juce::DynamicObject();
            root->setProperty ("page", name);
            root->setProperty ("w", editor->getWidth());
            root->setProperty ("h", editor->getHeight());
            root->setProperty ("items", items);
            outDir.getChildFile ("layout-" + name + ".json").replaceWithText (juce::JSON::toString (juce::var (root.get()), true));
        };
        for (const auto& page : pages->getPageIds())
        {
            pages->showPage (page);
            settle (500);
            dump (page.replaceCharacter ('/', '-'));
        }
        // PLAY's ENVELOPE and LFO EDIT pop-ups.
        pages->showPage ("MAIN");
        settle (400);
        // (PLAY with FILTER's WEST view, then back to F1.)
        if (auto* shell = dynamic_cast<IlanaSynthAudioProcessorEditor*> (editor.get()); shell != nullptr && shell->showPlayViews != nullptr)
        {
            shell->showPlayViews (2, -1, -1);
            settle (400);
            dump ("MAIN-WEST");
            shell->showPlayViews (0, -1, -1);
            settle (300);
        }
        for (const auto* popup : { "PLAY envelope EDIT", "PLAY LFO EDIT" })
        {
            std::vector<juce::Button*> buttons;
            findAll<juce::Button> (*editor, buttons);
            for (auto* button : buttons)
                if (button->getName() == popup && visibleInTree (button))
                {
                    button->triggerClick();
                    settle (500);
                    dump (juce::String (popup).containsIgnoreCase ("envelope") ? "POPUP-ENV" : "POPUP-LFO");
                    button->triggerClick();
                    settle (300);
                    break;
                }
        }
        return 0;
    }

    // ILANA_SNAPSHOT_PICKER: the section colour picker (OSC 1's, on the
    // pop-up's card) into colour-picker.png at the snapshot scale.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_PICKER", "").isNotEmpty())
    {
        struct Card : juce::Component
        {
            void paint (juce::Graphics& g) override
            {
                g.fillAll (IlanaTheme::Ui::bg);
                g.setColour (IlanaTheme::Ui::raised);
                g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (8.0f), 8.0f);
            }
        } card;
        SectionColourPicker picker (processor, SectionColours::oscKey (0), "OSC 1");
        card.setSize (picker.getWidth() + 16, picker.getHeight() + 16);
        card.addAndMakeVisible (picker);
        picker.setTopLeftPosition (8, 8);
        save (card, outDir.getChildFile ("colour-picker.png"));
    }

    // ILANA_SNAPSHOT_PRESET: every page of one factory preset, into
    // preset-<PAGE>.png; then stop (a failing layout test's patch, seen).
    if (const auto only = juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_PRESET", ""); only.isNotEmpty())
    {
        const auto program = processor.getFactoryPresetNames().indexOf (only);
        if (program < 0)
            return 1;
        processor.loadFactoryPreset (program);
        settle (500);
        for (const auto& page : pages->getPageIds())
        {
            pages->showPage (page);
            settle (400);
            save (*editor, outDir.getChildFile ("preset-" + page + ".png"));
        }
        return 0;
    }

    // ILANA_SNAPSHOT_MENU: a menu drawn by the look-and-feel as the component
    // sheet's MENU (a section name, a hovered item, a tick, a separator, a
    // sub-menu), into menu.png at the snapshot scale; then stop.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_MENU", "").isNotEmpty())
    {
        auto& lf = juce::LookAndFeel::getDefaultLookAndFeel(); // (what a desktop menu draws with)
        const auto scale = (float) juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_SCALE", "1.5").getDoubleValue();
        struct Item { juce::String text; bool header, separator, highlighted, ticked, sub; };
        const Item items[] { { "ADD EFFECT", true, false, false, false, false }, { "Vowel", false, false, true, false, false },
                             { "Drive", false, false, false, true, false }, { "OTT", false, false, false, false, false },
                             { {}, false, true, false, false, false }, { "Airwindows", false, false, false, false, true } };
        const auto width = 200;
        const auto border = lf.getPopupMenuBorderSize();
        auto height = border;
        std::vector<int> heights;
        for (const auto& item : items)
        {
            int w = 0, h = 0;
            if (item.header)
                lf.getIdealPopupMenuSectionHeaderSizeWithOptions (item.text, -1, w, h, juce::PopupMenu::Options());
            else
                lf.getIdealPopupMenuItemSize (item.text, item.separator, -1, w, h);
            heights.push_back (h);
            height += heights.back();
        }
        height += border;
        juce::Image image (juce::Image::ARGB, juce::roundToInt ((float) (width + 40) * scale), juce::roundToInt ((float) (height + 40) * scale), true);
        {
            juce::Graphics g (image);
            g.addTransform (juce::AffineTransform::scale (scale));
            g.fillAll (IlanaTheme::Ui::bg);
            g.setOrigin (20, 20);
            lf.drawPopupMenuBackground (g, width, height);
            auto y = border;
            for (size_t i = 0; i < std::size (items); ++i)
            {
                const auto& item = items[i];
                const juce::Rectangle<int> row (border, y, width - 2 * border, heights[i]);
                if (item.header)
                    lf.drawPopupMenuSectionHeader (g, row, item.text);
                else
                    lf.drawPopupMenuItem (g, row, item.separator, true, item.highlighted, item.ticked, item.sub, item.text, {}, nullptr, nullptr);
                y += heights[i];
            }
        }
        const auto file = outDir.getChildFile ("menu.png");
        file.deleteFile();
        juce::FileOutputStream stream (file);
        juce::PNGImageFormat().writeImageToStream (image, stream);
        std::cout << file.getFullPathName() << std::endl;
        return 0;
    }

    // ILANA_SNAPSHOT_FLASH: a preset load while the editor is open, as it
    // plays out (the knobs a load moved flash once): flash-0.png before,
    // flash-1.png 60 ms after, flash-2.png 250 ms after; then stop.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_FLASH", "").isNotEmpty())
    {
#if JUCE_LINUX
        // (A bare Xvfb has no window manager, so one window property names an atom that
        // does not exist; the window works without it.)
        // (xlib's own header clashes with JUCE's names, so find the call at run time.)
        if (auto* setHandler = (void* (*) (int (*) (void*, void*))) dlsym (RTLD_DEFAULT, "XSetErrorHandler"))
            setHandler ([] (void*, void*) { return 0; });
#endif
        editor->addToDesktop (0); // (the knobs only animate in a window that is showing)
        editor->setVisible (true);
        settle (400);
        processor.loadPresetByIndex (0);
        settle (400);
        save (*editor, outDir.getChildFile ("flash-0.png"));
        processor.loadPresetByIndex (juce::jmin (5, processor.getNumAllPresets() - 1));
        IlanaAnim::notePresetLoad();
        settle (60);
        save (*editor, outDir.getChildFile ("flash-1.png"));
        settle (190);
        save (*editor, outDir.getChildFile ("flash-2.png"));
        return 0;
    }

    // ILANA_SNAPSHOT_STRIP: the OSC tab's one-row strip with the strings' tuning open
    // (strip-tuning.png), then the macros sliding to page 2 (strip-slide-0..5.png, 50 ms
    // apart, and strip-page2.png), then stop.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_STRIP", "").isNotEmpty())
    {
#if JUCE_LINUX
        if (auto* setHandler = (void* (*) (int (*) (void*, void*))) dlsym (RTLD_DEFAULT, "XSetErrorHandler"))
            setHandler ([] (void*, void*) { return 0; });
#endif
        for (const auto* id : { "sym_on", "sym_manual", "subosc_on" })
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (1.0f);
        editor->addToDesktop (0);
        editor->setVisible (true);
        pages->showPage ("MAIN");
        settle (400);
        {
            // (The strings' chevron: the one TextButton that reads a lone ">".)
            std::vector<juce::TextButton*> buttons;
            findAll<juce::TextButton> (*editor, buttons);
            for (auto* button : buttons)
                if (button->getButtonText() == juce::String::fromUTF8 ("\xe2\x80\xba") && button->isShowing() && button->getTooltip().contains ("six notes"))
                    button->triggerClick();
        }
        settle (300);
        save (*editor, outDir.getChildFile ("strip-tuning.png"));
        if (auto* pager = findChild<MacroPageSwitch> (*editor); pager != nullptr && pager->onPick != nullptr)
        {
            pager->onPick (1);
            for (int i = 0; i < 6; ++i)
            {
                settle (50);
                save (*editor, outDir.getChildFile ("strip-slide-" + juce::String (i) + ".png"));
            }
            settle (300);
            save (*editor, outDir.getChildFile ("strip-page2.png"));
        }
        return 0;
    }

    // ILANA_SNAPSHOT_FMCABLES: the FM matrix with three routes (OSC 2 into OSC 1, OSC 1 into
    // OSC 2, noise into OSC 2 and OSC 1): the cables under the frosted cells, three frames
    // apart (the pulses run); fmcables-0/1/2.png, then stop.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_FMCABLES", "").isNotEmpty())
    {
#if JUCE_LINUX
        if (auto* setHandler = (void* (*) (int (*) (void*, void*))) dlsym (RTLD_DEFAULT, "XSetErrorHandler"))
            setHandler ([] (void*, void*) { return 0; });
#endif
        const auto setParam = [&] (const char* id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };
        setParam ("fm_amount", 0.30f);   // OSC 2 into OSC 1
        setParam ("fm_1to2", 0.15f);     // OSC 1 into OSC 2
        setParam ("fm_noise2", 0.45f);   // noise into OSC 2
        setParam ("fm_noise1", 0.20f);   // noise into OSC 1
        editor->addToDesktop (0);
        editor->setVisible (true);
        pages->showPage ("FM");
        settle (500);
        for (int i = 0; i < 3; ++i)
        {
            save (*editor, outDir.getChildFile ("fmcables-" + juce::String (i) + ".png"));
            settle (250);
        }
        return 0;
    }

    // ILANA_SNAPSHOT_SAMPLER: the sampler steps. OSC 1 plays a three-tone
    // sample cut into slices (the PLAYBACK row, slice lines on the wave),
    // then the zone map and the sample browser; sampler-card.png,
    // sampler-zones.png, sampler-browser.png, then stop.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_SAMPLER", "").isNotEmpty())
    {
#if JUCE_LINUX
        if (auto* setHandler = (void* (*) (int (*) (void*, void*))) dlsym (RTLD_DEFAULT, "XSetErrorHandler"))
            setHandler ([] (void*, void*) { return 0; });
#endif
        const auto setParam = [&] (const char* id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };
        auto tones = std::make_shared<SampleData>();
        tones->sampleRate = 48000.0;
        tones->name = "Three Tones";
        tones->buffer.setSize (2, 96000);
        for (int i = 0; i < 96000; ++i)
        {
            const auto part = i / 12000, local = i % 12000;
            const auto value = (float) (0.7 * std::exp (-4.0 * local / 12000.0)
                                        * std::sin (juce::MathConstants<double>::twoPi * (110.0 * (1 + part % 4)) * local / 48000.0));
            tones->buffer.setSample (0, i, value);
            tones->buffer.setSample (1, i, value);
        }
        setParam ("osc1_mode", 2.0f);
        processor.setUserSample (0, tones, {});
        SampleSlicer::Settings slicing;
        slicing.mode = SampleSlicer::transients;
        processor.setSampleSlices (0, slicing, "Slice sample");
        setParam ("osc1_sample_playback", 1.0f);
        setParam ("osc1_sample_quality", 2.0f);
        setParam ("osc1_sample_start_rand", 0.2f);
        editor->addToDesktop (0);
        editor->setVisible (true);
        pages->showPage ("MAIN");
        settle (500);
        save (*editor, outDir.getChildFile ("sampler-card.png"));
        pages->openZoneEditor (0, IlanaTheme::accent());
        settle (400);
        save (*editor, outDir.getChildFile ("sampler-zones.png"));
        pages->closeZoneEditor();
        settle (100);
        pages->openSampleBrowser (0);
        settle (400);
        save (*editor, outDir.getChildFile ("sampler-browser.png"));
        pages->closeSampleBrowser();
        return 0;
    }

    // ILANA_SNAPSHOT_WTEDITOR: the wavetable editor over OSC 1's table, in
    // each of its three modes (wteditor-draw.png, -spectrum.png,
    // -formula.png), then stop.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_WTEDITOR", "").isNotEmpty())
    {
#if JUCE_LINUX
        if (auto* setHandler = (void* (*) (int (*) (void*, void*))) dlsym (RTLD_DEFAULT, "XSetErrorHandler"))
            setHandler ([] (void*, void*) { return 0; });
#endif
        editor->addToDesktop (0);
        editor->setVisible (true);
        if (auto* parameter = processor.apvts.getParameter ("osc1_table"))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (3.0f));
        pages->showPage ("MAIN");
        settle (300);
        const auto clickButton = [&] (juce::Component& root, const juce::String& prefix)
        {
            std::vector<juce::TextButton*> buttons;
            findAll<juce::TextButton> (root, buttons);
            for (auto* button : buttons)
                if (button->getButtonText().startsWith (prefix) && visibleInTree (button))
                {
                    button->triggerClick();
                    return true;
                }
            return false;
        };
        clickButton (*editor, "EDIT TABLE");
        settle (400);
        if (auto* tableEditor = pages->getWavetableEditor())
        {
            for (const auto& [button, name] : { std::pair<const char*, const char*> { "DRAW", "draw" },
                                                { "SPECTRUM", "spectrum" }, { "FORMULA", "formula" } })
            {
                clickButton (*tableEditor, button);
                settle (300);
                save (*editor, outDir.getChildFile (juce::String ("wteditor-") + name + ".png"));
            }
        }
        return 0;
    }

    // ILANA_SNAPSHOT_SWEEP: a held chord with LFO 1 on filter 1's cutoff, the
    // filter graph with its sweep band: sweep-0.png before any audio, then
    // sweep-1.png and sweep-2.png after about 0.6 s and 1.3 s of playing; stop.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_SWEEP", "").isNotEmpty())
    {
       #if JUCE_LINUX
        if (auto* setHandler = (void* (*) (int (*) (void*, void*))) dlsym (RTLD_DEFAULT, "XSetErrorHandler"))
            setHandler ([] (void*, void*) { return 0; });
       #endif
        editor->addToDesktop (0);
        editor->setVisible (true);
        const auto set = [&processor] (const char* id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };
        processor.loadPresetByIndex (0);
        set ("f1_cutoff", 900.0f);
        set ("f1_reso", 0.45f);
        set ("mod1_src", (float) Mod::Source::Lfo1);
        set ("mod1_dst", (float) Mod::Destination::Filter1Cutoff);
        set ("mod1_amt", 0.8f);
        settle (400);
        save (*editor, outDir.getChildFile ("sweep-0.png"));
        processor.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buffer (2, 512);
        const auto play = [&] (int blocks, bool chord)
        {
            for (int b = 0; b < blocks; ++b)
            {
                juce::MidiBuffer midi;
                if (chord && b == 0)
                    for (const auto note : { 48, 55, 60, 64 })
                        midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
                buffer.clear();
                processor.processBlock (buffer, midi);
                if (b % 8 == 0)
                    settle (20);
            }
        };
        play (56, true);
        save (*editor, outDir.getChildFile ("sweep-1.png"));
        play (64, false);
        save (*editor, outDir.getChildFile ("sweep-2.png"));
        return 0;
    }

    // ILANA_SNAPSHOT_WIDGETS: the real pop-ups, drawn by the look the user
    // gets (the process default while the editor is open), then stop:
    //   widgets-knob-menu.png   FRAME's right-click menu with "Modulate with"
    //                           opened by the keys (the keyboard highlight)
    //   widgets-combo-menu.png  a type box's list
    //   widgets-sheet.png       tooltip, text fields (typing, with a selection,
    //                           and empty), scroll bar, progress, call-out box
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_WIDGETS", "").isNotEmpty())
    {
        const auto scale = (float) juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_SCALE", "1.5").getDoubleValue();
        auto& lf = juce::LookAndFeel::getDefaultLookAndFeel();
        const auto writePng = [&outDir] (const juce::Image& image, const juce::String& name)
        {
            const auto file = outDir.getChildFile (name);
            file.deleteFile();
            juce::FileOutputStream stream (file);
            juce::PNGImageFormat().writeImageToStream (image, stream);
            std::cout << file.getFullPathName() << std::endl;
        };

        // Offscreen, as the menu windows draw them (a real menu window
        // can't open under xvfb): the look is the process default, which
        // is what a desktop menu with no look of its own takes.
        pages->showPage ("MAIN");
        // Two routings into CUTOFF, so the menu shows their submenus.
        processor.assignModSlot ((int) Mod::Source::Lfo1, (int) Mod::Destination::Filter1Cutoff, 0.35f);
        processor.assignModSlot ((int) Mod::Source::FilterEnv, (int) Mod::Destination::Filter1Cutoff, 0.5f);
        settle (400);

        std::vector<KnobControl*> knobs;
        findAll<KnobControl> (*editor, knobs);
        KnobControl* knob = nullptr;
        for (auto* candidate : knobs)
            if (candidate->getRingDestination() == (int) Mod::Destination::Filter1Cutoff)
                candidate->syncRoutings();
        for (auto* candidate : knobs)
            if (knob == nullptr && candidate->getRingDestination() == (int) Mod::Destination::Filter1Cutoff && candidate->getNumRoutings() > 0)
                knob = candidate;
        for (auto* candidate : knobs)
            if (knob == nullptr && visibleInTree (candidate) && candidate->getNumRings() > 0)
                knob = candidate;

        if (knob != nullptr)
        {
            // The menu with "Modulate with" lit (keys), its submenu beside it.
            const auto menu = knob->buildModMenu();
            juce::PopupMenu sub;
            for (juce::PopupMenu::MenuItemIterator it (menu); it.next();)
                if (it.getItem().subMenu != nullptr && sub.getNumItems() == 0)
                    sub = *it.getItem().subMenu;
            writePng (renderMenuSheet (lf, { { menu, 0 }, { sub, 1 } }, scale), "widgets-knob-menu.png");

            // The same menu with a routing's submenu open (its polarity).
            juce::PopupMenu route;
            auto row = 0, routeRow = -1;
            for (juce::PopupMenu::MenuItemIterator it (menu); it.next(); ++row)
                if (it.getItem().subMenu != nullptr && route.getNumItems() == 0)
                    for (juce::PopupMenu::MenuItemIterator sub2 (*it.getItem().subMenu); sub2.next();)
                        if (sub2.getItem().text == "Unipolar")
                        {
                            route = *it.getItem().subMenu;
                            routeRow = row;
                            break;
                        }
            if (route.getNumItems() > 0)
                writePng (renderMenuSheet (lf, { { menu, routeRow }, { route, -1 } }, scale), "widgets-knob-route-menu.png");
        }

        std::vector<juce::ComboBox*> combos;
        findAll<juce::ComboBox> (*editor, combos);
        for (auto* combo : combos)
            if (visibleInTree (combo) && combo->getNumItems() > 3)
            {
                writePng (renderMenuSheet (combo->getLookAndFeel(), { { *combo->getRootMenu(), 2 } }, scale, combo->getWidth()),
                          "widgets-combo-menu.png");
                break;
            }

        // (No AlertWindow here: it is a top-level window, and a real window
        // can't open under xvfb. It draws through drawAlertBox in the
        // default look, which the uitest checks is the design's.)

        {
            struct Sheet : public juce::Component
            {
                void paint (juce::Graphics& g) override { g.fillAll (IlanaTheme::Ui::bg); }
            } sheet;
            sheet.setSize (640, 330);

            // A tooltip as the editor's TooltipWindow draws it.
            struct Tip : public juce::Component
            {
                juce::String text;
                void paint (juce::Graphics& g) override { getLookAndFeel().drawTooltip (g, text, getWidth(), getHeight()); }
            } tip;
            tip.text = "Frame\nWhere in the wavetable the oscillator plays. Drag a modulation source here to sweep it.";
            tip.setBounds (lf.getTooltipBounds (tip.text, { 0, 0 }, { 0, 0, 1000, 1000 }).withPosition (16, 16));
            sheet.addAndMakeVisible (tip);

            juce::TextEditor typing, empty;
            typing.setText ("Neuro Wobble");
            typing.setBounds (16, 120, 260, 30);
            empty.setTextToShowWhenEmpty ("Optional", IlanaTheme::Ui::text3);
            empty.setBounds (16, 160, 260, 30);
            for (auto* field : { &typing, &empty })
            {
                field->setFont (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::body, true)));
                field->setIndents (10, 7);
                sheet.addAndMakeVisible (*field);
            }

            juce::ScrollBar scroll (true);
            scroll.setRangeLimits (0.0, 100.0);
            scroll.setCurrentRange (20.0, 35.0);
            scroll.setBounds (300, 120, 10, 160);
            sheet.addAndMakeVisible (scroll);

            double progress = 0.62;
            juce::ProgressBar bar (progress);
            bar.setBounds (16, 210, 260, 12);
            sheet.addAndMakeVisible (bar);

            juce::Label callOutText ({}, "A call-out box");
            callOutText.setJustificationType (juce::Justification::centred);
            callOutText.setColour (juce::Label::textColourId, IlanaTheme::Ui::text);
            callOutText.setSize (180, 60);

            // (A child of the sheet, not a desktop window.)
            juce::CallOutBox callOut (callOutText, { 420, 40, 20, 20 }, &sheet);
            typing.setHighlightedRegion ({ 6, 12 });
            settle (300);
            writePng (sheet.createComponentSnapshot (sheet.getLocalBounds(), true, scale), "widgets-sheet.png");
        }

        return 0;
    }

    // ILANA_SNAPSHOT_PLAY: just PLAY (UI review 12): the patch as loaded, then
    // with oscillators 4 and 5 added, then all six; then stop.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_PLAY", "").isNotEmpty())
    {
        pages->showPage ("MAIN");
        settle (500);
        save (*editor, outDir.getChildFile ("play.png"));
        // The one-tab PLAY's other views (mockup v3, 2026-10-25): FILTER 2 in parallel on a morphing type,
        // KEYS, BOARD, the strings' tuning, ENVELOPE's EDIT pop-up and the dock's STEREO pop-up.
        {
            auto* shell = dynamic_cast<IlanaSynthAudioProcessorEditor*> (editor.get());
            const auto set = [&processor] (const char* id, float value)
            {
                if (auto* parameter = processor.apvts.getParameter (id))
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
            };
            const auto click = [&] (const juce::String& name)
            {
                std::vector<juce::Button*> buttons;
                findAll<juce::Button> (*editor, buttons);
                for (auto* button : buttons)
                    if (button->getName() == name && visibleInTree (button))
                    {
                        button->triggerClick();
                        return;
                    }
            };
            if (shell != nullptr && shell->showPlayViews != nullptr)
            {
                set ("filters_parallel", 1.0f);
                set ("f2_type", (float) FilterType::VowelMorph);
                set ("sym_on", 1.0f);
                set ("sym_manual", 1.0f);
                shell->showPlayViews (1, 1, 1);
                settle (600);
                save (*editor, outDir.getChildFile ("play-views.png"));
                shell->showPlayViews (-1, 0, -1);
                click ("Tuning");
                settle (400);
                save (*editor, outDir.getChildFile ("play-tuning.png"));
                click ("Tuning");
                click ("PLAY envelope EDIT");
                settle (500);
                save (*editor, outDir.getChildFile ("play-edit-popup.png"));
                // The decay handle hovered: its value beside it (Impeccable re-score toward 35).
                {
                    std::vector<EnvelopeDisplay*> displays;
                    findAll<EnvelopeDisplay> (*editor, displays);
                    EnvelopeDisplay* biggest = nullptr;
                    for (auto* display : displays)
                        if (visibleInTree (display) && ! display->showsHandlesOnHoverOnly())
                            biggest = display;
                    if (biggest != nullptr)
                    {
                        const auto at = biggest->getHandlePosition (1);
                        auto& component = static_cast<juce::Component&> (*biggest);
                        component.mouseMove (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at, juce::ModifierKeys(),
                                                               1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &component, &component, juce::Time::getCurrentTime(),
                                                               at, juce::Time::getCurrentTime(), 1, false));
                        settle (200);
                        save (*editor, outDir.getChildFile ("play-edit-popup-hover.png"));
                    }
                }
                click ("PLAY envelope EDIT");
                settle (300);
                click ("PLAY LFO EDIT");
                settle (500);
                save (*editor, outDir.getChildFile ("play-edit-lfo-popup.png"));
                // A simulated shape: TRIGGER, FIRE and every knob in the compact pop-up.
                set ("lfo1_shape", (float) LfoSimShapes::Pendulum);
                settle (500);
                save (*editor, outDir.getChildFile ("play-edit-lfo-sim-popup.png"));
                set ("lfo1_shape", 0.0f);
                settle (300);
                click ("PLAY LFO EDIT");
                settle (300);
                click ("STEREO");
                settle (400);
                save (*editor, outDir.getChildFile ("play-stereo-popup.png"));
                click ("STEREO");
                // OP PITCH (FM's link, UI review 25): its own editor in the pop-up on PLAY, by its dock chip.
                set ("osc1_amp_env", (float) OperatorEg::envelopeChoice);
                settle (500);
                FmOperatorInfo::openPitchAndLfo();
                settle (500);
                save (*editor, outDir.getChildFile ("play-op-pitch-popup.png"));
                {
                    std::vector<ModSourceChip*> dockChips;
                    findAll<ModSourceChip> (*editor, dockChips);
                    for (auto* chip : dockChips)
                        if (chip->getSourceIndex() == (int) Mod::Source::OpLfo && visibleInTree (chip) && chip->onClick != nullptr)
                        {
                            chip->onClick (*chip);
                            break;
                        }
                    settle (500);
                    save (*editor, outDir.getChildFile ("play-op-lfo-popup.png"));
                }
                {
                    std::vector<juce::TextButton*> buttons;
                    findAll<juce::TextButton> (*editor, buttons);
                    for (auto* button : buttons)
                        if (button->getButtonText() == juce::String (juce::CharPointer_UTF8 ("\xc3\x97")) && button->getName() != "removeRoute" && visibleInTree (button))
                        {
                            button->triggerClick();
                            break;
                        }
                }
                set ("osc1_amp_env", 0.0f);
                set ("filters_parallel", 0.0f);
                set ("f2_type", 0.0f);
                set ("sym_on", 0.0f);
                set ("sym_manual", 0.0f);
                shell->showPlayViews (0, 0, 0);
                settle (400);
            }
        }
        // Three open (OSC 2 and 3 switched on; OSC 3 is "sub"): three two-row cards fill the column.
        for (const char* id : { "osc2_on", "sub_on" })
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (1.0f);
        settle (500);
        // The pin on the SUB + NOISE seam: SUB + NOISE below the scrolling cards.
        const auto setPin = [&] (bool pinned)
        {
            std::vector<juce::Button*> buttons;
            findAll<juce::Button> (*editor, buttons);
            for (auto* button : buttons)
                if (button->getName() == "pin" && button->getToggleState() != pinned)
                {
                    button->triggerClick();
                    break;
                }
            settle (500);
        };
        setPin (false);
        save (*editor, outDir.getChildFile ("play-3osc.png"));
        // Scrolled to the end: the + ADD OSC 4 row under the third card.
        {
            std::vector<DashedAddButton*> adds;
            findAll<DashedAddButton> (*editor, adds);
            for (auto* add : adds)
                if (visibleInTree (add) && add->getButtonText().contains ("ADD OSC"))
                    if (auto* view = add->findParentComponentOfClass<juce::Viewport>())
                    {
                        view->setViewPosition (0, 1 << 20);
                        settle (300);
                        save (*editor, outDir.getChildFile ("play-3osc-scrolled.png"));
                        view->setViewPosition (0, 0);
                        settle (300);
                        break;
                    }
        }
        setPin (true);
        save (*editor, outDir.getChildFile ("play-3osc-pinned.png"));
        setPin (false);
        // The other engines' cards: a string, a sample and grains.
        for (const auto& [id, mode] : { std::pair<const char*, float> { "osc1_mode", 1.0f }, { "osc2_mode", 2.0f }, { "sub_mode", 3.0f } })
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (mode));
        settle (600);
        save (*editor, outDir.getChildFile ("play-engines.png"));
        for (const auto& [id, mode] : { std::pair<const char*, float> { "osc1_mode", 0.0f }, { "osc2_mode", 0.0f }, { "sub_mode", 0.0f } })
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (mode));
        settle (400);
        processor.addOscillator (3);
        processor.addOscillator (4);
        settle (500);
        save (*editor, outDir.getChildFile ("play-5osc.png"));
        processor.addOscillator (5);
        settle (500);
        save (*editor, outDir.getChildFile ("play-6osc.png"));
        setPin (true);
        save (*editor, outDir.getChildFile ("play-6osc-pinned.png"));
        setPin (false);
        // One wheel notch and two: the glide comes to rest on a card edge (UI review 28), pictured and logged.
        // (The oscillator area: the viewport on PLAY whose page overflows it.)
        std::vector<juce::Viewport*> viewports;
        findAll<juce::Viewport> (*editor, viewports);
        juce::Viewport* oscArea = nullptr;
        for (auto* candidate : viewports)
            if (candidate->isVisible() && candidate->getViewedComponent() != nullptr
                && candidate->getViewedComponent()->getHeight() > candidate->getHeight() + 1)
                oscArea = candidate;
        if (auto* view = oscArea)
            for (const auto notches : { 1, 2 })
            {
                view->setViewPosition (0, 0);
                settle (100);
                juce::MouseWheelDetails wheel {};
                wheel.deltaY = -1.0f;
                for (int i = 0; i < notches; ++i)
                    view->mouseWheelMove (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), juce::Point<float> { 40.0f, 40.0f }, juce::ModifierKeys {},
                                                            1.0f, 0.0f, 0.0f, 0.0f, 0.0f, view, view, juce::Time::getCurrentTime(),
                                                            juce::Point<float> { 40.0f, 40.0f }, juce::Time::getCurrentTime(), 1, false),
                                          wheel);
                settle (900);
                std::fprintf (stderr, "PLAY osc wheel %d notch(es): rests at %d\n", notches, view->getViewPositionY());
                save (*editor, outDir.getChildFile ("play-6osc-wheel" + juce::String (notches) + ".png"));
            }
        return 0;
    }

    // ILANA_SNAPSHOT_OSC: just OSC, in the states the design is checked in
    // (design review: oscillator counts 1, 3 and 6, a Sample, a String, an
    // Operator EG, a Grain and a Live oscillator); then stop.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_OSC", "").isNotEmpty())
    {
        const auto set = [&processor] (const juce::String& id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };
        const auto shot = [&] (const juce::String& name, int scrollTo = 0)
        {
            settle (500);
            if (auto* viewport = dynamic_cast<juce::Viewport*> (pages->getCurrentPage()))
            {
                viewport->setViewPosition (0, scrollTo);
                settle (200);
            }
            save (*editor, outDir.getChildFile (name + ".png"));
        };
        auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ilana-shot-sfz");
        folder.createDirectory();
        {
            juce::AudioBuffer<float> tone (1, 22050);
            for (int i = 0; i < tone.getNumSamples(); ++i)
                tone.setSample (0, i, 0.5f * std::sin ((float) i * 0.06f) * std::exp (-(float) i / 8000.0f) * (0.6f + 0.4f * std::sin ((float) i * 0.0011f)));
            juce::WavAudioFormat wav;
            auto file = folder.getChildFile ("tone.wav");
            file.deleteFile();
            if (auto stream = std::unique_ptr<juce::OutputStream> (file.createOutputStream()))
                if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.get(), 44100.0, 1, 16, {}, 0)))
                {
                    stream.release();
                    writer->writeFromAudioSampleBuffer (tone, 0, tone.getNumSamples());
                }
            folder.getChildFile ("test.sfz").replaceWithText ("<region> sample=tone.wav lokey=36 hikey=59 pitch_keycenter=48\n"
                                                              "<region> sample=tone.wav lokey=60 hikey=84 pitch_keycenter=72\n");
        }

        // (ILANA_SNAPSHOT_SMALL: the 75 % window the UI tests also check.)
        if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_SMALL", "").isNotEmpty())
            editor->getTopLevelComponent()->setSize (795, 540);
        pages->showPage ("MAIN");
        shot ("osc-as-loaded");
        processor.addOscillator (2);
        shot ("osc-3");
        processor.addOscillator (3);
        processor.addOscillator (4);
        processor.addOscillator (5);
        shot ("osc-6-top");
        shot ("osc-6-bottom", 10000);
        // One of each engine: Sample, String, FM / DX7 on the Operator EG, Grain, Live.
        set ("osc1_mode", 2.0f);
        set ("osc2_mode", 1.0f);
        set ("sub_mode", 5.0f);
        set ("sub_amp_env", 17.0f);
        set ("sub_tune", 1.0f);
        set ("osc4_mode", 3.0f);
        set ("osc5_mode", 4.0f);
        processor.loadUserSample (0, folder.getChildFile ("test.sfz"));
        settle (400);
        shot ("osc-mixed-1");
        shot ("osc-mixed-2", 330);
        shot ("osc-mixed-3", 10000);
        // The sample alone, with its unison open.
        for (const auto slot : { 1, 2, 3, 4, 5 })
            processor.removeOscillator (slot);
        shot ("osc-sample-alone");
        set ("osc1_unison", 4.0f);
        shot ("osc-sample-unison");
        set ("osc1_mode", 1.0f);
        shot ("osc-string-alone");
        set ("osc1_mode", 5.0f);
        set ("osc1_amp_env", 17.0f);
        set ("osc1_tune", 1.0f);
        shot ("osc-operator-alone");
        // FM / DX7 on another envelope: its LEVEL where OUTPUT stands, the wave.
        set ("osc1_amp_env", 0.0f);
        set ("osc1_unison", 1.0f);
        shot ("osc-fm-plain");
        set ("osc1_tune", 0.0f);
        set ("osc1_mode", 0.0f);
        shot ("osc-1");
        // The strip opened under MORE.
        {
            std::vector<juce::TextButton*> buttons;
            findAll<juce::TextButton> (*editor, buttons);
            for (auto* button : buttons)
                if (button->isShowing() && button->getButtonText().startsWith ("MORE"))
                    button->triggerClick();
        }
        set ("sym_on", 1.0f);
        set ("sym_manual", 1.0f);
        shot ("osc-strip-open");
        folder.deleteRecursively();
        return 0;
    }

    // ILANA_SNAPSHOT_FX: the FX rack in the states the design is checked in
    // (the mockup's five effects with one bypassed, a long rack, bands, the
    // delay's taps, a duplicate, Airwindows models, an empty rack); then stop.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_FX", "").isNotEmpty())
    {
        pages->showPage ("FX");
        const auto setParam = [&] (const juce::String& id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };
        const auto clear = [&]
        {
            for (int slot = 1; slot <= IlanaSynthAudioProcessor::numFxSlots; ++slot)
            {
                const auto prefix = "fx_slot" + juce::String (slot);
                setParam (prefix, 0.0f);
                setParam (prefix + "_bypass", 0.0f);
                setParam (prefix + "_solo", 0.0f);
                setParam (prefix + "_band", 0.0f);
                setParam (prefix + "_mix", 1.0f);
            }
        };
        const auto fill = [&] (std::vector<int> types)
        {
            clear();
            auto slot = 1;
            for (const auto type : types)
                processor.assignFxSlot (slot++, type);
        };
        const auto shot = [&] (const juce::String& name)
        {
            settle (500);
            save (*editor, outDir.getChildFile ("fx-" + name + ".png"));
        };

        fill ({ 27, 2, 20, 36, 34 });
        setParam ("fx_slot4_bypass", 1.0f);
        shot ("mock");
        // The same rack routed in parallel (design round 2: the ladder rail).
        setParam ("fx_routing", 1.0f);
        shot ("mock-parallel");
        setParam ("fx_routing", 0.0f);
        fill ({ 27, 2, 20, 7, 13, 9, 18, 1 });
        shot ("eight");
        fill ({ 27, 2, 20, 7, 13, 9, 18, 1, 29, 4 });
        shot ("ten");
        fill ({ 9, 16, 29, 4 });
        setParam ("fx_taps_on", 1.0f);
        shot ("taps-gate");
        // The gate's classic chop with LENGTH 50 %: half-width bars.
        setParam ("fx_gate_pattern", 12.0f);
        setParam ("fx_gate_length", 0.5f);
        shot ("gate-length");
        setParam ("fx_gate_pattern", 0.0f);
        setParam ("fx_gate_length", 1.0f);
        fill ({ 12, 22, 19, 21, 31, 25, 17, 10 });
        shot ("plain");
        fill ({ 2, 6, 20, 4 });
        setParam ("fx_slot2_band", 1.0f);
        setParam ("fx_slot3_band", 2.0f);
        setParam ("fx_slot4_band", 3.0f);
        shot ("bands");
        fill ({ 2, 2, 32, 34, 38, 37 });
        shot ("duplicate-aw");
        fill ({ 30, 13, 1, 3 });
        shot ("aw-all");
        // AW Delay's SYNC and DIV: off, then on (the time knob dims), and
        // StarChild2 (no time: no SYNC).
        fill ({ 35, 9, 2 });
        shot ("awdelay");
        setParam ("fx_awdelay_sync", 1.0f);
        shot ("awdelay-sync");
        setParam ("fx_awdelay_algo", 4.0f);
        shot ("awdelay-sync-doublelay");
        setParam ("fx_awdelay_algo", 2.0f);
        shot ("awdelay-starchild");
        setParam ("fx_awdelay_algo", 0.0f);
        setParam ("fx_awdelay_sync", 0.0f);

        // The effect picker (mockup "fxadd"): from the rack's + ADD EFFECT
        // tile and from slot 1's type button, at 100 % and 75 %. (Saved
        // quickly: a call-out closes itself within 200 ms under xvfb.)
        const auto picker = [&] (const juce::String& name, bool fromTitle)
        {
            settle (400);
            if (fromTitle)
            {
                std::vector<FxTypeButton*> titles;
                findAll<FxTypeButton> (*editor, titles);
                for (auto* title : titles)
                    if (visibleInTree (title) && title->getTooltip().startsWith ("Slot 1:") && title->onClick != nullptr)
                    {
                        title->onClick();
                        break;
                    }
            }
            else
            {
                std::vector<DashedAddButton*> tiles;
                findAll<DashedAddButton> (*editor, tiles);
                for (auto* tile : tiles)
                    if (visibleInTree (tile) && tile->getButtonText().contains ("ADD EFFECT"))
                    {
                        tile->triggerClick();
                        break;
                    }
            }
            settle (60);
            save (*editor, outDir.getChildFile ("fx-" + name + ".png"));
            std::vector<FxLibraryView*> views;
            findAll<FxLibraryView> (*editor, views);
            for (auto* view : views)
                if (auto* box = view->findParentComponentOfClass<juce::CallOutBox>())
                    box->dismiss();
            settle (300);
        };
        fill ({ 32, 2, 20 });
        picker ("picker-add", false);
        picker ("picker-replace", true);
        if (auto* top = editor->getTopLevelComponent())
        {
            const auto before = top->getBounds();
            top->setSize (795, 540);
            picker ("picker-replace-75", true);
            top->setBounds (before);
        }

        clear();
        shot ("empty");
        return 0;
    }

    // ILANA_SNAPSHOT_FILTERFX: FX type 42, FILTER (ilana 2026-10-11): its
    // RACK card, a Delay > FILTER > back into Delay loop on the PATCH tab
    // with the loop selected, and FILTER's mini window; then stop.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_FILTERFX", "").isNotEmpty())
    {
        const auto setParam = [&] (const juce::String& id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };
        const auto shot = [&] (const juce::String& name)
        {
            settle (500);
            save (*editor, outDir.getChildFile ("filterfx-" + name + ".png"));
        };
        for (int slot = 1; slot <= IlanaSynthAudioProcessor::numFxSlots; ++slot)
            setParam ("fx_slot" + juce::String (slot), 0.0f);
        settle (100);
        for (const auto type : { 9, 42, 13 })
            processor.addFxEffect (type);
        setParam ("fx_delay_on", 1.0f);
        setParam ("fx_reverb_on", 1.0f);
        setParam ("fx_filter_type", (float) FilterType::LadderLow);
        setParam ("fx_filter_cutoff", 900.0f);
        setParam ("fx_filter_reso", 0.7f);
        setParam ("fx_filter_spread", 0.5f);
        pages->showPage ("FX");
        shot ("rack");

        processor.setFxPatcherOn (true);
        settle (200);
        auto graph = processor.getFxPatchGraph();
        std::map<int, PatchGraph::NodeId> nodeOf;
        for (const auto& node : graph.nodes())
            if (! node.isFixed())
                nodeOf[node.ref] = node.id;
        graph.connect ({ nodeOf[2], 0 }, { nodeOf[1], 0 }, 0.5f); // FILTER back into Delay
        processor.setFxPatchGraph (graph);
        pages->showPage ("PATCH");
        settle (200);
        if (auto* view = findChild<PatchCableView> (*editor))
        {
            const auto loop = view->getGraph().findCable ({ nodeOf[2], 0 }, { nodeOf[1], 0 });
            view->selectCable (loop);
        }
        shot ("loop");
        if (auto* view = findChild<PatchCableView> (*editor))
            for (const auto& node : view->getGraph().nodes())
                if (node.ref == 2 && node.id < (1u << 20))
                {
                    const auto centre = view->nodeBounds (node).getCentre() * view->getZoom();
                    view->mouseDoubleClick (PatchCableUi::mouse (*view, centre, false));
                    break;
                }
        shot ("window");
        return 0;
    }

    // ILANA_SNAPSHOT_FXLFO: the PATCH tab's LFO boxes (ilana 2026-10-12):
    // Delay, Delay 2, FILTER and Reverb; FX LFO 1 cabled to Delay 2 moving
    // TIME and FEEDBACK, FX LFO 2 (Sample & Hold, synced) on FILTER's cutoff; the
    // patch, an LFO selected, its cable selected, the windows, and a knob's
    // right-click menu in an effect's window; then stop.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_FXLFO", "").isNotEmpty())
    {
        const auto setParam = [&] (const juce::String& id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };
        const auto shot = [&] (const juce::String& name)
        {
            settle (500);
            save (*editor, outDir.getChildFile ("fxlfo-" + name + ".png"));
        };
        const auto nodeCentre = [&] (PatchCableView& view, PatchGraph::NodeId id)
        {
            for (const auto& node : view.getGraph().nodes())
                if (node.id == id)
                    return view.nodeBounds (node).getCentre() * view.getZoom();
            return juce::Point<float>();
        };
        for (int slot = 1; slot <= IlanaSynthAudioProcessor::numFxSlots; ++slot)
            setParam ("fx_slot" + juce::String (slot), 0.0f);
        settle (100);
        for (const auto type : { 9, 9, 42, 13 })
            processor.addFxEffect (type);
        setParam ("fx_delay_on", 1.0f);
        setParam ("fx_reverb_on", 1.0f);
        setParam ("fx_filter_cutoff", 1200.0f);
        processor.setFxPatcherOn (true);
        settle (200);
        auto graph = processor.getFxPatchGraph();
        std::map<int, PatchGraph::NodeId> nodeOf;
        for (const auto& node : graph.nodes())
            if (! node.isFixed())
                nodeOf[node.ref] = node.id;
        for (const auto& cable : std::vector<PatchGraph::Cable> (graph.cables()))
            graph.disconnect (cable.from, cable.to);
        graph.moveNode (PatchGraph::inputNodeId, 0.0f, 100.0f);
        graph.moveNode (nodeOf[1], 160.0f, 40.0f);   // Delay
        graph.moveNode (nodeOf[2], 160.0f, 160.0f);  // Delay 2
        graph.moveNode (nodeOf[3], 360.0f, 100.0f);  // FILTER
        graph.moveNode (nodeOf[4], 560.0f, 100.0f);  // Reverb
        graph.moveNode (PatchGraph::outputNodeId, 760.0f, 100.0f);
        graph.connect ({ PatchGraph::inputNodeId, 0 }, { nodeOf[1], 0 });
        graph.connect ({ PatchGraph::inputNodeId, 0 }, { nodeOf[2], 0 });
        graph.connect ({ nodeOf[1], 0 }, { nodeOf[3], 0 });
        graph.connect ({ nodeOf[2], 0 }, { nodeOf[3], 0 });
        graph.connect ({ nodeOf[3], 0 }, { nodeOf[4], 0 });
        graph.connect ({ nodeOf[4], 0 }, { PatchGraph::outputNodeId, 0 });
        processor.setFxPatchGraph (graph);

        const auto first = processor.addFxLfo (360.0f, 260.0f);
        FxLfo::Settings slow;
        slow.rateHz = 0.4f;
        processor.setFxLfoSettings (first, slow);
        processor.setFxLfoCable (first, 1, true);
        processor.assignFxLfo (first, 1, "fx_delay_time", true, 0.4f);
        processor.assignFxLfo (first, 1, "fx_delay_feedback", true, -0.3f);
        const auto second = processor.addFxLfo (360.0f, -40.0f);
        FxLfo::Settings steps;
        steps.shape = 5;
        steps.sync = true;
        steps.division = 6; // 1/8
        steps.bipolar = false;
        steps.depth = 0.8f;
        processor.setFxLfoSettings (second, steps);
        processor.assignFxLfo (second, 2, "fx_filter_cutoff", true, 0.6f);

        pages->showPage ("PATCH");
        settle (300);
        PatchCableView* view = nullptr;
        {
            std::vector<PatchCableView*> views;
            findAll<PatchCableView> (*editor, views);
            for (auto* candidate : views)
                if (candidate->getName() == "PATCH")
                    view = candidate;
        }
        if (view == nullptr)
            return 1;
        shot ("patch");

        const auto lfoNode = (PatchGraph::NodeId) ((1u << 21) + (unsigned) first);
        view->mouseDown (PatchCableUi::mouse (*view, nodeCentre (*view, lfoNode), true));
        view->mouseUp (PatchCableUi::mouse (*view, nodeCentre (*view, lfoNode), false));
        shot ("lfo-selected");

        view->selectCable (view->getGraph().findCable ({ lfoNode, 0 }, { nodeOf[2], 0 }));
        shot ("cable-selected");

        view->mouseDoubleClick (PatchCableUi::mouse (*view, nodeCentre (*view, nodeOf[2]), false));
        view->mouseDoubleClick (PatchCableUi::mouse (*view, nodeCentre (*view, lfoNode), false));
        shot ("windows");

        // The LFO window with a drawn shape (a ramp up, a drop, a bump), SMOOTH
        // and RETRIG; then the same points as Steps.
        {
            auto drawn = slow;
            drawn.shape = 9;
            drawn.smooth = 0.06f;
            drawn.retrig = true;
            for (int i = 0; i < FxLfo::drawSteps; ++i)
            {
                const auto t = (float) i / (float) FxLfo::drawSteps;
                drawn.drawn[(size_t) i] = t < 0.5f ? -1.0f + 3.2f * t : t < 0.6f ? -0.4f - 6.0f * (t - 0.5f) : 0.6f * std::sin ((t - 0.6f) * 15.7f);
            }
            processor.setFxLfoSettings (first, drawn);
            shot ("drawn-window");
            drawn.shape = 10;
            drawn.smooth = 0.0f;
            processor.setFxLfoSettings (first, drawn);
            shot ("steps-window");
        }

        // A knob's right-click menu in Delay 2's window: assigned (TIME, its
        // amount submenu beside it) and not yet (DAMPING).
        auto& lf = juce::LookAndFeel::getDefaultLookAndFeel();
        const auto writePng = [&outDir] (const juce::Image& image, const juce::String& name)
        {
            const auto file = outDir.getChildFile (name);
            file.deleteFile();
            juce::FileOutputStream stream (file);
            juce::PNGImageFormat().writeImageToStream (image, stream);
            std::cout << file.getFullPathName() << std::endl;
        };
        std::vector<KnobControl*> knobs;
        findAll<KnobControl> (*editor, knobs);
        for (auto* knob : knobs)
        {
            auto inWindow = false;
            for (auto* parent = knob->getParentComponent(); parent != nullptr; parent = parent->getParentComponent())
                inWindow = inWindow || parent->getName().startsWith ("FX WINDOW") && ! parent->getName().contains ("LFO");
            if (! visibleInTree (knob) || ! inWindow)
                continue;
            if (knob->getParameterId() == "fx_delay_time")
            {
                const auto menu = knob->buildModMenu();
                juce::PopupMenu sub;
                for (juce::PopupMenu::MenuItemIterator it (menu); it.next();)
                    if (it.getItem().subMenu != nullptr && sub.getNumItems() == 0)
                        sub = *it.getItem().subMenu;
                writePng (renderMenuSheet (lf, { { menu, 0 }, { sub, 1 } }, 1.5f), "fxlfo-menu-assigned.png");
            }
            if (knob->getParameterId() == "fx_delay_damping")
                writePng (renderMenuSheet (lf, { { knob->buildModMenu(), 0 } }, 1.5f), "fxlfo-menu-assign.png");
        }
        return 0;
    }

    // ILANA_SNAPSHOT_FXDIM: a Delay with SYNC on, its RACK card and its
    // PATCH window side by side (ilana 2026-10-22: TIME greyed in the rack
    // but not in the window); prints each window's bounds; then stop.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_FXDIM", "").isNotEmpty())
    {
        const auto setParam = [&] (const juce::String& id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };
        for (int slot = 1; slot <= IlanaSynthAudioProcessor::numFxSlots; ++slot)
            setParam ("fx_slot" + juce::String (slot), 0.0f);
        settle (100);
        processor.addFxEffect (9);
        setParam ("fx_delay_on", 1.0f);
        setParam ("fx_delay_sync", 1.0f);
        settle (200);
        pages->showPage ("FX");
        settle (500);
        save (*editor, outDir.getChildFile ("fxdim-rack.png"));

        processor.setFxPatcherOn (true);
        pages->showPage ("PATCH");
        settle (400);
        std::vector<PatchCableView*> views;
        findAll<PatchCableView> (*editor, views);
        for (auto* view : views)
            if (view->getName() == "PATCH")
                for (const auto& node : view->getGraph().nodes())
                    if (! node.isFixed() && node.kind == 9)
                        view->mouseDoubleClick (PatchCableUi::mouse (*view, view->nodeBounds (node).getCentre() * view->getZoom(), false));
        settle (500);
        std::vector<juce::Component*> all;
        findAll<juce::Component> (*editor, all);
        for (auto* item : all)
            if (item->getName().startsWith ("FX WINDOW") && visibleInTree (item))
                std::cout << "WINDOW " << editor->getLocalArea (item, item->getLocalBounds()).toString() << std::endl;
        save (*editor, outDir.getChildFile ("fxdim-patch.png"));
        return 0;
    }

    // ILANA_SNAPSHOT_PATCHER: the FX tab's PATCHER (roadmap step 17): a rack
    // shown as cables, then a patch with a parallel split, two instances of
    // one effect and a loop, the RACK page with those instances, and the
    // patcher at 75 %; then stop.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_PATCHER", "").isNotEmpty())
    {
        const auto setParam = [&] (const juce::String& id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };
        const auto shot = [&] (const juce::String& name)
        {
            settle (500);
            save (*editor, outDir.getChildFile ("patcher-" + name + ".png"));
        };
        for (int slot = 1; slot <= IlanaSynthAudioProcessor::numFxSlots; ++slot)
            setParam ("fx_slot" + juce::String (slot), 0.0f);
        settle (100);
        for (const auto type : { 2, 9, 13, 7 })
            processor.addFxEffect (type);
        pages->showPage ("PATCH");
        shot ("rack");

        // Drive splits into two Delays (the second its own instance, a
        // shorter echo), both into Chorus then Reverb; Reverb feeds Drive back.
        processor.addFxEffect (9);
        setParam ("fx_delay_time", 0.12f);
        setParam ("fx_delay_feedback", 0.2f);
        processor.setFxPatcherOn (true);
        settle (200);
        auto graph = processor.getFxPatchGraph();
        std::map<int, PatchGraph::NodeId> nodeOf;
        for (const auto& node : graph.nodes())
            if (! node.isFixed())
                nodeOf[node.ref] = node.id;
        for (const auto& cable : std::vector<PatchGraph::Cable> (graph.cables()))
            graph.disconnect (cable.from, cable.to);
        const auto place = [&] (PatchGraph::NodeId id, float x, float y) { graph.moveNode (id, x, y); };
        place (PatchGraph::inputNodeId, 0.0f, 80.0f);
        place (nodeOf[1], 140.0f, 80.0f);   // Drive
        place (nodeOf[2], 330.0f, 20.0f);   // Delay 1
        place (nodeOf[5], 330.0f, 140.0f);  // Delay 2
        place (nodeOf[4], 520.0f, 80.0f);   // Chorus
        place (nodeOf[3], 710.0f, 80.0f);   // Reverb
        place (PatchGraph::outputNodeId, 900.0f, 80.0f);
        const auto wire = [&] (PatchGraph::NodeId from, PatchGraph::NodeId to, float gain = 1.0f) { graph.connect ({ from, 0 }, { to, 0 }, gain); };
        wire (PatchGraph::inputNodeId, nodeOf[1]);
        wire (nodeOf[1], nodeOf[2]);
        wire (nodeOf[1], nodeOf[5], 0.7f);
        wire (nodeOf[2], nodeOf[4]);
        wire (nodeOf[5], nodeOf[4]);
        wire (nodeOf[4], nodeOf[3]);
        wire (nodeOf[3], PatchGraph::outputNodeId);
        wire (nodeOf[3], nodeOf[1], 0.25f);
        processor.setFxPatchGraph (graph);
        if (auto* view = findChild<PatchCableView> (*editor))
            view->centreContent();
        shot ("loop");
        // Delay 2 selected: its row under the canvas.
        if (auto* view = findChild<PatchCableView> (*editor))
            for (const auto& node : view->getGraph().nodes())
                if (node.ref == 5 && node.id < (1u << 20)) // (not a voice box: PatchPage::voiceId)
                {
                    const auto centre = view->nodeBounds (node).getCentre() * view->getZoom();
                    view->mouseDown (PatchCableUi::mouse (*view, centre, true));
                    view->mouseUp (PatchCableUi::mouse (*view, centre, false));
                }
        shot ("selected");

        // Mini windows: Delay 2's and Reverb's, opened by double-clicks.
        if (auto* view = findChild<PatchCableView> (*editor))
            for (const auto ref : { 3, 5 })
            {
                for (const auto& node : view->getGraph().nodes())
                    if (node.ref == ref && node.id < (1u << 20))
                    {
                        const auto centre = view->nodeBounds (node).getCentre() * view->getZoom();
                        view->mouseDoubleClick (PatchCableUi::mouse (*view, centre, false));
                        break;
                    }
                // (One window docks at the right and the graph refits beside it.)
                if (ref == 3)
                    shot ("window-one");
            }
        shot ("windows");

        pages->showPage ("FX");
        shot ("rack-instances");

        // ilana's patch (2026-10-11): Reverb into Smear and EQ, Smear into
        // EQ, an EQ to Reverb loop (here at its 70 % top, so the guard holds
        // it), with the loop selected; then the RACK with a second Reverb.
        {
            for (int slot = 1; slot <= IlanaSynthAudioProcessor::numFxSlots; ++slot)
                setParam ("fx_slot" + juce::String (slot), 0.0f);
            settle (100);
            for (const auto type : { 13, 11, 29 })
                processor.addFxEffect (type);
            setParam ("fx_reverb_on", 1.0f);
            setParam ("fx_smear_on", 1.0f);
            processor.setFxPatcherOn (true);
            auto patch = processor.getFxPatchGraph();
            std::map<int, PatchGraph::NodeId> boxOf;
            for (const auto& node : patch.nodes())
                if (! node.isFixed())
                    boxOf[node.ref] = node.id;
            for (const auto& cable : std::vector<PatchGraph::Cable> (patch.cables()))
                patch.disconnect (cable.from, cable.to);
            patch.moveNode (PatchGraph::inputNodeId, 0.0f, 80.0f);
            patch.moveNode (boxOf[1], 140.0f, 140.0f);
            patch.moveNode (boxOf[2], 380.0f, 0.0f);
            patch.moveNode (boxOf[3], 450.0f, 80.0f);
            patch.moveNode (PatchGraph::outputNodeId, 650.0f, 80.0f);
            patch.connect ({ PatchGraph::inputNodeId, 0 }, { boxOf[1], 0 });
            patch.connect ({ boxOf[1], 0 }, { boxOf[2], 0 });
            patch.connect ({ boxOf[1], 0 }, { boxOf[3], 0 });
            patch.connect ({ boxOf[2], 0 }, { boxOf[3], 0 });
            patch.connect ({ boxOf[3], 0 }, { PatchGraph::outputNodeId, 0 });
            patch.connect ({ boxOf[3], 0 }, { boxOf[1], 0 }, 0.7f);
            processor.setFxPatchGraph (patch);
            auto hot = processor.getFxPatchGraph();
            hot.setCableGain (hot.findCable ({ boxOf[3], 0 }, { boxOf[1], 0 }), 0.7f);
            hot.setCableDelay (hot.findCable ({ boxOf[3], 0 }, { boxOf[1], 0 }), 120.0f);
            processor.setFxPatchGraph (hot);
            pages->showPage ("PATCH");
            settle (200);
            if (auto* view = findChild<PatchCableView> (*editor))
            {
                view->fitContent (1.4f);
                view->selectCable (processor.getFxPatchGraph().findCable ({ boxOf[3], 0 }, { boxOf[1], 0 }));
            }
            shot ("ilana-loop");

            processor.setFxPatcherOn (false);
            setParam ("fx_routing", 0.0f);
            processor.addFxEffect (13);
            setParam ("fx_reverb_size", 0.8f);
            pages->showPage ("FX");
            shot ("rack-reverb-instance");
        }

        if (auto* top = editor->getTopLevelComponent())
        {
            const auto before = top->getBounds();
            top->setSize (795, 540);
            pages->showPage ("PATCH");
            settle (200);
            if (auto* view = findChild<PatchCableView> (*editor))
                view->centreContent();
            shot ("loop-75");
            top->setBounds (before);
        }
        return 0;
    }

    // ILANA_SNAPSHOT_PATCHNAV: the PATCH tab without modes (2026-10-12):
    // fitted, zoomed in with the map, an effect dragged over a cable (lit
    // up) and dropped there; then stop. Best with Neuro Wobble.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_PATCHNAV", "").isNotEmpty())
    {
        const auto shot = [&] (const juce::String& name)
        {
            settle (400);
            save (*editor, outDir.getChildFile ("patchnav-" + name + ".png"));
        };
        pages->showPage ("PATCH");
        shot ("fit");
        auto* view = findChild<PatchCableView> (*editor);
        if (view == nullptr)
            return 1;
        const auto toScreen = [view] (juce::Point<float> p) { return p * view->getZoom(); };
        const auto& graph = view->getGraph();
        std::vector<const PatchGraph::Node*> effects;
        for (const auto& node : graph.nodes())
            if (! node.isFixed() && node.id < (1u << 20))
                effects.push_back (&node);
        std::sort (effects.begin(), effects.end(), [] (const auto* a, const auto* b) { return a->x < b->x; });

        // Zoomed in on the effects: the map in the corner.
        if (! effects.empty())
        {
            view->zoomAround (2.0f, toScreen (view->nodeBounds (*effects.front()).getCentre()));
            view->onNavigated();
            shot ("zoomed");
        }
        if (view->onFitRequested)
            view->onFitRequested();
        view->finishGlide();
        settle (200);

        // The last effect dragged onto the cable between the first two.
        if (effects.size() >= 3)
        {
            const auto moving = effects.back()->id, first = effects[0]->id, second = effects[1]->id;
            const auto target = (view->portPosition ({ first, 0 }, false) + view->portPosition ({ second, 0 }, true)) * 0.5f;
            const auto grab = view->nodeBounds (*graph.find (moving)).getCentre();
            view->mouseDown (PatchCableUi::mouse (*view, toScreen (grab), true));
            for (int step = 1; step <= 8; ++step)
                view->mouseDrag (PatchCableUi::mouse (*view, toScreen (grab + (target - grab) * ((float) step / 8.0f)), true));
            view->setMotion (false); // (the picture without the swing in flight)
            view->repaint();
            shot ("drop-hover");
            view->mouseUp (PatchCableUi::mouse (*view, toScreen (target), false));
            view->setMotion (true);
            shot ("dropped");
        }
        return 0;
    }

    // ILANA_SNAPSHOT_CAROUSEL: the PATCH tab's row of effects under the canvas
    // (2026-10-18): idle, scrolled, one tile dragged over a cable (lit up) and
    // dropped there; then stop. Best with Neuro Wobble.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_CAROUSEL", "").isNotEmpty())
    {
        const auto shot = [&] (const juce::String& name)
        {
            settle (400);
            save (*editor, outDir.getChildFile ("carousel-" + name + ".png"));
        };
        pages->showPage ("PATCH");
        shot ("idle");
        auto* view = findChild<PatchCableView> (*editor);
        auto* carousel = findChild<FxCarousel> (*editor);
        if (view == nullptr || carousel == nullptr)
        {
            std::printf ("carousel snapshot: view %p carousel %p\n", (void*) view, (void*) carousel);
            return 1;
        }
        std::printf ("carousel bounds %s showing %d tiles %d\n", carousel->getBounds().toString().toRawUTF8(), (int) carousel->isShowing(), carousel->numTiles());
        carousel->scrollTo (carousel->getScroll() + 400.0f);
        shot ("scrolled");
        carousel->mouseDown (PatchCableUi::mouse (*carousel, carousel->chipBounds (2).toFloat().getCentre(), true));
        carousel->mouseUp (PatchCableUi::mouse (*carousel, carousel->chipBounds (2).toFloat().getCentre(), false));
        shot ("category");
        carousel->showGroup (-1);
        carousel->scrollTo (0.0f);
        const auto& graph = view->getGraph();
        std::vector<const PatchGraph::Node*> effects;
        for (const auto& node : graph.nodes())
            if (! node.isFixed() && node.id < (1u << 20))
                effects.push_back (&node);
        std::sort (effects.begin(), effects.end(), [] (const auto* a, const auto* b) { return a->x < b->x; });
        auto tile = 0;
        for (int i = 0; i < carousel->numTiles(); ++i)
            if (carousel->tileAt (i).name == "Dimension")
                tile = i;
        const auto from = carousel->tileBounds (tile).toFloat().getCentre();
        if (effects.size() >= 2)
        {
            const auto target = (view->portPosition ({ effects[0]->id, 0 }, false) + view->portPosition ({ effects[1]->id, 0 }, true)) * 0.5f;
            const auto to = carousel->getLocalPoint (view, target * view->getZoom());
            carousel->mouseDown (PatchCableUi::mouse (*carousel, from, true));
            for (int step = 1; step <= 10; ++step)
                carousel->mouseDrag (PatchCableUi::mouse (*carousel, from + (to - from) * ((float) step / 10.0f), true, false, from));
            view->setMotion (false);
            view->repaint();
            shot ("drag-over-cable");
            carousel->mouseUp (PatchCableUi::mouse (*carousel, to, false, false, from));
            view->setMotion (true);
            shot ("dropped");
        }
        return 0;
    }

    // ILANA_SNAPSHOT_ROUTING: the FILTER tab's ROUTING page (roadmap step
    // 16): a preset's chain as cables, a cabled patch (WEST before the
    // filters, an oscillator split into Filter 1 and OUT, the sub straight
    // out), one box selected, the FILTER page's strip while cables rule, and
    // the page at 75 %; then stop.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_ROUTING", "").isNotEmpty())
    {
        const auto shot = [&] (const juce::String& name)
        {
            settle (500);
            save (*editor, outDir.getChildFile ("routing-" + name + ".png"));
        };
        const auto preset = juce::SystemStats::getEnvironmentVariable ("ILANA_ROUTING_PRESET", "Supersaw Anthem");
        processor.loadFactoryPreset (juce::jmax (0, processor.getFactoryPresetNames().indexOf (preset)));
        settle (300);
        pages->showPage ("PATCH");
        shot ("chain");
        // One effect's window: it docks at the right and the graph refits beside it.
        if (auto* view = findChild<PatchCableView> (*editor))
            for (const auto& node : view->getGraph().nodes())
                if (juce::String (node.label) == "REVERB")
                {
                    view->mouseDoubleClick (PatchCableUi::mouse (*view, view->nodeBounds (node).getCentre() * view->getZoom(), false));
                    break;
                }
        shot ("chain-window");

        using namespace VoiceRouting;
        auto patch = processor.getChainAsCables();
        patch.source[0] = toBody | toOut;
        patch.source[1] = toWest;
        patch.source[2] = toF2;
        patch.source[subNoiseSource] = toOut;
        patch.stage[body] = toF1;
        patch.stage[west] = toF1;
        patch.stage[f1] = toF2;
        patch.stage[f2] = toOut;
        processor.setRouteCables (patch, true);
        shot ("cables");

        if (auto* view = OscRoutingUi::findRoutingView (*editor))
            if (const auto* node = OscRoutingUi::nodeNamed (view->getGraph(), "OSC 1"))
            {
                const auto centre = view->nodeBounds (*node).getCentre() * view->getZoom();
                view->mouseDown (PatchCableUi::mouse (*view, centre, true));
                view->mouseUp (PatchCableUi::mouse (*view, centre, false));
            }
        shot ("selected");

        pages->showPage ("MAIN");
        shot ("filter-page");

        if (auto* top = editor->getTopLevelComponent())
        {
            const auto before = top->getBounds();
            top->setSize (795, 540);
            pages->showPage ("PATCH");
            shot ("75");
            top->setBounds (before);
        }
        return 0;
    }

    // ILANA_SNAPSHOT_FM: just the FM page (UI review 6's DX7 pass): the
    // first operator, its KEYS & VELOCITY tab, PITCH & LFO, each other
    // operator, then the patch on DX7 algorithm 1 (the tallest stack); then stop.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_FM", "").isNotEmpty())
    {
        pages->showPage ("FM");
        settle (400);
        save (*editor, outDir.getChildFile ("fm-1.png"));
        auto* page = pages->getCurrentPage();
        std::vector<CardTabs*> tabs;
        findAll<CardTabs> (*page, tabs);
        for (auto* tab : tabs)
            if (tab->isVisible() && tab->getNames().contains ("KEYS & VELOCITY"))
            {
                tab->setSelected (1, true);
                settle (300);
                save (*editor, outDir.getChildFile ("fm-1-keys.png"));
                tab->setSelected (0, true);
            }
        std::vector<juce::TextButton*> buttons;
        findAll<juce::TextButton> (*page, buttons);
        for (auto* button : buttons)
            if (button->getButtonText().contains ("OP PITCH") && button->onClick != nullptr)
            {
                // The link opens OP PITCH on MOD.
                button->onClick();
                settle (300);
                save (*editor, outDir.getChildFile ("fm-pitch-lfo.png"));
                pages->showPage ("FM");
                settle (300);
            }
        if (auto* picker = findChild<OscPicker> (*page))
            for (const auto osc : std::vector<int> (picker->getOscillators()))
                if (osc != 0)
                {
                    picker->pick (osc);
                    settle (200);
                    save (*editor, outDir.getChildFile ("fm-" + juce::String (osc + 1) + ".png"));
                }
        processor.applyDx7Algorithm (1);
        settle (400);
        save (*editor, outDir.getChildFile ("fm-dx7-algorithm-1.png"));
        // ILANA_SNAPSHOT_FM_ALGORITHMS="5,18,32": more DX7 algorithms.
        for (const auto& number : juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_FM_ALGORITHMS", ""), ",", ""))
        {
            processor.applyDx7Algorithm (number.getIntValue());
            settle (300);
            save (*editor, outDir.getChildFile ("fm-dx7-algorithm-" + number + ".png"));
        }
        // Operator 1 on the Operator Env: the card as the DX7 voices have it.
        if (auto* picker = findChild<OscPicker> (*page))
            picker->pick (0);
        if (auto* parameter = processor.apvts.getParameter ("osc1_amp_env"))
        {
            parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) OperatorEg::envelopeChoice));
            settle (400);
            save (*editor, outDir.getChildFile ("fm-opeg.png"));
        }
        // OSC 2's WARP FM from OSC 1, drawn dashed beside the routes.
        for (const auto& [id, value] : { std::pair<const char*, float> { "osc2_warp", (float) Warp::Fm }, { "osc2_warp_amt", 0.5f } })
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        settle (400);
        save (*editor, outDir.getChildFile ("fm-warp.png"));
        return 0;
    }

    // ILANA_SNAPSHOT_T2: review 9's package T2 views (the VOICE tab, PROB SEQ
    // and CLIP with notes, the tour with WHAT'S NEW open, the remap dock, the
    // table browser with a favourite), then stop.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_T2", "").isNotEmpty())
    {
        pages->showPage ("MAIN");
        settle (300);
        std::vector<StateTabs*> tabRows;
        findAll<StateTabs> (*editor, tabRows);
        for (auto* tabs : tabRows)
            for (int i = 0; i < tabs->getNumItems(); ++i)
                if (tabs->getItem (i).name == "VOICE" && tabs->onSelect != nullptr)
                {
                    tabs->setSelected (i);
                    tabs->onSelect (i);
                }
        settle (300);
        save (*editor, outDir.getChildFile ("t2-osc-voice.png"));

        pages->showPage ("ARP/SEQ");
        settle (300);
        std::vector<CardTabs*> engineTabs;
        findAll<CardTabs> (*editor, engineTabs);
        for (auto* tabs : engineTabs)
            if (tabs->getNames().contains ("PROB SEQ") && visibleInTree (tabs))
            {
                tabs->setSelected (2, true);
                settle (300);
                save (*editor, outDir.getChildFile ("t2-seq-probseq.png"));
                tabs->setSelected (1, true);
                settle (300);
                save (*editor, outDir.getChildFile ("t2-seq-euclid.png"));
                Clip clip;
                clip.bars = 2;
                clip.notes = { { 0.0f, 1.0f, 60, 100 }, { 1.0f, 0.5f, 64, 70 }, { 2.0f, 1.0f, 67, 127 }, { 4.0f, 2.0f, 62, 40 }, { 6.0f, 1.0f, 65, 90 } };
                processor.getClipState().setClip (0, clip);
                tabs->setSelected (3, true);
                settle (500);
                save (*editor, outDir.getChildFile ("t2-seq-clip.png"));
                tabs->setSelected (0, true);
            }

        pages->showPage ("MATRIX");
        settle (300);
        std::vector<CurveControl*> curves;
        findAll<CurveControl> (*editor, curves);
        for (auto* curve : curves)
            if (visibleInTree (curve))
            {
                curve->openRemapEditor();
                break;
            }
        settle (400);
        save (*editor, outDir.getChildFile ("t2-matrix-remap.png"));

        pages->showPage ("MAIN");
        if (auto* tutorial = findChild<TutorialOverlay> (*editor))
        {
            tutorial->setVisible (true);
            tutorial->toFront (false);
            settle (500);
            save (*editor, outDir.getChildFile ("t2-tour-closed.png"));
            tutorial->setWhatsNewOpen (true);
            settle (200);
            save (*editor, outDir.getChildFile ("t2-tour-open.png"));
            tutorial->setMovesOpen (true);
            settle (200);
            save (*editor, outDir.getChildFile ("t2-tour-moves.png"));
            tutorial->setMovesOpen (false);
            tutorial->setVisible (false);
        }

        // The undo note after a Remove (then the step undone again).
        if (auto* cutoff = processor.apvts.getParameter ("f1_cutoff"))
        {
            const auto before = cutoff->getValue();
            processor.performEdit ("Remove REVERB", [cutoff] { cutoff->setValueNotifyingHost (0.3f); });
            settle (300);
            save (*editor, outDir.getChildFile ("t2-undo-note.png"));
            processor.getUndoManager().undo();
            cutoff->setValueNotifyingHost (before);
            settle (200);
        }

        if (auto* display = findChild<PresetDisplay> (*editor); display != nullptr && display->onClick != nullptr)
        {
            display->onClick();
            settle (500);
            if (auto* panel = findChild<PresetPanel> (*editor); panel != nullptr && panel->onDockRequest != nullptr)
            {
                panel->selectFilter ("");
                panel->clickChip ("pack:dx7");
                panel->clickChip ("bank:ROM1A");
                settle (200);
                save (*editor, outDir.getChildFile ("t2-browser-dx7.png"));
                panel->onDockRequest (true);
                settle (500);
                save (*editor, outDir.getChildFile ("t2-browser-docked-dx7.png"));
                panel->clickChip ("bank:ROM1A");
                panel->clickChip ("pack:dx7");
                panel->selectFilter ("");
                panel->onDockRequest (false);
                settle (300);
                panel->close();
                settle (300);
            }
        }

        pages->savePresetAs();
        settle (300);
        save (*editor, outDir.getChildFile ("t2-save-as.png"));
        pages->getSaveOverlay().setTagsFieldOpen (true);
        settle (200);
        save (*editor, outDir.getChildFile ("t2-save-as-field.png"));
        pages->getSaveOverlay().cancel();

        if (auto* settingsFile = tableBrowserSettings())
            settingsFile->setValue ("tablefav_Basic", "1");
        {
            TableBrowser browser (processor, "osc1_table", IlanaTheme::accent());
            browser.setLookAndFeel (&pages->getLookAndFeel());
            browser.setSize (912, 576);
            settle (200);
            save (browser, outDir.getChildFile ("t2-table-browser.png"));
        }
        if (auto* settingsFile = tableBrowserSettings())
            settingsFile->removeValue ("tablefav_Basic");
        return 0;
    }

    // ILANA_SNAPSHOT_EXTRAS: the UI review 2 additions (the SPEC view, a
    // knob's source card, the docked browser), then stop.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_EXTRAS", "").isNotEmpty())
    {
        pages->showPage ("MAIN");
        settle (400);
        std::vector<WaveDisplay*> waves;
        findAll<WaveDisplay> (*editor, waves);
        for (auto* wave : waves)
            if (visibleInTree (wave) && wave->getOscIndex() == 0)
            {
                wave->setViewMode (2);
                settle (300);
            }
        std::vector<KnobControl*> knobs;
        findAll<KnobControl> (*editor, knobs);
        KnobControl* busiest = nullptr;
        for (auto* knob : knobs)
            if (visibleInTree (knob) && knob->getNumRoutings() > (busiest != nullptr ? busiest->getNumRoutings() : 0))
                busiest = knob;
        if (busiest != nullptr)
            busiest->openModCard (true);
        save (*editor, outDir.getChildFile ("extra-spec-and-card.png"));
        for (auto* knob : knobs)
            knob->closeModCard();

        if (auto* display = findChild<PresetDisplay> (*editor); display != nullptr && display->onClick != nullptr)
        {
            display->onClick();
            settle (500);
            save (*editor, outDir.getChildFile ("extra-browser-dropdown.png"));
            if (auto* panel = findChild<PresetPanel> (*editor); panel != nullptr && panel->onDockRequest != nullptr)
            {
                // DX7 with its bank chips (the settings are left as they were).
                panel->selectFilter ("");
                panel->clickChip ("pack:dx7");
                panel->clickChip ("bank:ROM1A");
                settle (200);
                save (*editor, outDir.getChildFile ("extra-browser-dx7.png"));
                panel->onDockRequest (true);
                settle (500);
                save (*editor, outDir.getChildFile ("extra-browser-docked-dx7.png"));
                panel->clickChip ("bank:ROM1A");
                panel->clickChip ("pack:dx7");
                panel->selectFilter ("");
                settle (200);
                save (*editor, outDir.getChildFile ("extra-browser-docked.png"));
                panel->onDockRequest (false);
                settle (300);
                panel->close();
                settle (300);
            }
        }

        // SAVE AS, with a name that loses characters (cancelled, so nothing is written).
        pages->savePresetAs();
        pages->getSaveOverlay().getNameField().setText ("Acid: Bass / Mk 2", true);
        settle (300);
        save (*editor, outDir.getChildFile ("extra-save-as.png"));
        pages->getSaveOverlay().cancel();

        {
            TableBrowser browser (processor, "osc1_table", IlanaTheme::accent());
            browser.setLookAndFeel (&pages->getLookAndFeel());
            browser.setSize (912, 576);
            browser.setSearchText ("saw");
            settle (200);
            save (browser, outDir.getChildFile ("extra-table-search.png"));
            browser.setSearchText ("emu vsco");
            settle (400);
            save (browser, outDir.getChildFile ("extra-table-builtin.png"));
        }
        return 0;
    }

    // ILANA_SNAPSHOT_LFOSH: LFO 1 on Sample & Hold with the dot in the second half of the cycle
    // (the dot used to count sixteen steps against the eight the trace draws), then stop.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_LFOSH", "").isNotEmpty())
    {
        const auto setParam = [&] (const char* id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };
        setParam ("lfo1_shape", (float) LfoShapes::SampleHold);
        setParam ("lfo1_sync", 0.0f);
        setParam ("lfo1_rate", 1.0f);
        pages->showPage ("MAIN");
        settle (400);
        juce::AudioBuffer<float> audio (2, 512);
        const auto sampleRate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 44100.0;
        const auto blocksPerCycle = sampleRate / 512.0; // 1 Hz
        int block = 0;
        for (const auto [fraction, name] : std::initializer_list<std::pair<double, const char*>> {
                 { 0.30, "lfosh-a.png" }, { 0.62, "lfosh-b.png" }, { 0.88, "lfosh-c.png" } })
        {
            const auto target = (int) (fraction * blocksPerCycle) + (int) blocksPerCycle * 2;
            for (; block < target; ++block)
            {
                juce::MidiBuffer midi;
                if (block == 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.9f), 0);
                audio.clear();
                processor.processBlock (audio, midi);
            }
            settle (150);
            save (*editor, outDir.getChildFile (name));
        }
        return 0;
    }

    // ILANA_SNAPSHOT_R16P4: review 16's FILTER / MOD / MATRIX states, then stop.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_R16P4", "").isNotEmpty())
    {
        const auto setParam = [&] (const char* id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };
        pages->showPage ("MAIN");
        settle (500);
        save (*editor, outDir.getChildFile ("r-filter.png"));
        pages->showPage ("MATRIX");
        settle (500);
        save (*editor, outDir.getChildFile ("r-matrix.png"));
        std::vector<CurveControl*> curves;
        findAll<CurveControl> (*editor, curves);
        for (auto* curve : curves)
            if (visibleInTree (curve))
            {
                curve->openRemapEditor();
                settle (500);
                save (*editor, outDir.getChildFile ("r-matrix-remap.png"));
                curve->openRemapEditor();
                break;
            }
        return 0;
    }

    // ILANA_SNAPSHOT_REMAP: the matrix with a drawn remap on the first row,
    // its editor open, then stop.
    // ILANA_SNAPSHOT_GRAPH: the cable view of roadmap step A, then stop.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_GRAPH", "").isNotEmpty())
    {
        snapshotPatchCableView (processor, outDir);
        return 0;
    }

    // ILANA_SNAPSHOT_ROWCLICK: Neuro Wobble's matrix, then a click on its
    // second row's SOURCE (the curve dock follows the row), then stop.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_ROWCLICK", "").isNotEmpty())
    {
        processor.loadFactoryPreset (processor.getFactoryPresetNames().indexOf ("Neuro Wobble"));
        settle (300);
        pages->showPage ("MATRIX");
        settle (500);
        save (*editor, outDir.getChildFile ("rowclick-before.png"));
        std::vector<MatrixRow*> rows;
        findAll<MatrixRow> (*editor, rows);
        std::vector<MatrixRow*> shown;
        for (auto* row : rows)
            if (visibleInTree (row))
                shown.push_back (row);
        std::sort (shown.begin(), shown.end(), [] (auto* a, auto* b) { return a->getY() < b->getY(); });
        if (shown.size() > 1)
        {
            auto* on = &shown[1]->getSourceBox();
            const juce::Point<float> at { 4.0f, 4.0f };
            shown[1]->getRowClickListener().mouseDown (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at,
                                                                         juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier), 1.0f,
                                                                         0.0f, 0.0f, 0.0f, 0.0f, on, on, juce::Time::getCurrentTime(), at,
                                                                         juce::Time::getCurrentTime(), 1, false));
            settle (500);
            save (*editor, outDir.getChildFile ("rowclick-after.png"));
        }
        return 0;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_REMAP", "").isNotEmpty())
    {
        pages->showPage ("MATRIX");
        settle (400);
        std::vector<CurveControl*> curves;
        findAll<CurveControl> (*editor, curves);
        for (auto* curve : curves)
            if (visibleInTree (curve))
            {
                const auto slot = curve->getSlotIndex();
                processor.setModRemap (slot, RemapEditor::shape (4));
                settle (300);
                save (*editor, outDir.getChildFile ("remap-matrix.png"));
                // The editor, docked under its row.
                curve->openRemapEditor();
                settle (500);
                save (*editor, outDir.getChildFile ("remap-editor.png"));
                curve->openRemapEditor(); // closes it again
                break;
            }
        return 0;
    }

    // ILANA_SNAPSHOT_P1: the modulation views of UI review 6 (P1), then
    // stop: knob rings and badges, a knob's and a macro's cards (with a
    // target whose module is off), a group chip's tray, and the matrix with
    // an idle row, a repeated row and a docked remap editor.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_P1", "").isNotEmpty())
    {
        const auto route = [&processor] (int slot, Mod::Source source, Mod::Destination destination, float depth)
        {
            const auto prefix = "mod" + juce::String (slot + 1);
            for (const auto& [field, value] : { std::pair<const char*, float> { "_src", (float) source },
                                                { "_dst", (float) destination }, { "_amt", depth } })
                if (auto* parameter = processor.apvts.getParameter (prefix + field))
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };
        auto freeSlot = 0;
        while (freeSlot < Mod::maxSlots && processor.readModSlot (freeSlot).source != Mod::Source::None)
            ++freeSlot;

        // Macro 1 into OSC 3 (off in most patches) and a repeat of slot 1.
        route (freeSlot, Mod::macroSourceFor (0), Mod::Destination::SubLevel, 0.5f);
        if (const auto first = processor.readModSlot (0); first.source != Mod::Source::None)
            route (freeSlot + 1, first.source, (Mod::Destination) first.destination, 0.25f);
        settle (300);

        for (const auto* page : { "MAIN" })
        {
            pages->showPage (page);
            settle (500);
            save (*editor, outDir.getChildFile ("p1-" + juce::String (page).toLowerCase() + ".png"));
        }

        pages->showPage ("MAIN");
        settle (400);
        std::vector<KnobControl*> knobs;
        findAll<KnobControl> (*editor, knobs);
        KnobControl* busiest = nullptr;
        for (auto* knob : knobs)
            if (visibleInTree (knob) && ! knob->isCompact() && knob->getNumRoutings() > (busiest != nullptr ? busiest->getNumRoutings() : 0))
                busiest = knob;
        if (busiest != nullptr)
        {
            busiest->openModCard (true);
            settle (300);
            save (*editor, outDir.getChildFile ("p1-knob-card.png"));
            busiest->closeModCard();
            settle (200);
        }

        std::vector<StripKnob*> macros;
        findAll<StripKnob> (*editor, macros);
        for (auto* macro : macros)
            if (auto* card = ModHoverPopup::instance(); card != nullptr && macro->getMacroIndex() == 0)
            {
                card->holdOpen (true);
                macro->openCard();
                settle (400);
                save (*editor, outDir.getChildFile ("p1-macro-card.png"));
                card->holdOpen (false);
                card->close();
            }

        // Every LFO and envelope in the pool: the bar folds them into group
        // chips; open the envelopes' tray.
        for (int i = 0; i < IlanaSynthAudioProcessor::numLfos; ++i)
            processor.setRevealed (IlanaSynthAudioProcessor::Module::Lfo, i, true);
        for (int i = 0; i < 16; ++i)
            processor.setRevealed (IlanaSynthAudioProcessor::Module::Envelope, i, true);
        settle (400);
        std::vector<ModSourceGroupChip*> groups;
        findAll<ModSourceGroupChip> (*editor, groups);
        for (auto* group : groups)
            if (visibleInTree (group) && group->onOpen != nullptr)
            {
                group->onOpen (*group);
                settle (300);
                save (*editor, outDir.getChildFile ("p1-chip-tray.png"));
                group->onOpen (*group);
                break;
            }

        pages->showPage ("MATRIX");
        settle (500);
        save (*editor, outDir.getChildFile ("p1-matrix.png"));
        std::vector<CurveControl*> curves;
        findAll<CurveControl> (*editor, curves);
        for (auto* curve : curves)
            if (visibleInTree (curve) && curve->getSlotIndex() == 1)
            {
                curve->openRemapEditor();
                settle (500);
                save (*editor, outDir.getChildFile ("p1-remap.png"));
                curve->openRemapEditor();
                break;
            }
        return 0;
    }

    // ILANA_SNAPSHOT_ADDOSC="3,4,5": those oscillator slots (0-based) added first, to see PLAY at five or six strips.
    for (const auto& slot : juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_ADDOSC", ""), ",", ""))
        processor.addOscillator (slot.getIntValue());

    // ILANA_SNAPSHOT_SET="osc2_mode=1,osc3_mode=2": parameters set to plain values first.
    for (const auto& pair : juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_SET", ""), ",", ""))
        if (auto* parameter = processor.apvts.getParameter (pair.upToFirstOccurrenceOf ("=", false, false)))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (pair.fromFirstOccurrenceOf ("=", false, false).getFloatValue()));

    // ILANA_SNAPSHOT_SAMPLE="1": oscillator 1 holds a decaying electric-piano-like sample (as the mockups do), so a
    // Sample or Granular card shows its waveform instead of the drop zone.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_SAMPLE", "").isNotEmpty())
    {
        auto piano = std::make_shared<SampleData>();
        piano->sampleRate = 48000.0;
        piano->name = "Rhodes C3.wav";
        piano->buffer.setSize (2, 144000);
        for (int i = 0; i < 144000; ++i)
        {
            const auto t = i / 48000.0;
            const auto tone = std::sin (juce::MathConstants<double>::twoPi * 131.0 * t) + 0.5 * std::sin (juce::MathConstants<double>::twoPi * 262.0 * t + 0.4)
                              + 0.25 * std::sin (juce::MathConstants<double>::twoPi * 393.0 * t);
            const auto value = (float) (0.5 * std::exp (-1.1 * t) * (1.0 - std::exp (-t * 400.0)) * tone);
            piano->buffer.setSample (0, i, value);
            piano->buffer.setSample (1, i, value);
        }
        processor.setUserSample (0, piano, {});
    }

    const auto pageIds = pages->getPageIds();
    // ILANA_SNAPSHOT_PAGES="MAIN,OSC": only those pages (no extras), then stop.
    const auto onlyPages = juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_PAGES", ""), ",", "");
    // ILANA_SNAPSHOT_BROWSER: the wavetable browser, sized as it opens over the editor.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_BROWSER", "").isNotEmpty())
    {
        TableBrowser browser (processor, "osc1_table", IlanaTheme::accent());
        browser.setLookAndFeel (&editor->getLookAndFeel());
        browser.setSize (juce::jmax (740, juce::roundToInt ((float) editor->getWidth() * 0.86f)), juce::jmax (520, juce::roundToInt ((float) editor->getHeight() * 0.8f)));
        settle (200);
        save (browser, outDir.getChildFile ("table-browser.png"));
    }

    // ILANA_SNAPSHOT_POPOVER="LFO 1:FILTER": a dock chip's pop-out editor open over a page.
    if (const auto popover = juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_POPOVER", ""); popover.isNotEmpty())
    {
        std::vector<ModSourceChip*> found;
        findAll<ModSourceChip> (*editor, found);
        for (auto* chip : found)
            if (chip->getSourceName() == popover.upToFirstOccurrenceOf (":", false, false) && chip->isVisible())
            {
                pages->showPage (popover.fromFirstOccurrenceOf (":", false, false));
                settle (300);
                pages->toggleSourcePopover (chip->getSourceIndex(), *chip);
                settle (400);
                save (*editor, outDir.getChildFile ("popover.png"));
                pages->toggleSourcePopover (chip->getSourceIndex(), *chip);
                break;
            }
    }

    // ILANA_SNAPSHOT_ARMED: PLAY with macro 1 armed by its + ASSIGN, waiting
    // for a knob (armed.png; UI review 31, item 26).
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_ARMED", "").isNotEmpty())
    {
        pages->showPage ("MAIN");
        settle (300);
        setArmedModSource ((int) Mod::macroSourceFor (0));
        settle (500);
        save (*editor, outDir.getChildFile ("armed.png"));
        setArmedModSource (0);
        settle (200);
    }

    for (int i = 0; i < pageIds.size(); ++i)
    {
        if (! onlyPages.isEmpty() && ! onlyPages.contains (pageIds[i]))
            continue;

        pages->showPage (pageIds[i]);
        settle (450);
        // ILANA_SNAPSHOT_3D: the wavetable pictures flipped to their 3D stack.
        if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_3D", "").isNotEmpty())
        {
            std::vector<WaveDisplay*> pictures;
            findAll<WaveDisplay> (*editor, pictures);
            for (auto* picture : pictures)
                picture->setStack3d (true);
            settle (300);
        }
        const auto stem = juce::String (i + 1).paddedLeft ('0', 2) + "-" + pageIds[i].replaceCharacter ('/', '-');
        save (*editor, outDir.getChildFile (stem + ".png"));

        // ILANA_SNAPSHOT_KNOBS: every shown knob's id, cell and drawn dial (the dial-size guard's numbers).
        if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_KNOBS", "").isNotEmpty())
        {
            std::vector<KnobControl*> knobs;
            findAll<KnobControl> (*editor, knobs);
            for (auto* knob : knobs)
                if (visibleInTree (knob) && knob->getWidth() > 0)
                    std::fprintf (stderr, "KNOB %s %s cell %dx%d maxDial %d dial %d radius %.1f\n", pageIds[i].toRawUTF8(), knob->getParameterId().toRawUTF8(),
                                  knob->getWidth(), knob->getHeight(), knob->getMaxDial(), knob->getDialSize(), knob->getDialRadius());
        }

        // Every sub-tab past the first on this page.
        std::vector<SubTabBar*> bars;

        if (auto* page = pages->getCurrentPage())
            findAll<SubTabBar> (*page, bars);

        for (size_t b = 0; b < bars.size(); ++b)
        {
            for (int item = 1; item < bars[b]->getNumRevealed(); ++item)
            {
                bars[b]->onSelect (item);
                settle (300);
                save (*editor, outDir.getChildFile (stem + "-bar" + juce::String ((int) b + 1) + "-" + juce::String (item + 1) + ".png"));
            }

            if (bars[b]->getNumRevealed() > 1)
                bars[b]->onSelect (0);
        }

        // The FM page's PITCH & LFO (the Operator Env's pitch envelope and
        // LFO, which a DX7 preset's operators follow).
        if (auto* page = pages->getCurrentPage())
        {
            std::vector<juce::TextButton*> buttons;
            findAll<juce::TextButton> (*page, buttons);
            for (auto* button : buttons)
                if (button->getButtonText().contains ("OP PITCH") && button->isVisible() && button->onClick != nullptr)
                {
                    button->onClick();
                    settle (300);
                    save (*editor, outDir.getChildFile (stem + "-pitch-lfo.png"));
                    // (The link jumps to MOD's pools: back to this page, so the shots after it are this page's, N16-10.)
                    pages->showPage (pageIds[i]);
                    settle (300);
                    break;
                }
        }

        // Each FX module's editor, placed in slot 1.
        if (pageIds[i] == "FX")
        {
            for (int type = 1; type <= IlanaSynthAudioProcessor::numFxTypes; ++type)
            {
                processor.assignFxSlot (1, type);
                settle (900);
                save (*editor, outDir.getChildFile ("fx-" + juce::String (type).paddedLeft ('0', 2) + ".png"));
            }

            // UI review 6: a split group (drive on the lows, reverb on the
            // highs) and a duplicate card, then the rack as it was.
            std::array<float, 3 * IlanaSynthAudioProcessor::numFxSlots> kept {};
            for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
                for (int k = 0; k < 3; ++k)
                    kept[(size_t) (slot * 3 + k)] = processor.apvts.getParameter ("fx_slot" + juce::String (slot + 1) + (k == 0 ? "" : (k == 1 ? "_band" : "_bypass")))->getValue();
            const auto setBand = [&] (int slot, int band)
            {
                auto* parameter = processor.apvts.getParameter ("fx_slot" + juce::String (slot) + "_band");
                parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) band));
            };
            const int splitRack[] { 7, 2, 13, 20, 0, 0, 0, 0, 0, 0 };
            for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
            {
                processor.assignFxSlot (slot + 1, splitRack[slot]);
                setBand (slot + 1, 0);
            }
            setBand (2, 1);
            setBand (3, 3);
            settle (600);
            save (*editor, outDir.getChildFile ("fx-split.png"));
            processor.assignFxSlot (5, 20); // a second OTT
            settle (600);
            if (auto* viewport = findChild<juce::Viewport> (*pages->getCurrentPage()))
                viewport->setViewPosition (0, 10000);
            settle (100);
            save (*editor, outDir.getChildFile ("fx-duplicate.png"));
            // A mid / side group, then the empty rack with its library.
            const int midSideRack[] { 2, 7, 13, 0, 0, 0, 0, 0, 0, 0 };
            for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
            {
                processor.assignFxSlot (slot + 1, midSideRack[slot]);
                setBand (slot + 1, 0);
            }
            setBand (2, 5);
            setBand (3, 4);
            settle (600);
            save (*editor, outDir.getChildFile ("fx-midside.png"));
            for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
            {
                processor.assignFxSlot (slot + 1, 0);
                setBand (slot + 1, 0);
            }
            settle (600);
            save (*editor, outDir.getChildFile ("fx-empty.png"));
            for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
                for (int k = 0; k < 3; ++k)
                    processor.apvts.getParameter ("fx_slot" + juce::String (slot + 1) + (k == 0 ? "" : (k == 1 ? "_band" : "_bypass")))
                        ->setValueNotifyingHost (kept[(size_t) (slot * 3 + k)]);
            settle (300);
        }
    }

    // The scope floats over a page; the hover line over the chip row
    // (ILANA_SNAPSHOT_PAGES=SCOPE shows only these).
    if (onlyPages.isEmpty() || onlyPages.contains ("SCOPE"))
    {
        pages->showPage ("MAIN");
        pages->setScopeOpen (true);
        settle (500);
        save (*editor, outDir.getChildFile ("scope-panel.png"));
        pages->setScopeOpen (false);
        settle (100);

        std::vector<KnobControl*> knobs;
        findAll<KnobControl> (*editor, knobs);
        for (auto* knob : knobs)
            if (visibleInTree (knob))
            {
                pages->getHoverLine().restOn (knob);
                save (*editor, outDir.getChildFile ("hover-line.png"));
                pages->getHoverLine().restOn (nullptr);
                break;
            }
    }

    if (! onlyPages.isEmpty())
        return 0;

    // The drawable Curve LFO editor.
    if (auto* shape = processor.apvts.getParameter ("lfo1_shape"))
    {
        pages->showPage ("MAIN");
        for (const auto physicsShape : { LfoShapes::Bounce, LfoShapes::Pendulum, LfoShapes::Spring, LfoShapes::Friction })
        {
            shape->setValueNotifyingHost (shape->convertTo0to1 ((float) physicsShape));
            settle (350);
            save (*editor, outDir.getChildFile ("lfo-physics-" + juce::String (physicsShape) + ".png"));
        }
    }

    // The drawable Curve LFO editor.
    if (auto* shape = processor.apvts.getParameter ("lfo1_shape"))
    {
        shape->setValueNotifyingHost (shape->convertTo0to1 ((float) IlanaSynthAudioProcessor::curveShape));
        processor.setLfoCurve (0, LfoCurve::preset (9));
        pages->showPage ("MAIN");


        settle (500);
        save (*editor, outDir.getChildFile ("lfo-curve.png"));

        // Steps, edited on the LFO graph, and the MSEG card's editor.
        shape->setValueNotifyingHost (shape->convertTo0to1 ((float) LfoShapes::Steps));
        settle (400);
        save (*editor, outDir.getChildFile ("lfo-steps.png"));
        shape->setValueNotifyingHost (shape->convertTo0to1 ((float) IlanaSynthAudioProcessor::curveShape));
    }

    // The LFO pool, every card revealed, the last one selected.
    {
        for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
            processor.setRevealed (IlanaSynthAudioProcessor::Module::Lfo, lfo, true);
        pages->showPage ("MAIN");
        settle (400);
        save (*editor, outDir.getChildFile ("lfo-pool-full.png"));
        pages->showPage ("MAIN");
        settle (300);
        save (*editor, outDir.getChildFile ("lfo-pool-main.png"));
        for (int lfo = 3; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
            processor.setRevealed (IlanaSynthAudioProcessor::Module::Lfo, lfo, false);
    }

    // M8.1: the simulated LFO shapes, each with its picture and named knobs
    // (the envelope pool back to the patch's, so the chips show in full).
    {
        for (int env = 3; env < 16; ++env)
            processor.setRevealed (IlanaSynthAudioProcessor::Module::Envelope, env, false);
        pages->showPage ("MAIN");
        const std::pair<int, const char*> shapes[] {
            { LfoSimShapes::Bounce, "bounce" }, { LfoSimShapes::Pendulum, "pendulum" }, { LfoSimShapes::Spring, "spring" },
            { LfoSimShapes::Friction, "friction" }, { LfoSimShapes::Lorenz, "lorenz" }, { LfoSimShapes::DoublePendulum, "double-pendulum" },
            { LfoSimShapes::Duffing, "duffing" }, { LfoSimShapes::Perlin, "perlin" }, { LfoSimShapes::Henon, "henon" } };
        for (const auto& [shape, name] : shapes)
        {
            if (auto* parameter = processor.apvts.getParameter ("lfo1_shape"))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) shape));
            const auto& info = LfoSimInfo::get (shape);
            for (int param = 0; param < LfoSimInfo::numParams; ++param)
                if (auto* parameter = processor.apvts.getParameter ("lfo1_p" + juce::String (param + 1)))
                    parameter->setValueNotifyingHost (info.params[(size_t) param].defaultValue);
            if (auto* rate = processor.apvts.getParameter ("lfo1_rate"))
                rate->setValueNotifyingHost (rate->convertTo0to1 (1.0f));
            settle (1500);
            save (*editor, outDir.getChildFile ("lfo-sim-" + juce::String (name) + ".png"));
        }
        if (auto* parameter = processor.apvts.getParameter ("lfo1_shape"))
            parameter->setValueNotifyingHost (0.0f);
        if (auto* rate = processor.apvts.getParameter ("lfo1_rate"))
            rate->setValueNotifyingHost (rate->convertTo0to1 (4.0f));
        settle (200);
    }

    // The filter overhaul: the grid's second page with the new models
    // (303 Acid on Filter 1, Vowel Morph on Filter 2).
    {
        const auto setType = [&processor] (const char* id, int type)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) type));
        };
        setType ("f1_type", FilterType::Acid303);
        setType ("f2_type", FilterType::VowelMorph);
        pages->showPage ("MAIN");
        settle (500);
        save (*editor, outDir.getChildFile ("filter-new-models.png"));
        setType ("f1_type", 0);
        setType ("f2_type", 0);
        settle (200);
    }

    // M8.3: the WEST card (its own card on the FILTER page), after the
    // filters, then in Filter 2's place.
    {
        if (auto* parameter = processor.apvts.getParameter ("west_on"))
            parameter->setValueNotifyingHost (1.0f);
        pages->showPage ("MAIN");
        settle (500);
        save (*editor, outDir.getChildFile ("filter-west.png"));
        if (auto* parameter = processor.apvts.getParameter ("west_pos"))
            parameter->setValueNotifyingHost (1.0f);
        settle (500);
        save (*editor, outDir.getChildFile ("filter-west-replace.png"));
        if (auto* parameter = processor.apvts.getParameter ("west_pos"))
            parameter->setValueNotifyingHost (0.0f);

        // UI review 7 (I7-1): the busiest SIGNAL FLOW: WEST, BODY, strings
        // and board on, the filters in parallel, OSC 1 bypassing them.
        const auto setPlain = [&processor] (const char* id, float plain)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
        };
        const std::array<const char*, 5> busy { "res_on", "sym_on", "sb_on", "filters_parallel", "osc1_route" };
        std::array<float, 5> before {};
        for (size_t i = 0; i < busy.size(); ++i)
            before[i] = processor.apvts.getRawParameterValue (busy[i])->load();
        for (size_t i = 0; i < busy.size(); ++i)
            setPlain (busy[i], i + 1 < busy.size() ? 1.0f : 3.0f);
        settle (500);
        save (*editor, outDir.getChildFile ("filter-flow-busy.png"));
        for (size_t i = 0; i < busy.size(); ++i)
            setPlain (busy[i], before[i]);
        setPlain ("west_on", 0.0f);
    }

    // M8.6: an oscillator playing a bounce of the patch.
    {
        IlanaSynthAudioProcessor::BounceRequest request;
        request.targetOsc = 1;
        request.muteOthers = false;
        processor.startBounce (request);
        for (int i = 0; i < 400 && processor.getBounceState() == IlanaSynthAudioProcessor::BounceState::Rendering; ++i)
            settle (50);
        pages->showPage ("MAIN");
        settle (600);
        save (*editor, outDir.getChildFile ("osc-bounce.png"));
        for (const auto* id : { "osc2_mode", "osc2_on", "osc2_semi" })
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->getDefaultValue());
    }

    // M8.7: a Physical oscillator's card, a moment after a note (Felt Hammer
    // Board, then a feedback guitar).
    for (const auto& [presetName, file] : { std::pair<const char*, const char*> { "Felt Hammer Board", "physical-page.png" },
                                            { "", "physical-feedback.png" } })
    {
        if (juce::String (presetName).isNotEmpty())
            processor.loadFactoryPreset (processor.getFactoryPresetNames().indexOf (presetName));
        else
        {
            processor.loadFactoryPreset (0);
            for (const auto& [id, value] : { std::pair<const char*, float> { "osc1_mode", 1.0f }, { "osc1_excite", 10.0f },
                                             { "osc1_string_sustain", 0.6f }, { "osc2_on", 0.0f } })
                if (auto* parameter = processor.apvts.getParameter (id))
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        }
        pages->showPage ("MAIN");
        settle (300);
        juce::AudioBuffer<float> buffer (2, 512);
        for (int block = 0; block < 20; ++block)
        {
            juce::MidiBuffer midi;
            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 110), 0);
            buffer.clear();
            processor.processBlock (buffer, midi);
        }
        settle (350);
        save (*editor, outDir.getChildFile (file));
        processor.panic();
    }
    processor.loadFactoryPreset (0);

    // M8.5: the VECTOR page, with the pad on and a path.
    {
        for (const auto& [id, value] : { std::pair<const char*, float> { "vec_on", 1.0f }, { "vec_path", 1.0f },
                                         { "vec_drift", 0.3f }, { "macro1_evolve", 0.4f }, { "macro3_evolve", 0.2f },
                                         { "osc4_on", 1.0f } })
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        pages->showPage ("VECTOR");
        settle (600);
        save (*editor, outDir.getChildFile ("vector-page.png"));
        for (const auto* id : { "vec_on", "vec_path", "vec_drift", "macro1_evolve", "macro3_evolve", "osc4_on" })
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->getDefaultValue());
    }

    // M8.4: the second page of filter models.
    {
        if (auto* parameter = processor.apvts.getParameter ("f1_type"))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) FilterType::VowelBank));
        if (auto* parameter = processor.apvts.getParameter ("f1_reso"))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (0.6f));
        pages->showPage ("MAIN");
        settle (500);
        save (*editor, outDir.getChildFile ("filter-models-2.png"));
        if (auto* parameter = processor.apvts.getParameter ("f1_type"))
            parameter->setValueNotifyingHost (0.0f);
        if (auto* parameter = processor.apvts.getParameter ("f1_reso"))
            parameter->setValueNotifyingHost (parameter->getDefaultValue());
    }

    // M4: the Hammered Strings preset on the OSC page.
    if (const auto program = processor.getFactoryPresetNames().indexOf ("Hammered Strings"); program >= 0)
    {
        processor.loadFactoryPreset (program);
        pages->showPage ("MAIN");
        settle (500);
        save (*editor, outDir.getChildFile ("keys-grand-osc.png"));
        if (auto* viewport = dynamic_cast<juce::Viewport*> (pages->getCurrentPage()))
        {
            viewport->setViewPosition (0, 10000);
            settle (200);
            save (*editor, outDir.getChildFile ("keys-grand-osc-scrolled.png"));
            viewport->setViewPosition (0, 0);
        }
        processor.loadFactoryPreset (0);
    }

    // Added oscillators: MAIN and OSC scroll, FM grows its matrix.
    {
        processor.addOscillator (3);
        processor.addOscillator (4);
        for (const auto* page : { "MAIN", "FM" })
        {
            pages->showPage (page);
            settle (400);
            save (*editor, outDir.getChildFile ("added-osc-" + juce::String (page) + ".png"));

            if (auto* content = pages->getCurrentPage())
            {
                std::vector<juce::Viewport*> views;
                if (auto* own = dynamic_cast<juce::Viewport*> (content))
                    views.push_back (own);
                findAll<juce::Viewport> (*content, views);
                for (auto* view : views)
                    view->setViewPosition (0, 10000);
                if (! views.empty())
                {
                    settle (150);
                    save (*editor, outDir.getChildFile ("added-osc-" + juce::String (page) + "-scrolled.png"));
                    for (auto* view : views)
                        view->setViewPosition (0, 0);
                }
            }
        }
        processor.removeOscillator (4);
        processor.removeOscillator (3);
    }

    // M5/M6: a six-operator algorithm, a PD chain and a DAHDSR envelope.
    {
        const auto set = [&processor] (const juce::String& id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };

        processor.applyFmAlgorithm (10);
        set ("osc1_tune", 1.0f);
        set ("fm_noise3", 0.2f);
        pages->showPage ("FM");
        settle (400);
        save (*editor, outDir.getChildFile ("fm-dx-keys.png"));
        pages->showPage ("MAIN");
        settle (300);
        save (*editor, outDir.getChildFile ("filter-six-osc.png"));

        set ("osc1_mode", 0.0f);
        set ("osc1_table", 8.0f);
        set ("osc1_warp", (float) Warp::PdRes2);
        set ("osc1_warp_amt", 0.4f);
        set ("osc1_warp2", 8.0f);
        set ("osc1_warp2_amt", 0.3f);
        set ("osc1_pd_env", 2.0f);
        pages->showPage ("MAIN");
        settle (400);
        save (*editor, outDir.getChildFile ("osc-pd-chain.png"));

        set ("amp_delay", 0.15f);
        set ("amp_hold", 0.3f);
        pages->showPage ("MAIN");
        settle (400);
        save (*editor, outDir.getChildFile ("env-dahdsr.png"));

        set ("lfo1_shape", 12.0f);   // Bounce
        set ("lfo1_phys_b", 0.7f);
        settle (400);
        save (*editor, outDir.getChildFile ("lfo-bounce.png"));
    }

    // FM into oscillators that ignore it: OSC 2 as a sample, OSC 3 a string.
    {
        const auto set = [&processor] (const juce::String& id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };

        processor.loadFactoryPreset (0);
        set ("osc2_mode", 2.0f);
        set ("sub_mode", 1.0f);
        set ("fm_1to2", 0.4f);
        pages->showPage ("FM");
        settle (400);
        save (*editor, outDir.getChildFile ("fm-no-input.png"));
        processor.loadFactoryPreset (0);
        pages->showPage ("MAIN");
        settle (300);
    }

    // Sample mode (UI review 9, I9-22): OSC 1 empty with its LOAD, then an
    // SFZ's two zones under the wave.
    {
        auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ilana-shot-sfz");
        folder.createDirectory();
        {
            juce::AudioBuffer<float> tone (1, 22050);
            for (int i = 0; i < tone.getNumSamples(); ++i)
                tone.setSample (0, i, 0.5f * std::sin ((float) i * 0.06f) * std::exp (-(float) i / 8000.0f));
            juce::WavAudioFormat wav;
            auto file = folder.getChildFile ("tone.wav");
            file.deleteFile();
            if (auto stream = std::unique_ptr<juce::OutputStream> (file.createOutputStream()))
                if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.get(), 44100.0, 1, 16, {}, 0)))
                {
                    stream.release();
                    writer->writeFromAudioSampleBuffer (tone, 0, tone.getNumSamples());
                }
            folder.getChildFile ("test.sfz").replaceWithText ("<region> sample=tone.wav lokey=36 hikey=59 pitch_keycenter=48\n"
                                                              "<region> sample=tone.wav lokey=60 hikey=84 pitch_keycenter=72\n");
        }
        processor.loadFactoryPreset (0);
        if (auto* parameter = processor.apvts.getParameter ("osc1_mode"))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (2.0f));
        pages->showPage ("MAIN");
        settle (400);
        save (*editor, outDir.getChildFile ("osc-sample-empty.png"));
        // (The factory preset's queued sample clear runs on the message thread; on a busy box it can
        // land after the load and wipe it, which made this shot look like the empty one. Load again
        // until the sample is still there after the settle.)
        for (int attempt = 0; attempt < 4; ++attempt)
        {
            processor.loadUserSample (0, folder.getChildFile ("test.sfz"));
            settle (500);
            if (processor.getSampleForOsc (0) != nullptr)
                break;
        }
        save (*editor, outDir.getChildFile ("osc-sample-sfz.png"));
        folder.deleteRecursively();
        processor.loadFactoryPreset (0);
        pages->showPage ("MAIN");
        settle (300);
    }

    // M7.1: the Generative card's EUCLID and PROB SEQ tabs, and strum.
    {
        const auto set = [&processor] (const juce::String& id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };

        pages->showPage ("ARP/SEQ");
        settle (100);
        std::vector<CardTabs*> cardTabs;
        findAll<CardTabs> (*editor, cardTabs);
        CardTabs* engineTabs = nullptr;
        for (auto* candidate : cardTabs)
            if (candidate->getNames().contains ("EUCLID"))
                engineTabs = candidate;

        set ("euc_on", 1.0f);
        set ("euc_hits", 7.0f);
        set ("euc_rotate", 2.0f);
        set ("spray_strum", 1.0f);

        if (engineTabs != nullptr)
        {
            engineTabs->setSelected (1, true);
            settle (400);
            save (*editor, outDir.getChildFile ("gen-euclid.png"));

            set ("pseq_on", 1.0f);
            set ("pseq_length", 12.0f);
            for (int step = 1; step <= 16; ++step)
            {
                set ("pseq_chance" + juce::String (step), step % 3 == 0 ? 0.4f : 0.9f);
                set ("pseq_range" + juce::String (step), (float) ((step * 5) % 13));
                set ("pseq_ratchet" + juce::String (step), step % 4 == 0 ? 3.0f : 1.0f);
            }
            engineTabs->setSelected (2, true);
            settle (400);
            save (*editor, outDir.getChildFile ("gen-probseq.png"));

            // The CLIP tab with a short riff and chords at mixed velocities.
            {
                Clip riff;
                riff.bars = 2;
                const int pitches[] = { 48, 55, 60, 58, 55, 51, 53, 55 };
                for (int i = 0; i < 8; ++i)
                    riff.notes.push_back ({ (float) i * 0.75f, 0.5f, pitches[i], 60 + (i * 37) % 67 });
                for (const auto& [start, root] : { std::pair<float, int> { 0.0f, 63 }, { 4.0f, 65 } })
                    for (const auto interval : { 0, 4, 7 })
                        riff.notes.push_back ({ start, 3.5f, root + interval, 90 });
                processor.getClipState().setClip (0, riff);
                processor.clipsEdited();
            }
            set ("pseq_on", 0.0f);
            set ("clip_on", 1.0f);
            engineTabs->setSelected (3, true);
            settle (200);

            if (auto* roll = findChild<ClipEditor> (*editor))
            {
                roll->reload (true);
                // Review 7 (V7-38): part of the clip selected (the second
                // bar), so picked and unpicked notes show side by side.
                roll->selectStartingIn (4.0f, 8.0f);
                settle (200);
                save (*editor, outDir.getChildFile ("gen-clip-selected.png"));
                roll->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
            }

            settle (200);
            save (*editor, outDir.getChildFile ("gen-clip.png"));

            // Review 6: the roll expanded over a folded GENERATE.
            {
                std::vector<juce::TextButton*> buttons;
                findAll<juce::TextButton> (*editor, buttons);
                for (auto* button : buttons)
                    if (button->getButtonText() == "EXPAND" && visibleInTree (button))
                    {
                        button->triggerClick();
                        settle (300);
                        save (*editor, outDir.getChildFile ("gen-clip-expanded.png"));
                        button->triggerClick();
                        settle (100);
                        break;
                    }
            }

            // Review 6: the arp's step lanes, drawn, with the arp on.
            set ("clip_on", 0.0f);
            set ("euc_on", 0.0f);
            set ("arp_on", 1.0f);
            set ("arp_steps", 12.0f);
            for (int step = 1; step <= 16; ++step)
            {
                set ("arp_vel" + juce::String (step), (float) (step % 4 == 1 ? 120 : 60 + (step * 23) % 50));
                set ("arp_len" + juce::String (step), step % 6 == 0 ? 0.0f : step % 4 == 3 ? 1.6f : 0.6f + 0.1f * (float) (step % 3));
                set ("arp_pitch" + juce::String (step), (float) (step % 8 == 5 ? 12 : step % 8 == 7 ? -5 : step % 4 == 2 ? 7 : 0));
            }
            engineTabs->setSelected (0, true);
            settle (400);
            save (*editor, outDir.getChildFile ("gen-arp-lanes.png"));
            engineTabs->setSelected (0, true);
        }

        processor.loadFactoryPreset (0);
        pages->showPage ("MAIN");
        settle (300);
    }

    // A source chip clicked: the knobs it drives stay lit (the first chip that drives a visible knob).
    {
        pages->showPage ("MAIN");
        settle (300);
        std::vector<ModSourceChip*> chipsFound;
        std::vector<KnobControl*> knobsFound;
        findAll<ModSourceChip> (*editor, chipsFound);
        findAll<KnobControl> (*editor, knobsFound);

        for (auto* chip : chipsFound)
        {
            auto drives = false;
            for (auto* knob : knobsFound)
                drives = drives || (visibleInTree (knob) && knob->isDrivenBy (chip->getSourceIndex()));

            if (! visibleInTree (chip) || ! drives)
                continue;

            chip->togglePinned();
            settle (150);
            save (*editor, outDir.getChildFile ("mod-pinned-chip.png"));
            chip->togglePinned();
            break;
        }
    }

    // The preset browser, opened from the preset name.
    if (auto* display = findChild<PresetDisplay> (*editor); display != nullptr && display->onClick != nullptr)
    {
        display->onClick();
        settle (500);
        save (*editor, outDir.getChildFile ("preset-browser.png"));
        display->onClick();
        settle (400);
    }

    // The wavetable browser on its own.
    {
        TableBrowser browser (processor, "osc1_table", IlanaTheme::accent());
        browser.setLookAndFeel (&editor->getLookAndFeel());
        browser.setSize (740, 520);
        settle (100);
        save (browser, outDir.getChildFile ("table-browser.png"));

        // As the oscillator opens it: with LOAD... and LIBRARY.
        browser.setLoaders ([] {}, [] (const juce::File&) {});
        settle (100);
        save (browser, outDir.getChildFile ("table-browser-library.png"));

        // The size it opens at over a 100% editor (see TableBrowser::show).
        browser.setSize (912, 576);
        settle (100);
        save (browser, outDir.getChildFile ("table-browser-editor.png"));
    }

    editor.reset();
    return 0;
}
