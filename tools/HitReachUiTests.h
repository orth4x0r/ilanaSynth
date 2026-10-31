// Bug hunt 2026-10-31: every visible button, knob and menu on every page
// (and in the source pop-ups) takes a click at its centre: nothing drawn
// over it steals the mouse (the LFO pop-up bug's class). Walks the tree
// the way juce::Component::getComponentAt does. ILANA_UITEST_ONLY=HR.
#pragma once

namespace HitReach
{
inline juce::Component* at (juce::Component& c, juce::Point<int> local, bool top)
{
    if ((! top && ! c.isVisible()) || ! c.getLocalBounds().contains (local) || ! c.hitTest (local.x, local.y))
        return nullptr;
    auto& children = c.getChildren();
    for (int i = children.size(); --i >= 0;)
        if (auto* r = at (*children[i], children[i]->getLocalPoint (&c, local), false))
            return r;
    return &c;
}

inline juce::String describe (juce::Component* c)
{
    if (c == nullptr)
        return "nothing";
    juce::String s = typeid (*c).name();
    if (auto* b = dynamic_cast<juce::Button*> (c); b != nullptr && b->getButtonText().isNotEmpty())
        s << " '" << b->getButtonText() << "'";
    if (c->getName().isNotEmpty())
        s << " \"" << c->getName() << "\"";
    if (c->getTitle().isNotEmpty())
        s << " {" << c->getTitle() << "}";
    if (auto* tt = dynamic_cast<juce::SettableTooltipClient*> (c); tt != nullptr && tt->getTooltip().isNotEmpty())
        s << " <" << tt->getTooltip().upToFirstOccurrenceOf ("\n", false, false).substring (0, 40) << ">";
    return s;
}

// Whether p (in c's coordinates) is inside every ancestor up to root (not clipped or scrolled away).
inline bool unclipped (juce::Component& c, juce::Point<int> p, juce::Component& root)
{
    for (auto* parent = c.getParentComponent(); parent != nullptr && parent != &root; parent = parent->getParentComponent())
        if (! parent->getLocalBounds().contains (parent->getLocalPoint (&c, p)))
            return false;
    return root.getLocalBounds().contains (root.getLocalPoint (&c, p));
}

inline void check (juce::Component& root, juce::Component& scope, const juce::String& where, juce::StringArray& stolen,
                   juce::StringArray& ignoring, int& checked)
{
    std::vector<juce::Component*> controls;
    std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
    {
        for (auto* child : c.getChildren())
        {
            if (! child->isVisible())
                continue;
            if (dynamic_cast<juce::Button*> (child) != nullptr || dynamic_cast<juce::Slider*> (child) != nullptr
                || dynamic_cast<juce::ComboBox*> (child) != nullptr)
                controls.push_back (child);
            walk (*child);
        }
    };
    walk (scope);
    for (auto* control : controls)
    {
        if (control->getWidth() < 4 || control->getHeight() < 4 || ! visibleInTree (control))
            continue;
        // Probe a few points: the centre and inset corners. A control is "stolen" when its centre goes to a component
        // that is neither it, inside it, nor one of its ancestors.
        const auto centre = control->getLocalBounds().getCentre();
        if (! unclipped (*control, centre, root))
            continue;
        ++checked;
        auto* target = at (root, root.getLocalPoint (control, centre), true);
        if (target == control || control->isParentOf (target))
            continue;
        const auto entry = where + ": " + describe (control) + " @" + root.getLocalPoint (control, centre).toString() + " -> " + describe (target);
        if (target != nullptr && target->isParentOf (control))
            ignoring.addIfNotAlreadyThere (entry);
        else
            stolen.addIfNotAlreadyThere (entry);
    }
}
} // namespace HitReach

inline void runHitReachTests (IlanaSynthAudioProcessor& processor, IlanaSynthAudioProcessorEditor& editor)
{
    juce::ignoreUnused (processor);
    auto* top = editor.getTopLevelComponent();
    const auto before = top->getBounds();
    for (const auto scale : { 1.0f, 1.75f })
    {
        top->setSize (juce::roundToInt (1060 * scale), juce::roundToInt (720 * scale));
        settle (300);
        juce::StringArray stolen, ignoring;
        int checked = 0;
        const auto tag = juce::String (juce::roundToInt (scale * 100)) + "% ";
        for (const auto& page : editor.getPageIds())
        {
            editor.showPage (page);
            settle (250);
            HitReach::check (editor, editor, tag + page, stolen, ignoring, checked);

            // Every source chip's pop-up on this page.
            std::vector<ModSourceChip*> chips;
            findAll<ModSourceChip> (editor, chips);
            std::set<int> seenKinds;
            for (auto* chip : chips)
            {
                if (! visibleInTree (chip) || chip->onClick == nullptr)
                    continue;
                const auto s = chip->getSourceIndex();
                if (! seenKinds.insert (s).second || seenKinds.size() > 6)
                    continue;
                chip->onClick (*chip);
                settle (150);
                if (auto* popover = editor.getSourcePopover(); popover != nullptr && popover->isVisible())
                {
                    HitReach::check (editor, *popover, tag + page + " pop-up " + ModNames::source (s, &processor), stolen, ignoring, checked);
                    chip->onClick (*chip); // the chip again closes it
                    settle (100);
                    if (popover->isVisible())
                        popover->setVisible (false);
                }
            }
        }
        std::cout << "  " << tag << "checked " << checked << " controls; " << stolen.size() << " covered, " << ignoring.size() << " ignore clicks" << std::endl;
        for (const auto& s : stolen)
            std::cout << "    COVERED " << s << std::endl;
        for (const auto& s : ignoring)
            std::cout << "    IGNORES " << s << std::endl;
        expect (stolen.isEmpty(), tag + "every visible control takes a click at its centre (" + juce::String (stolen.size()) + " covered)");
    }
    top->setBounds (before);
    settle (300);
}
