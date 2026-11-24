// Process CPU time in seconds (std::clock is wall time on Windows).
double cpuSeconds()
{
   #if JUCE_WINDOWS
    FILETIME created, exited, kernel, user;
    GetProcessTimes (GetCurrentProcess(), &created, &exited, &kernel, &user);
    const auto toSeconds = [] (const FILETIME& t) { return (double) (((unsigned long long) t.dwHighDateTime << 32) | t.dwLowDateTime) * 1.0e-7; };
    return toSeconds (kernel) + toSeconds (user);
   #else
    return (double) std::clock() / CLOCKS_PER_SEC;
   #endif
}

// An opaque window holding the editor, as hosts do: a non-opaque editor put
// on the desktop directly gets a layered window that redraws in full.
struct HostWindow : juce::Component
{
    explicit HostWindow (juce::Component& editor)
    {
        setOpaque (true);
        addAndMakeVisible (editor);
        setSize (editor.getWidth(), editor.getHeight());
#if JUCE_LINUX
        // (A bare Xvfb has no window manager: the title bar's property names a missing atom.)
        if (auto* setHandler = (void* (*) (int (*) (void*, void*))) dlsym (RTLD_DEFAULT, "XSetErrorHandler"))
            setHandler ([] (void*, void*) { return 0; });
#endif
        addToDesktop (juce::ComponentPeer::windowHasTitleBar);
        setVisible (true);
        toFront (true); // a background window may get fewer vblanks
    }
    ~HostWindow() override { removeFromDesktop(); }
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colours::black); }
};

// Records what gets repainted: a transparent component over the editor
// sees the clip of every paint.
struct RegionProbe : juce::Component
{
    RegionProbe() { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics& g) override
    {
        ++frames;
        // Which 40-px cells the clip region touches (the clip's bounds
        // alone can be the whole window).
        for (int y = 0; y < getHeight(); y += 40)
            for (int x = 0; x < getWidth(); x += 40)
                if (g.clipRegionIntersects ({ x, y, 40, 40 }))
                    ++counts[juce::Rectangle<int> (x, y, 40, 40).toString()];
    }
    int frames = 0;
    std::map<juce::String, int> counts;
};

int runIdleCpu()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1060, 720);
    HostWindow window (*editor);
    settle (800);

    if (auto* tutorial = findChild<TutorialOverlay> (*editor))
        tutorial->setVisible (false);

    // Listing regions needs exact clips: Direct2D's clip tests are coarse.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_IDLE_REGIONS", "").isNotEmpty())
        if (auto* peer = window.getPeer())
            peer->setCurrentRenderingEngine (0);

    auto* pages = dynamic_cast<IlanaSynthAudioProcessorEditor*> (editor.get());
    auto worst = 0.0;
    auto idleFailures = 0;

    // ILANA_IDLE_HIDE=LogoComponent,ModSourceChip,... hides every component
    // of those classes (by their type name) to find what keeps repainting.
    const auto hide = juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("ILANA_IDLE_HIDE", ""), ",", "");
    if (! hide.isEmpty())
    {
        std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
        {
            for (auto* child : c.getChildren())
            {
                const juce::String type (typeid (*child).name());
                for (const auto& h : hide)
                    if (h.isNotEmpty() && type.contains (h))
                        child->setVisible (false);
                walk (*child);
            }
        };
        walk (*editor);
    }

    const auto only = juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("ILANA_FPS_PAGES", ""), ",", "");

    for (const auto& id : pages->getPageIds())
    {
        if (! only.isEmpty() && ! only.contains (id))
            continue;
        pages->showPage (id);
        // Let entrance animations finish (ILANA_IDLE_SETTLE=<ms>: the MATRIX
        // cable plays for a few seconds).
        settle (juce::jmax (id == "MATRIX" ? 8000 : 1200,
                            juce::SystemStats::getEnvironmentVariable ("ILANA_IDLE_SETTLE", "").getIntValue()));
        RegionProbe regions;
        const auto listRegions = juce::SystemStats::getEnvironmentVariable ("ILANA_IDLE_REGIONS", "").isNotEmpty();
        // (Always attached: the paints in the window are the idle check below.)
        editor->addAndMakeVisible (regions);
        regions.setBounds (editor->getLocalBounds());
        const auto start = cpuSeconds();
        settle (3000);
        const auto percent = 100.0 * (cpuSeconds() - start) / 3.0;
        worst = juce::jmax (worst, percent);
        std::cout << "idle " << id << ": " << juce::String (percent, 1) << "% of a core, " << regions.frames << " paints in 3 s" << std::endl;
        // An idle page redraws a few times a second at most (a clock
        // readout, the 2 s safety repaint). A page that keeps animating when
        // nothing sounds or moves is the bug this catches.
        if (regions.frames > 45)
        {
            std::cout << "FAIL: " << id << " keeps repainting while idle (" << regions.frames << " paints in 3 s, at most 45)" << std::endl;
            idleFailures++;
        }
        editor->removeChildComponent (&regions);
        if (listRegions)
        {
            std::vector<std::pair<int, juce::String>> sorted;
            for (auto& [rect, n] : regions.counts)
                sorted.push_back ({ n, rect });
            std::sort (sorted.rbegin(), sorted.rend());
            std::cout << "  " << regions.frames << " paints in 3 s" << std::endl;
            for (auto* top : editor->getChildren())
                for (auto* child : top->getChildren())
                    if (child->isVisible())
                        std::cout << "    child " << typeid (*child).name() << " " << child->getBounds().toString() << std::endl;
            for (size_t i = 0; i < std::min<size_t> (40, sorted.size()); ++i)
            {
                const auto r = juce::Rectangle<int>::fromString (sorted[i].second);
                auto* at = editor->getComponentAt (r.getCentre());
                juce::String path;
                for (auto* c = at; c != nullptr && c != editor.get(); c = c->getParentComponent())
                    path = juce::String (typeid (*c).name()).fromLastOccurrenceOf (" ", false, false) + (path.isEmpty() ? "" : " > " + path);
                std::cout << "  " << sorted[i].first << "x " << sorted[i].second << "  " << path.substring (juce::jmax (0, path.length() - 160)) << std::endl;
            }
        }
    }

    pages->setScopeOpen (true);
    settle (1200);
    const auto start = cpuSeconds();
    settle (3000);
    std::cout << "idle SCOPE panel: " << juce::String (100.0 * (cpuSeconds() - start) / 3.0, 1) << "% of a core" << std::endl;
    std::cout << "worst page: " << juce::String (worst, 1) << "%" << std::endl;
    window.removeChildComponent (editor.get());
    return idleFailures == 0 ? 0 : 1;
}

// Frames the editor actually paints while it animates: a page switch, then a
// held note (scopes, meters and modulation move). A transparent component
// over the whole editor is painted whenever anything under it is, so its
// paints are the frames drawn. 60 fps means gaps of at most ~17 ms.
struct FrameProbe : juce::Component
{
    FrameProbe() { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics&) override { times.push_back (juce::Time::getMillisecondCounterHiRes()); }
    std::vector<double> times;
};

juce::String frameStats (const std::vector<double>& times, double windowMs)
{
    if (times.size() < 2)
        return juce::String ((int) times.size()) + " frames";
    std::vector<double> gaps;
    for (size_t i = 1; i < times.size(); ++i)
        gaps.push_back (times[i] - times[i - 1]);
    std::sort (gaps.begin(), gaps.end());
    const auto p95 = gaps[(size_t) ((double) (gaps.size() - 1) * 0.95)];
    return juce::String (1000.0 * (double) times.size() / windowMs, 0) + " fps, median gap "
         + juce::String (gaps[gaps.size() / 2], 1) + " ms, p95 " + juce::String (p95, 1) + " ms, worst " + juce::String (gaps.back(), 1) + " ms";
}

// --foldrepro <dir>: PLAY's module block folded and unfolded with motion on, in a real window, then the screen
// (what the window really shows, `import`) and a fresh paint of the editor saved side by side. A difference is
// paint the slide left behind (ilana 2026-11-05: stale card frames after unfolding).
static int runFoldRepro (const juce::File& outDir)
{
    outDir.createDirectory();
    IlanaAnim::fullMotion() = true;
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1060, 720);
#if JUCE_LINUX
    if (auto* setHandler = (void* (*) (int (*) (void*, void*))) dlsym (RTLD_DEFAULT, "XSetErrorHandler"))
        setHandler ([] (void*, void*) { return 0; });
#endif
    editor->addToDesktop (0);
    editor->setTopLeftPosition (0, 0);
    editor->setVisible (true);
    settle (800);
    if (auto* tutorial = findChild<TutorialOverlay> (*editor))
        tutorial->setVisible (false);
    settle (300);
    std::vector<juce::Button*> buttons;
    findAll<juce::Button> (*editor, buttons);
    juce::Button* tab = nullptr;
    juce::Button* bar = nullptr;
    for (auto* button : buttons)
    {
        if (button->getName() == "Fold modules")
            tab = button;
        if (button->getName() == "Unfold modules")
            bar = button;
    }
    if (tab == nullptr || bar == nullptr)
        return 1;
    tab->triggerClick();
    settle (900);
    bar->triggerClick();
    settle (900);
    std::system (("import -window root " + outDir.getChildFile ("screen.png").getFullPathName().quoted()).toRawUTF8());
    if (auto stream = outDir.getChildFile ("fresh.png").createOutputStream())
    {
        stream->setPosition (0);
        stream->truncate();
        juce::PNGImageFormat().writeImageToStream (editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f), *stream);
    }
    // (Leave it open: settings remember the open block.)
    return 0;
}

// --fxsizes DIR: the FX rack with one to six effects (pictures fx-N.png),
// and each card's top printed, to check every card keeps one size.
static int runFxSizes (const juce::File& outDir)
{
    outDir.createDirectory();
    UiCtx ctx;
    ctx.pages->showPage ("FX");
    settle (600);
    const int rack[] { 7, 13, 2, 4, 20, 27, 5 };
    for (int count = 1; count <= 7; ++count)
    {
        for (int slot = 1; slot <= IlanaSynthAudioProcessor::numFxSlots; ++slot)
            ctx.processor.assignFxSlot (slot, slot <= count ? rack[slot - 1] : 0);
        settle (900);
        std::vector<FxTypeButton*> found;
        findAll<FxTypeButton> (*ctx.editor, found);
        juce::String tops;
        for (auto* button : found)
            if (visibleInTree (button))
                tops << juce::String (ctx.editor->getLocalPoint (button, juce::Point<int>()).y) << " ";
        std::cout << "FXSIZES " << count << ": " << tops << std::endl;
        save (*ctx.editor, outDir.getChildFile ("fx-" + juce::String (count) + ".png"));
    }
    ctx.closeEditor();
    return 0;
}

int runFps()
{
    IlanaSynthAudioProcessor processor;
    const auto blockSize = juce::jmax (32, juce::SystemStats::getEnvironmentVariable ("ILANA_FPS_BLOCK", "256").getIntValue());
    processor.prepareToPlay (48000.0, blockSize);
    // ILANA_FPS_PRESET=<factory preset> and ILANA_FPS_SET="id=value;..."
    // (real values) set the patch the chord plays.
    if (const auto preset = juce::SystemStats::getEnvironmentVariable ("ILANA_FPS_PRESET", ""); preset.isNotEmpty())
        processor.loadFactoryPreset (processor.getFactoryPresetNames().indexOf (preset));
    for (const auto& pair : juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("ILANA_FPS_SET", ""), ";", ""))
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (processor.apvts.getParameter (pair.upToFirstOccurrenceOf ("=", false, false))))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (pair.fromFirstOccurrenceOf ("=", false, false).getFloatValue()));
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1060, 720);
    HostWindow window (*editor);
    settle (800);

    if (auto* peer = window.getPeer())
    {
        const auto engines = peer->getAvailableRenderingEngines();
        std::cout << "renderer: " << engines[peer->getCurrentRenderingEngine()] << " (of " << engines.joinIntoString (", ") << ")" << std::endl;
    }

    if (auto* ilanaEditor = dynamic_cast<IlanaSynthAudioProcessorEditor*> (editor.get()))
        std::cout << "GPU (OpenGL) UI: " << (ilanaEditor->isGpuRendering() ? "on" : "off") << std::endl;

    if (auto* tutorial = findChild<TutorialOverlay> (*editor))
        tutorial->setVisible (false);

    // The display's refresh rate.
    {
        int vblanks = 0;
        juce::VBlankAttachment counter (editor.get(), [&vblanks] { ++vblanks; });
        settle (1000);
        std::cout << "display: " << vblanks << " vblanks per second" << std::endl;
    }

    FrameProbe probe;
    editor->addAndMakeVisible (probe);
    int vblanks = 0;
    juce::VBlankAttachment vblankCounter (editor.get(), [&vblanks] { ++vblanks; });
    probe.setBounds (editor->getLocalBounds());

    // Audio in real time on another thread, a chord held while "playing".
    std::atomic<bool> running { true }, playing { false };
    std::thread audio ([&]
    {
        juce::AudioBuffer<float> buffer (2, blockSize);
        auto wasPlaying = false;
        auto next = juce::Time::getMillisecondCounterHiRes();
        while (running)
        {
            juce::MidiBuffer midi;
            const auto now = playing.load();
            if (now != wasPlaying)
                for (auto note : { 48, 55, 60, 64 })
                    midi.addEvent (now ? juce::MidiMessage::noteOn (1, note, 0.8f) : juce::MidiMessage::noteOff (1, note), 0);
            wasPlaying = now;
            buffer.clear();
            processor.processBlock (buffer, midi);
            next += (double) blockSize / 48.0;
            const auto wait = next - juce::Time::getMillisecondCounterHiRes();
            if (wait > 0.0)
                std::this_thread::sleep_for (std::chrono::microseconds ((int) (wait * 1000.0)));
        }
    });

    auto* pages = dynamic_cast<IlanaSynthAudioProcessorEditor*> (editor.get());
    const auto filter = juce::SystemStats::getEnvironmentVariable ("ILANA_FPS_PAGES", "");

    for (const auto& id : pages->getPageIds())
    {
        if (filter.isNotEmpty() && ! juce::StringArray::fromTokens (filter, ",", "").contains (id))
            continue;
        playing = false;
        settle (400);
        probe.times.clear();
        const auto cpuStart = cpuSeconds();
        pages->showPage (id);
        probe.toFront (false);
        settle (400);
        const auto switchCpu = 100.0 * (cpuSeconds() - cpuStart) / 0.4;
        const auto switchStats = frameStats (probe.times, 400.0);

        playing = true;
        settle (300);
        probe.times.clear();
        vblanks = 0;
        IlanaAnim::paintStats().clear();
        IlanaAnim::paintStatsOn() = true;
        const auto playStart = cpuSeconds();
        settle (1500);
        std::cout << id << "\n  switch:  " << switchStats << " (" << juce::String (switchCpu, 0) << "% cpu)"
                  << "\n  playing: " << frameStats (probe.times, 1500.0) << " (" << juce::String (100.0 * (cpuSeconds() - playStart) / 1.5, 0) << "% cpu, "
                  << juce::String (vblanks / 1.5, 0) << " vblanks/s)" << std::endl;
        IlanaAnim::paintStatsOn() = false;
        for (const auto& entry : IlanaAnim::paintStats())
            std::cout << "    paints/s " << entry.first << ": " << juce::String ((double) entry.second / 1.5, 0) << std::endl;
    }

    running = false;
    audio.join();
    editor->removeChildComponent (&probe);
    window.removeChildComponent (editor.get());
    return 0;
}

// What each animation costs per frame, at the window scale ilana's host uses
// (ILANA_ANIM_SCALE, default 1.75): frames drawn, the paint time of each
// frame (from a probe under every child to one over them), the message
// thread's CPU and the share of the window each frame redraws. A smooth
// animation draws ~60 frames a second with each paint well under 16 ms.
struct PaintStart : juce::Component
{
    PaintStart() { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics&) override { start = juce::Time::getMillisecondCounterHiRes(); }
    double start = 0.0;
};

struct PaintEnd : juce::Component
{
    explicit PaintEnd (PaintStart& s) : startProbe (s) { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics& g) override
    {
        const auto now = juce::Time::getMillisecondCounterHiRes();
        times.push_back (now);
        durations.push_back (now - startProbe.start);
        int cells = 0, all = 0;
        for (int y = 0; y < getHeight(); y += 20)
            for (int x = 0; x < getWidth(); x += 20, ++all)
                cells += g.clipRegionIntersects ({ x, y, 20, 20 }) ? 1 : 0;
        area.push_back ((double) cells / (double) juce::jmax (1, all));
    }
    void clear() { times.clear(); durations.clear(); area.clear(); }
    PaintStart& startProbe;
    std::vector<double> times, durations, area;
};

// Which components paint, how often and for how long (own paint() only).
struct PaintWho : juce::ComponentListener
{
    void attach (juce::Component& c)
    {
        c.addComponentListener (this);
        for (auto* child : c.getChildren())
            attach (*child);
    }
    void detach (juce::Component& c)
    {
        c.removeComponentListener (this);
        for (auto* child : c.getChildren())
            detach (*child);
    }
    void componentPainted (juce::Component& c, const juce::ComponentPaintDiagnostics& d) override
    {
        auto name = juce::String (typeid (c).name());
       #if ! JUCE_WINDOWS
        int status = 0;
        if (auto* plain = abi::__cxa_demangle (typeid (c).name(), nullptr, nullptr, &status))
        {
            name = plain;
            std::free (plain);
        }
       #endif
        auto& entry = stats[name.replace ("(anonymous namespace)::", "")];
        entry.first++;
        entry.second += d.paintDuration.get<std::chrono::duration<double, std::milli>>() + d.paintOverChildrenDuration.get<std::chrono::duration<double, std::milli>>();
    }
    void print (double wallMs)
    {
        std::vector<std::pair<double, juce::String>> sorted;
        for (auto& [name, e] : stats)
            sorted.push_back ({ e.second, name });
        std::sort (sorted.rbegin(), sorted.rend());
        for (size_t i = 0; i < std::min<size_t> (12, sorted.size()); ++i)
            std::cout << "      " << juce::String (sorted[i].first * 1000.0 / wallMs, 1) << " ms/s in " << stats[sorted[i].second].first << " paints: " << sorted[i].second << std::endl;
        stats.clear();
    }
    std::map<juce::String, std::pair<int, double>> stats;
};

int runAnimCost()
{
    const auto scale = (float) juce::jmax (1.0, juce::SystemStats::getEnvironmentVariable ("ILANA_ANIM_SCALE", "1.75").getDoubleValue());
    juce::Desktop::getInstance().setGlobalScaleFactor (scale);
    IlanaAnim::fullMotion() = true;
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);
    if (const auto preset = juce::SystemStats::getEnvironmentVariable ("ILANA_FPS_PRESET", ""); preset.isNotEmpty())
        processor.loadFactoryPreset (processor.getFactoryPresetNames().indexOf (preset));
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1060, 720);
    HostWindow window (*editor);
    // (Background work at start, the wavetable builds, done first.)
    settle (juce::jmax (1000, juce::SystemStats::getEnvironmentVariable ("ILANA_ANIM_SETTLE", "8000").getIntValue()));
    if (auto* tutorial = findChild<TutorialOverlay> (*editor))
        tutorial->setVisible (false);
    if (auto* peer = window.getPeer())
        std::cout << "window " << peer->getBounds().toString() << " at scale " << scale << std::endl;

    PaintStart startProbe;
    PaintEnd endProbe (startProbe);
    editor->addAndMakeVisible (startProbe, 0);
    editor->addAndMakeVisible (endProbe);
    startProbe.setBounds (editor->getLocalBounds());
    endProbe.setBounds (editor->getLocalBounds());

    auto* pages = dynamic_cast<IlanaSynthAudioProcessorEditor*> (editor.get());
    // (Windows draws with Direct2D, not OpenGL: on Linux the software renderer is the nearer stand-in.)
    pages->setGpuRendering (juce::SystemStats::getEnvironmentVariable ("ILANA_ANIM_GPU", "").isNotEmpty());
    settle (300);
    const auto only = juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("ILANA_ANIM_ONLY", ""), ",", "");

    const auto measure = [&] (const juce::String& name, int milliseconds, std::function<void()> start)
    {
        if (! only.isEmpty() && ! only.contains (name))
            return;
        settle (300);
        if (juce::SystemStats::getEnvironmentVariable ("ILANA_ANIM_NOALPHA", "").isNotEmpty())
        {
            std::function<void (juce::Component&)> opaque = [&] (juce::Component& c) { for (auto* child : c.getChildren()) { if (child != &endProbe && child->getAlpha() < 1.0f) child->setAlpha (1.0f); opaque (*child); } };
            opaque (*editor);
        }
        endProbe.toFront (false);
        endProbe.clear();
        const auto cpuStart = cpuSeconds();
        const auto wallStart = juce::Time::getMillisecondCounterHiRes();
        IlanaAnim::paintStats().clear();
        IlanaAnim::paintStatsOn() = true;
        static PaintWho who; // (outlives the components it may still be attached to)
        const auto listWho = juce::SystemStats::getEnvironmentVariable ("ILANA_ANIM_WHO", "").isNotEmpty();
        if (listWho)
            who.attach (*editor);
        start();
        settle (milliseconds);
        const auto wall = juce::Time::getMillisecondCounterHiRes() - wallStart;
        const auto cpu = 100.0 * (cpuSeconds() - cpuStart) / (wall * 0.001);
        auto d = endProbe.durations;
        std::sort (d.begin(), d.end());
        const auto at = [&d] (double q) { return d.empty() ? 0.0 : d[(size_t) ((double) (d.size() - 1) * q)]; };
        double meanArea = 0.0;
        for (auto a : endProbe.area)
            meanArea += a;
        meanArea /= (double) juce::jmax ((size_t) 1, endProbe.area.size());
        std::cout << name << ": " << frameStats (endProbe.times, wall) << "\n    paint median " << juce::String (at (0.5), 1) << " ms, p95 "
                  << juce::String (at (0.95), 1) << " ms, max " << juce::String (at (1.0), 1) << " ms; redraws " << juce::String (100.0 * meanArea, 0)
                  << "% of the window; " << juce::String (cpu, 0) << "% cpu" << std::endl;
        IlanaAnim::paintStatsOn() = false;
        for (const auto& entry : IlanaAnim::paintStats())
            std::cout << "    paints/s " << entry.first << ": " << juce::String ((double) entry.second * 1000.0 / wall, 0) << std::endl;
        if (listWho)
        {
            who.detach (*editor);
            who.print (wall);
        }
    };

    std::vector<juce::Button*> buttons;
    const auto button = [&] (const juce::String& name) -> juce::Button*
    {
        buttons.clear();
        findAll<juce::Button> (*editor, buttons);
        for (auto* b : buttons)
            if (b->getName() == name && b->isShowing())
                return b;
        return nullptr;
    };

    // Components drawn through a transparency layer (alpha under 1): each paint
    // draws them into an image first, then blends it in.
    const auto listLayers = [&] (const juce::String& where)
    {
        std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
        {
            for (auto* child : c.getChildren())
            {
                if (! child->isVisible())
                    continue;
                if (child->getAlpha() < 1.0f && child->getWidth() * child->getHeight() > 400)
                    std::cout << "  layer on " << where << ": " << typeid (*child).name() << " alpha " << child->getAlpha() << " " << child->getBounds().toString() << std::endl;
                walk (*child);
            }
        };
        walk (*editor);
    };
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_ANIM_LAYERS", "").isNotEmpty())
        for (const auto& id : pages->getPageIds())
        {
            pages->showPage (id);
            settle (500);
            listLayers (id);
        }

    // ILANA_ANIM_SHOTS=<dir>: pictures of the fold partway (every 60 ms), to check the slide by eye.
    if (const auto shots = juce::SystemStats::getEnvironmentVariable ("ILANA_ANIM_SHOTS", ""); shots.isNotEmpty())
    {
        const auto dir = juce::File::getCurrentWorkingDirectory().getChildFile (shots);
        dir.createDirectory();
        pages->showPage ("PLAY");
        settle (800);
        IlanaAnim::motionClockRate() = 0.1; // (ten times slower, so the pictures land partway)
        for (const auto* name : { "Fold modules", "Unfold modules" })
        {
            if (auto* b = button (name))
                b->triggerClick();
            for (int i = 0; i < 7; ++i)
            {
                settle (i == 0 ? 300 : 500);
                const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
                juce::FileOutputStream out (dir.getChildFile (juce::String (name).replace (" ", "-") + "-" + juce::String (i) + ".png"));
                out.setPosition (0);
                out.truncate();
                juce::PNGImageFormat().writeImageToStream (image, out);
            }
            settle (3500);
        }
        IlanaAnim::motionClockRate() = 1.0;
    }

    pages->showPage ("PLAY");
    settle (600);
    measure ("idle PLAY", 1000, [] {});
    for (int round = 0; round < 2; ++round)
    {
        measure ("fold", 600, [&] { if (auto* b = button ("Fold modules")) b->triggerClick(); else std::cout << "(no fold tab)" << std::endl; });
        measure ("unfold", 600, [&] { if (auto* b = button ("Unfold modules")) b->triggerClick(); else std::cout << "(no unfold bar)" << std::endl; });
    }

    // A wheel flick over the oscillator cards (they glide to a stop).
    const auto wheel = [&] (float deltaY)
    {
        std::vector<juce::Viewport*> views;
        findAll<juce::Viewport> (*editor, views);
        for (auto* view : views)
        {
            if (! view->isShowing() || view->getViewedComponent() == nullptr || view->getViewedComponent()->getHeight() <= view->getHeight())
                continue;
            juce::MouseWheelDetails details {};
            details.deltaY = deltaY;
            const auto centre = view->getLocalBounds().getCentre().toFloat();
            const juce::MouseEvent e (juce::Desktop::getInstance().getMainMouseSource(), centre, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                      view, view, juce::Time::getCurrentTime(), centre, juce::Time::getCurrentTime(), 1, false);
            view->mouseWheelMove (e, details);
            return;
        }
        std::cout << "(nothing scrolls here)" << std::endl;
    };
    measure ("scroll PLAY oscillators", 700, [&] { wheel (-0.5f); });
    measure ("scroll back", 700, [&] { wheel (0.5f); });

    for (const auto& id : pages->getPageIds())
        measure ("switch to " + id, 400, [&] { pages->showPage (id); });

    for (const auto& id : pages->getPageIds())
    {
        if (! only.isEmpty() && ! only.contains ("idle " + id))
            continue;
        pages->showPage (id);
        settle (1500);
        measure ("idle " + id, 2000, [] {});
    }

    pages->showPage ("MAIN");
    settle (500);
    measure ("scroll OSC", 700, [&] { wheel (-0.5f); });

    window.removeChildComponent (editor.get());
    return 0;
}

// Times what a host does when it opens a saved project: construct, prepare,
// restore the state, open the editor, and restore again with it open.
static int runLoadTime()
{
    const auto now = [] { return juce::Time::getMillisecondCounterHiRes(); };
    const auto pump = []
    {
        for (int i = 0; i < 20; ++i)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (5);
    };

    auto start = now();
    auto processor = std::make_unique<IlanaSynthAudioProcessor>();
    std::cout << "construct:        " << juce::String (now() - start, 0) << " ms" << std::endl;

    start = now();
    processor->prepareToPlay (48000.0, 512);
    std::cout << "prepareToPlay:    " << juce::String (now() - start, 0) << " ms" << std::endl;

    for (const auto* name : { "Swarm", "Init" })
    {
        const auto index = processor->getFactoryPresetNames().indexOf (name);
        start = now();
        processor->loadFactoryPreset (index);
        std::cout << "load preset " << name << ": " << juce::String (now() - start, 0) << " ms" << std::endl;
    }

    processor->loadFactoryPreset (processor->getFactoryPresetNames().indexOf ("Swarm"));
    juce::MemoryBlock state;
    start = now();
    processor->getStateInformation (state);
    std::cout << "getState:         " << juce::String (now() - start, 0) << " ms (" << (int) state.getSize() << " bytes)" << std::endl;

    start = now();
    processor->setStateInformation (state.getData(), (int) state.getSize());
    std::cout << "setState:         " << juce::String (now() - start, 0) << " ms" << std::endl;

    start = now();
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditorIfNeeded());
    std::cout << "createEditor:     " << juce::String (now() - start, 0) << " ms" << std::endl;
    start = now();
    pump();
    std::cout << "editor settle:    " << juce::String (now() - start - 100.0, 0) << " ms (beyond the 100 ms pump)" << std::endl;

    start = now();
    processor->setStateInformation (state.getData(), (int) state.getSize());
    std::cout << "setState+editor:  " << juce::String (now() - start, 0) << " ms" << std::endl;

    // What a host hears while presets are clicked through with the editor open.
    struct Counter : juce::AudioProcessorListener
    {
        int changes = 0;
        void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override { ++changes; }
        void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override {}
    } counter;
    processor->addListener (&counter);

    for (const auto* name : { "Init", "Swarm", "Rip Bass", "Grain Choir", "Scream Lead" })
    {
        counter.changes = 0;
        start = now();
        processor->loadFactoryPreset (processor->getFactoryPresetNames().indexOf (name));
        const auto loadMs = now() - start;
        start = now();
        pump();
        std::cout << "preset " << name << " with editor: " << juce::String (loadMs, 0) << " ms, settle "
                  << juce::String (now() - start - 100.0, 0) << " ms, " << counter.changes << " host notifications" << std::endl;
    }

    processor->removeListener (&counter);
    start = now();
    pump();
    std::cout << "after settle:     " << juce::String (now() - start - 100.0, 0) << " ms (beyond the 100 ms pump)" << std::endl;

    editor.reset();
    return 0;
}

// A 1-bar clip looped in a DAW: four notes held for the whole bar, the
// note-offs and the next loop's note-ons on the same sample. Prints each
// loop's render time and the live voice count.
static int runLoopTest (const juce::String& presetName, bool allNotesOffAtLoop)
{
    IlanaSynthAudioProcessor processor;
    const auto rate = 48000.0;
    const auto blockSize = 512;
    processor.prepareToPlay (rate, blockSize);
    if (juce::File::isAbsolutePath (presetName))
        processor.loadPresetFromFile (juce::File (presetName));
    else
        processor.loadFactoryPreset (processor.getFactoryPresetNames().indexOf (presetName));

    // ILANA_LOOP_SET="id=value;id=value" sets parameters (real values) after the load.
    for (const auto& pair : juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("ILANA_LOOP_SET", ""), ";", ""))
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (processor.apvts.getParameter (pair.upToFirstOccurrenceOf ("=", false, false))))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (pair.fromFirstOccurrenceOf ("=", false, false).getFloatValue()));

    const int notes[] { 48, 55, 60, 64 };
    const auto barSamples = (int) (rate * 2.0); // 120 BPM, 4/4
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::int64 position = 0;

    for (int loop = 0; loop < 12; ++loop)
    {
        const auto start = juce::Time::getMillisecondCounterHiRes();
        auto maxVoices = 0;

        for (int offset = 0; offset < barSamples; offset += blockSize)
        {
            juce::MidiBuffer midi;
            const auto n = juce::jmin (blockSize, barSamples - offset);

            if (offset == 0)
            {
                if (loop > 0)
                {
                    if (allNotesOffAtLoop)
                        midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
                    else
                        for (auto note : notes)
                            midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
                }

                for (auto note : notes)
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
            }

            buffer.setSize (2, n, false, false, true);
            buffer.clear();
            processor.processBlock (buffer, midi);
            maxVoices = juce::jmax (maxVoices, processor.getActiveVoiceCount());
            position += n;
        }

        const auto ms = juce::Time::getMillisecondCounterHiRes() - start;
        std::cout << "loop " << loop + 1 << ": " << juce::String (100.0 * ms / 2000.0, 1) << "% of real time, up to "
                  << maxVoices << " voices" << std::endl;
    }

    // Release everything and see how long the voices take to free up.
    juce::MidiBuffer offs;
    for (auto note : notes)
        offs.addEvent (juce::MidiMessage::noteOff (1, note), 0);

    buffer.setSize (2, blockSize, false, false, true);
    for (int block = 0; block < (int) (rate * 60.0) / blockSize; ++block)
    {
        buffer.clear();
        juce::MidiBuffer none;
        processor.processBlock (buffer, block == 0 ? offs : none);

        if (processor.getActiveVoiceCount() == 0)
        {
            std::cout << "all voices free " << juce::String (block * blockSize / rate, 2) << " s after the last note-off" << std::endl;
            return 0;
        }
    }

    std::cout << processor.getActiveVoiceCount() << " voices still live 60 s after the last note-off" << std::endl;
    return 0;
}

// CPU of a preset with twelve notes held (C2 to G5, struck at once, velocity
// 100): percent of real time per second of audio, for 8 s
// (ILANA_HELD_SECONDS). The preset is a
// factory name or a file; ILANA_LOOP_SET applies as in --looptest.
static int runHeldCpu (const juce::String& presetName)
{
    IlanaSynthAudioProcessor processor;
    const auto rate = 48000.0;
    const auto blockSize = 256;
    processor.prepareToPlay (rate, blockSize);
    if (juce::File::isAbsolutePath (presetName))
        processor.loadPresetFromFile (juce::File (presetName));
    else
        processor.loadFactoryPreset (processor.getFactoryPresetNames().indexOf (presetName));
    // ILANA_HELD_CABLES=1: the preset's chain played by the ROUTING cables
    // (roadmap step 16; the same sound, the cables' code path), before
    // ILANA_LOOP_SET, so that can then change cable_* masks.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_HELD_CABLES", "").isNotEmpty())
        processor.setRouteCables (processor.getChainAsCables(), true);
    for (const auto& pair : juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("ILANA_LOOP_SET", ""), ";", ""))
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (processor.apvts.getParameter (pair.upToFirstOccurrenceOf ("=", false, false))))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (pair.fromFirstOccurrenceOf ("=", false, false).getFloatValue()));
    // ILANA_HELD_PRINT="id,id,...": print those parameters first.
    for (const auto& id : juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("ILANA_HELD_PRINT", ""), ",", ""))
        if (auto* parameter = processor.apvts.getParameter (id))
            std::cout << id << " = " << parameter->getCurrentValueAsText() << std::endl;

    const int notes[] { 36, 43, 48, 52, 55, 60, 64, 67, 72, 76, 79, 84 };
    // ILANA_HELD_DUMP=file: the raw output (each block's left then right
    // floats), to compare two builds sample for sample.
    std::unique_ptr<juce::FileOutputStream> dump;
    if (const auto dumpPath = juce::SystemStats::getEnvironmentVariable ("ILANA_HELD_DUMP", ""); dumpPath.isNotEmpty())
    {
        juce::File (dumpPath).deleteFile();
        dump = std::make_unique<juce::FileOutputStream> (juce::File (dumpPath));
    }
    // ILANA_HELD_NOTES=n: only the first n of them.
    const auto heldNotes = juce::jlimit (1, 12, juce::SystemStats::getEnvironmentVariable ("ILANA_HELD_NOTES", "12").getIntValue());
    juce::AudioBuffer<float> buffer (2, blockSize);
    const auto blocksPerSecond = (int) (rate / blockSize);
    double total = 0.0, sumSquares = 0.0, worstBlock = 0.0;
    const auto seconds = juce::jmax (1, juce::SystemStats::getEnvironmentVariable ("ILANA_HELD_SECONDS", "8").getIntValue());
    const auto releaseAfter = juce::SystemStats::getEnvironmentVariable ("ILANA_HELD_RELEASE", "0").getIntValue();
    const auto arp = juce::SystemStats::getEnvironmentVariable ("ILANA_HELD_ARP", "").isNotEmpty();
    // ILANA_NO_RELEASE_SILENCE=1: released voices run their whole release.
    Voice::disableReleaseSilence = juce::SystemStats::getEnvironmentVariable ("ILANA_NO_RELEASE_SILENCE", "").isNotEmpty();
    Voice::disableSlowEnvelopes = juce::SystemStats::getEnvironmentVariable ("ILANA_NO_SLOW_ENVELOPES", "").isNotEmpty();
    IlanaSynthAudioProcessor::forceVoiceThreads = juce::SystemStats::getEnvironmentVariable ("ILANA_THREADS", "-1").getIntValue();
    IlanaSynthAudioProcessor::disableFxSleep = juce::SystemStats::getEnvironmentVariable ("ILANA_NO_FX_SLEEP", "").isNotEmpty();
    for (int second = 0; second < seconds; ++second)
    {
        const auto start = juce::Time::getMillisecondCounterHiRes();
        for (int block = 0; block < blocksPerSecond; ++block)
        {
            juce::MidiBuffer midi;
            // (ILANA_HELD_RESTRIKE=1: the chord again every 2 s, as in playing.)
            // (ILANA_HELD_ARP=1: instead of the chord, the sustain pedal down
            // and a new note every 0.1 s across four octaves, each let go
            // after 0.05 s: the pedal holds them all, as in playing a piano.)
            if (arp)
            {
                const auto blockIndex = second * blocksPerSecond + block;
                const auto every = juce::jmax (1, (int) (0.1 * rate / blockSize));
                if (blockIndex == 0)
                    midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
                if (blockIndex % every == 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 36 + (blockIndex / every * 7) % 48, (juce::uint8) 90), 0);
                if (blockIndex % every == every / 2)
                    midi.addEvent (juce::MidiMessage::noteOff (1, 36 + (blockIndex / every * 7) % 48), 0);
            }
            else
            // (ILANA_HELD_RELEASE=n: let go of the chord after n seconds.)
            if (block == 0 && second > 0 && second == releaseAfter)
                for (int n = 0; n < heldNotes; ++n)
                    midi.addEvent (juce::MidiMessage::noteOff (1, notes[n]), 0);
            if (! arp && block == 0 && (second == 0 || (second % 2 == 0 && juce::SystemStats::getEnvironmentVariable ("ILANA_HELD_RESTRIKE", "").isNotEmpty())))
                for (int n = 0; n < heldNotes; ++n)
                {
                    const auto note = notes[n];
                    if (second > 0)
                        midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
                }
            buffer.clear();
            const auto blockStart = juce::Time::getMillisecondCounterHiRes();
            processor.processBlock (buffer, midi);
            const auto blockMs = juce::Time::getMillisecondCounterHiRes() - blockStart;
            worstBlock = juce::jmax (worstBlock, blockMs);
            if (blockMs > 1000.0 * blockSize / rate && juce::SystemStats::getEnvironmentVariable ("ILANA_HELD_SPIKES", "").isNotEmpty())
                std::cout << "  block " << second * blocksPerSecond + block << ": " << juce::String (100.0 * blockMs / (1000.0 * blockSize / rate), 0) << "%" << std::endl;
            for (int i = 0; i < blockSize; ++i)
                sumSquares += (double) buffer.getSample (0, i) * buffer.getSample (0, i);
            if (dump != nullptr)
                for (int channel = 0; channel < 2; ++channel)
                    dump->write (buffer.getReadPointer (channel), (size_t) blockSize * sizeof (float));
        }
        const auto ms = juce::Time::getMillisecondCounterHiRes() - start;
        total += ms;
        std::cout << "second " << second + 1 << ": " << juce::String (100.0 * ms / (1000.0 * blocksPerSecond * blockSize / rate), 1)
                  << "% of real time, " << processor.getActiveVoiceCount() << " voices, "
                  << processor.getRenderingVoiceCount() << " rendering" << std::endl;
    }
    std::cout << "mean " << juce::String (100.0 * total / (1000.0 * seconds * blocksPerSecond * blockSize / rate), 1) << "%, rms "
              << juce::String (juce::Decibels::gainToDecibels ((float) std::sqrt (sumSquares / ((double) seconds * blocksPerSecond * blockSize)), -120.0f), 2)
              << " dB, worst block " << juce::String (100.0 * worstBlock / (1000.0 * blockSize / rate), 0) << "% of its time" << std::endl;
    return 0;
}

// CPU of every factory preset, then every preset file in a folder, with the
// twelve notes of --heldcpu held for 2 s: one CSV row each (index, name,
// percent of real time over the second second, voices, what it uses).
//   ilanaSnapshot --cpurank [folder] > cpu.csv   (ILANA_RANK_FACTORY=0: the folder only)
static int runCpuRank (const juce::File& folder)
{
    const auto rate = 48000.0;
    const auto blockSize = 256;
    const int notes[] { 36, 43, 48, 52, 55, 60, 64, 67, 72, 76, 79, 84 };
    const auto blocksPerSecond = (int) (rate / blockSize);

    juce::String lastTags;
    const auto measure = [&] (const std::function<void (IlanaSynthAudioProcessor&)>& load) -> std::pair<double, int>
    {
        auto processor = std::make_unique<IlanaSynthAudioProcessor>();
        processor->prepareToPlay (rate, blockSize);
        load (*processor);
        juce::AudioBuffer<float> buffer (2, blockSize);
        double ms = 0.0;
        for (int block = 0; block < 2 * blocksPerSecond; ++block)
        {
            juce::MidiBuffer midi;
            if (block == 0)
                for (auto note : notes)
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
            buffer.clear();
            const auto start = juce::Time::getMillisecondCounterHiRes();
            processor->processBlock (buffer, midi);
            if (block >= blocksPerSecond)
                ms += juce::Time::getMillisecondCounterHiRes() - start;
        }
        // What the patch uses, for finding what heavy presets share.
        juce::StringArray tags;
        const auto text = [&processor] (const juce::String& id)
        {
            auto* parameter = processor->apvts.getParameter (id);
            return parameter != nullptr ? parameter->getCurrentValueAsText() : juce::String();
        };
        const auto on = [&processor] (const juce::String& id)
        {
            auto* parameter = processor->apvts.getParameter (id);
            return parameter != nullptr && parameter->getValue() > 0.5f;
        };
        for (const auto& osc : { "osc1", "osc2", "sub", "osc4", "osc5", "osc6" })
            if (on (juce::String (osc) + "_on"))
                tags.add (juce::String (osc) + ":" + text (juce::String (osc) + "_mode") + "x" + text (juce::String (osc) + "_unison"));
        if (on ("west_on")) tags.add ("west");
        if (on ("res_on")) tags.add ("body:" + text ("body_type"));
        if (on ("sym_on")) tags.add ("sym");
        if (on ("oversampling")) tags.add ("oversample");
        tags.add ("q:" + text ("quality"));
        tags.add ("f1:" + text ("f1_type"));
        for (int slot = 1; slot <= 10; ++slot)
            if (auto* fx = processor->apvts.getParameter ("fx_slot" + juce::String (slot)); fx != nullptr && fx->getValue() > 0.0f)
                tags.add ("fx:" + fx->getCurrentValueAsText());
        lastTags = tags.joinIntoString (" ");
        return { 100.0 * ms / (1000.0 * blocksPerSecond * blockSize / rate), processor->getActiveVoiceCount() };
    };

    std::cout << "index,name,cpu_percent,voices,uses" << std::endl;
    const auto names = IlanaSynthAudioProcessor().getFactoryPresetNames();
    const auto factory = juce::SystemStats::getEnvironmentVariable ("ILANA_RANK_FACTORY", "1") != "0";
    for (int i = 0; factory && i < names.size(); ++i)
    {
        const auto [cpu, voices] = measure ([i] (IlanaSynthAudioProcessor& p) { p.loadFactoryPreset (i); });
        std::cout << i << ",\"" << names[i] << "\"," << juce::String (cpu, 2) << "," << voices << ",\"" << lastTags << "\"" << std::endl;
    }
    if (folder.isDirectory())
        for (const auto& file : folder.findChildFiles (juce::File::findFiles, true, "*.ilanapreset"))
        {
            const auto [cpu, voices] = measure ([file] (IlanaSynthAudioProcessor& p) { p.loadPresetFromFile (file); });
            std::cout << "user,\"" << file.getFileNameWithoutExtension() << "\"," << juce::String (cpu, 2) << "," << voices << ",\"" << lastTags << "\"" << std::endl;
        }
    return 0;
}

// Loudness of each physical exciter on Init with OSC 1 switched to Physical:
// one note held 1 s, RMS and peak in dBFS. ILANA_LOOP_SET applies as above.
static int runExciterLevels()
{
    const juce::StringArray names { "Burst", "Noise", "Saw", "Pulse", "Bow", "Hammer", "Osc In", "Tine", "Reed", "Piano", "Feedback" };
    const auto rate = 48000.0;
    const auto blockSize = 512;
    // ILANA_EXCITER_VELOCITY: the note-on velocity (100 by default).
    const auto velocity = juce::jlimit (1, 127, juce::SystemStats::getEnvironmentVariable ("ILANA_EXCITER_VELOCITY", "100").getIntValue());

    for (const auto note : { 48, 60, 72 })
    {
        std::cout << "note " << note << ":" << std::endl;

        for (int excite = 0; excite < names.size(); ++excite)
        {
            IlanaSynthAudioProcessor processor;
            processor.prepareToPlay (rate, blockSize);
            // ILANA_EXCITER_PRESET: a factory preset by name instead of Init
            // (its osc1 exciter is switched; the mode is left as it is).
            const auto presetName = juce::SystemStats::getEnvironmentVariable ("ILANA_EXCITER_PRESET", "");
            processor.loadFactoryPreset (juce::jmax (0, processor.getFactoryPresetNames().indexOf (presetName)));
            const auto set = [&processor] (const juce::String& id, float value)
            {
                if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (processor.apvts.getParameter (id)))
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
            };
            if (presetName.isEmpty())
                set ("osc1_mode", 1.0f);
            set ("osc1_excite", (float) excite);
            for (const auto& pair : juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("ILANA_LOOP_SET", ""), ";", ""))
                set (pair.upToFirstOccurrenceOf ("=", false, false), pair.fromFirstOccurrenceOf ("=", false, false).getFloatValue());

            juce::AudioBuffer<float> buffer (2, blockSize);
            double sum = 0.0;
            float peak = 0.0f;
            int count = 0;
            // The loudest 400 ms (what a percussive sound is heard at).
            std::vector<double> squares, weighted;
            // ITU-R BS.1770 K-weighting at 48 kHz (what loudness meters hear).
            struct Biquad
            {
                double b0, b1, b2, a1, a2, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
                double process (double x)
                {
                    const auto y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
                    x2 = x1; x1 = x; y2 = y1; y1 = y;
                    return y;
                }
            };
            Biquad shelf { 1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, 0.73248077421585 };
            Biquad highPass { 1.0, -2.0, 1.0, -1.99004745483398, 0.99007225036621 };

            for (int block = 0; block < (int) rate / blockSize; ++block)
            {
                juce::MidiBuffer midi;
                if (block == 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) velocity), 0);
                buffer.clear();
                processor.processBlock (buffer, midi);

                for (int i = 0; i < blockSize; ++i)
                {
                    const auto s = buffer.getSample (0, i);
                    sum += (double) s * s;
                    squares.push_back ((double) s * s);
                    const auto k = highPass.process (shelf.process ((double) s));
                    weighted.push_back (k * k);
                    peak = juce::jmax (peak, std::abs (s));
                    ++count;
                }
            }

            const auto loudestOver = [rate] (const std::vector<double>& squares, double seconds)
            {
                const auto window = (size_t) (seconds * rate);
                double windowSum = 0.0, loudest = 0.0;
                for (size_t i = 0; i < squares.size(); ++i)
                {
                    windowSum += squares[i] - (i >= window ? squares[i - window] : 0.0);
                    if (i + 1 >= window)
                        loudest = juce::jmax (loudest, windowSum / (double) window);
                }
                return juce::Decibels::gainToDecibels ((float) std::sqrt (loudest), -120.0f);
            };

            std::cout << "  " << names[excite].paddedRight (' ', 9) << " rms " << juce::String (juce::Decibels::gainToDecibels ((float) std::sqrt (sum / count), -120.0f), 1)
                      << " dB, loudest 400 ms " << juce::String (loudestOver (squares, 0.4), 1)
                      << " dB, 50 ms " << juce::String (loudestOver (squares, 0.05), 1)
                      << " dB, K 400 ms " << juce::String (loudestOver (weighted, 0.4), 1)
                      << " dB, peak " << juce::String (juce::Decibels::gainToDecibels (peak, -120.0f), 1) << " dB" << std::endl;
        }
    }

    return 0;
}

// Loads a saved state (as a host would hand it back) and plays a note: for
// chasing a crash the state fuzz test finds. ILANA_STRIP="Samples;Wavetables"
// removes those children first.
static int runLoadState (const juce::File& file)
{
    juce::MemoryBlock data;
    file.loadFileAsData (data);

    if (const auto strip = juce::SystemStats::getEnvironmentVariable ("ILANA_STRIP", ""); strip.isNotEmpty())
        if (auto xml = juce::AudioProcessor::getXmlFromBinary (data.getData(), (int) data.getSize()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            for (const auto& name : juce::StringArray::fromTokens (strip, ";", ""))
                tree.removeChild (tree.getChildWithName (name), nullptr);
            data.reset();
            juce::AudioProcessor::copyXmlToBinary (*tree.createXml(), data);
        }

    auto target = std::make_unique<IlanaSynthAudioProcessor>();
    std::cout << "setState" << std::endl;
    target->setStateInformation (data.getData(), (int) data.getSize());
    std::cout << "flush" << std::endl;
    target->flushAsyncUpdates();
    std::cout << "play" << std::endl;
    target->prepareToPlay (48000.0, 256);
    juce::AudioBuffer<float> buffer (2, 256);
    for (int b = 0; b < 30; ++b)
    {
        buffer.clear();
        juce::MidiBuffer midi;
        if (b == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        target->processBlock (buffer, midi);
    }
    std::cout << "save" << std::endl;
    juce::MemoryBlock again;
    target->getStateInformation (again);
    target.reset();
    std::cout << "ok" << std::endl;
    return 0;
}

// A menu drawn offscreen row by row as its window draws it (border,
// section headers, separators, ticks, submenu chevrons), `lit` the row
// highlighted; several menus side by side on the page's ink.
static juce::Image renderMenuSheet (juce::LookAndFeel& lf, std::vector<std::pair<juce::PopupMenu, int>> menus, float scale, int minWidth = 0)
{
    struct Row { juce::PopupMenu::Item item; int height; };
    std::vector<std::vector<Row>> rows;
    std::vector<juce::Point<int>> sizes;
    const auto border = lf.getPopupMenuBorderSize();

    for (auto& [menu, lit] : menus)
    {
        std::vector<Row> list;
        auto width = minWidth, height = 2 * border;
        for (juce::PopupMenu::MenuItemIterator it (menu); it.next();)
        {
            const auto& item = it.getItem();
            int w = 0, h = 0;
            if (item.isSectionHeader)
                lf.getIdealPopupMenuSectionHeaderSizeWithOptions (item.text, -1, w, h, juce::PopupMenu::Options());
            else
                // Measured as JUCE's menu window measures it: the text and its hint.
                lf.getIdealPopupMenuItemSize (item.shortcutKeyDescription.isNotEmpty() ? item.text + "   " + item.shortcutKeyDescription : item.text,
                                              item.isSeparator, -1, w, h);
            list.push_back ({ item, h });
            width = juce::jmax (width, w + 2 * border);
            height += h;
        }
        rows.push_back (std::move (list));
        sizes.push_back ({ width, height });
    }

    auto total = 20, tallest = 0;
    for (const auto& size : sizes)
    {
        total += size.x + 20;
        tallest = juce::jmax (tallest, size.y);
    }

    juce::Image image (juce::Image::ARGB, juce::roundToInt ((float) total * scale), juce::roundToInt ((float) (tallest + 40) * scale), true);
    juce::Graphics g (image);
    g.addTransform (juce::AffineTransform::scale (scale));
    g.fillAll (IlanaTheme::Ui::bg);
    auto x = 20;

    for (size_t m = 0; m < rows.size(); ++m)
    {
        juce::Graphics::ScopedSaveState state (g);
        g.setOrigin (x, 20);
        g.reduceClipRegion (0, 0, sizes[m].x, sizes[m].y); // (a menu background may fill its whole clip)
        lf.drawPopupMenuBackground (g, sizes[m].x, sizes[m].y);
        auto y = border;
        for (size_t i = 0; i < rows[m].size(); ++i)
        {
            const auto& [item, h] = rows[m][i];
            const juce::Rectangle<int> row (border, y, sizes[m].x - 2 * border, h);
            if (item.isSectionHeader)
                lf.drawPopupMenuSectionHeader (g, row, item.text);
            else
                lf.drawPopupMenuItem (g, row, item.isSeparator, item.isEnabled, (int) i == menus[m].second, item.isTicked,
                                      item.subMenu != nullptr, item.text, item.shortcutKeyDescription, item.image.get(),
                                      item.colour != juce::Colour() ? &item.colour : nullptr);
            y += h;
        }
        x += sizes[m].x + 20;
    }

    return image;
}

// --envdot <dir> [curve]: frames of the amp envelope graph while a note is
// held and let go, for judging the playhead's motion and trail (ffmpeg joins
// them into a GIF). `curve` is the CURVE knob, -1 to 1.
int runEnvDot (const juce::File& outDir, float curve)
{
    outDir.createDirectory();
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    const auto setParam = [&processor] (const juce::String& id, float plain)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
    };
    setParam ("amp_attack", 0.3f);
    setParam ("amp_decay", 0.7f);
    setParam ("amp_sustain", 0.55f);
    setParam ("amp_release", 0.9f);
    setParam ("amp_curve", curve);

    EnvelopeDisplay display (processor, "amp");
    display.setSize (480, 190);
    juce::AudioBuffer<float> buffer (2, 512);
    auto frame = 0;
    const auto step = [&] (bool noteOn, bool noteOff)
    {
        // About 33 ms of audio, then the picture.
        for (int block = 0; block < 3; ++block)
        {
            juce::MidiBuffer midi;
            if (block == 0 && noteOn)
                midi.addEvent (juce::MidiMessage::noteOn (1, 57, 0.8f), 0);
            if (block == 0 && noteOff)
                midi.addEvent (juce::MidiMessage::noteOff (1, 57), 0);
            buffer.clear();
            processor.processBlock (buffer, midi);
        }
        juce::Thread::sleep (33);
        const auto image = display.createComponentSnapshot (display.getLocalBounds(), true, 2.0f);
        juce::FileOutputStream stream (outDir.getChildFile (juce::String::formatted ("f%03d.png", frame++)));
        if (stream.openedOk())
        {
            stream.setPosition (0);
            stream.truncate();
            juce::PNGImageFormat().writeImageToStream (image, stream);
        }
    };
    step (false, false);
    for (int i = 0; i < 60; ++i)
        step (i == 0, false); // 2 s held
    for (int i = 0; i < 45; ++i)
        step (false, i == 0); // 1.5 s released
    for (int i = 0; i < 15; ++i)
        step (false, false);  // and quiet
    std::cout << frame << " frames in " << outDir.getFullPathName() << std::endl;
    return 0;
}

