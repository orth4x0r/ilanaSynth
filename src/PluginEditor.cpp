#if defined (_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
#endif

#include "PluginEditor.h"

#include <algorithm>
#include <array>

#include "dsp/Modulation.h"
#include "dsp/TableFactory.h"
#include "gui/EnvelopeDisplay.h"
#include "gui/EqCurve.h"
#include "gui/FilterDisplay.h"
#include "gui/LfoDisplay.h"
#include "gui/CardTabs.h"
#include "gui/EnvThumbs.h"
#include "gui/FilterWidgets.h"
#include "gui/VectorPad.h"
#include "gui/PhysicalView.h"
#include "gui/FmDiagram.h"
#include "gui/FmWidgets.h"
#include "gui/GenerativeWidgets.h"
#include "gui/ClipEditor.h"
#include "gui/TableBrowser.h"
#include "gui/WavetableEditor.h"
#include "gui/LfoThumbs.h"
#include "gui/MatrixWidgets.h"
#include "gui/ParamControls.h"
#include "gui/OutputView.h"
#include "gui/ScopeDisplay.h"
#include "gui/SequencerEditors.h"
#include "gui/SubTabBar.h"
#include "gui/TutorialOverlay.h"
#include "gui/WaveDisplay.h"
#include "gui/XtraDisplays.h"

// Page classes (anonymous namespace), in dependency order.
#include "gui/pages/PageHelpers.h"
#include "gui/pages/OscPage.h"
#include "gui/pages/FilterVectorPhysicalPages.h"
#include "gui/pages/EnvLfoPages.h"
#include "gui/pages/FmInputPages.h"
#include "gui/pages/SeqPage.h"
#include "gui/pages/MainPage.h"
#include "gui/pages/MatrixPage.h"
#include "gui/pages/FxWidgets.h"
#include "gui/pages/FxPage.h"


// The scope as a panel floating over the current page (bottom right), or
// expanded over the whole page area.
class ScopePanel : public juce::Component
{
public:
    explicit ScopePanel (IlanaSynthAudioProcessor& p)
        : scope (p)
    {
        addAndMakeVisible (scope);

        expandButton.setButtonText ("EXPAND");
        expandButton.setClickingTogglesState (true);
        expandButton.setTooltip ("Fill the page area with the scope");
        expandButton.onClick = [this]
        {
            expandButton.setButtonText (expandButton.getToggleState() ? "SHRINK" : "EXPAND");

            if (onExpand != nullptr)
                onExpand();
        };

        closeButton.setButtonText (juce::String::fromUTF8 ("\xc3\x97"));
        closeButton.setTooltip ("Close the scope");
        closeButton.onClick = [this]
        {
            if (onClose != nullptr)
                onClose();
        };

        addAndMakeVisible (expandButton);
        addAndMakeVisible (closeButton);
    }

    std::function<void()> onClose, onExpand;

    bool isExpanded() const { return expandButton.getToggleState(); }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (4.0f);
        // A floating layer: a soft dark shadow under it, so the cards it
        // covers read as behind it rather than cut off.
        for (int ring = 4; ring >= 1; --ring)
        {
            g.setColour (juce::Colours::black.withAlpha (0.12f));
            g.fillRoundedRectangle (bounds.expanded ((float) ring).translated (0.0f, 2.0f), 8.0f + (float) ring);
        }
        IlanaTheme::paintGlow (g, bounds, 8.0f, IlanaTheme::accent(), 0.9f);
        g.setColour (IlanaTheme::Ui::panel);
        g.fillRoundedRectangle (bounds, 8.0f);
        g.setColour (IlanaTheme::Ui::line.interpolatedWith (IlanaTheme::accent(), 0.4f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

        IlanaTheme::paintTag (g, { bounds.getX() + 15.0f, bounds.getY() + 15.0f }, IlanaTheme::accent());
        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
        g.drawText ("SCOPE", bounds.withTrimmedLeft (26).withHeight (30), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (4);
        auto header = area.removeFromTop (30).reduced (8, 5);
        closeButton.setBounds (header.removeFromRight (22));
        header.removeFromRight (6);
        expandButton.setBounds (header.removeFromRight (66));
        scope.setBounds (area.reduced (6, 0).withTrimmedBottom (6));
    }

private:
    ScopeDisplay scope;
    juce::TextButton expandButton, closeButton;
};

IlanaSynthAudioProcessorEditor::IlanaSynthAudioProcessorEditor (IlanaSynthAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p)
{
   #if defined (_WIN32)
    // Some hosts create plugin windows on a thread that has been switched to
    // DPI-unaware, which makes Windows virtualise (bitmap-stretch) the editor
    // on scaled displays.  If the process itself is DPI aware we can safely
    // switch this thread back for the window creation so the window matches
    // the host's awareness; if the whole process is unaware there is nothing
    // a plug-in can do about the stretch, so we leave it alone.
    if (processDpiAwareness() > 0)
        previousDpiContext = setThreadDpiContext (perMonitorAwareV2Context());
   #endif

    setLookAndFeel (&lookAndFeel);

    {
        juce::PropertiesFile::Options options;
        options.applicationName = "ilanaSynth";
        options.filenameSuffix = "settings";
        options.folderName = "ilanaSynth";
        settings = std::make_unique<juce::PropertiesFile> (options);
    }

    themeIndex = juce::jlimit (0, IlanaTheme::numPalettes - 1, settings->getIntValue ("themeIndex", 0));
    lookAndFeel.setAccent (juce::Colour (IlanaTheme::palette[themeIndex]));

    addAndMakeVisible (content);
    content.onPaint = [this] (juce::Graphics& g) { paintHeader (g); };

    content.addAndMakeVisible (logo);
    logo.isSounding = [this] { return processorRef.getActiveVoiceCount() > 0; };
    content.addAndMakeVisible (infoStrip);

    keysButton.setClickingTogglesState (true);
    keysButton.setTooltip ("Keyboard\nShow or hide the on-screen keyboard.  Hiding it gives the pages more room.");
    keysButton.onClick = [this] { setKeyboardVisible (keysButton.getToggleState()); };
    infoStrip.setToolbarButton (&keysButton);

    infoStrip.onHelp = [this]
    {
        tutorial.setVisible (true);
        tutorial.toFront (false);
    };

    tutorial.onDismiss = [this] (bool dontShowAgain)
    {
        if (settings != nullptr)
        {
            settings->setValue ("seenIntro", dontShowAgain ? "1" : "0");
            settings->saveIfNeeded();
        }
    };

    auto* mainPage = new MainPage (p);
    auto* envLfoPage = new EnvLfoPage (p, *settings);

    // Seven tabs; the ones holding several pages switch them from the tab
    // row (PLAY: overview and vector, OSC: oscillators and the physical
    // view, MOD: envelopes and LFOs, step LFOs and MSEG, the matrix).
    const auto addSection = [this] (const juce::String& name, std::initializer_list<std::tuple<juce::String, juce::String, juce::Component*>> pages)
    {
        auto* section = new SectionPage();

        for (const auto& [id, label, page] : pages)
            section->addPage (id, label, page);

        // The page slides in as a whole; its displays don't replay their
        // own entrances on top.
        section->onPageShown = [this] (juce::Component& page) { startTabTransition (&page); };
        section->switcher.onSelect = [this, section] (int index)
        {
            section->show (index, true);
        };
        tabs.addTab (name, IlanaTheme::Ui::panel, section, true);
        sections.push_back (section);
    };

    addSection ("PLAY", { { "MAIN", "OVERVIEW", mainPage }, { "VECTOR", "VECTOR", new VectorPage (p) } });
    addSection ("OSC", { { "OSC", "OSCILLATORS", new OscPageViewport (p) }, { "PHYSICAL", "PHYSICAL", new PhysicalPage (p) } });
    addSection ("FILTER", { { "FILTER", "FILTER", new FilterPage (p) } });
    addSection ("MOD", { { "ENV/LFO", "ENV / LFO", envLfoPage },
                         { "STEPS", "STEPS & MSEG", new SeqPage (p, SeqPage::Part::modulators) },
                         { "MATRIX", "MATRIX", new MatrixPage (p) } });
    addSection ("FM", { { "FM", "FM", new FmPage (p) } });
    addSection ("SEQ", { { "ARP/SEQ", "SEQ", new SeqPage (p, SeqPage::Part::notes) } });
    addSection ("FX", { { "FX", "FX", new FxPage (p) } });
    // M7.5: ilanaSynth FX adds its INPUT page (last, so tab shortcuts stay).
    if (IlanaSynthAudioProcessor::isEffectBuild)
        addSection ("INPUT", { { "INPUT", "INPUT", new InputPage (p) } });

    mainPage->onEditLfo = [this, envLfoPage] (int lfo)
    {
        envLfoPage->selectLfo (lfo);
        showPage ("ENV/LFO");
    };

    mainPage->onEditEnvelope = [this, envLfoPage] (int envelope)
    {
        envLfoPage->selectEnvelope (envelope);
        showPage ("ENV/LFO");
    };

    mainPage->onOpenPage = [this] (const juce::String& name) { showPage (name); };

    // The scope floats over any page.
    scopePanel = std::make_unique<ScopePanel> (p);
    static_cast<ScopePanel*> (scopePanel.get())->onClose = [this] { setScopeOpen (false); };
    static_cast<ScopePanel*> (scopePanel.get())->onExpand = [this] { resized(); };
    scopeButton.setClickingTogglesState (true);
    scopeButton.setTooltip ("Scope\nShow the oscilloscope and spectrum over any page.");
    scopeButton.onClick = [this] { setScopeOpen (scopeButton.getToggleState()); };

    content.addAndMakeVisible (tabs);

    // In front of the tab bar: the page switches, the scope button and panel.
    for (auto* section : sections)
        content.addChildComponent (section->switcher);

    content.addAndMakeVisible (scopeButton);
    content.addChildComponent (*scopePanel);

    // Bottom strip: macros, then performance controls, then master.
    for (int macro = 0; macro < Mod::numMacros; ++macro)
    {
        auto knob = std::make_unique<StripKnob> (p, "macro" + juce::String (macro + 1),
                                                 "Macro " + juce::String (macro + 1), macro,
                                                 modSourceColour ((int) Mod::macroSourceFor (macro)), false);
        content.addChildComponent (*knob);
        macroKnobs.push_back (std::move (knob));
    }
    // Four macros fit the strip: a small switch pages between 1-4 and 5-8.
    macroPageButton.setTooltip ("Show macros 1-4 or 5-8");
    macroPageButton.onClick = [this] { showMacroPage (1 - macroPage); };
    content.addAndMakeVisible (macroPageButton);
    showMacroPage (0);

    glideKnob = std::make_unique<StripKnob> (p, "glide", "Glide");
    legatoToggle = std::make_unique<ToggleControl> (p.apvts, "glide_legato", "LEGATO");
    legatoToggle->showAsSwitch();
    legatoToggle->setTooltip ("Glide only between overlapping (legato) notes");
    content.addAndMakeVisible (*legatoToggle);
    // Master is a setting rather than a sound control: neutral, so it doesn't
    // outshine the page (and yellow stays the macros' colour).
    masterKnob = std::make_unique<StripKnob> (p, "master", "Master", -1, IlanaTheme::Ui::text2, false);
    outputMeter = std::make_unique<OutputMeter> (p);
    content.addAndMakeVisible (*outputMeter);
    content.addChildComponent (modHoverPopup);

    headerScope.setStrip (true);
    headerScope.onStripClick = [this]
    {
        scopeButton.setToggleState (! scopeButton.getToggleState(), juce::dontSendNotification);
        setScopeOpen (scopeButton.getToggleState());
    };
    content.addAndMakeVisible (headerScope);

    voicesArea.setTooltip ("Voices\nThe dots light for each note sounding. Click for the voice mode, how many voices and the pitch-bend range.");
    voicesArea.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    voicesArea.onClick = [this] { showSettingsMenu (true); };
    content.addAndMakeVisible (voicesArea);

    for (auto* component : { static_cast<juce::Component*> (glideKnob.get()), static_cast<juce::Component*> (masterKnob.get()) })
        content.addAndMakeVisible (*component);

    presetDisplay.onClick = [this] { togglePresetPanel(); };

    prevButton.onClick = [this]
    {
        const auto count = processorRef.getNumAllPresets();

        if (count > 0)
            loadPresetIndex ((juce::jmax (0, currentPresetIndex) + count - 1) % count);
    };
    nextButton.onClick = [this]
    {
        const auto count = processorRef.getNumAllPresets();

        if (count > 0)
            loadPresetIndex ((currentPresetIndex + 1) % count);
    };
    favButton.setClickingTogglesState (true);
    favButton.onClick = [this] { toggleFavourite(); updateHeaderButtons(); };
    saveButton.setText ("SAVE");
    saveButton.setEmphasis (true);
    saveButton.onClick = [this] { savePreset(); };
    saveButton.setTooltip ("Save preset  (Ctrl+S)\nSaves over your preset; a factory or new patch asks for a name.");
    moreButton.onClick = [this] { showPresetMenu(); };
    undoButton.onClick = [this] { undoOrRedo (false); };
    redoButton.onClick = [this] { undoOrRedo (true); };
    historyButton.onClick = [this] { showHistoryMenu(); };
    abButton.setButtonText ("A/B:  A");
    abButton.setTooltip ("Compare\nFlip between two versions of the patch (A and B) to compare them.");
    abButton.onClick = [this] { toggleAB(); };
    diceButton.onClick = [this] { showDiceMenu(); };
    settingsButton.onClick = [this] { showSettingsMenu(); };

    for (auto* button : std::initializer_list<juce::Component*> { &presetDisplay, &prevButton, &nextButton, &favButton,
                                                                  &saveButton, &moreButton, &undoButton, &redoButton,
                                                                  &historyButton, &abButton, &diceButton, &settingsButton })
        content.addAndMakeVisible (*button);

    keyboard = std::make_unique<KeyboardStrip> (p);
    content.addAndMakeVisible (*keyboard);

    setWantsKeyboardFocus (true);

    // Macros are dragged from their own names in the strip, so they have no
    // chip here. Every LFO and envelope chip follows the MOD page's pool:
    // shown while that module is added (or the matrix uses it), with the
    // rest one click away behind "+". The performance sources always show.
    struct ChipSpec
    {
        juce::String name, shortName;
        Mod::Source source;
        int revealKind = -1, revealIndex = 0;
    };
    std::vector<ChipSpec> chipSpecs;
    for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
        chipSpecs.push_back ({ "LFO " + juce::String (lfo + 1), "L" + juce::String (lfo + 1), Mod::lfoSourceFor (lfo),
                               (int) IlanaSynthAudioProcessor::Module::Lfo, lfo });
    {
        const auto envelope = (int) IlanaSynthAudioProcessor::Module::Envelope;
        // The pool's order: AMP, FILT, FILT 2, MOD, ENV 5, then ENV 6-16.
        for (const auto& spec : { ChipSpec { "AMP ENV", "AMP", Mod::Source::AmpEnv, envelope, 0 },
                                  ChipSpec { "FILT ENV", "FLT", Mod::Source::FilterEnv, envelope, 1 },
                                  ChipSpec { "FILT 2 ENV", "FLT2", Mod::Source::FilterEnv2, envelope, 2 },
                                  ChipSpec { "MOD ENV", "MOD", Mod::Source::ModEnv, envelope, 3 },
                                  ChipSpec { "ENV 5", "E5", Mod::Source::Env4, envelope, 4 } })
            chipSpecs.push_back (spec);
        for (int env = 6; env <= 16; ++env)
            chipSpecs.push_back ({ "ENV " + juce::String (env), "E" + juce::String (env),
                                   (Mod::Source) ((int) Mod::Source::Env6 + env - 6), envelope, env - 1 });
    }
    for (const auto& spec : { ChipSpec { "MSEG", "MSEG", Mod::Source::Mseg }, ChipSpec { "VELOCITY", "VEL", Mod::Source::Velocity },
                              ChipSpec { "KEY", "KEY", Mod::Source::KeyTrack }, ChipSpec { "RANDOM", "RND", Mod::Source::Random },
                              ChipSpec { "WHEEL", "WHL", Mod::Source::ModWheel }, ChipSpec { "PRESSURE", "AT", Mod::Source::Aftertouch },
                              ChipSpec { "INPUT", "IN", Mod::Source::InputEnv } })
        chipSpecs.push_back (spec);

    for (const auto& spec : chipSpecs)
    {
        // The input's envelope only exists in ilanaSynth FX.
        if (spec.source == Mod::Source::InputEnv && ! IlanaSynthAudioProcessor::isEffectBuild)
            continue;

        auto chip = std::make_unique<ModSourceChip> (spec.name, (int) spec.source);
        // The chip glows with its source while that source modulates
        // something (macros, the wheel and pressure always).
        chip->valueProvider = [this, source = spec.source]
        {
            using S = Mod::Source;
            const auto played = Mod::macroIndexFor (source) >= 0 || source == S::ModWheel || source == S::Aftertouch
                                || source == S::InputEnv;

            if (! played && ! usedModSources[(size_t) juce::jlimit (0, (int) S::Count - 1, (int) source)])
                return 0.0f;

            if ((source == S::Velocity || source == S::KeyTrack || source == S::Random)
                && processorRef.getActiveVoiceCount() == 0)
                return 0.0f;

            return processorRef.getSourceDisplayValue ((int) source);
        };
        chip->setShortName (spec.shortName);
        content.addAndMakeVisible (*chip);
        chip->setVisible (spec.revealKind < 0);
        chips.push_back (std::move (chip));
        chipReveal.push_back ({ spec.revealKind, spec.revealIndex });
        chipWanted.push_back (spec.revealKind < 0);
    }

    moreChipsButton.setButtonText ("+");
    moreChipsButton.setTooltip ("Add an LFO or envelope: it joins the pool on MOD > ENV / LFO and gets a chip here to drag.");
    moreChipsButton.onClick = [this] { showChipPicker(); };
    content.addChildComponent (moreChipsButton);

    tutorial.setPresetCount (processorRef.getFactoryPresetNames().size());
    content.addAndMakeVisible (tutorial);
    content.addChildComponent (confirmOverlay);
    content.addChildComponent (saveOverlay);
    saveOverlay.onSaved = [this] (const juce::File&) { presetSaved (false); };

    // Applied after adding: addAndMakeVisible forces the component visible.
    tutorial.setVisible (! settings->getBoolValue ("seenIntro", false));

    if (tutorial.isVisible())
        tutorial.toFront (false);

    // The keyboard opens on request (KEYS), so the pages get the room.
    keyboardVisible = settings->getBoolValue ("showKeyboard", false);
    keysButton.setToggleState (keyboardVisible, juce::dontSendNotification);
    keyboard->setVisible (keyboardVisible);

    updateHeaderButtons();
    updateUndoButtons();
    adoptLoadedFingerprint();

    // Drag the corner (or the host's window edge) to any size between 75% and
    // 200%; the aspect ratio is fixed and the size is remembered.
    setResizable (true, true);

    startTimer (250);
    frameSource = std::make_unique<IlanaAnim::FrameClock::Source> (*this);
    animator.onFrame = [this] { animate(); };

    presetDocked = settings->getBoolValue ("presetBrowserDocked", false);
    addChildComponent (dockHolder);
    dockHolder.onPaint = [] (juce::Graphics& g) { g.fillAll (IlanaTheme::Ui::bg); };

    if (presetDocked && settings->getBoolValue ("presetBrowserDockOpen", false))
    {
        createPresetPanel();
        presetPanel->setDocked (true);
        dockHolder.addAndMakeVisible (*presetPanel);
        presetDockShown = true;
        presetPanel->open();
    }

    applyUiZoom ((float) settings->getDoubleValue ("uiZoom", 1.0));
    displayScaleApplied = settings->containsKey ("uiZoom") && ! juce::approximatelyEqual (uiZoom, 1.0f);

    tabs.getTabbedButtonBar().addChangeListener (this);
}

void IlanaSynthAudioProcessorEditor::openWavetableEditor (int slot, juce::Colour colour)
{
    closeWavetableEditor();
    wavetableEditor = std::make_unique<WavetableEditor> (processorRef, slot, colour);
    wavetableEditor->onClose = [safe = juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> (this)]
    {
        if (safe != nullptr)
            safe->closeWavetableEditor();
    };
    content.addAndMakeVisible (*wavetableEditor);
    wavetableEditor->setBounds (content.getLocalBounds().reduced (24, 62));
    wavetableEditor->grabKeyboardFocus();
}

void IlanaSynthAudioProcessorEditor::closeWavetableEditor()
{
    if (wavetableEditor != nullptr)
    {
        content.removeChildComponent (wavetableEditor.get());
        wavetableEditor.reset();
    }
}

IlanaSynthAudioProcessorEditor::~IlanaSynthAudioProcessorEditor()
{
    closeWavetableEditor();
    tabs.getTabbedButtonBar().removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void IlanaSynthAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    layoutTabRow();

    if (auto* page = tabs.getCurrentContentComponent())
        page->repaint();

    startTabTransition();
}

void IlanaSynthAudioProcessorEditor::startTabTransition (juce::Component* pageToAnimate)
{
    if (transitionPage != nullptr)
    {
        transitionPage->setTransform ({});
        transitionPage->setAlpha (1.0f);
        transitionPage = nullptr;
    }

    if (auto* page = pageToAnimate != nullptr ? pageToAnimate : tabs.getCurrentContentComponent())
    {
        transitionPage = page;
        transitionStart = juce::Time::getMillisecondCounterHiRes();
        page->setAlpha (0.0f);
        page->setTransform (juce::AffineTransform::translation (0.0f, 8.0f));
        animator.startTimerHz (60);
    }
}

float IlanaSynthAudioProcessorEditor::displayScale() const
{
    auto scale = 1.0f;

    if (auto* peer = getPeer())
    {
        scale = juce::jmax (scale, (float) peer->getPlatformScaleFactor());

       #if defined (_WIN32)
        scale = juce::jmax (scale, queryMonitorScale (peer->getNativeHandle()));
       #endif
    }

    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        scale = juce::jmax (scale, (float) display->scale);

    return juce::jlimit (1.0f, 3.0f, scale);
}

float IlanaSynthAudioProcessorEditor::hostScaleFactor() const
{
    const auto& transform = getTransform();
    return (float) juce::jmax (std::abs (transform.mat00), std::abs (transform.mat11));
}

void IlanaSynthAudioProcessorEditor::applyUiZoom (float newZoom)
{
    uiZoom = juce::jlimit (0.75f, 2.0f, newZoom);

    if (settings != nullptr)
    {
        settings->setValue ("uiZoom", uiZoom);
        settings->saveIfNeeded();
    }

    applyAspectAndLimits();
    setSize (juce::roundToInt ((float) currentDesignWidth() * uiZoom),
             juce::roundToInt ((float) designHeight * uiZoom));
}

void IlanaSynthAudioProcessorEditor::applyAspectAndLimits()
{
    const auto width = currentDesignWidth();
    setResizeLimits (juce::roundToInt ((float) width * 0.75f), juce::roundToInt ((float) designHeight * 0.75f),
                     width * 2, designHeight * 2);

    if (auto* boundsConstrainer = getConstrainer())
        boundsConstrainer->setFixedAspectRatio ((double) width / (double) designHeight);
}

void IlanaSynthAudioProcessorEditor::applyDisplayScale()
{
   #if ! defined (_WIN32)
    // macOS and Linux hosts scale plugin windows themselves.
    displayScaleApplied = true;
    return;
   #else
    // The thread context was switched to per-monitor aware for the window
    // creation; put it back now that the peer exists.
    if (previousDpiContext != nullptr && getPeer() != nullptr)
    {
        setThreadDpiContext (previousDpiContext);
        previousDpiContext = nullptr;
    }

    if (displayScaleApplied || getPeer() == nullptr)
        return;

    // If JUCE applied a host-window scale to this editor, it is already
    // handling the DPI and resizing ourselves would double up.
    if (hostScaleFactor() > 1.01f)
    {
        displayScaleApplied = true;
        return;
    }

    const auto scale = displayScale();

    if (scale <= 1.01f)
        return;

    displayScaleApplied = true;

    // The host is not scaling for us (our window is at design size), so size
    // ourselves for the real display DPI: the host then creates a matching
    // window and the UI renders at native resolution instead of being
    // bitmap-stretched.
    if (getHeight() > designHeight + 20)
        return;

    setResizeLimits (juce::roundToInt ((float) currentDesignWidth() * 0.75f * scale), juce::roundToInt (540.0f * scale),
                     juce::roundToInt ((float) currentDesignWidth() * 1.5f * scale), juce::roundToInt (1080.0f * scale));

    if (auto* boundsConstrainer = getConstrainer())
        boundsConstrainer->setFixedAspectRatio ((double) currentDesignWidth() / (double) designHeight);

    setSize (juce::roundToInt ((float) currentDesignWidth() * scale),
             juce::roundToInt ((float) designHeight * scale));
   #endif
}

// Pool chips follow the modules added (and whatever the matrix uses), read
// against last tick's usage; the row re-lays out only when that changes.
void IlanaSynthAudioProcessorEditor::updateChipVisibility()
{
    auto changed = false;

    for (size_t i = 0; i < chips.size() && i < chipReveal.size(); ++i)
    {
        const auto [kind, index] = chipReveal[i];

        if (kind < 0)
            continue;

        const auto source = juce::jlimit (0, (int) Mod::Source::Count - 1, chips[i]->getSourceIndex());
        const auto wanted = processorRef.isRevealed ((IlanaSynthAudioProcessor::Module) kind, index) || usedModSources[(size_t) source];

        if (chipWanted[i] != wanted)
        {
            chipWanted[i] = wanted;
            changed = true;
        }
    }

    if (changed)
        resized();
}

// Every shown chip at its name's width with the spare shared out, then the
// "+" picker. A crowded row shortens every chip at once ("L5", "E6", "VEL"),
// never some and not others, and only then narrows them; none is hidden.
void IlanaSynthAudioProcessorEditor::layoutChips (juce::Rectangle<int> row)
{
    if (chips.empty())
        return;

    const auto font = IlanaTheme::font (IlanaTheme::TextSize::label, true);
    // A full chip has its colour dot beside the name; a short one has it
    // under the name and needs less room.
    const auto widthOf = [&font] (const juce::String& text, bool compact)
    {
        return (float) juce::GlyphArrangement::getStringWidthInt (font, text) + (compact ? 12.0f : 34.0f);
    };
    const auto picker = std::find (chipWanted.begin(), chipWanted.end(), false) != chipWanted.end();
    const auto pickerWidth = picker ? 34.0f : 0.0f;
    const auto total = [&] (bool compact)
    {
        auto sum = pickerWidth;
        for (size_t i = 0; i < chips.size(); ++i)
            if (chipWanted[i])
                sum += widthOf (compact ? chips[i]->getShortName() : chips[i]->getSourceName(), compact);
        return sum;
    };

    const auto available = (float) row.getWidth();
    const auto compact = total (false) > available;
    const auto used = total (compact);
    const auto count = (float) std::count (chipWanted.begin(), chipWanted.end(), true);
    const auto spare = juce::jmax (0.0f, (available - used) / juce::jmax (1.0f, count));
    const auto squeeze = juce::jmin (1.0f, available / juce::jmax (1.0f, used));
    auto x = (float) row.getX();

    for (size_t i = 0; i < chips.size(); ++i)
    {
        chips[i]->setVisible (chipWanted[i]);
        chips[i]->setCompact (compact);

        if (! chipWanted[i])
            continue;

        const auto natural = widthOf (compact ? chips[i]->getShortName() : chips[i]->getSourceName(), compact);
        const auto width = (natural + spare) * squeeze;
        chips[i]->setBounds (juce::Rectangle<float> (x, (float) row.getY(), width, (float) row.getHeight()).toNearestInt().reduced (2, 1));
        x += width;
    }

    moreChipsButton.setVisible (picker);
    if (picker)
        moreChipsButton.setBounds (juce::Rectangle<float> (x, (float) row.getY(), pickerWidth * squeeze, (float) row.getHeight())
                                       .toNearestInt().reduced (2, 1));
}

// The "+" after the chips: the LFOs and envelopes not in the pool yet.
// Picking one adds it (its tile on MOD > ENV / LFO and its chip here).
void IlanaSynthAudioProcessorEditor::showChipPicker()
{
    juce::PopupMenu lfos, envelopes;

    for (size_t i = 0; i < chips.size() && i < chipReveal.size(); ++i)
    {
        const auto [kind, index] = chipReveal[i];

        if (kind < 0 || chipWanted[i])
            continue;

        auto& menu = kind == (int) IlanaSynthAudioProcessor::Module::Lfo ? lfos : envelopes;
        menu.addItem ((int) i + 1, chips[i]->getSourceName());
    }

    juce::PopupMenu menu;
    menu.addSectionHeader ("Add a source");
    if (lfos.getNumItems() > 0)
        menu.addSubMenu ("LFOs", lfos);
    if (envelopes.getNumItems() > 0)
        menu.addSubMenu ("Envelopes", envelopes);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&moreChipsButton),
                        [safeThis = juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> (this)] (int result)
                        {
                            if (safeThis != nullptr && result > 0)
                                safeThis->addPoolSource (result - 1);
                        });
}

void IlanaSynthAudioProcessorEditor::addPoolSource (int chipIndex)
{
    if (! juce::isPositiveAndBelow (chipIndex, (int) chipReveal.size()))
        return;

    const auto [kind, index] = chipReveal[(size_t) chipIndex];

    if (kind >= 0)
        processorRef.setRevealed ((IlanaSynthAudioProcessor::Module) kind, index, true);

    updateChipVisibility();
}

void IlanaSynthAudioProcessorEditor::timerCallback()
{
    applyDisplayScale();

    if (zoomNeedsSaving && ! juce::ModifierKeys::getCurrentModifiersRealtime().isAnyMouseButtonDown()
        && settings != nullptr)
    {
        zoomNeedsSaving = false;
        settings->setValue ("uiZoom", uiZoom);
        settings->saveIfNeeded();
    }


    // Program changes, host state restores and A/B swaps change the preset
    // behind the editor's back.
    if (processorRef.getCurrentPresetName() != shownPresetName)
        updateHeaderButtons();

    // "Edited" marker: cheap checksum of every parameter against the one
    // taken when the preset was loaded.
    if (presetLoadFlash <= 0.01f)
        presetDisplay.setPreset (shownPresetName, shownCategory, isFavourite (shownPresetName),
                                 parameterFingerprint() != loadedFingerprint);

    // Which sources the matrix uses, for the source chips' glow.
    updateChipVisibility();
    {
        usedModSources.fill (false);

        for (int slot = 0; slot < Mod::maxSlots; ++slot)
        {
            const auto routing = processorRef.readModSlot (slot);

            if (routing.isActive())
            {
                usedModSources[(size_t) juce::jlimit (0, (int) Mod::Source::Count - 1, (int) routing.source)] = true;
                usedModSources[(size_t) juce::jlimit (0, (int) Mod::Source::Count - 1, (int) routing.aux)] = true;
            }
        }
    }

    if (transitionPage == nullptr)
    {
        updateUndoButtons();
        content.repaint (0, 0, designWidth, 56);
    }
}

// Runs on every display frame while the page slides in or the preset
// display flashes, then stops.
void IlanaSynthAudioProcessorEditor::animate()
{
    if (presetLoadFlash > 0.0f)
    {
        // Tuned at one 0.86 step per 60 ms.
        presetLoadFlash = IlanaAnim::decay (presetLoadFlash, 0.86f, animator.frameSeconds() * 1000.0f / 60.0f);

        if (presetLoadFlash <= 0.01f)
            presetLoadFlash = 0.0f;

        presetDisplay.setFlash (presetLoadFlash);
    }

    if (transitionPage != nullptr)
    {
        // A short slide up with a fade: no scaling, which would soften text.
        constexpr double duration = 160.0;
        const auto elapsed = juce::Time::getMillisecondCounterHiRes() - transitionStart;
        const auto progress = juce::jlimit (0.0, 1.0, elapsed / duration);
        const auto eased = IlanaAnim::easeOutCubic ((float) progress);

        transitionPage->setAlpha (eased);
        transitionPage->setTransform (juce::AffineTransform::translation (0.0f, std::round ((1.0f - eased) * 8.0f)));

        if (progress >= 1.0)
        {
            transitionPage->setTransform ({});
            transitionPage->setAlpha (1.0f);
            transitionPage = nullptr;
        }
    }

    if (transitionPage == nullptr && presetLoadFlash <= 0.0f)
        animator.stopTimer();
}

void IlanaSynthAudioProcessorEditor::paint (juce::Graphics& g)
{
    IlanaTheme::paintPageBackground (g, getLocalBounds());
}

void IlanaSynthAudioProcessorEditor::paintHeader (juce::Graphics& g)
{
    g.setColour (IlanaTheme::Ui::header);
    g.fillRect (juce::Rectangle<int> (0, 0, designWidth, 56));

    g.setColour (IlanaTheme::Ui::line);
    g.fillRect (juce::Rectangle<int> (0, 55, designWidth, 1));

    // Status line along the bottom edge of the header (tempo, voices and
    // CPU at the right), clear of the buttons above.
    const auto statusY = 41;

    // The rules between the action groups.
    g.setColour (IlanaTheme::Ui::line.brighter (0.25f));
    for (const auto x : headerSeparatorX)
        if (x > 0)
            g.fillRect (juce::Rectangle<int> (x, 9, 1, 20));


    const auto cpu = processorRef.getCpuUsage() * 100.0f;

    const auto cpuColour = cpu < 30.0f
                               ? IlanaTheme::Ui::text3
                               : (cpu < 60.0f
                                      ? IlanaTheme::Ui::text3
                                            .interpolatedWith (IlanaTheme::accent(), (cpu - 30.0f) / 30.0f)
                                      : IlanaTheme::accent().interpolatedWith (juce::Colours::red,
                                                                               juce::jlimit (0.0f, 1.0f, (cpu - 60.0f) / 40.0f)));

    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, false, true)); // live numbers
    g.setColour (cpuColour);
    g.drawText ("CPU " + juce::String (juce::roundToInt (cpu)) + "%",
                juce::Rectangle<int> (designWidth - 80, statusY, 64, 11), juce::Justification::centredRight);

    g.setColour (IlanaTheme::Ui::text3);
    g.drawText (juce::String (processorRef.getCurrentBpm(), 1) + " BPM",
                juce::Rectangle<int> (designWidth - 356, statusY, 70, 11), juce::Justification::centredRight);
    {
        // The voice mode when it isn't the usual Poly, so Mono or Legato
        // shows without opening the settings.
        const auto* modeValue = processorRef.apvts.getRawParameterValue ("voice_mode");
        const auto mode = modeValue != nullptr ? juce::roundToInt (modeValue->load()) : 0;
        g.setColour (voicesArea.isMouseOver() ? IlanaTheme::Ui::text2 : IlanaTheme::Ui::text3);
        g.drawText (mode == 1 ? "MONO" : (mode == 2 ? "LEGATO" : "VOICES"),
                    juce::Rectangle<int> (designWidth - 290, statusY, 54, 11), juce::Justification::centredRight);
        g.setColour (IlanaTheme::Ui::text3);
    }

    const auto activeVoices = processorRef.getActiveVoiceCount();
    const auto* voicesValue = processorRef.apvts.getRawParameterValue ("poly_voices");
    const auto maxVoices = juce::jlimit (1, 32, voicesValue != nullptr ? juce::roundToInt (voicesValue->load()) : 32);
    // Up to 16 dots at full size; more share the same width at half size.
    const auto spacing = maxVoices > 16 ? 4.0f : 8.0f;
    const auto size = maxVoices > 16 ? 3.0f : 5.0f;

    for (int i = 0; i < maxVoices; ++i)
    {
        const auto lit = i < activeVoices;
        const auto dot = juce::Rectangle<float> ((float) (designWidth - 232) + (float) i * spacing,
                                                 (float) statusY + 3.0f + (5.0f - size) * 0.5f, size, size);

        // Playing voices light up with a halo.
        if (lit)
        {
            g.setColour (IlanaTheme::accent().withAlpha (0.25f));
            g.fillEllipse (dot.expanded (2.5f));
        }

        g.setColour (lit ? IlanaTheme::accent() : IlanaTheme::Ui::track);
        g.fillEllipse (dot);
    }
}

void IlanaSynthAudioProcessorEditor::resized()
{
    const auto scale = (float) getHeight() / (float) designHeight;
    IlanaTheme::uiScaleRef() = scale * hostScaleFactor();

    // A corner drag changes the zoom; remember it (saved on the timer so a
    // drag doesn't hit the disk on every step).
    if (displayScaleApplied && std::abs (scale - uiZoom) > 0.005f)
    {
        uiZoom = scale;
        zoomNeedsSaving = true;
    }

    content.setBounds (0, 0, designWidth, designHeight);
    content.setTransform (juce::AffineTransform::scale (scale));
    dockHolder.setBounds (designWidth, 0, dockWidth, designHeight);
    dockHolder.setTransform (juce::AffineTransform::scale (scale));
    dockHolder.setVisible (presetDockShown);

    if (presetDockShown && presetPanel != nullptr)
        presetPanel->setBounds (dockHolder.getLocalBounds().reduced (0, 8).withTrimmedRight (8));

    auto area = content.getLocalBounds();

    logo.setBounds (16, 5, 250, 46);
    logo.version = juce::String ("v") + appVersion;
    logo.nameCentreY = 19.0f - 5.0f; // the header buttons' centre line (they span 4 to 34)

    // Header: preset display in the middle with its browse / save controls,
    // editing tools on the right. Buttons sit in the top 36 px; the status
    // line runs underneath them.
    auto headerRow = area.removeFromTop (56).withTrimmedTop (4).withHeight (30).withTrimmedRight (14);
    headerRow.removeFromLeft (292);

    // Three groups, right to left: tools (dice, settings), edit (undo, redo,
    // history, A/B) and file (star, save, menu), each pair of groups parted by
    // a gap with a thin rule in it.
    constexpr int key = 30, groupGap = 24;
    settingsButton.setBounds (headerRow.removeFromRight (key));
    headerRow.removeFromRight (4);
    diceButton.setBounds (headerRow.removeFromRight (key));
    headerRow.removeFromRight (groupGap);
    headerSeparatorX[1] = headerRow.getRight() + groupGap / 2;
    abButton.setBounds (headerRow.removeFromRight (92));
    headerRow.removeFromRight (6);
    historyButton.setBounds (headerRow.removeFromRight (key));
    headerRow.removeFromRight (3);
    redoButton.setBounds (headerRow.removeFromRight (key));
    headerRow.removeFromRight (3);
    undoButton.setBounds (headerRow.removeFromRight (key));
    headerRow.removeFromRight (groupGap);
    headerSeparatorX[0] = headerRow.getRight() + groupGap / 2;

    moreButton.setBounds (headerRow.removeFromRight (key));
    headerRow.removeFromRight (4);
    saveButton.setBounds (headerRow.removeFromRight (82));
    headerRow.removeFromRight (4);
    favButton.setBounds (headerRow.removeFromRight (key));
    headerRow.removeFromRight (10);
    nextButton.setBounds (headerRow.removeFromRight (26));
    prevButton.setBounds (headerRow.removeFromLeft (26));
    headerRow.removeFromLeft (3);
    headerRow.removeFromRight (3);
    presetDisplay.setBounds (headerRow.withTrimmedTop (-3).withHeight (headerRow.getHeight() + 6));

    // The status line (y 41-52): the live waveform under the preset name,
    // then tempo, VOICES and CPU at the right.
    headerScope.setBounds (prevButton.getX(), 40, juce::jmin (nextButton.getRight(), designWidth - 372) - prevButton.getX(), 14);
    voicesArea.setBounds (designWidth - 284, 38, 190, 17);

    // Bottom: source chips, the macro / performance strip, the info line and
    // the optional keyboard.
    if (keyboard != nullptr)
    {
        keyboard->setVisible (keyboardVisible);

        if (keyboardVisible)
            keyboard->setBounds (area.removeFromBottom (32).reduced (14, 2));
    }

    infoStrip.setBounds (area.removeFromBottom (22).reduced (14, 2));
    tutorial.setBounds (content.getLocalBounds());
    confirmOverlay.setBounds (content.getLocalBounds());
    saveOverlay.setBounds (content.getLocalBounds());

    auto strip = area.removeFromBottom (52).reduced (14, 1);
    outputMeter->setBounds (strip.removeFromRight (24).withSizeKeepingCentre (24, 48));
    strip.removeFromRight (4);
    masterKnob->setBounds (strip.removeFromRight (108));
    strip.removeFromRight (4);
    // The bar's switch puts its name on the knobs' title line and the switch
    // on the value line, like the knobs' text beside them.
    const auto titleTop = strip.getCentreY() - 15;
    legatoToggle->setBounds (strip.removeFromRight (80).withTop (titleTop).withHeight (13 + 20).reduced (2, 0));
    glideKnob->setBounds (strip.removeFromRight (92));
    strip.removeFromRight (6);

    macroPageButton.setBounds (strip.removeFromLeft (30).withSizeKeepingCentre (30, 20));
    strip.removeFromLeft (4);
    const auto macroWidth = strip.getWidth() / 4;

    for (int macro = 0; macro < (int) macroKnobs.size(); ++macro)
        if (macro % 4 == 0)
            for (int k = 0; k < 4 && macro + k < (int) macroKnobs.size(); ++k)
                macroKnobs[(size_t) (macro + k)]->setBounds (strip.getX() + k * macroWidth, strip.getY(),
                                                             macroWidth - 6, strip.getHeight());

    auto chipsRow = area.removeFromBottom (26).reduced (14, 1);

    layoutChips (chipsRow);

    tabs.setBounds (area.reduced (14, 0).withTrimmedBottom (2));
    layoutTabRow();

    if (scopePanel != nullptr)
    {
        auto pageArea = tabs.getBounds().withTrimmedTop (tabs.getTabBarDepth());
        const auto expanded = static_cast<ScopePanel*> (scopePanel.get())->isExpanded();
        // A running fade-in would snap the panel back to its old bounds.
        juce::Desktop::getInstance().getAnimator().cancelAnimation (scopePanel.get(), false);
        scopePanel->setAlpha (1.0f);
        scopePanel->setBounds (expanded ? pageArea
                                        : pageArea.reduced (8).removeFromRight (476)); // inside the page's frame, full height so it never cuts a card in half
    }
}

SectionPage* IlanaSynthAudioProcessorEditor::currentSection() const
{
    const auto index = tabs.getCurrentTabIndex();
    return index >= 0 && index < (int) sections.size() ? sections[(size_t) index] : nullptr;
}

void IlanaSynthAudioProcessorEditor::showPage (const juce::String& id)
{
    if (id == "SCOPE")
    {
        setScopeOpen (true);
        return;
    }

    for (int index = 0; index < (int) sections.size(); ++index)
    {
        const auto page = sections[(size_t) index]->indexOf (id);

        if (page < 0)
            continue;

        const auto sameTab = tabs.getCurrentTabIndex() == index;
        sections[(size_t) index]->show (page, sameTab);

        if (! sameTab)
            tabs.setCurrentTabIndex (index);

        layoutTabRow();
        return;
    }
}

juce::String IlanaSynthAudioProcessorEditor::getCurrentPageId() const
{
    if (auto* section = currentSection())
        return section->getCurrentId();

    return {};
}

juce::StringArray IlanaSynthAudioProcessorEditor::getPageIds() const
{
    juce::StringArray ids;

    for (auto* section : sections)
        for (int page = 0; page < section->getNumPages(); ++page)
            ids.add (section->getPageId (page));

    return ids;
}

juce::Component* IlanaSynthAudioProcessorEditor::getCurrentPage() const
{
    if (auto* section = currentSection())
        return section->getCurrentPage();

    return nullptr;
}

void IlanaSynthAudioProcessorEditor::setScopeOpen (bool shouldBeOpen)
{
    scopeButton.setToggleState (shouldBeOpen, juce::dontSendNotification);

    if (shouldBeOpen == scopePanel->isVisible())
        return;

    if (shouldBeOpen)
    {
        resized();
        scopePanel->setAlpha (0.0f);
        scopePanel->setVisible (true);
        scopePanel->toFront (false);
        juce::Desktop::getInstance().getAnimator().fadeIn (scopePanel.get(), 180);
    }
    else
    {
        scopePanel->setVisible (false);
    }
}

bool IlanaSynthAudioProcessorEditor::isScopeOpen() const
{
    return scopePanel != nullptr && scopePanel->isVisible();
}

// The current tab's page switch (and the scope button) sit at the right end
// of the tab row.
void IlanaSynthAudioProcessorEditor::layoutTabRow()
{
    const auto bar = tabs.getBounds().withHeight (tabs.getTabBarDepth());
    auto row = bar.reduced (4, 5);
    scopeButton.setBounds (row.removeFromRight (74));
    row.removeFromRight (10);

    for (auto* section : sections)
    {
        const auto current = section == currentSection() && section->getNumPages() > 1;
        section->switcher.setVisible (current);

        if (current)
            section->switcher.setBounds (row.removeFromRight (section->switcher.getIdealWidth()));
    }
}

void IlanaSynthAudioProcessorEditor::setKeyboardVisible (bool shouldBeVisible)
{
    keyboardVisible = shouldBeVisible;
    keysButton.setToggleState (shouldBeVisible, juce::dontSendNotification);

    if (settings != nullptr)
    {
        settings->setValue ("showKeyboard", shouldBeVisible);
        settings->saveIfNeeded();
    }

    resized();
}

// Every parameter, plus the patch data that isn't a parameter (drawn LFO
// steps and curves, remap curves, clips), so drawing lights EDITED too.
juce::int64 IlanaSynthAudioProcessorEditor::parameterFingerprint() const
{
    juce::int64 hash = 0;
    auto index = 1;

    for (auto* parameter : processorRef.getParameters())
    {
        hash += (juce::int64) juce::roundToInt (parameter->getValue() * 100000.0f) * (index * 2654435761LL % 1000003);
        ++index;
    }

    return hash ^ processorRef.getPatchDataHash();
}

bool IlanaSynthAudioProcessorEditor::isPatchEdited() const
{
    return parameterFingerprint() != loadedFingerprint;
}

bool IlanaSynthAudioProcessorEditor::asksBeforeReplacingEdits() const
{
    return settings == nullptr || settings->getBoolValue ("confirmReplaceEdited", true);
}

void IlanaSynthAudioProcessorEditor::setAsksBeforeReplacingEdits (bool shouldAsk)
{
    if (settings != nullptr)
    {
        settings->setValue ("confirmReplaceEdited", shouldAsk);
        settings->saveIfNeeded();
    }
}

// Runs then (true) at once while the patch is unedited; otherwise asks.
// Stepping through presets asks once: the patch it lands on is unedited.
void IlanaSynthAudioProcessorEditor::confirmReplacingPatch (const juce::String& replacement, const juce::String& confirmText,
                                                            std::function<void (bool confirmed)> then)
{
    if (! isPatchEdited() || ! asksBeforeReplacingEdits())
    {
        then (true);
        return;
    }

    const auto current = processorRef.getCurrentPresetName();
    confirmOverlay.ask ("Replace your edits?",
                        "'" + (current.isNotEmpty() ? current : juce::String ("Init")) + "' has changes that aren't saved. "
                            + replacement + " replaces them.",
                        confirmText,
                        [safeThis = juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> (this), then] (bool confirmed, bool dontAsk)
                        {
                            if (safeThis == nullptr)
                                return;

                            if (confirmed && dontAsk)
                                safeThis->setAsksBeforeReplacingEdits (false);

                            then (confirmed);
                        });
}

// Edits still open (a gesture without its mouse-up, parameter changes not
// yet in the tree) join their own step before it is undone.
void IlanaSynthAudioProcessorEditor::undoOrRedo (bool redo)
{
    processorRef.endEdit();
    processorRef.apvts.copyState();

    if (redo)
        processorRef.getUndoManager().redo();
    else
        processorRef.getUndoManager().undo();
}

void IlanaSynthAudioProcessorEditor::savePreset()
{
    const auto file = PresetFiles::loadedUserPreset (processorRef);

    if (! file.existsAsFile())
    {
        savePresetAs();
        return;
    }

    if (processorRef.savePresetToFile (file))
        presetSaved (true);
    else
        presetDisplay.showNotice ("COULDN'T SAVE");
}

void IlanaSynthAudioProcessorEditor::savePresetAs()
{
    if (presetPanel != nullptr && presetPanel->isOpen() && ! presetPanel->isDocked())
        presetPanel->close();

    saveOverlay.setBounds (content.getLocalBounds());
    saveOverlay.show();
}

// After a save: the patch is no longer EDITED, the browser lists the file,
// and the preset name says so for a moment.
void IlanaSynthAudioProcessorEditor::presetSaved (bool inPlace)
{
    rememberLoadedFingerprint();

    if (presetPanel != nullptr)
        presetPanel->refresh();

    updateHeaderButtons();
    presetDisplay.showNotice (inPlace ? "SAVED" : "SAVED AS NEW PRESET");
    presetLoadFlash = 1.0f;
    animator.startTimerHz (60);
}

void IlanaSynthAudioProcessorEditor::exportPreset()
{
    const auto directory = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                               .getChildFile ("ilanaSynth Presets");
    directory.createDirectory();

    fileChooser = std::make_unique<juce::FileChooser> ("Export preset",
                                                       directory.getChildFile (processorRef.getCurrentPresetName() + ".ilanapreset"),
                                                       "*.ilanapreset");

    juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> safeThis (this);

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safeThis] (const juce::FileChooser& chooser)
                              {
                                  const auto file = chooser.getResult();

                                  if (file != juce::File() && safeThis != nullptr)
                                  {
                                      safeThis->processorRef.savePresetToFile (file.withFileExtension ("ilanapreset"));

                                      if (safeThis->presetPanel != nullptr)
                                          safeThis->presetPanel->refresh();
                                  }
                              });
}

void IlanaSynthAudioProcessorEditor::loadPreset()
{
    const auto directory = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                               .getChildFile ("ilanaSynth Presets");
    directory.createDirectory();

    fileChooser = std::make_unique<juce::FileChooser> ("Load preset", directory, "*.ilanapreset");

    juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> safeThis (this);

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
                              [safeThis] (const juce::FileChooser& chooser)
                              {
                                  const auto file = chooser.getResult();

                                  if (! file.existsAsFile() || safeThis == nullptr)
                                      return;

                                  safeThis->confirmReplacingPatch ("Loading '" + file.getFileNameWithoutExtension() + "'", "Load anyway",
                                                                   [safeThis, file] (bool confirmed)
                                                                   {
                                                                       if (! confirmed || safeThis == nullptr)
                                                                           return;

                                                                       auto& processor = safeThis->processorRef;
                                                                       processor.beginEdit ("Load " + file.getFileNameWithoutExtension());
                                                                       processor.loadPresetFromFile (file);
                                                                       processor.endEdit();
                                                                   });
                              });
}

void IlanaSynthAudioProcessorEditor::loadPresetIndex (int index, std::function<void (bool loaded)> then)
{
    const auto names = processorRef.getAllPresetNames();

    if (! juce::isPositiveAndBelow (index, names.size()))
        return;

    confirmReplacingPatch ("Loading '" + names[index] + "'", "Load anyway",
                           [safeThis = juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> (this), index, name = names[index], then] (bool confirmed)
                           {
                               if (safeThis == nullptr)
                                   return;

                               if (confirmed)
                               {
                                   // One undo step for the whole preset, its data included.
                                   auto& self = *safeThis;
                                   self.processorRef.beginEdit ("Load " + name);
                                   self.processorRef.loadPresetByIndex (index);
                                   self.processorRef.endEdit();
                                   self.presetLoadFlash = 1.0f;
                                   self.animator.startTimerHz (60);

                                   self.updateHeaderButtons();
                                   self.rememberLoadedFingerprint();
                               }

                               if (then != nullptr)
                                   then (confirmed);
                           });
}

void IlanaSynthAudioProcessorEditor::showHistoryMenu()
{
    auto& undoManager = processorRef.getUndoManager();

    const auto undos = undoManager.getUndoDescriptions();
    const auto redos = undoManager.getRedoDescriptions();

    constexpr int maxEntries = 32;

    juce::PopupMenu menu;

    if (undos.isEmpty() && redos.isEmpty())
    {
        menu.addItem (1, "No history yet", false);
    }
    else
    {
        if (! redos.isEmpty())
        {
            menu.addSectionHeader ("REDO");

            for (int i = 0; i < juce::jmin (maxEntries, redos.size()); ++i)
                menu.addItem (2000 + i, redos[i].isNotEmpty() ? redos[i]
                                                              : "Redo change " + juce::String (i + 1));
        }

        if (! undos.isEmpty())
        {
            if (! redos.isEmpty())
                menu.addSeparator();

            menu.addSectionHeader ("UNDO");

            for (int i = 0; i < juce::jmin (maxEntries, undos.size()); ++i)
                menu.addItem (1000 + i, undos[i].isNotEmpty() ? undos[i]
                                                              : "Undo change " + juce::String (i + 1));
        }
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&historyButton),
                        [safeThis = juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> (this)] (int result)
                        {
                            if (safeThis == nullptr || result < 1000)
                                return;

                            if (result < 2000)
                            {
                                for (int i = 0; i <= result - 1000; ++i)
                                    safeThis->undoOrRedo (false);
                            }
                            else
                            {
                                for (int i = 0; i <= result - 2000; ++i)
                                    safeThis->undoOrRedo (true);
                            }

                            safeThis->updateHeaderButtons();
                        });
}

void IlanaSynthAudioProcessorEditor::updateHeaderButtons()
{
    const auto nameChanged = processorRef.getCurrentPresetName() != shownPresetName;
    shownPresetName = processorRef.getCurrentPresetName();

    const auto names = processorRef.getAllPresetNames();
    currentPresetIndex = names.indexOf (shownPresetName);
    shownCategory = juce::isPositiveAndBelow (currentPresetIndex, names.size())
                        ? processorRef.getAllPresetCategories()[currentPresetIndex]
                        : juce::String();

    if (nameChanged)
        adoptLoadedFingerprint();

    presetDisplay.setPreset (shownPresetName, shownCategory, isFavourite (shownPresetName),
                             parameterFingerprint() != loadedFingerprint);
    favButton.setToggleState (isFavourite (shownPresetName), juce::dontSendNotification);
    favButton.setIconColour (isFavourite (shownPresetName) ? std::optional<juce::Colour> (juce::Colour (0xffffd447))
                                                           : std::nullopt);
    abButton.setButtonText (showingA ? "A/B:  A" : "A/B:  B");
    abButton.setToggleState (! showingA, juce::dontSendNotification);

    for (auto& knob : macroKnobs)
        knob->refreshName();
}

void IlanaSynthAudioProcessorEditor::showMacroPage (int page)
{
    macroPage = juce::jlimit (0, (Mod::numMacros - 1) / 4, page);
    for (int macro = 0; macro < (int) macroKnobs.size(); ++macro)
        macroKnobs[(size_t) macro]->setVisible (macro / 4 == macroPage);
    macroPageButton.setButtonText (macroPage == 0 ? "5-8" : "1-4");
}

void IlanaSynthAudioProcessorEditor::updateUndoButtons()
{
    auto& undoManager = processorRef.getUndoManager();
    undoButton.setEnabled (undoManager.canUndo());
    redoButton.setEnabled (undoManager.canRedo());
    historyButton.setEnabled (undoManager.canUndo() || undoManager.canRedo());
}

bool IlanaSynthAudioProcessorEditor::isFavourite (const juce::String& presetName) const
{
    return settings != nullptr && settings->getValue ("fav_" + presetName, "0") == "1";
}

void IlanaSynthAudioProcessorEditor::toggleFavourite()
{
    if (settings == nullptr)
        return;

    const auto names = processorRef.getAllPresetNames();

    if (! juce::isPositiveAndBelow (currentPresetIndex, names.size()))
        return;

    const auto name = names[currentPresetIndex];
    settings->setValue ("fav_" + name, isFavourite (name) ? "0" : "1");
    settings->saveIfNeeded();
}

void IlanaSynthAudioProcessorEditor::toggleAB()
{
    if (showingA)
    {
        slotA = processorRef.apvts.copyState();

        if (slotB.isValid())
            processorRef.apvts.replaceState (slotB);
        else
            slotB = slotA;
    }
    else
    {
        slotB = processorRef.apvts.copyState();
        processorRef.apvts.replaceState (slotA);
    }

    showingA = ! showingA;
    abButton.setButtonText (showingA ? "A/B:  A" : "A/B:  B");
    abButton.setToggleState (! showingA, juce::dontSendNotification);
}

void IlanaSynthAudioProcessorEditor::setTheme (int newThemeIndex)
{
    themeIndex = juce::jlimit (0, IlanaTheme::numPalettes - 1, newThemeIndex);
    lookAndFeel.setAccent (juce::Colour (IlanaTheme::palette[themeIndex]));

    if (settings != nullptr)
    {
        settings->setValue ("themeIndex", themeIndex);
        settings->saveIfNeeded();
    }

    sendLookAndFeelChange();
}

void IlanaSynthAudioProcessorEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Init patch");
    menu.addSeparator();
    menu.addItem (2, "Save preset", true);
    menu.addItem (7, "Save preset as...");
    menu.addItem (5, "Export preset file...");
    menu.addItem (3, "Load preset file...");
    menu.addItem (4, "Open user preset folder");
    menu.addSeparator();
    menu.addItem (6, "Import DX7 / Dexed bank (.syx)...");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&moreButton),
                        [safeThis = juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> (this)] (int result)
                        {
                            if (safeThis == nullptr)
                                return;

                            switch (result)
                            {
                                case 1: safeThis->loadPresetIndex (0); break;
                                case 2: safeThis->savePreset(); break;
                                case 7: safeThis->savePresetAs(); break;
                                case 3: safeThis->loadPreset(); break;
                                case 5: safeThis->exportPreset(); break;
                                case 6:
                                {
                                    // Each voice becomes a user preset under DX7/<bank>/.
                                    safeThis->fileChooser = std::make_unique<juce::FileChooser> (
                                        "Import DX7 bank", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.syx");
                                    safeThis->fileChooser->launchAsync (
                                        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                        [safeThis] (const juce::FileChooser& chooser)
                                        {
                                            const auto file = chooser.getResult();
                                            if (safeThis == nullptr || ! file.existsAsFile())
                                                return;
                                            juce::String message;
                                            safeThis->processorRef.importDx7File (file, message);
                                            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                                                    "Import DX7 bank", message);
                                        });
                                    break;
                                }
                                case 4:
                                {
                                    const auto directory = safeThis->processorRef.getUserPresetDirectory();
                                    directory.createDirectory();
                                    directory.startAsProcess();
                                    break;
                                }
                                default: break;
                            }
                        });
}

void IlanaSynthAudioProcessorEditor::showDiceMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("RANDOMISE");
    menu.addItem (1, "Whole patch");
    menu.addItem (2, "Oscillators only");
    menu.addItem (3, "Filters only");
    menu.addItem (4, "Envelopes only");
    menu.addItem (5, "Modulation only");
    menu.addItem (6, "Effects chain only");
    menu.addSeparator();
    menu.addSectionHeader ("MUTATE");
    menu.addItem (10, "Nudge  (small changes)");
    menu.addItem (11, "Shake  (bigger changes)");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&diceButton),
                        [safeThis = juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> (this)] (int result)
                        {
                            if (safeThis == nullptr || result == 0)
                                return;

                            // Each roll is one named undo step.
                            const juce::String names[] { {}, "Random patch", "Random oscillators", "Random filters",
                                                         "Random envelopes", "Random modulation", "Random effects" };
                            const auto name = result <= 6 ? names[result] : juce::String (result == 10 ? "Nudge" : "Shake");
                            const auto roll = [safeThis, result, name]
                            {
                                if (safeThis == nullptr)
                                    return;

                                auto& self = *safeThis;
                                self.processorRef.performEdit (name, [&self, result]
                                {
                                    if (result == 1)
                                        self.randomize();
                                    else if (result == 6)
                                        self.processorRef.randomizeFxChain();
                                    else if (result == 10)
                                        self.mutate (0.06f);
                                    else if (result == 11)
                                        self.mutate (0.18f);
                                    else
                                        self.randomizeGroup (result);
                                });
                            };

                            // A whole new patch replaces the edited one: ask first.
                            if (result == 1)
                                safeThis->confirmReplacingPatch ("A random patch", "Roll anyway",
                                                                 [roll] (bool confirmed) { if (confirmed) roll(); });
                            else
                                roll();
                        });
}

void IlanaSynthAudioProcessorEditor::showSettingsMenu (bool voicesOnly)
{
    const char* const skinNames[] { "Ember", "Ice", "Acid", "Neon" };
    const float zoomChoices[] { 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };

    juce::PopupMenu skins;

    for (int i = 0; i < IlanaTheme::numPalettes; ++i)
        skins.addItem (100 + i, skinNames[i], true, i == themeIndex);

    juce::PopupMenu sizes;

    for (int i = 0; i < 6; ++i)
        sizes.addItem (200 + i, juce::String (juce::roundToInt (zoomChoices[i] * 100.0f)) + "%",
                       true, juce::approximatelyEqual (uiZoom, zoomChoices[i]));

    // Engine quality and oversampling, also on the scope panel (where they were
    // hard to find).
    const auto read = [this] (const char* id) { return processorRef.apvts.getRawParameterValue (id)->load(); };
    juce::PopupMenu quality;
    const char* const qualityNames[] { "Eco (unison capped at 4)", "Normal", "High" };
    for (int i = 0; i < 3; ++i)
        quality.addItem (600 + i, qualityNames[i], true, juce::roundToInt (read ("quality")) == i);
    juce::PopupMenu oversampling;
    const auto oversampled = read ("oversampling") > 0.5f;
    const auto factor = juce::roundToInt (read ("os_factor"));
    oversampling.addItem (700, "Off", true, ! oversampled);
    oversampling.addItem (701, "2x", true, oversampled && factor == 0);
    oversampling.addItem (702, "4x", true, oversampled && factor == 1);

    // Scala microtuning: the tick shows it is on, with the scale's name.
    juce::PopupMenu tuning;
    const auto& tuningState = processorRef.getTuningState();
    const auto hasScale = tuningState.hasScale();
    const auto tuningOn = hasScale && read ("tuning_on") > 0.5f;
    tuning.addItem (800, hasScale ? "Tuning on: " + tuningState.getDescription() : juce::String ("Tuning on (no scale loaded)"),
                    hasScale, tuningOn);
    tuning.addSeparator();
    tuning.addItem (801, "Load Scala tuning (.scl)...");
    tuning.addItem (802, "Load keyboard mapping (.kbm)...");
    tuning.addItem (803, "Reset to 12-TET", hasScale || read ("tuning_on") > 0.5f);
    tuning.addSeparator();
    // MTS-ESP: shown for information; a master in the session takes over.
    tuning.addItem (804, processorRef.isMtsEspConnected() ? "MTS-ESP: " + processorRef.getMtsEspScaleName() + " (overrides the scale)"
                                                          : juce::String ("MTS-ESP: no master in this session"),
                    false, processorRef.isMtsEspConnected());

    // The voice settings that used to sit in the bottom bar: how notes are
    // shared out, how many can sound at once and the pitch-bend range.
    static constexpr int voiceCounts[] { 1, 2, 3, 4, 6, 8, 12, 16, 24, 32 };
    static constexpr int bendRanges[] { 0, 1, 2, 3, 4, 5, 7, 12, 24 };
    const auto currentVoices = juce::roundToInt (read ("poly_voices"));
    const auto currentBend = juce::roundToInt (read ("bend_range"));
    const auto currentMode = juce::roundToInt (read ("voice_mode"));
    juce::PopupMenu voiceModes, voiceLimits, bendMenu;
    const char* const modeNames[] { "Poly", "Mono", "Legato" };
    for (int i = 0; i < 3; ++i)
        voiceModes.addItem (900 + i, modeNames[i], true, currentMode == i);
    for (int i = 0; i < (int) std::size (voiceCounts); ++i)
        voiceLimits.addItem (910 + i, juce::String (voiceCounts[i]), true, currentVoices == voiceCounts[i]);
    for (int i = 0; i < (int) std::size (bendRanges); ++i)
        bendMenu.addItem (930 + i, juce::String (bendRanges[i]) + (bendRanges[i] == 1 ? " semitone" : " semitones"),
                          true, currentBend == bendRanges[i]);

    juce::PopupMenu menu;
    menu.addSubMenu ("Voice mode:  " + juce::String (modeNames[juce::jlimit (0, 2, currentMode)]), voiceModes);
    menu.addSubMenu ("Voices:  " + juce::String (currentVoices), voiceLimits);
    menu.addSubMenu ("Pitch bend range:  " + juce::String (currentBend) + " st", bendMenu);

    menu.addSeparator();
    menu.addSubMenu ("Skin", skins);
    menu.addSubMenu ("Interface size", sizes);
    menu.addSubMenu ("Engine quality", quality);
    menu.addSubMenu ("Oversampling", oversampling);
    menu.addSubMenu (tuningOn ? "Tuning: " + tuningState.getDescription() : juce::String ("Tuning"), tuning, true, nullptr, tuningOn);
    menu.addItem (300, "Show keyboard", true, keyboardVisible);
    menu.addItem (500, "MPE mode (per-note pitch, pressure and slide)", true,
                  processorRef.apvts.getRawParameterValue ("mpe_mode")->load() > 0.5f);
    menu.addItem (410, "Ask before replacing an edited patch", true, asksBeforeReplacingEdits());
    menu.addSeparator();
    menu.addItem (400, "Show welcome tour");

    // The status line's VOICES opens just the voice settings.
    if (voicesOnly)
    {
        menu = juce::PopupMenu();
        menu.addSectionHeader ("VOICES");
        menu.addSubMenu ("Voice mode:  " + juce::String (modeNames[juce::jlimit (0, 2, currentMode)]), voiceModes);
        menu.addSubMenu ("Voices:  " + juce::String (currentVoices), voiceLimits);
        menu.addSubMenu ("Pitch bend range:  " + juce::String (currentBend) + " st", bendMenu);
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (voicesOnly ? static_cast<juce::Component*> (&voicesArea) : &settingsButton),
                        [safeThis = juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> (this)] (int result)
                        {
                            if (safeThis == nullptr || result == 0)
                                return;

                            const float zooms[] { 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };

                            if (result >= 100 && result < 200)
                                safeThis->setTheme (result - 100);
                            else if (result >= 200 && result < 300)
                                safeThis->applyUiZoom (zooms[juce::jlimit (0, 5, result - 200)]);
                            else if (result == 300)
                                safeThis->setKeyboardVisible (! safeThis->keyboardVisible);
                            else if (result == 500)
                            {
                                if (auto* mpe = safeThis->processorRef.apvts.getParameter ("mpe_mode"))
                                {
                                    mpe->beginChangeGesture();
                                    mpe->setValueNotifyingHost (mpe->getValue() > 0.5f ? 0.0f : 1.0f);
                                    mpe->endChangeGesture();
                                }
                            }
                            else if (result >= 600 && result < 800)
                            {
                                const auto set = [&safeThis] (const char* id, float value)
                                {
                                    if (auto* parameter = safeThis->processorRef.apvts.getParameter (id))
                                    {
                                        parameter->beginChangeGesture();
                                        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
                                        parameter->endChangeGesture();
                                    }
                                };
                                if (result < 700)
                                    set ("quality", (float) (result - 600));
                                else
                                {
                                    set ("oversampling", result == 700 ? 0.0f : 1.0f);
                                    if (result > 700)
                                        set ("os_factor", (float) (result - 701));
                                }
                            }
                            else if (result >= 900 && result < 950)
                            {
                                const auto set = [&safeThis] (const char* id, float value)
                                {
                                    if (auto* parameter = safeThis->processorRef.apvts.getParameter (id))
                                    {
                                        parameter->beginChangeGesture();
                                        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
                                        parameter->endChangeGesture();
                                    }
                                };

                                if (result < 910)
                                    set ("voice_mode", (float) (result - 900));
                                else if (result < 930)
                                    set ("poly_voices", (float) voiceCounts[juce::jlimit (0, (int) std::size (voiceCounts) - 1, result - 910)]);
                                else
                                    set ("bend_range", (float) bendRanges[juce::jlimit (0, (int) std::size (bendRanges) - 1, result - 930)]);
                            }
                            else if (result == 400)
                            {
                                safeThis->tutorial.setVisible (true);
                                safeThis->tutorial.toFront (false);
                            }
                            else if (result == 410)
                            {
                                safeThis->setAsksBeforeReplacingEdits (! safeThis->asksBeforeReplacingEdits());
                            }
                            else if (result == 800)
                            {
                                if (auto* on = safeThis->processorRef.apvts.getParameter ("tuning_on"))
                                {
                                    on->beginChangeGesture();
                                    on->setValueNotifyingHost (on->getValue() > 0.5f ? 0.0f : 1.0f);
                                    on->endChangeGesture();
                                }
                            }
                            else if (result == 801 || result == 802)
                            {
                                const auto mapping = result == 802;
                                safeThis->fileChooser = std::make_unique<juce::FileChooser> (
                                    mapping ? "Load keyboard mapping" : "Load Scala tuning",
                                    juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                    mapping ? "*.kbm" : "*.scl");
                                safeThis->fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                                                        | juce::FileBrowserComponent::canSelectFiles,
                                                                    [safeThis, mapping] (const juce::FileChooser& chooser)
                                                                    {
                                                                        const auto file = chooser.getResult();
                                                                        if (safeThis == nullptr || ! file.existsAsFile())
                                                                            return;
                                                                        juce::String error;
                                                                        const auto text = file.loadFileAsString();
                                                                        const auto loaded = mapping ? safeThis->processorRef.loadTuningMapping (text, error)
                                                                                                    : safeThis->processorRef.loadTuningScale (text, error);
                                                                        if (! loaded)
                                                                            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                                                                                    "Could not load " + file.getFileName(), error);
                                                                    });
                            }
                            else if (result == 803)
                                safeThis->processorRef.resetTuning();
                        });
}

void IlanaSynthAudioProcessorEditor::createPresetPanel()
{
    if (presetPanel != nullptr)
        return;

    presetPanel = std::make_unique<PresetPanel> (processorRef, settings.get());
    presetPanel->onLoad = [this] (int index, bool closeAfter)
    {
        loadPresetIndex (index, [safeThis = juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> (this), closeAfter] (bool loaded)
        {
            if (safeThis == nullptr || safeThis->presetPanel == nullptr)
                return;

            if (! loaded)
                safeThis->presetPanel->selectLoadedPreset(); // back to the patch still playing
            else if (closeAfter)
                safeThis->presetPanel->close();
        });
    };
    presetPanel->onFavouriteChanged = [this] { updateHeaderButtons(); };
    presetPanel->onSaveAs = [this] { savePresetAs(); };
    presetPanel->setAnchor (&presetDisplay);
    presetPanel->onDockRequest = [this] (bool dock)
    {
        presetDocked = dock;

        if (settings != nullptr)
        {
            settings->setValue ("presetBrowserDocked", presetDocked);
            settings->saveIfNeeded();
        }

        if (dock)
        {
            presetPanel->setVisible (false);
            setPresetDockShown (true);
        }
        else
        {
            setPresetDockShown (false);
            togglePresetPanel();
        }
    };
    presetPanel->onDockedClose = [this] { setPresetDockShown (false); };

    // Hidden until open() fades it in.
    content.addChildComponent (*presetPanel);
}

void IlanaSynthAudioProcessorEditor::setPresetDockShown (bool shown)
{
    createPresetPanel();

    if (shown == presetDockShown)
        return;

    const auto scale = (float) getHeight() / (float) designHeight;
    presetDockShown = shown;

    if (shown)
    {
        presetPanel->setVisible (false);
        presetPanel->setDocked (true);
        dockHolder.addAndMakeVisible (*presetPanel);
        presetPanel->open();
    }
    else
    {
        presetPanel->setVisible (false);
        presetPanel->setDocked (false);
        content.addChildComponent (*presetPanel);
    }

    if (settings != nullptr)
    {
        settings->setValue ("presetBrowserDockOpen", presetDockShown);
        settings->saveIfNeeded();
    }

    // Grow or shrink the window by the column, keeping the zoom.
    applyAspectAndLimits();
    setSize (juce::roundToInt ((float) currentDesignWidth() * scale), getHeight());
    resized();
}

void IlanaSynthAudioProcessorEditor::togglePresetPanel()
{
    createPresetPanel();

    if (presetDocked)
    {
        setPresetDockShown (! presetDockShown);
        return;
    }

    // Drop down under the preset name, kept inside the window.
    {
        const auto width = 640;
        const auto x = juce::jlimit (10, designWidth - 10 - width, presetDisplay.getX() - 40);
        presetPanel->setBounds (x, presetDisplay.getBottom() + 6, width, 528);
        presetPanel->setScrimArea (content.getLocalBounds());
    }

    if (presetPanel->isOpen())
        presetPanel->close();
    else
        presetPanel->open();
}

void IlanaSynthAudioProcessorEditor::randomize()
{
    for (const auto group : { 2, 3, 4, 5 })
        randomizeGroup (group);
}

// Groups: 2 oscillators, 3 filters, 4 envelopes, 5 modulation.
void IlanaSynthAudioProcessorEditor::randomizeGroup (int group)
{
    juce::Random random;

    const auto setValue = [this] (const char* id, float value)
    {
        if (auto* parameter = processorRef.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    const auto randomRange = [&random] (float minimum, float maximum)
    {
        return minimum + random.nextFloat() * (maximum - minimum);
    };

    const auto tableCount = TableFactory::getNumFactoryTables();

    if (group == 2)
    {
        setValue ("osc1_on", 1.0f);
        setValue ("sub_on", 1.0f);
        setValue ("osc2_mode", 0.0f);
        setValue ("sub_mode", 0.0f);

        if (random.nextFloat() < 0.25f)
        {
            setValue ("osc1_mode", 1.0f);
            setValue ("osc1_excite", (float) random.nextInt (4));
            setValue ("osc1_string_decay", randomRange (0.6f, 0.95f));
            setValue ("osc1_string_damp", randomRange (0.1f, 0.6f));
        }
        else
        {
            setValue ("osc1_mode", 0.0f);
        }

        setValue ("osc1_table", (float) random.nextInt (tableCount));
        setValue ("osc1_frame", random.nextFloat());
        setValue ("osc1_unison", (float) (1 + random.nextInt (4)));
        setValue ("osc1_detune", randomRange (5.0f, 30.0f));
        setValue ("osc2_on", random.nextBool() ? 1.0f : 0.0f);
        setValue ("osc2_table", (float) random.nextInt (tableCount));
        setValue ("osc2_frame", random.nextFloat());
        setValue ("osc2_semi", (float) (random.nextBool() ? -12 : 12));
        setValue ("subosc_on", random.nextFloat() < 0.6f ? 1.0f : 0.0f);
        setValue ("subosc_level", randomRange (0.2f, 0.6f));
        setValue ("sub_shape", (float) random.nextInt (3));
        setValue ("fm_amount", random.nextFloat() < 0.6f ? randomRange (0.0f, 0.5f) : 0.0f);
        setValue ("fm_feedback", random.nextFloat() < 0.3f ? randomRange (0.0f, 0.4f) : 0.0f);
        setValue ("ring_mod", random.nextFloat() < 0.3f ? randomRange (0.0f, 0.8f) : 0.0f);
        setValue ("hard_sync", random.nextFloat() < 0.25f ? 1.0f : 0.0f);
        setValue ("drift", randomRange (0.0f, 0.5f));
        setValue ("osc1_chord", (float) random.nextInt (7));
        setValue ("voice_spread", randomRange (0.0f, 0.5f));
        setValue ("unison_random", randomRange (0.0f, 1.0f));
    }
    else if (group == 3)
    {
        // Low pass, band pass, high pass and both ladders; notch is rarely useful at random.
        const int types[] { FilterType::LowPass, FilterType::BandPass, FilterType::HighPass,
                            FilterType::LadderLow, FilterType::LadderLow, FilterType::LadderHigh };
        setValue ("f1_type", (float) types[random.nextInt (6)]);
        setValue ("f1_cutoff", randomRange (200.0f, 8000.0f));
        setValue ("f1_reso", randomRange (0.0f, 0.6f));
        setValue ("f1_env", randomRange (-1.0f, 3.0f));
        setValue ("res_on", random.nextFloat() < 0.25f ? 1.0f : 0.0f);
    }
    else if (group == 4)
    {
        setValue ("fe_decay", randomRange (0.05f, 1.5f));
        setValue ("amp_attack", random.nextFloat() < 0.3f ? randomRange (0.05f, 0.8f) : 0.005f);
        setValue ("amp_release", randomRange (0.05f, 2.0f));
    }
    else if (group == 5)
    {
        setValue ("lfo1_rate", randomRange (0.2f, 8.0f));

        if (random.nextFloat() < 0.6f)
        {
            setValue ("mod1_src", 1.0f);
            setValue ("mod1_dst", random.nextFloat() < 0.5f ? 9.0f : 2.0f);
            setValue ("mod1_amt", randomRange (0.15f, 0.6f));
        }
        else
        {
            setValue ("mod1_src", 0.0f);
            setValue ("mod1_dst", 0.0f);
            setValue ("mod1_amt", 0.0f);
        }
    }
}

// Nudges every continuous sound parameter by a small random amount, keeping
// the patch's character. Routing, switches, levels at the end of the chain
// and the effect rack layout are left alone.
void IlanaSynthAudioProcessorEditor::mutate (float amount)
{
    juce::Random random;

    for (auto* parameter : processorRef.getParameters())
    {
        auto* floatParameter = dynamic_cast<juce::AudioParameterFloat*> (parameter);

        if (floatParameter == nullptr)
            continue;

        const auto id = floatParameter->getParameterID();

        if (id == "master" || id.startsWith ("macro") || id.startsWith ("mod") || id.startsWith ("fx_slot")
            || id == "bend_range" || id == "master_clip_gain" || id.contains ("_step"))
            continue;

        // Only touch what's already doing something: a parameter sitting at
        // its default is usually off on purpose.
        const auto current = parameter->getValue();

        if (std::abs (current - parameter->getDefaultValue()) < 1.0e-4f && random.nextFloat() < 0.7f)
            continue;

        const auto offset = ((random.nextFloat() + random.nextFloat() + random.nextFloat()) / 1.5f - 1.0f) * amount;
        floatParameter->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, current + offset));
    }
}

// Esc closes the topmost thing open over the page, one per press: the save
// panel, the tour, the drop-down browser, a held mod card, the remap editor,
// the scope. False when nothing was open.
bool IlanaSynthAudioProcessorEditor::closeTopPopup()
{
    if (saveOverlay.isShowing())
    {
        saveOverlay.cancel();
        return true;
    }

    if (tutorial.isVisible())
    {
        tutorial.dismiss();
        return true;
    }

    if (presetPanel != nullptr && presetPanel->isOpen() && ! presetPanel->isDocked())
    {
        presetPanel->close();
        return true;
    }

    {
        std::vector<KnobControl*> knobs;
        std::function<void (juce::Component&)> collect = [&] (juce::Component& parent)
        {
            for (auto* child : parent.getChildren())
            {
                if (auto* knob = dynamic_cast<KnobControl*> (child))
                    knobs.push_back (knob);

                collect (*child);
            }
        };
        collect (content);
        auto closed = false;

        for (auto* knob : knobs)
        {
            if (knob->isModCardOpen())
            {
                knob->closeModCard();
                closed = true;
            }
        }

        if (closed)
            return true;

        std::function<RemapEditor* (juce::Component&)> findRemap = [&] (juce::Component& parent) -> RemapEditor*
        {
            for (auto* child : parent.getChildren())
            {
                if (auto* remap = dynamic_cast<RemapEditor*> (child); remap != nullptr && remap->isShowing())
                    return remap;

                if (auto* found = findRemap (*child))
                    return found;
            }

            return nullptr;
        };

        if (auto* remap = findRemap (content); remap != nullptr && remap->close())
            return true;
    }

    if (isScopeOpen())
    {
        setScopeOpen (false);
        return true;
    }

    return false;
}

bool IlanaSynthAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    if (confirmOverlay.isAsking())
        return confirmOverlay.keyPressed (key);

    if (saveOverlay.isShowing())
        return saveOverlay.keyPressed (key);

    if (key.getKeyCode() == juce::KeyPress::escapeKey)
        return closeTopPopup();

    const auto modifiers = key.getModifiers();

    if (modifiers.isCommandDown() || modifiers.isCtrlDown())
    {
        const auto code = juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) key.getKeyCode());

        if (code == 'Z')
        {
            undoOrRedo (modifiers.isShiftDown());
            return true;
        }

        if (code == 'Y')
        {
            undoOrRedo (true);
            return true;
        }

        if (code == 'S')
        {
            modifiers.isShiftDown() ? savePresetAs() : savePreset();
            return true;
        }

        // Previous / next preset, asking first over an edited patch.
        if (key.getKeyCode() == juce::KeyPress::leftKey || key.getKeyCode() == juce::KeyPress::rightKey)
        {
            (key.getKeyCode() == juce::KeyPress::leftKey ? prevButton : nextButton).onClick();
            return true;
        }

        return false;
    }

    // Bare digits pick a tab, unless something is being typed.
    if (dynamic_cast<juce::TextEditor*> (juce::Component::getCurrentlyFocusedComponent()) != nullptr
        || modifiers.isAltDown())
        return false;

    const auto code = key.getKeyCode();

    if (code >= '1' && code <= '9')
    {
        const auto index = code - '1';

        if (index < tabs.getNumTabs())
        {
            tabs.setCurrentTabIndex (index);
            return true;
        }
    }

    return false;
}

// The preset's fingerprint when it was loaded or saved, kept in the
// processor with the preset's name so a reopened editor still shows EDITED.
void IlanaSynthAudioProcessorEditor::rememberLoadedFingerprint()
{
    loadedFingerprint = parameterFingerprint();
    processorRef.loadedPresetFingerprint = { processorRef.getCurrentPresetName(), loadedFingerprint };
}

void IlanaSynthAudioProcessorEditor::adoptLoadedFingerprint()
{
    const auto& kept = processorRef.loadedPresetFingerprint;

    if (kept.has_value() && kept->first == processorRef.getCurrentPresetName())
        loadedFingerprint = kept->second;
    else
        rememberLoadedFingerprint();
}
