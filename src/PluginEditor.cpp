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
#include "gui/FmOperatorInfo.h"
#include "gui/OperatorEnvDisplay.h"
#include "gui/OperatorPoolCards.h"
#include "gui/GenerativeWidgets.h"
#include "gui/ClipEditor.h"
#include "gui/TableBrowser.h"
#include "gui/WavetableEditor.h"
#include "gui/LfoShapeMenu.h"
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
#include "gui/StateTabs.h"
#include "gui/pages/PageHelpers.h"
#include "gui/pages/OscPage.h"
#include "gui/pages/FilterVectorPhysicalPages.h"
#include "gui/pages/EnvLfoPages.h"
#include "gui/pages/FmInputPages.h"
#include "gui/pages/SeqPage.h"
#include "gui/pages/MainPage.h"
#include "gui/pages/MatrixPage.h"
#include "gui/FxInfoText.h"
#include "gui/pages/FxWidgets.h"
#include "gui/pages/FxPage.h"


// The scope over the current page. Docked (the default, review 7) it fills
// the page area, as the docked preset browser does; FLOAT makes it a small
// panel at the page's bottom right that drags anywhere in the page area by
// its title bar, DOCK puts it back. The cross closes it. FLOAT / DOCK and
// the cross are the browser's pair, in the same place. Engine quality and
// oversampling live in the settings menu, not here.
class ScopePanel : public juce::Component
{
public:
    explicit ScopePanel (IlanaSynthAudioProcessor& p)
        : scope (p)
    {
        addAndMakeVisible (scope);

        dockButton.onClick = [this]
        {
            setDocked (! docked);

            if (onDockChange != nullptr)
                onDockChange (docked);
        };

        closeButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xc3\x97")));
        closeButton.setTooltip ("Close the scope (or click SCOPE in the tab row again)");
        closeButton.onClick = [this]
        {
            if (onClose != nullptr)
                onClose();
        };

        addAndMakeVisible (dockButton);
        addAndMakeVisible (closeButton);
        setDocked (true);
    }

    std::function<void()> onClose;
    std::function<void (bool docked)> onDockChange;

    bool isDocked() const { return docked; }

    void setDocked (bool shouldDock)
    {
        docked = shouldDock;
        dockButton.setButtonText (docked ? "FLOAT" : "DOCK");
        dockButton.setTooltip (docked ? "A small panel over the page instead, that you can drag"
                                      : "Fill the page area with the scope");
        resized();
        repaint();
    }

    // Where the panel goes in the page area: all of it when docked, else
    // its own size where it was dragged (first at the bottom right), kept
    // inside.
    juce::Rectangle<int> placeIn (juce::Rectangle<int> area)
    {
        pageArea = area;

        if (docked)
            return area.reduced (2, 0).withTrimmedBottom (2);

        const auto size = juce::Point<int> (juce::jmin (floatingWidth, area.getWidth() - 16), juce::jmin (floatingHeight, area.getHeight() - 16));

        if (! placed)
            topLeft = { area.getRight() - 8 - size.x, area.getBottom() - 8 - size.y };

        return constrain ({ topLeft.x, topLeft.y, size.x, size.y });
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (docked ? 0.0f : 4.0f);

        // Floating: a soft dark shadow under it, so the cards it covers
        // read as behind it rather than cut off.
        if (! docked)
        {
            for (int ring = 4; ring >= 1; --ring)
            {
                g.setColour (juce::Colours::black.withAlpha (0.12f));
                g.fillRoundedRectangle (bounds.expanded ((float) ring).translated (0.0f, 2.0f), 8.0f + (float) ring);
            }
        }

        // The browser's card and title: an accent dot, then the name.
        IlanaTheme::paintCard (g, bounds, 9.0f, IlanaTheme::accent().withAlpha (docked ? 0.3f : 0.45f));

        auto header = bounds.toNearestInt().reduced (14, 0).removeFromTop (34);
        g.setColour (IlanaTheme::accent());
        g.fillEllipse ((float) header.getX(), (float) header.getCentreY() - 3.0f, 6.0f, 6.0f);
        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
        g.drawText ("SCOPE", header.withTrimmedLeft (14), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (docked ? 0 : 4);
        auto header = area.removeFromTop (34).reduced (12, 0).withSizeKeepingCentre (area.getWidth() - 24, 20);
        closeButton.setBounds (header.removeFromRight (22));
        header.removeFromRight (4);
        dockButton.setBounds (header.removeFromRight (56));
        scope.setBounds (area.reduced (8, 0).withTrimmedBottom (8));
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        setMouseCursor (! docked && event.y < 34 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        dragging = ! docked && event.y < 34;
        dragStart = getPosition();
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (! dragging)
            return;

        const auto moved = constrain (getBounds().withPosition (dragStart + event.getOffsetFromDragStart()));
        topLeft = moved.getPosition();
        placed = true;
        setBounds (moved);
    }

    void mouseUp (const juce::MouseEvent&) override { dragging = false; }

private:
    static constexpr int floatingWidth = 440, floatingHeight = 300;

    juce::Rectangle<int> constrain (juce::Rectangle<int> bounds) const
    {
        return pageArea.isEmpty() ? bounds : bounds.constrainedWithin (pageArea);
    }

    ScopeDisplay scope;
    juce::TextButton dockButton, closeButton;
    juce::Rectangle<int> pageArea;
    juce::Point<int> topLeft, dragStart;
    bool docked = true, placed = false, dragging = false;
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
        tableBrowserSettings() = settings.get();
    }

    themeIndex = juce::jlimit (0, IlanaTheme::numPalettes - 1, settings->getIntValue ("themeIndex", 0));
    lookAndFeel.setAccent (juce::Colour (IlanaTheme::palette[themeIndex]));

    addAndMakeVisible (content);
    content.onPaint = [this] (juce::Graphics& g) { paintHeader (g); };

    content.addAndMakeVisible (logo);
    logo.isSounding = [this] { return processorRef.getActiveVoiceCount() > 0; };
    content.addAndMakeVisible (infoStrip);

    // KEYS and ? sit at the right of the tab row, beside SCOPE (the status
    // line they shared is now only a hover line over the chips).
    keysButton.setClickingTogglesState (true);
    keysButton.setTooltip ("Keyboard\nShow or hide the on-screen keyboard.  Hiding it gives the pages more room.");
    keysButton.onClick = [this] { setKeyboardVisible (keysButton.getToggleState()); };
    helpButton.setTooltip ("Welcome tour\nThe one-minute tour: the essentials, the shortcuts and what's new.");
    helpButton.onClick = [this]
    {
        tutorial.setVisible (true);
        tutorial.toFront (false);
    };

    // A "new in" chip opens its page.
    tutorial.onShowPage = [this] (const juce::String& id) { showPage (id); };

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
    // view, MOD: envelopes, LFOs and the MSEG, the matrix).
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
    auto* oscViewport = new OscPageViewport (p);
    addSection ("OSC", { { "OSC", "OSCILLATORS", oscViewport }, { "PHYSICAL", "PHYSICAL", new PhysicalPage (p) } });
    addSection ("FILTER", { { "FILTER", "FILTER", new FilterPage (p) } });
    auto* matrixPage = new MatrixPage (p);
    addSection ("MOD", { { "ENV/LFO", "ENV / LFO", envLfoPage },
                         { "MATRIX", "MATRIX", matrixPage } });
    // An LFO card's DRIVES row opens its routing in the matrix.
    ModNames::openMatrixRow() = [this, matrixPage] (int slot)
    {
        showPage ("MATRIX");
        matrixPage->focusSlot (slot);
    };
    auto* fmPage = new FmPage (p);
    addSection ("FM", { { "FM", "FM", fmPage } });
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
    // The header's VOICES: the OSC page's VOICE tab (one voice panel).
    showVoicePanel = [this, oscViewport]
    {
        if (auto* page = oscViewport->getPage())
            page->selectShared (OscPage::sharedVoice);
        showPage ("OSC");
    };
    getOscCardBounds = [this, oscViewport] (int osc) -> juce::Rectangle<int>
    {
        if (auto* page = oscViewport->getPage())
            return getLocalArea (page, page->getCardBounds (osc));
        return {};
    };
    // PLAY's oscillator title: that oscillator's full page.
    mainPage->onEditOscillator = [this, oscViewport] (int osc)
    {
        if (auto* page = oscViewport->getPage())
            page->selectOscillator (osc);
        showPage ("OSC");
    };

    // EDIT OP ENV (PLAY and OSC): the operator's envelope lives on FM.
    editOperator = [this, fmPage] (int op)
    {
        fmPage->selectOperator (op);
        showPage ("FM");
    };
    mainPage->onEditOperator = [this] (int op) { showOperatorEnvelope (op); };

    // Any page can open the FM page on an operator (-1: as it was), and
    // MOD on OP PITCH and OP LFO (the FM card's link to them).
    FmOperatorInfo::hooks().openOperator = [this, fmPage] (int osc)
    {
        if (osc >= 0)
            fmPage->selectOperator (osc);
        showPage ("FM");
    };
    FmOperatorInfo::hooks().openPitchAndLfo = [this, envLfoPage]
    {
        envLfoPage->selectEnvelope (17);
        envLfoPage->selectLfo (IlanaSynthAudioProcessor::numLfos + 2);
        showPage ("ENV/LFO");
    };

    // The scope floats over any page.
    scopePanel = std::make_unique<ScopePanel> (p);
    static_cast<ScopePanel*> (scopePanel.get())->onClose = [this] { setScopeOpen (false); };
    if (settings != nullptr)
        static_cast<ScopePanel*> (scopePanel.get())->setDocked (settings->getBoolValue ("scopeDocked", true));
    static_cast<ScopePanel*> (scopePanel.get())->onDockChange = [this] (bool docked)
    {
        if (settings != nullptr)
        {
            settings->setValue ("scopeDocked", docked);
            settings->saveIfNeeded();
        }

        resized();
    };
    scopeButton.setClickingTogglesState (true);
    scopeButton.setTooltip ("Scope\nShow the oscilloscope and spectrum over any page.");
    scopeButton.onClick = [this] { setScopeOpen (scopeButton.getToggleState()); };

    content.addAndMakeVisible (tabs);

    // In front of the tab bar: the page switches, the scope button and panel.
    for (auto* section : sections)
        content.addChildComponent (section->switcher);

    content.addAndMakeVisible (scopeButton);
    content.addAndMakeVisible (keysButton);
    content.addAndMakeVisible (helpButton);
    content.addChildComponent (*scopePanel);

    // Bottom strip: macros, then performance controls, then master.
    for (int macro = 0; macro < Mod::numMacros; ++macro)
    {
        auto knob = std::make_unique<StripKnob> (p, "macro" + juce::String (macro + 1),
                                                 "Macro " + juce::String (macro + 1), macro,
                                                 modSourceColour ((int) Mod::macroSourceFor (macro)), false);
        // + ASSIGN: a new matrix row from this macro, then the matrix.
        knob->onAssign = [this] (int index)
        {
            for (int slot = 0; slot < Mod::maxSlots; ++slot)
            {
                const auto routing = processorRef.readModSlot (slot);

                if (routing.source == Mod::Source::None && routing.destination == 0)
                {
                    processorRef.performEdit ("Assign macro", [this, slot, index]
                    {
                        processorRef.clearModSlot (slot);
                        processorRef.setModSlotValue (slot, "src", (float) Mod::macroSourceFor (index));
                        processorRef.setModSlotValue (slot, "amt", 0.5f);
                    });
                    break;
                }
            }

            showPage ("MATRIX");
        };
        content.addAndMakeVisible (*knob);
        macroKnobs.push_back (std::move (knob));
    }
    macroPlusButton.setTooltip ("Show the next macro (up to 8).  Macros 5 to 8 appear once they are named or assigned.");
    macroPlusButton.onClick = [this]
    {
        addedMacros = juce::jmin (4, shownMacros - 4 + 1);
        updateMacroStrip();
    };
    content.addChildComponent (macroPlusButton);
    // Master is a setting rather than a sound control: neutral, so it doesn't
    // outshine the page (and yellow stays the macros' colour).
    masterKnob = std::make_unique<StripKnob> (p, "master", "Master", -1, IlanaTheme::Ui::text2, false);
    outputMeter = std::make_unique<OutputMeter> (p);
    content.addAndMakeVisible (*outputMeter);
    content.addChildComponent (modHoverPopup);

    headerScope.setStrip (true);
    headerScope.setInterceptsMouseClicks (false, false); // the preset display under it takes the click
    headerScope.onStripClick = [this]
    {
        scopeButton.setToggleState (! scopeButton.getToggleState(), juce::dontSendNotification);
        setScopeOpen (scopeButton.getToggleState());
    };
    content.addChildComponent (headerScope);

    voicesArea.setTooltip ("Voices\nNotes sounding, of the most that can. Click for the voice settings: mode, how many voices, the pitch-bend range and glide.");
    voicesArea.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    voicesArea.onClick = [this] { showVoicePanel(); };
    content.addAndMakeVisible (voicesArea);

    // BPM and CPU are plain read-outs; VOICES opens the voice settings. All
    // three live in the tab bar now (tiny bold caps, label grey, value light).
    {
        const auto readoutFont = [] { return IlanaTheme::font (13.2f, true, true).withKerningFactor (0.04f); };
        const auto paintReadout = [readoutFont] (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& label, const juce::String& value,
                                                 juce::Colour valueColour, juce::Colour labelColour)
        {
            const auto font = juce::Font (readoutFont());
            const auto labelWidth = juce::GlyphArrangement::getStringWidthInt (font, label + " ");
            const auto valueWidth = juce::GlyphArrangement::getStringWidthInt (font, value);
            auto at = area.withTrimmedLeft (juce::jmax (0, area.getWidth() - labelWidth - valueWidth));
            g.setFont (readoutFont());
            g.setColour (labelColour);
            g.drawText (label, at.removeFromLeft (labelWidth), juce::Justification::centredLeft);
            g.setColour (valueColour);
            g.drawText (value, at, juce::Justification::centredLeft);
        };

        bpmArea.setInterceptsMouseClicks (false, false);
        bpmArea.onPaint = [this, paintReadout] (juce::Graphics& g, juce::Rectangle<int> area, bool)
        {
            paintReadout (g, area, "BPM", juce::String (processorRef.getCurrentBpm(), 1), IlanaTheme::Ui::text, IlanaTheme::Ui::text3);
        };
        content.addAndMakeVisible (bpmArea);

        cpuArea.setInterceptsMouseClicks (false, false);
        cpuArea.onPaint = [this, paintReadout] (juce::Graphics& g, juce::Rectangle<int> area, bool)
        {
            const auto cpu = processorRef.getCpuUsage() * 100.0f;
            const auto colour = cpu < 30.0f ? IlanaTheme::Ui::text
                                            : (cpu < 60.0f ? IlanaTheme::Ui::text.interpolatedWith (IlanaTheme::accent(), (cpu - 30.0f) / 30.0f)
                                                           : IlanaTheme::accent().interpolatedWith (juce::Colours::red, juce::jlimit (0.0f, 1.0f, (cpu - 60.0f) / 40.0f)));
            paintReadout (g, area, "CPU", juce::String (juce::roundToInt (cpu)) + "%", colour, IlanaTheme::Ui::text3);
        };
        content.addAndMakeVisible (cpuArea);

        voicesArea.onPaint = [this, readoutFont] (juce::Graphics& g, juce::Rectangle<int> area, bool hover)
        {
            // The voice mode when it isn't the usual Poly, so Mono or Legato
            // shows without opening the settings.
            const auto* modeValue = processorRef.apvts.getRawParameterValue ("voice_mode");
            const auto mode = modeValue != nullptr ? juce::roundToInt (modeValue->load()) : 0;
            const juce::String label (mode == 1 ? "MONO" : (mode == 2 ? "LEGATO" : "VOICES"));
            const auto font = juce::Font (readoutFont());
            const auto activeVoices = getVoicesText().upToFirstOccurrenceOf ("/", false, false).getIntValue();
            const auto maxVoices = getVoicesText().fromFirstOccurrenceOf ("/", false, false).getIntValue();
            const juce::String active (activeVoices), maxText (" / " + juce::String (maxVoices));
            const auto labelWidth = juce::GlyphArrangement::getStringWidthInt (font, label + " ");
            const auto activeWidth = juce::GlyphArrangement::getStringWidthInt (font, active);
            const auto maxWidth = juce::GlyphArrangement::getStringWidthInt (font, maxText);
            auto at = area.withTrimmedLeft (juce::jmax (0, area.getWidth() - labelWidth - activeWidth - maxWidth));
            g.setFont (readoutFont());
            g.setColour (hover ? IlanaTheme::Ui::text : IlanaTheme::Ui::text3);
            g.drawText (label, at.removeFromLeft (labelWidth), juce::Justification::centredLeft);
            g.setColour (IlanaTheme::Ui::text);
            g.drawText (active, at.removeFromLeft (activeWidth), juce::Justification::centredLeft);
            g.setColour (IlanaTheme::Ui::text);
            g.drawText (maxText, at, juce::Justification::centredLeft);

            if (hover)
                g.fillRect (area.getX() + juce::jmax (0, area.getWidth() - labelWidth - activeWidth - maxWidth), area.getBottom() - 3,
                            labelWidth + activeWidth + maxWidth - 2, 1);
        };
    }

    // TUNING while a Scala scale (or an MTS-ESP master) retunes the notes: a
    // small accent pill with the scale's name in the free space of the header.
    tuningArea.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    tuningArea.onClick = [this] { showSettingsMenu (true); };
    tuningArea.onPaint = [this] (juce::Graphics& g, juce::Rectangle<int> area, bool hover)
    {
        const auto pill = area.toFloat().reduced (0.5f);
        g.setColour (IlanaTheme::accent().withAlpha (hover ? 0.22f : 0.12f));
        g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
        g.setColour (IlanaTheme::accent().withAlpha (0.7f));
        g.drawRoundedRectangle (pill, pill.getHeight() * 0.5f, 1.0f);
        g.setColour (hover ? IlanaTheme::Ui::text : IlanaTheme::accent().interpolatedWith (IlanaTheme::Ui::text, 0.4f));
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.drawText (tuningText.fromFirstOccurrenceOf ("TUNING: ", false, false), area.reduced (8, 0), juce::Justification::centred, true);
    };
    content.addChildComponent (tuningArea);

    content.addAndMakeVisible (*masterKnob);

    presetDisplay.onClick = [this] { togglePresetPanel(); };
    prevButton.setFlat (true);
    nextButton.setFlat (true);

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
    saveButton.setTooltip ("Save preset  (Ctrl+S)\nSaves over your preset; a factory or new preset asks for a name.");
    moreButton.onClick = [this] { showPresetMenu(); };
    undoButton.onClick = [this] { undoOrRedo (false); };
    redoButton.onClick = [this] { undoOrRedo (true); };
    historyButton.onClick = [this] { showHistoryMenu(); };
    abButton.setButtonText ("A");
    abButton.setTooltip ("Compare A | B\nFlip between two versions of the patch to compare them; the lit letter is the one playing.");
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
    // chip here. The bar is built from the MOD page's pools (review 8, V8-3):
    // an LFO or envelope chip shows while its card is in the pool (added, or
    // in use), with the rest one click away behind "+". The performance
    // sources always show. Names are the sources' one name (ModNames), never
    // a code: a full region folds its last chips instead (layoutChips).
    // Three groups, as the MOD page's rows file them (I7-16): LFOs, then
    // envelopes, then the performance sources and the vector's X / Y. The
    // Operator Env's LFO and pitch envelope lead their groups while an
    // oscillator plays the Operator Env (they are the voice's own
    // modulators then: S8-2, I8-9) and are absent otherwise. The patch MSEG
    // (one drawn-shape system: LFO SHAPE > MSEG) shows only while an older
    // patch routes it.
    struct ChipSpec
    {
        Mod::Source source;
        int revealKind = -1, revealIndex = 0;
    };
    std::vector<ChipSpec> chipSpecs;
    chipSpecs.push_back ({ Mod::Source::OpLfo, -2 });
    for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
        chipSpecs.push_back ({ Mod::lfoSourceFor (lfo), (int) IlanaSynthAudioProcessor::Module::Lfo, lfo });
    chipSpecs.push_back ({ Mod::Source::Mseg, -4 });
    chipSpecs.push_back ({ Mod::Source::OpPitchEnv, -2 });
    for (int env = 0; env < 16; ++env)
        chipSpecs.push_back ({ ModNames::envelopeSourceFor (env), (int) IlanaSynthAudioProcessor::Module::Envelope, env });
    for (const auto source : { Mod::Source::Velocity, Mod::Source::KeyTrack, Mod::Source::ModWheel, Mod::Source::Aftertouch,
                               Mod::Source::Random, Mod::Source::InputEnv })
        chipSpecs.push_back ({ source });
    // The vector pad's position, while the vector is on (kind -3; I7-22).
    chipSpecs.push_back ({ Mod::Source::VectorX, -3 });
    chipSpecs.push_back ({ Mod::Source::VectorY, -3 });

    for (const auto& spec : chipSpecs)
    {
        // The input's envelope only exists in ilanaSynth FX.
        if (spec.source == Mod::Source::InputEnv && ! IlanaSynthAudioProcessor::isEffectBuild)
            continue;
        // One chip per source, whatever else lists it.
        if (std::any_of (chips.begin(), chips.end(), [&spec] (const auto& c) { return c->getSourceIndex() == (int) spec.source; }))
            continue;

        auto chip = makeSourceChip ((int) spec.source);
        content.addAndMakeVisible (*chip);
        chip->setVisible (spec.revealKind == -1);
        const auto group = ModNames::groupOf ((int) spec.source);
        chipGroup.push_back (group == ModNames::SourceGroup::lfo ? 0 : group == ModNames::SourceGroup::envelope ? 1 : 2);
        chips.push_back (std::move (chip));
        chipReveal.push_back ({ spec.revealKind, spec.revealIndex });
        chipWanted.push_back (spec.revealKind == -1);
        chipFolded.push_back (false);
    }

    const char* const groupNames[] { "LFOs", "Envelopes", "More sources" };
    for (size_t group = 0; group < groupChips.size(); ++group)
    {
        groupChips[group] = std::make_unique<ModSourceGroupChip> (groupNames[group]);
        groupChips[group]->onOpen = [this] (ModSourceGroupChip& groupChip)
        {
            if (chipTray.isOpenFor (groupChip))
                chipTray.close();
            else
                chipTray.openFor (groupChip);
        };
        content.addChildComponent (*groupChips[group]);
    }
    chipTray.makeChip = [this] (int source) { return makeSourceChip (source); };
    content.addChildComponent (chipTray);

    moreChipsButton.setButtonText ("+");
    moreChipsButton.setTooltip ("Add an LFO or envelope: it joins the pool on MOD > ENV / LFO and gets a chip here to drag.");
    moreChipsButton.onClick = [this] { showChipPicker(); };
    content.addChildComponent (moreChipsButton);

    {
        // The presets the browser lists (a voice two cartridges carry counts once).
        auto listed = processorRef.getFactoryPresetNames().size();
        for (auto original : processorRef.getPresetRepeats())
            listed -= original >= 0 ? 1 : 0;
        tutorial.setPresetCount (listed);
    }
    content.addAndMakeVisible (tutorial);
    content.addChildComponent (confirmOverlay);
    content.addChildComponent (saveOverlay);
    saveOverlay.setSettings (settings.get());
    saveOverlay.onSaved = [this] (const juce::File&)
    {
        presetSaved (false);

        if (auto then = std::exchange (afterSave, nullptr))
            then();
    };
    saveOverlay.onCancelled = [this] { afterSave = nullptr; };

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

    applyUiZoom ((float) settings->getDoubleValue ("uiZoom", 1.0));

    if (presetDocked && settings->getBoolValue ("presetBrowserDockOpen", false))
        setPresetDockShown (true);
    displayScaleApplied = settings->containsKey ("uiZoom") && ! juce::approximatelyEqual (uiZoom, 1.0f);

    headerScope.setVisible (settings->getBoolValue ("headerWaveform", true));
    setGpuRendering (settings->getBoolValue ("gpuRendering", true)
                     && juce::SystemStats::getEnvironmentVariable ("ILANA_NO_GPU", "").isEmpty());

    tabs.getTabbedButtonBar().addChangeListener (this);
}

void IlanaSynthAudioProcessorEditor::setGpuRendering (bool shouldUseGpu)
{
   #if ILANA_GPU_UI
    if (shouldUseGpu == (openGL != nullptr))
        return;

    if (shouldUseGpu)
    {
        // The whole component tree is painted into the context on its render
        // thread. Frames come from repaints (the frame clock, ChangeGate), not
        // a continuous loop, so an idle UI stays idle. The context attaches
        // once the editor is on screen: off-screen snapshots still draw in
        // software.
        openGL = std::make_unique<juce::OpenGLContext>();
        openGL->setComponentPaintingEnabled (true);
        openGL->setContinuousRepainting (false);
        openGL->attachTo (*this);
    }
    else
    {
        openGL->detach();
        openGL.reset();
        repaint();
    }
   #else
    juce::ignoreUnused (shouldUseGpu);
   #endif
}

bool IlanaSynthAudioProcessorEditor::isGpuRendering() const
{
   #if ILANA_GPU_UI
    return openGL != nullptr && openGL->isAttached();
   #else
    return false;
   #endif
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
    setGpuRendering (false); // before any child goes: the render thread paints them
    FmOperatorInfo::hooks().openOperator = nullptr;
    FmOperatorInfo::hooks().openPitchAndLfo = nullptr;
    ModNames::openMatrixRow() = nullptr;
    tableBrowserSettings() = nullptr;
    closeWavetableEditor();
    tabs.getTabbedButtonBar().removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void IlanaSynthAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // Picking a tab shows its page: the docked browser over it steps aside.
    if (presetDockShown)
        setPresetDockShown (false);

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

// Pool chips follow the pools (a card added or in use), read against last
// tick's usage; the row re-lays out only when that changes. Routing a chip
// never moves it: only whether it shows does (V8-3).
void IlanaSynthAudioProcessorEditor::updateChipVisibility()
{
    auto changed = false;

    for (size_t i = 0; i < chips.size() && i < chipReveal.size(); ++i)
    {
        const auto [kind, index] = chipReveal[i];

        if (kind == -1)
            continue;

        const auto source = juce::jlimit (0, (int) Mod::Source::Count - 1, chips[i]->getSourceIndex());
        const auto lfoKind = (int) IlanaSynthAudioProcessor::Module::Lfo;
        const auto shown = kind == -2   ? FmOperatorInfo::anyOperatorEnv (processorRef)
                         : kind == -3   ? processorRef.apvts.getRawParameterValue ("vec_on")->load() > 0.5f
                         : kind == -4   ? msegModuleInUse (processorRef)
                         : kind == lfoKind ? processorRef.isLfoShown (index)
                         // AMP ENV leaves the bar while the voice plays the
                         // Operator Env and nothing uses it (UI review 9, S9-17).
                         : index == 0      ? envelopeInUse (processorRef, 0) || ! FmOperatorInfo::anyOperatorEnv (processorRef)
                                           : envelopeShown (processorRef, index);
        const auto wanted = shown || usedModSources[(size_t) source];

        if (chipWanted[i] != wanted)
        {
            chipWanted[i] = wanted;
            changed = true;
        }
    }

    // An LFO chip widens for its "B" while its shape has a second output.
    std::vector<bool> seconds (chips.size());
    for (size_t i = 0; i < chips.size(); ++i)
        seconds[i] = chips[i]->hasSecondOutput != nullptr && chips[i]->hasSecondOutput();
    if (seconds != chipSecondOutputs)
    {
        chipSecondOutputs = seconds;
        changed = true;
    }

    if (changed)
        resized();
}

// A source chip for the bar or a group's tray. It glows with its source
// while that source modulates something (macros, the wheel and pressure
// always).
std::unique_ptr<ModSourceChip> IlanaSynthAudioProcessorEditor::makeSourceChip (int sourceIndex)
{
    auto chip = std::make_unique<ModSourceChip> (ModNames::sourceUpper (sourceIndex), sourceIndex);
    chip->valueProvider = [this, source = (Mod::Source) sourceIndex]
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
    // A simulated LFO shape's second output gets a "B" on its chip.
    if (const auto lfo = Mod::lfoIndexFor ((Mod::Source) sourceIndex); lfo >= 0)
    {
        chip->secondIndex = (int) Mod::lfoBSourceFor (lfo);
        chip->hasSecondOutput = [this, lfo]
        {
            const auto* shape = processorRef.apvts.getRawParameterValue ("lfo" + juce::String (lfo + 1) + "_shape");
            return shape != nullptr && LfoSimShapes::isSim ((int) shape->load());
        };
    }
    return chip;
}

// The bar, in three regions (LFOs | envelopes | performance) and the "+"
// picker at the right end, each region at a fixed place and width, so a
// chip sits where it sat on the last patch (review 8, V8-3, S8-2, I8-9).
// The regions are sized from a fixed set of names (the widest likely set
// of each: the Operator Env's chips with three LFOs or envelopes, and the
// four main performance sources), the same at every zoom (V7-39). In a
// region the chips keep the pool's order at their full names' width, the
// spare shared out; when they don't fit, the last fold into a "+N" chip at
// the region's end (its tray holds them). Folding depends only on width
// and order, never on routing: sources the patch plays (the Operator Env's
// LFO and pitch envelope, the vector's X / Y while it is on) fold last.
// Nothing is ever shortened to a code.
void IlanaSynthAudioProcessorEditor::layoutChips (juce::Rectangle<int> row)
{
    if (chips.empty())
        return;

    std::array<std::vector<size_t>, 3> members;

    for (size_t i = 0; i < chips.size(); ++i)
        if (chipWanted[i])
            members[(size_t) chipGroup[i]].push_back (i);

    // A pool chip left to add (the Operator Env's come and go by themselves).
    auto picker = false;
    for (size_t i = 0; i < chips.size() && i < chipReveal.size(); ++i)
        picker = picker || (chipReveal[i].first >= 0 && ! chipWanted[i]);

    for (size_t i = 0; i < chips.size(); ++i)
        chipFolded[i] = false;

    auto operatorChips = false; // the Operator Env's own chips only take room while the patch shows them
    for (size_t i = 0; i < chips.size(); ++i)
        operatorChips = operatorChips || (chipWanted[i] && (chips[i]->getSourceName() == "OP LFO" || chips[i]->getSourceName() == "OP PITCH"));

    const auto regions = chipRegions (row, operatorChips);
    chipSeparatorX = { juce::roundToInt (regions[0].getRight() + (regions[1].getX() - regions[0].getRight()) * 0.5f),
                       juce::roundToInt (regions[1].getRight() + (regions[2].getX() - regions[1].getRight()) * 0.5f) };

    for (size_t g = 0; g < 3; ++g)
    {
        const auto& list = members[g];
        const auto played = [&] (size_t index)
        {
            const auto kind = chipReveal[index].first;
            return kind == -2 || kind == -3 || chips[index]->getSourceIndex() == (int) Mod::Source::InputEnv;
        };
        const auto widthOf = [&]
        {
            auto sum = 0.0f;
            auto folded = 0;
            for (const auto index : list)
            {
                if (chipFolded[index])
                    ++folded;
                else
                    sum += chips[index]->getLayoutWidth();
            }
            return sum + (folded > 0 ? ModSourceGroupChip::layoutWidthFor ({}, folded) : 0.0f);
        };

        // From the end: the sources the patch doesn't play first.
        for (auto pass = 0; pass < 2 && widthOf() > regions[g].getWidth(); ++pass)
            for (auto k = (int) list.size() - 1; k >= 0 && widthOf() > regions[g].getWidth(); --k)
                if (pass == 1 || ! played (list[(size_t) k]))
                    chipFolded[list[(size_t) k]] = true;

        std::vector<int> folded;
        auto items = 0;
        for (const auto index : list)
        {
            items += chipFolded[index] ? 0 : 1;
            if (chipFolded[index])
                folded.push_back (chips[index]->getSourceIndex());
        }
        items += folded.empty() ? 0 : 1;

        const auto used = widthOf();
        const auto spare = 0.0f; // the dock's chips keep their natural width, left-aligned
        const auto squeeze = juce::jmin (1.0f, regions[g].getWidth() / juce::jmax (1.0f, used));
        auto x = regions[g].getX();
        const auto place = [&] (juce::Component& component, float natural)
        {
            const auto width = (natural + spare) * squeeze;
            component.setBounds (juce::Rectangle<float> (x, (float) row.getY(), width, (float) row.getHeight()).toNearestInt().reduced (2, 1));
            x += width;
        };

        for (const auto index : list)
            if (! chipFolded[index])
                place (*chips[index], chips[index]->getLayoutWidth());

        groupChips[g]->setSources (folded);
        groupChips[g]->setVisible (! folded.empty());

        if (! folded.empty())
            place (*groupChips[g], groupChips[g]->getLayoutWidth());
    }

    for (size_t i = 0; i < chips.size(); ++i)
        chips[i]->setVisible (chipWanted[i] && ! chipFolded[i]);

    moreChipsButton.setVisible (picker);
    moreChipsButton.setBounds (juce::Rectangle<int> (row.getRight() - chipPickerWidth, row.getY(), chipPickerWidth, row.getHeight()).reduced (2, 1));
}

// The three regions' places in the bar, fixed for a given width (and the
// same at every zoom): each in proportion to its planned names.
std::array<juce::Rectangle<float>, 3> IlanaSynthAudioProcessorEditor::chipRegions (juce::Rectangle<int> row, bool withOperatorChips)
{
    static const std::array<std::vector<const char*>, 3> planned { {
        { "OP LFO", "LFO 1", "LFO 2", "LFO 3" },
        { "OP PITCH", "AMP ENV", "FILT ENV", "FILT 2 ENV" },
        { "VELOCITY", "KEY TRACK", "MOD WHEEL", "PRESSURE", "RANDOM" },
    } };
    constexpr float groupGap = 14.0f; // room for the thin rule between groups

    std::array<float, 3> wanted {};
    auto total = 0.0f;
    for (size_t g = 0; g < 3; ++g)
    {
        for (const auto* name : planned[g])
            if (withOperatorChips || std::string_view (name).substr (0, 3) != "OP ")
                wanted[g] += ModSourceChip::layoutWidthFor (name);
        if (g == 0)
            wanted[g] += ModSourceChip::secondOutputRoom; // room for an LFO's OUT 2
        total += wanted[g];
    }

    const auto usable = (float) row.getWidth() - (float) chipPickerWidth - groupGap * 3.0f;
    std::array<juce::Rectangle<float>, 3> result;
    auto x = (float) row.getX();
    for (size_t g = 0; g < 3; ++g)
    {
        const auto width = usable * wanted[g] / total;
        result[g] = { x, (float) row.getY(), width, (float) row.getHeight() };
        x += width + groupGap;
    }
    return result;
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
        menu.addItem ((int) i + 1, ModNames::source (chips[i]->getSourceIndex()));
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

// The macros the strip shows: the first four, any named, moved or routed,
// and the ones added with "+" (S10-10: half of the eight were placeholders).
void IlanaSynthAudioProcessorEditor::updateMacroStrip()
{
    auto wanted = 4 + addedMacros;

    for (int macro = 0; macro < (int) macroKnobs.size(); ++macro)
    {
        const auto* value = processorRef.apvts.getRawParameterValue ("macro" + juce::String (macro + 1));
        const auto named = processorRef.getMacroName (macro) != "Macro " + juce::String (macro + 1);
        const auto routed = usedModSources[(size_t) juce::jlimit (0, (int) Mod::Source::Count - 1, (int) Mod::macroSourceFor (macro))];

        if (named || routed || (value != nullptr && value->load() > 0.001f))
            wanted = juce::jmax (wanted, macro + 1);
    }

    wanted = juce::jlimit (4, (int) macroKnobs.size(), wanted);

    if (wanted != shownMacros)
    {
        shownMacros = wanted;
        resized();
    }

    auto anyAssigned = false;
    for (int macro = 0; macro < shownMacros; ++macro)
        anyAssigned = anyAssigned || macroKnobs[(size_t) macro]->isAssigned();

    for (int macro = 0; macro < (int) macroKnobs.size(); ++macro)
    {
        macroKnobs[(size_t) macro]->setVisible (macro < shownMacros);
        macroKnobs[(size_t) macro]->setQuietAssign (! anyAssigned && macro > 0);
    }
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

    updateMasterTooltip();

    // "Edited" marker: cheap checksum of every parameter against the one
    // taken when the preset was loaded.
    if (presetLoadFlash <= 0.01f)
        presetDisplay.setPreset (presetDisplayName, shownCategory, isFavourite (shownPresetName), isPatchEdited());

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

    // The voice dots' tooltip counts them (each dot is a voice).
    {
        const auto* voicesValue = processorRef.apvts.getRawParameterValue ("poly_voices");
        const auto maxVoices = juce::jlimit (1, 32, voicesValue != nullptr ? juce::roundToInt (voicesValue->load()) : 32);
        const auto text = "Voices: " + juce::String (juce::jmin (maxVoices, processorRef.getActiveVoiceCount())) + " of "
                          + juce::String (maxVoices) + " playing\nNotes sounding / the most that can sound. "
                          + "Click for the voice settings (OSC > VOICE).";

        if (voicesArea.getTooltip() != text)
            voicesArea.setTooltip (text);
    }

    updateMacroStrip();
    updateTuningIndicator();

    if (transitionPage == nullptr)
    {
        updateUndoButtons();
        bpmArea.repaint();
        cpuArea.repaint();
        voicesArea.repaint();
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
    // The header (52 px).
    g.setColour (IlanaTheme::Ui::header);
    g.fillRect (juce::Rectangle<int> (0, 0, designWidth, headerHeight));
    g.setColour (IlanaTheme::Ui::line.darker (0.3f));
    g.fillRect (juce::Rectangle<int> (0, headerHeight - 1, designWidth, 1));

    // The preset display's field (its rim, flash and text are the display's
    // own, over the live wave that sits on this field).
    g.setColour (IlanaTheme::Ui::panel);
    g.fillRoundedRectangle (presetDisplay.getBounds().toFloat().reduced (0.5f), 10.0f);

    // The rule between the edit and tool groups.
    g.setColour (IlanaTheme::Ui::line.brighter (0.25f));
    for (const auto x : headerSeparatorX)
        if (x > 0)
            g.fillRect (juce::Rectangle<int> (x, 15, 1, 22));

    // The tab bar's ground and its hairline, under the tabs.
    g.setColour (IlanaTheme::Ui::bg);
    g.fillRect (juce::Rectangle<int> (0, headerHeight, designWidth, tabBarHeight));
    g.setColour (IlanaTheme::Ui::line.darker (0.3f));
    g.fillRect (juce::Rectangle<int> (0, headerHeight + tabBarHeight - 1, designWidth, 1));

    // The dock (78 px): sources, then macros, master and the meter.
    if (! dockBounds.isEmpty())
    {
        g.setColour (IlanaTheme::Ui::header);
        g.fillRect (dockBounds);
        g.setColour (IlanaTheme::Ui::line.darker (0.3f));
        g.fillRect (dockBounds.withHeight (1));

        g.setColour (IlanaTheme::Ui::line);
        for (const auto x : chipSeparatorX)
            if (x > 0)
                g.fillRect (juce::Rectangle<int> (x, chipRowBounds.getCentreY() - 8, 1, 16));
    }
}

// The status line's TUNING tag: the scale's name while tuning is on, or the
// MTS-ESP master's; hidden on 12-TET.
juce::String IlanaSynthAudioProcessorEditor::getVoicesText() const
{
    const auto* voicesValue = processorRef.apvts.getRawParameterValue ("poly_voices");
    const auto maxVoices = juce::jlimit (1, 32, voicesValue != nullptr ? juce::roundToInt (voicesValue->load()) : 32);
    return juce::String (juce::jmin (maxVoices, processorRef.getActiveVoiceCount())) + "/" + juce::String (maxVoices);
}

void IlanaSynthAudioProcessorEditor::updateTuningIndicator()
{
    juce::String text;

    if (processorRef.isMtsEspConnected())
        text = "TUNING: " + processorRef.getMtsEspScaleName() + " (MTS-ESP)";
    else if (processorRef.getTuningState().hasScale() && processorRef.apvts.getRawParameterValue ("tuning_on")->load() > 0.5f)
        text = "TUNING: " + processorRef.getTuningState().getDescription();

    if (text == tuningText)
        return;

    tuningText = text;
    tuningArea.setVisible (text.isNotEmpty());
    tuningArea.setTooltip (text.isNotEmpty() ? "Tuning\nThe notes play " + text.fromFirstOccurrenceOf ("TUNING: ", false, false)
                                                   + ", not 12-TET. Click to load another scale or switch back."
                                             : juce::String());
    tuningArea.repaint();
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

    auto area = content.getLocalBounds();

    // ---- Header (52 px): the mark and name, the preset display with its
    // prev / next segments and the live scope, star, SAVE and the menu; at
    // the right undo, redo, history, A/B and the dice and settings keys.
    logo.setBounds (14, 9, 160, 34);
    logo.version = juce::String ("v") + appVersion;

    constexpr int key = 30, step = 10, rowY = 11;
    int x = designWidth - 14;
    const auto placeFromRight = [&] (juce::Component& c, int width)
    {
        x -= width;
        c.setBounds (x, rowY, width, key);
        x -= step;
    };
    placeFromRight (settingsButton, key);
    placeFromRight (diceButton, key);
    headerSeparatorX[1] = x - 1; // the rule between A/B and the tools: 10 px, 1 px, 10 px
    x -= 11;
    placeFromRight (abButton, 54);
    placeFromRight (historyButton, key);
    placeFromRight (redoButton, key);
    placeFromRight (undoButton, key);
    headerSeparatorX[0] = 0;

    presetDisplay.setBounds (180, 7, 330, 38);
    prevButton.setBounds (180, 7, PresetDisplay::arrowWidth, 38);
    nextButton.setBounds (180 + 330 - PresetDisplay::arrowWidth, 7, PresetDisplay::arrowWidth, 38);
    headerScope.setBounds (330, 13, 150, 26);
    favButton.setBounds (519, rowY, key, key);
    saveButton.setBounds (559, rowY, 75, key);
    moreButton.setBounds (644, rowY, key, key);
    // TUNING (while a scale or MTS-ESP retunes the notes) in the free space between
    // the file keys and the edit keys; its text is the scale's name.
    tuningArea.setBounds (684, rowY + 4, juce::jmax (0, undoButton.getX() - step - 684), key - 8);
    area.removeFromTop (headerHeight);

    tutorial.setBounds (content.getLocalBounds());
    confirmOverlay.setBounds (content.getLocalBounds());
    saveOverlay.setBounds (content.getLocalBounds());

    // ---- Bottom: the optional keyboard, then the 78 px dock.
    if (keyboard != nullptr)
    {
        keyboard->setVisible (keyboardVisible);

        if (keyboardVisible)
            keyboard->setBounds (area.removeFromBottom (32).reduced (14, 2));
    }

    dockBounds = area.removeFromBottom (dockHeight);

    // The macro row: the macros and + MACRO at the left, then the hover line
    // in the free middle, MASTER and the meter at the right. The window's
    // resize grip owns the corner: the meter keeps clear of it.
    auto strip = dockBounds.withTrimmedTop (1 + 6 + 22 + 6).withTrimmedBottom (6).reduced (14, 0);
    strip.removeFromRight (12); // (the meter ends 26 px from the edge: the resize grip's corner)
    outputMeter->setBounds (strip.removeFromRight (110).withSizeKeepingCentre (110, 30));
    strip.removeFromRight (8);
    masterKnob->setBounds (strip.removeFromRight (100));
    strip.removeFromRight (10);

    // The "+ MACRO" tile sits right after the last macro (review 11, S11-9).
    const auto plusWidth = shownMacros < (int) macroKnobs.size() ? macroPlusButton.getIdealWidth() + 4 : 0;
    const auto macroWidth = juce::jlimit (72, 92, (strip.getWidth() - plusWidth) / juce::jmax (1, shownMacros));

    for (int macro = 0; macro < (int) macroKnobs.size(); ++macro)
        macroKnobs[(size_t) macro]->setBounds (strip.getX() + macro * macroWidth, strip.getY(), macroWidth - 4, strip.getHeight());

    const auto macrosEnd = strip.getX() + shownMacros * macroWidth;
    macroPlusButton.setBounds (macrosEnd, strip.getCentreY() - 12, macroPlusButton.getIdealWidth(), 24);
    macroPlusButton.setVisible (plusWidth > 0);

    // The hover line: in what is left of the macro row, between the last
    // macro (or + MACRO) and MASTER, so it never sits over a card or a chip.
    {
        const auto from = (plusWidth > 0 ? macroPlusButton.getRight() : macrosEnd) + 12;
        const auto to = masterKnob->getX() - 8;
        infoStrip.setBounds (juce::Rectangle<int> (from, strip.getCentreY() - infoLineHeight / 2, juce::jmax (0, to - from), infoLineHeight));
        infoStrip.setVisible (infoStrip.getWidth() >= 140);
    }

    infoStrip.setQuietArea (dockBounds.withTop (dockBounds.getY() - 2));
    infoStrip.toFront (false);

    // The source chips: one row, 22 px chips.
    chipRowBounds = juce::Rectangle<int> (14, dockBounds.getY() + 5, designWidth - 28, 24);
    layoutChips (chipRowBounds);

    tabs.setTabBarDepth (tabBarHeight);
    tabs.setBounds (area);
    layoutTabRow();

    if (scopePanel != nullptr)
    {
        auto* panel = static_cast<ScopePanel*> (scopePanel.get());
        // A running fade-in would snap the panel back to its old bounds.
        juce::Desktop::getInstance().getAnimator().cancelAnimation (scopePanel.get(), false);
        scopePanel->setAlpha (1.0f);
        scopePanel->setBounds (panel->placeIn (pageArea()));
    }

    if (presetDockShown && presetPanel != nullptr)
    {
        presetPanel->setBounds (pageArea().reduced (2, 0).withTrimmedBottom (2));
        presetPanel->toFront (false);
    }

    for (auto* overlay : std::initializer_list<juce::Component*> { &tutorial, &confirmOverlay, &saveOverlay })
        if (overlay->isVisible())
            overlay->toFront (false);
}

// The pages' area: under the tab row, above the chips.
juce::Rectangle<int> IlanaSynthAudioProcessorEditor::pageArea() const
{
    // (The overlays that dock over the pages keep a 14 px margin at each side.)
    return tabs.getBounds().withTrimmedTop (tabs.getTabBarDepth()).reduced (14, 0);
}

SectionPage* IlanaSynthAudioProcessorEditor::currentSection() const
{
    const auto index = tabs.getCurrentTabIndex();
    return index >= 0 && index < (int) sections.size() ? sections[(size_t) index] : nullptr;
}

void IlanaSynthAudioProcessorEditor::showOperatorEnvelope (int op)
{
    if (editOperator != nullptr)
        editOperator (op);
}

void IlanaSynthAudioProcessorEditor::showPage (const juce::String& id)
{
    if (id == "SCOPE")
    {
        setScopeOpen (true);
        return;
    }

    // STEPS & MSEG went into ENV / LFO (UI review 6, V5-7): its old id
    // opens there.
    if (id == "STEPS")
    {
        showPage ("ENV/LFO");
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

    // A docked scope covers the page: the tab and the page switch it covers
    // step back, so SCOPE alone reads as the page shown (UI review 9, V9-14).
    const auto coversPage = [this] (bool open)
    {
        const auto alpha = open && static_cast<ScopePanel*> (scopePanel.get())->isDocked() ? 0.45f : 1.0f;
        tabs.getTabbedButtonBar().setAlpha (alpha);
        for (auto* section : sections)
            section->switcher.setAlpha (alpha);
    };

    if (shouldBeOpen == scopePanel->isVisible())
    {
        coversPage (shouldBeOpen);
        return;
    }

    coversPage (shouldBeOpen);

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

// The current tab's page switch follows the tabs, left-aligned beside the
// tab it belongs to (review 8, V8-36: at the right end, beside SCOPE, it read
// as a tool, not as the page's own pages); the scope, keys and help buttons
// sit at the right end of the tab row.
void IlanaSynthAudioProcessorEditor::layoutTabRow()
{
    const auto bar = tabs.getBounds().withHeight (tabBarHeight);
    constexpr int gap = 6, buttonHeight = 24;
    const auto y = bar.getY() + (tabBarHeight - buttonHeight) / 2;

    // From the right: ?, KEYBOARD, SCOPE, a rule, then CPU, VOICES and BPM
    // (moved here from the header).
    auto x = bar.getRight() - 14;
    const auto place = [&] (juce::Component& component, int width, int after)
    {
        x -= width;
        component.setBounds (x, y, width, buttonHeight);
        x -= after;
    };
    place (helpButton, 26, gap);
    place (keysButton, keysButton.getIdealWidth(), gap);
    place (scopeButton, scopeButton.getIdealWidth(), 14);

    const auto textWidth = [] (const juce::String& text)
    {
        return juce::GlyphArrangement::getStringWidthInt (juce::Font (IlanaTheme::font (13.2f, true, true).withKerningFactor (0.04f)), text) + 2;
    };
    constexpr int readoutGap = 12;
    place (cpuArea, textWidth ("CPU 100%"), readoutGap);
    place (voicesArea, textWidth ("VOICES 32 / 32"), readoutGap);
    place (bpmArea, textWidth ("BPM 120.0"), 0);
    statusStartX = x;

    auto& tabBar = tabs.getTabbedButtonBar();
    auto* lastTab = tabBar.getNumTabs() > 0 ? tabBar.getTabButton (tabBar.getNumTabs() - 1) : nullptr;
    const auto tabsRight = lastTab != nullptr ? tabs.getX() + tabBar.getX() + lastTab->getRight() : bar.getX();
    auto row = juce::Rectangle<int> (tabsRight + 12, bar.getY() + 3, juce::jmax (0, statusStartX - 12 - (tabsRight + 12)), tabBarHeight - 7);

    for (auto* section : sections)
    {
        const auto current = section == currentSection() && section->getNumPages() > 1;
        section->switcher.setVisible (current);

        if (current)
            section->switcher.setBounds (row.removeFromLeft (juce::jmin (row.getWidth(), section->switcher.getIdealWidth())));
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
// The question and the header's EDITED badge read the same isPatchEdited,
// and the badge is brought up to date before the question shows (it
// otherwise follows on the editor's timer). "Save and load" saves first:
// in place for your own preset, through SAVE AS otherwise (the load
// waits for it, and a cancelled SAVE AS loads nothing).
void IlanaSynthAudioProcessorEditor::confirmReplacingPatch (const juce::String& replacement, const juce::String& confirmText,
                                                            std::function<void (bool confirmed)> then)
{
    if (! isPatchEdited() || ! asksBeforeReplacingEdits())
    {
        then (true);
        return;
    }

    presetDisplay.setPreset (presetDisplayName, shownCategory, isFavourite (shownPresetName), true);

    const auto current = processorRef.getCurrentPresetName();
    ConfirmOverlay::Choices choices;
    choices.confirmText = confirmText;
    // A factory preset has no file to save over: the first step is a name
    // dialog, and the button says so (I13-16).
    const auto asksName = ! PresetFiles::loadedUserPreset (processorRef).existsAsFile();
    choices.alternativeText = juce::String (asksName ? "Save as... and " : "Save and ") + (confirmText.startsWith ("Roll") ? "roll" : "load");
    choices.onAlternative = [safeThis = juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> (this), then]
    {
        if (safeThis == nullptr)
            return;

        if (PresetFiles::loadedUserPreset (safeThis->processorRef).existsAsFile())
        {
            safeThis->savePreset();

            if (! safeThis->isPatchEdited())
                then (true);

            return;
        }

        safeThis->afterSave = [then] { then (true); };
        safeThis->savePresetAs();
    };

    confirmOverlay.ask ("Replace your edits?",
                        "'" + (current.isNotEmpty() ? (presetDisplayName.isNotEmpty() ? presetDisplayName : current) : juce::String ("Init"))
                            + "' has changes that aren't saved. " + replacement + " replaces them.",
                        choices,
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

    const auto bank = processorRef.getAllPresetBanks()[index];
    // While the browser is open, one "Load anyway" covers the session.
    const auto browsing = presetPanel != nullptr && (presetPanel->isOpen() || presetDockShown);
    const auto asking = browsing && ! browsingAccepted && isPatchEdited() && asksBeforeReplacingEdits();
    const auto load = [safeThis = juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> (this), index, name = names[index], then] (bool confirmed)
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
    };

    if (browsing && browsingAccepted)
    {
        load (true);
        return;
    }

    confirmReplacingPatch ("Loading '" + (bank.isNotEmpty() ? Presets::dx7DisplayName (names[index]) : names[index]) + "'", "Load anyway",
                           [safeThis = juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> (this), load, asking] (bool confirmed)
                           {
                               if (safeThis != nullptr && confirmed && asking)
                                   safeThis->browsingAccepted = true;

                               load (confirmed);
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

    // A DX7 voice shows its name in Title Case and its bank by the category
    // ("KEYS  ·  DX7 ROM1A"); the saved name is unchanged.
    const auto bank = juce::isPositiveAndBelow (currentPresetIndex, names.size()) ? processorRef.getAllPresetBanks()[currentPresetIndex]
                                                                                   : juce::String();
    presetDisplayName = bank.isNotEmpty() ? Presets::dx7DisplayName (shownPresetName) : shownPresetName;

    if (bank.isNotEmpty())
        shownCategory << juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  DX7 ")) << bank;

    if (nameChanged)
        adoptLoadedFingerprint();

    presetDisplay.setPreset (presetDisplayName, shownCategory, isFavourite (shownPresetName), isPatchEdited());
    favButton.setToggleState (isFavourite (shownPresetName), juce::dontSendNotification);
    favButton.setIconColour (isFavourite (shownPresetName) ? std::optional<juce::Colour> (juce::Colour (0xffffd447))
                                                           : std::nullopt);
    abButton.setButtonText (showingA ? "A" : "B");
    abButton.setToggleState (! showingA, juce::dontSendNotification);

    for (auto& knob : macroKnobs)
        knob->refreshName();

    // A new patch starts from the macros it uses.
    if (nameChanged)
        addedMacros = 0;
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
    abButton.setButtonText (showingA ? "A" : "B");
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

// The preset's own level (output_trim, review 7): each factory preset sets
// it so that MASTER reads 0 dB, and it is saved with a patch, so two patches
// at MASTER 0 dB can sit 15 dB apart. MASTER's hover names it, and the
// preset menu resets it.
juce::String IlanaSynthAudioProcessorEditor::presetLevelText() const
{
    const auto* trim = processorRef.apvts.getRawParameterValue ("output_trim");
    const auto db = trim != nullptr ? trim->load() : 0.0f;
    return (db > 0.05f ? "+" : "") + juce::String (std::abs (db) < 0.05f ? 0.0f : db, 1) + " dB";
}

void IlanaSynthAudioProcessorEditor::updateMasterTooltip()
{
    const auto text = presetLevelText();

    if (masterKnob == nullptr || text == shownPresetLevel)
        return;

    auto& knob = masterKnob->getKnob();

    if (masterBaseTooltip.isEmpty())
        masterBaseTooltip = knob.getTooltip();

    shownPresetLevel = text;
    const auto tooltip = masterBaseTooltip + "  This preset's own level: " + text
                         + " (set when it loads, saved with it; the preset menu resets it).";
    knob.setTooltip (tooltip);

    for (auto* child : knob.getChildren())
        if (auto* slider = dynamic_cast<juce::Slider*> (child))
            slider->setTooltip (tooltip);
}

void IlanaSynthAudioProcessorEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "New patch");
    menu.addSeparator();
    menu.addItem (2, "Save preset", true);
    menu.addItem (7, "Save preset as...");
    menu.addItem (5, "Export preset file...");
    menu.addItem (3, "Load preset file...");
    menu.addItem (4, "Open user preset folder");
    menu.addSeparator();
    menu.addItem (8, "Reset preset level  (" + presetLevelText() + ")", presetLevelText() != "0.0 dB");
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
                                case 6: safeThis->chooseSyxFile(); break;
                                case 8: safeThis->resetPresetLevel(); break;
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

void IlanaSynthAudioProcessorEditor::resetPresetLevel()
{
    if (auto* trim = processorRef.apvts.getParameter ("output_trim"))
    {
        processorRef.beginEdit ("Reset preset level");
        trim->setValueNotifyingHost (trim->getDefaultValue());
        processorRef.endEdit();
        updateMasterTooltip();
    }
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

void IlanaSynthAudioProcessorEditor::showSettingsMenu (bool tuningOnly)
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

    juce::PopupMenu menu;
    // One voice panel: OSC > VOICE (review 10, S10-1); the header's VOICES
    // opens it too.
    menu.addItem (960, "Voice settings (OSC > VOICE)...");
    menu.addSeparator();
    menu.addSubMenu ("Skin", skins);
    menu.addSubMenu ("Interface size", sizes);
    menu.addSubMenu ("Engine quality", quality);
    menu.addSubMenu ("Oversampling", oversampling);
    menu.addSubMenu (tuningOn ? "Tuning: " + tuningState.getDescription() : juce::String ("Tuning"), tuning, true, nullptr, tuningOn);
    menu.addItem (300, "Show keyboard", true, keyboardVisible);
    // The live waveform under the preset name moves all the time beside
    // the most-read text: it can be turned off (review 8, S8-41).
    menu.addItem (320, "Waveform under the preset name", true, headerScope.isVisible());
   #if ILANA_GPU_UI
    menu.addItem (310, "GPU rendering (OpenGL)", true, openGL != nullptr);
   #endif
    menu.addItem (500, "MPE mode (per-note pitch, pressure and slide)", true,
                  processorRef.apvts.getRawParameterValue ("mpe_mode")->load() > 0.5f);
    menu.addItem (410, "Ask before replacing an edited patch", true, asksBeforeReplacingEdits());
    menu.addSeparator();
    menu.addItem (400, "Show welcome tour");

    // The header's tuning indicator opens just the tuning items.
    if (tuningOnly)
    {
        menu = juce::PopupMenu();
        menu.addSectionHeader ("TUNING");
        for (juce::PopupMenu::MenuItemIterator items (tuning); items.next();)
            menu.addItem (items.getItem());
    }

    auto* target = tuningOnly ? static_cast<juce::Component*> (&tuningArea) : &settingsButton;
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target),
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
                            else if (result == 320)
                            {
                                const auto show = ! safeThis->headerScope.isVisible();
                                safeThis->headerScope.setVisible (show);
                                safeThis->settings->setValue ("headerWaveform", show);
                                safeThis->settings->saveIfNeeded();
                            }
                           #if ILANA_GPU_UI
                            else if (result == 310)
                            {
                                const auto useGpu = safeThis->openGL == nullptr;
                                safeThis->setGpuRendering (useGpu);
                                safeThis->settings->setValue ("gpuRendering", useGpu);
                                safeThis->settings->saveIfNeeded();
                            }
                           #endif
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
                            else if (result == 960)
                                safeThis->showVoicePanel();
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
    presetPanel->onImportSyx = [this] { chooseSyxFile(); };
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
    presetPanel->onClosed = [this] { browsingAccepted = false; };

    // Hidden until open() fades it in.
    content.addChildComponent (*presetPanel);
}

// Docked, the browser covers the page area (the tab row, the chips, the
// macros and the keyboard stay): the window keeps its size (review 6; it
// used to grow by a column).
void IlanaSynthAudioProcessorEditor::setPresetDockShown (bool shown)
{
    createPresetPanel();

    if (shown == presetDockShown)
        return;

    presetDockShown = shown;
    browsingAccepted = false;
    presetPanel->setVisible (false);
    presetPanel->setDocked (shown);

    if (shown)
    {
        presetPanel->setBounds (pageArea().reduced (2, 0).withTrimmedBottom (2));
        presetPanel->open();
    }

    if (settings != nullptr)
    {
        settings->setValue ("presetBrowserDockOpen", presetDockShown);
        settings->saveIfNeeded();
    }

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
        // Wide enough for the details card beside the list (S10-5).
        const auto width = 920;
        const auto x = juce::jlimit (10, designWidth - 10 - width, presetDisplay.getX() - 40);
        presetPanel->setBounds (x, presetDisplay.getBottom() + 6, width, 540);
        presetPanel->setScrimArea (content.getLocalBounds());
    }

    if (presetPanel->isOpen())
        presetPanel->close();
    else
        presetPanel->open();
}

// IMPORT .SYX (the browser) and the preset menu's import: each voice of the
// bank becomes a user preset under DX7/<bank>/.
void IlanaSynthAudioProcessorEditor::chooseSyxFile()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Import a DX7 or Dexed bank",
                                                       juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.syx");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safeThis = juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> (this)] (const juce::FileChooser& chooser)
                              {
                                  const auto file = chooser.getResult();

                                  if (safeThis != nullptr && file.existsAsFile())
                                      safeThis->importSyxFile (file);
                              });
}

// Imports, then says so in the theme; "Show in browser" opens the browser
// on the new bank.
void IlanaSynthAudioProcessorEditor::importSyxFile (const juce::File& file)
{
    juce::String message;
    const auto count = processorRef.importDx7File (file, message);
    const auto bank = file.getFileNameWithoutExtension();

    if (presetPanel != nullptr)
        presetPanel->refresh();

    ConfirmOverlay::Choices choices;
    choices.confirmText = count > 0 ? "Show in browser" : "OK";
    choices.cancelText = count > 0 ? "Close" : "Cancel";
    choices.offerDontAsk = false;

    confirmOverlay.ask (count > 0 ? "Imported " + bank : juce::String ("Nothing imported"),
                        count > 0 ? juce::String (count) + (count == 1 ? " DX7 voice is" : " DX7 voices are")
                                        + " now your presets, filed by sound with the bank " + bank + " under the browser's DX7 chip."
                                  : message,
                        choices,
                        [safeThis = juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> (this), count, bank] (bool show, bool)
                        {
                            if (safeThis == nullptr || ! show || count == 0)
                                return;

                            safeThis->createPresetPanel();

                            if (! safeThis->presetPanel->isOpen())
                                safeThis->togglePresetPanel();

                            safeThis->presetPanel->showDx7Bank (bank);
                        });
}

bool IlanaSynthAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& path : files)
        if (path.endsWithIgnoreCase (".syx"))
            return true;

    return false;
}

void IlanaSynthAudioProcessorEditor::filesDropped (const juce::StringArray& files, int, int)
{
    for (const auto& path : files)
        if (path.endsWithIgnoreCase (".syx"))
        {
            importSyxFile (juce::File (path));
            return; // one bank at a time: the confirm names it
        }
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

    if (presetDockShown)
    {
        setPresetDockShown (false);
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

        // Ctrl / Cmd + Shift + 1-3 pick the page inside the tab (PLAY:
        // overview, vector; OSC: oscillators, physical; MOD: env / lfo,
        // matrix), review 9, S9-21. A shifted digit arrives as the digit
        // key or as the symbol on it.
        if (modifiers.isShiftDown() && ! modifiers.isAltDown())
        {
            const auto symbol = juce::String ("!@#$%^&*(").indexOfChar (key.getTextCharacter());
            const auto digit = key.getKeyCode() >= '1' && key.getKeyCode() <= '9' ? key.getKeyCode() - '1' : symbol;

            if (auto* section = currentSection(); section != nullptr && digit >= 0 && digit < section->getNumPages())
            {
                showPage (section->getPageId (digit));
                return true;
            }
        }

        // Ctrl / Cmd + 1-7 pick a tab (bare digits are left to the host:
        // DAWs play notes or run actions with them).
        if (const auto digit = key.getKeyCode(); digit >= '1' && digit <= '9' && ! modifiers.isAltDown() && ! modifiers.isShiftDown())
        {
            const auto index = digit - '1';

            if (index < tabs.getNumTabs())
            {
                tabs.setCurrentTabIndex (index);
                return true;
            }
        }

        return false;
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
