#pragma once

#include <functional>

#include "IlanaLookAndFeel.h"
#include "../processor/PatchGraph.h"

// The cable view (roadmap step A): a patch graph (processor/PatchGraph.h)
// drawn as small node cards with ports, and cables dragged from an output
// port to an input port. The FX patcher (step 17) and the oscillator routing
// (step 16) both put one on a page; this file knows nothing about effects or
// oscillators. The owner hands it a graph, a colour per node and the menus,
// and hears about every edit through onEdited, where it recompiles.
//
//   drag from an output port     a new cable; drop it on an input port
//   drag from an input port      picks up the newest cable into it, so it can
//                                be moved to another input or dropped off
//   drag a node                  moves it on the 10 px grid
//   click a cable or node        selects it; Delete or Backspace removes it
//   right-click                  a cable: remove, its gain, its loop delay;
//                                a node: bypass, remove, the owner's items;
//                                empty space: the owner's add menu
//
// A cable that closes a loop is drawn dashed with its delay ("1.3 ms"): the
// compiler gives every such cable one render chunk of delay.
class PatchCableView : public juce::Component
{
public:
    enum class Edit
    {
        cables,  // a cable was made, moved, removed or re-weighted: recompile
        nodes,   // a node was added, removed or bypassed: recompile
        layout   // a node moved: save, nothing to recompile
    };

    // Design sizes, on the 10 px grid.
    static constexpr float nodeWidth = 130.0f;
    static constexpr float nodeHeight = 50.0f;
    static constexpr float fixedWidth = 90.0f;
    static constexpr float portRadius = 5.0f;
    static constexpr float grid = 10.0f;

    explicit PatchCableView (PatchGraph::Graph& graphToShow) : graph (graphToShow)
    {
        setWantsKeyboardFocus (true);
        setOpaque (false);
    }

    // Owner hooks.
    std::function<void (Edit)> onEdited;
    std::function<juce::Colour (const PatchGraph::Node&)> colourOf;
    std::function<float (PatchGraph::NodeId)> cpuOf;                 // 0..1 of a core, or < 0 for none
    std::function<void (juce::PopupMenu&, juce::Point<float>)> addMenu; // empty space, at a graph position
    std::function<void (juce::PopupMenu&, const PatchGraph::Node&)> nodeMenu;

    // How menus open. The tests swap it for one that picks an item.
    std::function<void (juce::PopupMenu&, std::function<void (int)>)> openMenu = [this] (juce::PopupMenu& menu, std::function<void (int)> done)
    {
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), std::move (done));
    };

    void setLoopLatencyMs (float ms) { loopLatencyMs = ms; repaint(); }

    // Moves the view so the graph's nodes sit in the middle (after loading).
    void centreContent()
    {
        auto content = contentBounds();
        if (content.isEmpty())
            return;
        origin = getLocalBounds().toFloat().getCentre() - content.getCentre();
        origin = { std::round (origin.x / grid) * grid, std::round (origin.y / grid) * grid };
        repaint();
    }

    void graphChanged()
    {
        selectedCable = -1;
        if (graph.find (selectedNode) == nullptr)
            selectedNode = 0;
        repaint();
    }

    //==========================================================================
    // Geometry (public for the tests).
    juce::Rectangle<float> nodeBounds (const PatchGraph::Node& node) const
    {
        const auto width = node.isFixed() ? fixedWidth : nodeWidth;
        return { origin.x + node.x, origin.y + node.y, width, nodeHeight };
    }

    juce::Point<float> portPosition (PatchGraph::Port port, bool input) const
    {
        const auto* node = graph.find (port.node);
        if (node == nullptr)
            return {};
        const auto bounds = nodeBounds (*node);
        const auto count = juce::jmax (1, input ? node->numInputs : node->numOutputs);
        const auto spacing = bounds.getHeight() / (float) (count + 1);
        return { input ? bounds.getX() : bounds.getRight(), bounds.getY() + spacing * (float) (port.port + 1) };
    }

    PatchGraph::NodeId getSelectedNode() const { return selectedNode; }
    int getSelectedCable() const { return selectedCable; }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        using namespace IlanaTheme;
        paintWell (g, getLocalBounds().toFloat(), 8.0f);

        // A quiet dot grid every 20 px, so the 10 px snapping has something to sit on.
        g.setColour (Ui::line2);
        const auto startX = std::fmod (origin.x, 20.0f), startY = std::fmod (origin.y, 20.0f);
        for (auto y = startY; y < (float) getHeight(); y += 20.0f)
            for (auto x = startX; x < (float) getWidth(); x += 20.0f)
                g.fillRect (x - 0.5f, y - 0.5f, 1.5f, 1.5f);

        const auto feedback = graph.feedbackCables();
        const auto& cables = graph.cables();
        for (size_t c = 0; c < cables.size(); ++c)
        {
            if (drag.kind == DragKind::cable && drag.pickedCable == (int) c)
                continue;
            paintCable (g, cables[c], feedback[c], (int) c == selectedCable);
        }

        for (const auto& node : graph.nodes())
            paintNode (g, node);

        if (drag.kind == DragKind::cable)
        {
            const auto from = portPosition (drag.from, false);
            const auto* source = graph.find (drag.from.node);
            const auto colour = source != nullptr ? colourFor (*source) : Ui::text2;
            g.setColour (colour.withAlpha (0.9f));
            g.strokePath (cablePath (from, drag.mouse), juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    //==========================================================================
    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        const auto position = e.position;

        if (e.mods.isPopupMenu())
        {
            showMenuAt (position);
            return;
        }

        if (const auto out = portAt (position, false); out.node != 0)
        {
            startCableDrag (out, -1, position);
            return;
        }

        if (const auto in = portAt (position, true); in.node != 0)
        {
            // Pick up the newest cable into this port.
            const auto& cables = graph.cables();
            for (int c = (int) cables.size() - 1; c >= 0; --c)
                if (cables[(size_t) c].to == in)
                {
                    startCableDrag (cables[(size_t) c].from, c, position);
                    return;
                }
            return;
        }

        if (const auto* node = nodeAt (position))
        {
            selectedNode = node->id;
            selectedCable = -1;
            drag = {};
            drag.kind = DragKind::node;
            drag.node = node->id;
            drag.grabOffset = position - juce::Point<float> (origin.x + node->x, origin.y + node->y);
            repaint();
            return;
        }

        selectedCable = cableAt (position);
        selectedNode = 0;
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (drag.kind == DragKind::node)
        {
            const auto target = e.position - drag.grabOffset - origin;
            graph.moveNode (drag.node, std::round (target.x / grid) * grid, std::round (target.y / grid) * grid);
            drag.moved = true;
            repaint();
        }
        else if (drag.kind == DragKind::cable)
        {
            drag.mouse = e.position;
            repaint();
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (drag.kind == DragKind::node && drag.moved && onEdited)
            onEdited (Edit::layout);

        if (drag.kind == DragKind::cable)
            finishCableDrag (portAt (e.position, true));

        drag = {};
        repaint();
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto overPort = portAt (e.position, false).node != 0 || portAt (e.position, true).node != 0;
        setMouseCursor (overPort ? juce::MouseCursor::CrosshairCursor
                                 : nodeAt (e.position) != nullptr ? juce::MouseCursor::DraggingHandCursor
                                                                  : juce::MouseCursor::NormalCursor);
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
        {
            if (selectedCable >= 0 && selectedCable < (int) graph.cables().size())
            {
                const auto cable = graph.cables()[(size_t) selectedCable];
                graph.disconnect (cable.from, cable.to);
                selectedCable = -1;
                edited (Edit::cables);
                return true;
            }
            if (selectedNode != 0 && graph.removeNode (selectedNode))
            {
                selectedNode = 0;
                edited (Edit::nodes);
                return true;
            }
        }
        return false;
    }

    //==========================================================================
    // Hit tests (public for the tests).
    PatchGraph::Port portAt (juce::Point<float> position, bool input) const
    {
        const auto reach = portRadius + 4.0f;
        for (auto it = graph.nodes().rbegin(); it != graph.nodes().rend(); ++it)
        {
            const auto count = input ? it->numInputs : it->numOutputs;
            for (int p = 0; p < count; ++p)
                if (portPosition ({ it->id, p }, input).getDistanceFrom (position) <= reach)
                    return { it->id, p };
        }
        return {};
    }

    const PatchGraph::Node* nodeAt (juce::Point<float> position) const
    {
        for (auto it = graph.nodes().rbegin(); it != graph.nodes().rend(); ++it)
            if (nodeBounds (*it).contains (position))
                return &*it;
        return nullptr;
    }

    int cableAt (juce::Point<float> position) const
    {
        const auto& cables = graph.cables();
        auto best = -1;
        auto bestDistance = 6.0f;
        for (size_t c = 0; c < cables.size(); ++c)
        {
            const auto path = cablePath (portPosition (cables[c].from, false), portPosition (cables[c].to, true));
            juce::Point<float> nearest;
            const auto along = path.getNearestPoint (position, nearest);
            juce::ignoreUnused (along);
            const auto distance = nearest.getDistanceFrom (position);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = (int) c;
            }
        }
        return best;
    }

private:
    enum class DragKind { none, node, cable };

    struct Drag
    {
        DragKind kind = DragKind::none;
        PatchGraph::NodeId node = 0;
        juce::Point<float> grabOffset;
        bool moved = false;
        PatchGraph::Port from;
        int pickedCable = -1; // a cable lifted off an input port, or -1 for a new one
        float pickedGain = 1.0f;
        juce::Point<float> mouse;
    };

    void edited (Edit edit)
    {
        repaint();
        if (onEdited)
            onEdited (edit);
    }

    juce::Colour colourFor (const PatchGraph::Node& node) const
    {
        if (node.isFixed())
            return IlanaTheme::Ui::text2;
        return colourOf ? colourOf (node) : IlanaTheme::accent();
    }

    void startCableDrag (PatchGraph::Port from, int picked, juce::Point<float> position)
    {
        drag = {};
        drag.kind = DragKind::cable;
        drag.from = from;
        drag.pickedCable = picked;
        drag.pickedGain = picked >= 0 ? graph.cables()[(size_t) picked].gain : 1.0f;
        drag.mouse = position;
        selectedCable = -1;
        repaint();
    }

    void finishCableDrag (PatchGraph::Port to)
    {
        auto changed = false;
        if (drag.pickedCable >= 0 && drag.pickedCable < (int) graph.cables().size())
        {
            const auto old = graph.cables()[(size_t) drag.pickedCable];
            if (to.node != 0 && to == old.to)
                return; // put back where it was
            graph.disconnect (old.from, old.to);
            changed = true;
        }
        if (to.node != 0 && graph.connect (drag.from, to, drag.pickedGain) == PatchGraph::ConnectResult::ok)
        {
            selectedCable = graph.findCable (drag.from, to);
            changed = true;
        }
        if (changed)
            edited (Edit::cables);
    }

    void showMenuAt (juce::Point<float> position)
    {
        enum { removeCable = 1, gainBase = 10, bypassNode = 30, removeNode = 31, ownerBase = 100 };

        juce::PopupMenu menu;
        if (const auto* node = nodeAt (position))
        {
            const auto id = node->id;
            selectedNode = id;
            selectedCable = -1;
            repaint();
            if (! node->isFixed())
            {
                menu.addItem (bypassNode, "Bypass", true, node->bypassed);
                menu.addItem (removeNode, "Remove");
            }
            if (nodeMenu)
            {
                juce::PopupMenu extra;
                nodeMenu (extra, *node);
                if (extra.getNumItems() > 0)
                {
                    menu.addSeparator();
                    for (juce::PopupMenu::MenuItemIterator it (extra); it.next();)
                        menu.addItem (it.getItem());
                }
            }
            if (menu.getNumItems() == 0)
                return;
            openMenu (menu, [this, id] (int result)
            {
                const auto* target = graph.find (id);
                if (target == nullptr)
                    return;
                if (result == bypassNode)
                {
                    graph.setBypassed (id, ! target->bypassed);
                    edited (Edit::nodes);
                }
                else if (result == removeNode && graph.removeNode (id))
                {
                    selectedNode = 0;
                    edited (Edit::nodes);
                }
            });
            return;
        }

        if (const auto c = cableAt (position); c >= 0)
        {
            selectedCable = c;
            selectedNode = 0;
            repaint();
            const auto cable = graph.cables()[(size_t) c];
            if (graph.feedbackCables()[(size_t) c])
                menu.addItem (-1, "Loop: " + juce::String (loopLatencyMs, 1) + " ms delay", false);
            juce::PopupMenu gains;
            const float levels[] = { 1.0f, 0.7f, 0.5f, 0.25f };
            for (int i = 0; i < 4; ++i)
                gains.addItem (gainBase + i, juce::String (juce::roundToInt (levels[i] * 100.0f)) + " %", true, cable.gain == levels[i]);
            menu.addSubMenu ("Level", gains);
            menu.addSeparator();
            menu.addItem (removeCable, "Remove cable");
            openMenu (menu, [this, cable, levels] (int result)
            {
                const auto index = graph.findCable (cable.from, cable.to);
                if (index < 0)
                    return;
                if (result == removeCable)
                {
                    graph.disconnect (cable.from, cable.to);
                    selectedCable = -1;
                    edited (Edit::cables);
                }
                else if (result >= gainBase && result < gainBase + 4)
                {
                    graph.setCableGain (index, levels[result - gainBase]);
                    edited (Edit::cables);
                }
            });
            return;
        }

        if (addMenu)
        {
            addMenu (menu, position - origin);
            if (menu.getNumItems() > 0)
                openMenu (menu, [] (int) {}); // the owner's items carry their own actions
        }
    }

    juce::Path cablePath (juce::Point<float> from, juce::Point<float> to) const
    {
        juce::Path path;
        path.startNewSubPath (from);
        const auto dx = to.x - from.x;
        if (dx >= 20.0f)
        {
            const auto pull = juce::jmax (30.0f, dx * 0.5f);
            path.cubicTo (from.translated (pull, 0.0f), to.translated (-pull, 0.0f), to);
        }
        else
        {
            // Backwards (a loop, or a cable to a node on the left): out to the
            // right, under both nodes, and in from the left.
            // (under every node, so a loop never runs behind one)
            const auto drop = juce::jmax (juce::jmax (from.y, to.y) + nodeHeight * 0.5f, origin.y + contentBounds().getBottom()) + 20.0f;
            path.cubicTo (from.translated (60.0f, 0.0f), { from.x + 60.0f, drop }, { (from.x + to.x) * 0.5f, drop });
            path.cubicTo ({ to.x - 60.0f, drop }, to.translated (-60.0f, 0.0f), to);
        }
        return path;
    }

    void paintCable (juce::Graphics& g, const PatchGraph::Cable& cable, bool loop, bool selected) const
    {
        using namespace IlanaTheme;
        const auto from = portPosition (cable.from, false);
        const auto to = portPosition (cable.to, true);
        const auto path = cablePath (from, to);
        const auto* source = graph.find (cable.from.node);
        auto colour = source != nullptr ? colourFor (*source) : Ui::text2;
        colour = colour.withAlpha (selected ? 1.0f : 0.35f + 0.5f * juce::jlimit (0.0f, 1.0f, std::abs (cable.gain)));
        const auto width = selected ? 2.6f : 2.0f;

        if (loop)
        {
            juce::Path dashed;
            const float dashes[] = { 5.0f, 4.0f };
            juce::PathStrokeType (width).createDashedStroke (dashed, path, dashes, 2);
            g.setColour (colour);
            g.fillPath (dashed);

            const auto middle = path.getPointAlongPath (path.getLength() * 0.5f);
            const auto text = juce::String (loopLatencyMs, 1) + " ms";
            g.setFont (font (TextSize::tiny, true));
            const auto box = juce::Rectangle<float> (0.0f, 0.0f, 50.0f, 18.0f).withCentre (middle);
            g.setColour (Ui::raised);
            g.fillRoundedRectangle (box, 9.0f);
            g.setColour (colour);
            g.drawText (text, box, juce::Justification::centred, false);
        }
        else
        {
            g.setColour (colour);
            g.strokePath (path, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        if (cable.gain != 1.0f && ! loop)
        {
            // A third of the way along: where cables fan into one port their
            // middles bunch up, their starts don't.
            const auto middle = path.getPointAlongPath (path.getLength() * 0.35f);
            const auto box = juce::Rectangle<float> (0.0f, 0.0f, 40.0f, 18.0f).withCentre (middle);
            g.setColour (Ui::raised);
            g.fillRoundedRectangle (box, 9.0f);
            g.setColour (Ui::text2);
            g.setFont (font (TextSize::tiny, true, true));
            g.drawText (juce::String (juce::roundToInt (cable.gain * 100.0f)) + "%", box, juce::Justification::centred, false);
        }
    }

    void paintNode (juce::Graphics& g, const PatchGraph::Node& node) const
    {
        using namespace IlanaTheme;
        const auto bounds = nodeBounds (node);
        const auto colour = colourFor (node);
        const auto selected = node.id == selectedNode;
        const auto alpha = node.bypassed ? 0.45f : 1.0f;

        if (node.isFixed())
        {
            g.setColour (Ui::raised);
            g.fillRoundedRectangle (bounds, bounds.getHeight() * 0.5f);
            g.setColour (selected ? Ui::text2 : Ui::line);
            g.drawRoundedRectangle (bounds.reduced (0.5f), bounds.getHeight() * 0.5f, 1.0f);
            g.setColour (Ui::text2);
            g.setFont (pillFont());
            g.drawText (juce::String (node.label), bounds, juce::Justification::centred, false);
        }
        else
        {
            g.setColour (Ui::panel);
            g.fillRoundedRectangle (bounds, 8.0f);
            g.setColour (colour.withAlpha (0.08f * alpha));
            g.fillRoundedRectangle (bounds.reduced (1.0f), 7.0f);
            g.setColour (selected ? colour : Ui::line);
            g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, selected ? 1.5f : 1.0f);

            auto inner = bounds.reduced (14.0f, 8.0f).toNearestInt();
            auto top = inner.removeFromTop (inner.getHeight() / 2);
            paintTag (g, { (float) top.getX() + 3.0f, (float) top.getCentreY() }, colour.withMultipliedAlpha (alpha));
            g.setColour (Ui::text.withAlpha (alpha));
            g.setFont (font (TextSize::body, true));
            drawFitted (g, juce::String (node.label), top.withTrimmedLeft (12), juce::Justification::centredLeft, 1, TextSize::minPassive);

            juce::String status = node.bypassed ? "BYPASSED" : juce::String();
            if (status.isEmpty() && cpuOf)
                if (const auto cpu = cpuOf (node.id); cpu >= 0.0f)
                    status = juce::String (cpu * 100.0f, cpu < 0.1f ? 2 : 1) + " % CPU";
            g.setColour (Ui::text3);
            g.setFont (font (TextSize::tiny, false, true));
            g.drawText (status, inner.withTrimmedLeft (12), juce::Justification::centredLeft, false);
        }

        // Ports: filled while a cable is on them.
        const auto paintPort = [&] (PatchGraph::Port port, bool input)
        {
            const auto centre = portPosition (port, input);
            auto connected = false;
            for (const auto& cable : graph.cables())
                connected = connected || (input ? cable.to == port : cable.from == port);
            const auto target = drag.kind == DragKind::cable && input && portAt (drag.mouse, true) == port;
            const auto disc = juce::Rectangle<float> (portRadius * 2.0f, portRadius * 2.0f).withCentre (centre);
            g.setColour (connected || target ? colour.withMultipliedAlpha (alpha) : Ui::raised);
            g.fillEllipse (disc);
            g.setColour (target ? Ui::text : colour.withAlpha (0.8f * alpha));
            g.drawEllipse (disc.reduced (0.5f), target ? 2.0f : 1.2f);
        };
        for (int p = 0; p < node.numInputs; ++p)
            paintPort ({ node.id, p }, true);
        for (int p = 0; p < node.numOutputs; ++p)
            paintPort ({ node.id, p }, false);
    }

    juce::Rectangle<float> contentBounds() const
    {
        juce::Rectangle<float> area;
        for (const auto& node : graph.nodes())
        {
            const auto width = node.isFixed() ? fixedWidth : nodeWidth;
            const juce::Rectangle<float> box (node.x, node.y, width, nodeHeight);
            area = area.isEmpty() ? box : area.getUnion (box);
        }
        return area;
    }

    PatchGraph::Graph& graph;
    juce::Point<float> origin { 20.0f, 20.0f };
    PatchGraph::NodeId selectedNode = 0;
    int selectedCable = -1;
    float loopLatencyMs = 1.3f;
    Drag drag;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatchCableView)
};
