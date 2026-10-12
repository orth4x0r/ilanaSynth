#pragma once

#include <array>
#include <functional>
#include <map>

#include "AnimationUtils.h"
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
//   drag (or scroll) a loop's    its level (ilana 2026-10-11: every loop
//   tag up and down              gets a level she can reach on the cable)
//
// A cable that closes a loop is drawn dashed and runs under the boxes, with
// a tag halfway along that shows its level and delay ("20% | 1.3 ms"). The
// tag turns the accent colour while the runner's loop guard is holding the
// loop at 0 dB (it rings by itself: turn its level down).
//
// Cable motion (roadmap step 47): a forward cable hangs with a soft sag, and
// when a box, the cable's other end or the pointer carrying a new cable
// moves, the cable swings behind it on a damped spring and settles. This is
// display only (the graph, the compiled plan and the audio never see it). A
// frame timer runs only while some cable is still moving; at rest the view
// draws on demand like before, so an idle patcher costs nothing.
class PatchCableView : public juce::Component,
                       private IlanaAnim::FrameTimer
{
public:
    enum class Edit
    {
        cables,  // a cable was made, moved, removed or re-weighted: recompile
        nodes,   // a node was added, removed or bypassed: recompile
        layout,  // a node moved: save, nothing to recompile
        loop     // a loop's level, while its tag is dragged (inside onGesture)
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
    // Optional: the owner bypasses or removes a node itself (the FX patcher's
    // nodes are rack slots, whose bypass and effect are parameters); unset,
    // the view edits the graph. onOpen: a node double-clicked.
    std::function<void (const PatchGraph::Node&)> onBypass, onRemove, onOpen;
    // Optional, for a fixed set of nodes (the oscillator routing, step 16):
    // statusOf names a node's second line (instead of its CPU); acceptGraph
    // vets a cable edit before it happens (false: the cable snaps back and
    // onRefused hears why it was refused).
    std::function<juce::String (const PatchGraph::Node&)> statusOf;
    std::function<bool (const PatchGraph::Graph&)> acceptGraph;
    std::function<void()> onRefused;
    bool showInputNode = true;   // false: the fixed INPUT node is not drawn
    bool nodesRemovable = true;  // false: no Bypass / Remove, Delete leaves nodes alone
    bool cableLevels = true;     // false: a cable's menu has no Level
    // Optional, per node or cable, for a view that mixes both kinds (the
    // PATCH tab: the voice's fixed boxes beside the rack's effects).
    std::function<bool (const PatchGraph::Node&)> removable, movable;
    std::function<bool (const PatchGraph::Cable&)> hasLevel;
    // Optional: drawn over the dot grid, under the cables (zoomed; graph
    // positions are at getOrigin() + (x, y)).
    std::function<void (juce::Graphics&)> paintBackdrop;
    // Optional: the loop guard's gain on a loop cable (under 1: ringing).
    std::function<float (const PatchGraph::Cable&)> loopGuardOf;
    // A drag on a loop's tag: true at its start, false at its end; the Edit::loop
    // edits come between (the owner makes them one undo step).
    std::function<void (bool)> onGesture;
    // The highest level a loop cable takes (the owner's limit).
    float maxLoopLevel = 0.7f;

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
        origin = viewArea().getCentre() - content.getCentre();
        origin = { std::round (origin.x / grid) * grid, std::round (origin.y / grid) * grid };
        repaint();
    }

    // Zoom: the whole view drawn larger (1 = design size). Positions in the
    // hit tests and geometry below are in unzoomed view coordinates.
    void setZoom (float newZoom)
    {
        zoom = juce::jlimit (0.5f, 3.0f, newZoom);
        repaint();
    }
    float getZoom() const { return zoom; }

    // The largest zoom (from minZoom to maxZoom) at which the graph, with a
    // margin, fits the view; then centred.
    void fitContent (float maxZoom = 1.5f, float minZoom = 1.0f)
    {
        const auto content = contentBounds();
        if (content.isEmpty() || getWidth() <= 0 || getHeight() <= 0)
            return;
        const auto fit = juce::jmin ((float) getWidth() / (content.getWidth() + 160.0f),
                                     (float) getHeight() / (content.getHeight() + 220.0f));
        zoom = juce::jlimit (minZoom, juce::jmax (minZoom, maxZoom), std::floor (fit * 10.0f) / 10.0f);
        centreContent();
    }

    // Whether every node is inside the view (else the owner may refit).
    bool contentFits() const
    {
        const auto content = contentBounds();
        return content.isEmpty() || viewArea().contains (content.translated (origin.x, origin.y));
    }

    void graphChanged()
    {
        // (A cable stays selected while it is still there.)
        selectedCable = selectedCable >= 0 ? graph.findCable (selectedFrom, selectedTo) : -1;
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

    const PatchGraph::Graph& getGraph() const { return graph; }
    juce::Point<float> getOrigin() const { return origin; }
    PatchGraph::NodeId getSelectedNode() const { return selectedNode; }
    int getSelectedCable() const { return selectedCable; }

    void selectCable (int index)
    {
        selectedCable = juce::isPositiveAndBelow (index, (int) graph.cables().size()) ? index : -1;
        if (selectedCable >= 0)
        {
            selectedFrom = graph.cables()[(size_t) selectedCable].from;
            selectedTo = graph.cables()[(size_t) selectedCable].to;
            selectedNode = 0;
        }
        repaint();
    }

    // A loop's delay as its tag shows it (the chunk at the least).
    juce::String loopDelayText (const PatchGraph::Cable& cable) const
    {
        const auto ms = juce::jmax (loopLatencyMs, cable.delayMs);
        return ms < 10.0f ? juce::String (ms, 1) + " ms" : ms < 1000.0f ? juce::String (juce::roundToInt (ms)) + " ms" : juce::String (ms / 1000.0f, 2) + " s";
    }

    // The tag halfway along a loop cable (its level and delay).
    juce::Rectangle<float> loopTagBounds (const PatchGraph::Cable& cable) const
    {
        const auto path = cablePathOf (cable, true);
        return juce::Rectangle<float> (0.0f, 0.0f, 100.0f, 20.0f).withCentre (path.getPointAlongPath (path.getLength() * 0.5f));
    }

    // The loop cable whose tag is at a position, or -1.
    int loopTagAt (juce::Point<float> position) const
    {
        const auto feedback = graph.feedbackCables();
        for (size_t c = 0; c < graph.cables().size(); ++c)
            if (feedback[c] && loopTagBounds (graph.cables()[c]).contains (position))
                return (int) c;
        return -1;
    }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        using namespace IlanaTheme;
        paintWell (g, getLocalBounds().toFloat(), 8.0f);
        g.addTransform (juce::AffineTransform::scale (zoom));

        // A quiet dot grid every 20 px, so the 10 px snapping has something to sit on.
        g.setColour (Ui::line2);
        const auto startX = std::fmod (origin.x, 20.0f), startY = std::fmod (origin.y, 20.0f);
        for (auto y = startY; y < viewArea().getBottom(); y += 20.0f)
            for (auto x = startX; x < viewArea().getRight(); x += 20.0f)
                g.fillRect (x - 0.5f, y - 0.5f, 1.5f, 1.5f);
        if (paintBackdrop)
            paintBackdrop (g);

        const auto feedback = graph.feedbackCables();
        const auto& cables = graph.cables();
        for (size_t c = 0; c < cables.size(); ++c)
        {
            if (drag.kind == DragKind::cable && drag.pickedCable == (int) c)
                continue;
            paintCable (g, cables[c], feedback[c], (int) c == selectedCable);
        }

        for (const auto& node : graph.nodes())
            if (isShown (node))
                paintNode (g, node);

        if (drag.kind == DragKind::cable)
        {
            const auto from = portPosition (drag.from, false);
            const auto* source = graph.find (drag.from.node);
            const auto colour = source != nullptr ? colourFor (*source) : Ui::text2;
            g.setColour (colour.withAlpha (0.9f));
            const auto pending = swings.find (pendingKey);
            g.strokePath (cablePath (from, drag.mouse, false, pending != swings.end() ? pending->second.offset : juce::Point<float>()), juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    //==========================================================================
    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        const auto position = e.position / zoom;

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

        if (const auto tag = loopTagAt (position); tag >= 0 && nodeAt (position) == nullptr)
        {
            selectCable (tag);
            drag = {};
            drag.kind = DragKind::loopLevel;
            drag.pickedCable = tag;
            drag.pickedGain = graph.cables()[(size_t) tag].gain;
            drag.mouse = position;
            return;
        }

        if (const auto* node = nodeAt (position))
        {
            selectedNode = node->id;
            selectedCable = -1;
            drag = {};
            drag.kind = movable && ! movable (*node) ? DragKind::none : DragKind::node;
            drag.node = node->id;
            drag.grabOffset = position - juce::Point<float> (origin.x + node->x, origin.y + node->y);
            repaint();
            return;
        }

        selectCable (cableAt (position));
        selectedNode = 0;
        if (selectedCable < 0)
        {
            // Empty space: drag to look around a big patch.
            drag = {};
            drag.kind = DragKind::pan;
            drag.grabOffset = position - origin;
        }
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (const auto* node = nodeAt (e.position / zoom); node != nullptr && ! node->isFixed() && onOpen)
            onOpen (*node);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (drag.kind == DragKind::node)
        {
            const auto target = e.position / zoom - drag.grabOffset - origin;
            graph.moveNode (drag.node, std::round (target.x / grid) * grid, std::round (target.y / grid) * grid);
            drag.moved = true;
            wakeCables();
            repaint();
        }
        else if (drag.kind == DragKind::cable)
        {
            drag.mouse = e.position / zoom;
            wakeCables();
            repaint();
        }
        else if (drag.kind == DragKind::loopLevel)
        {
            // 200 px from nothing to full; finer with a modifier, like the knobs.
            const auto scale = e.mods.isShiftDown() || e.mods.isCommandDown() ? 0.001f : 0.005f;
            const auto level = juce::jlimit (0.0f, maxLoopLevel, drag.pickedGain + (drag.mouse.y - e.position.y / zoom) * scale);
            setLoopLevel (drag.pickedCable, std::round (level * 100.0f) / 100.0f);
        }
        else if (drag.kind == DragKind::pan)
        {
            const auto target = e.position / zoom - drag.grabOffset;
            origin = { std::round (target.x / grid) * grid, std::round (target.y / grid) * grid };
            repaint();
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (drag.kind == DragKind::loopLevel)
        {
            if (drag.moved && onGesture)
                onGesture (false);
            drag = {};
            repaint();
            return;
        }
        if (drag.kind == DragKind::node && drag.moved && onEdited)
            onEdited (Edit::layout);

        if (drag.kind == DragKind::cable)
            finishCableDrag (portAt (e.position / zoom, true));

        drag = {};
        repaint();
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        const auto tag = loopTagAt (e.position / zoom);
        if (tag < 0 || nodeAt (e.position / zoom) != nullptr)
            return juce::Component::mouseWheelMove (e, wheel);
        selectCable (tag);
        const auto step = (wheel.deltaY != 0.0f ? wheel.deltaY : wheel.deltaX) > 0.0f ? 0.01f : -0.01f;
        const auto level = juce::jlimit (0.0f, maxLoopLevel, std::round ((graph.cables()[(size_t) tag].gain + step) * 100.0f) / 100.0f);
        if (onGesture)
            onGesture (true);
        drag.moved = true;
        setLoopLevel (tag, level);
        if (onGesture)
            onGesture (false);
        drag = {};
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto position = e.position / zoom;
        if (loopTagAt (position) >= 0 && nodeAt (position) == nullptr)
        {
            setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
            return;
        }
        const auto overPort = portAt (position, false).node != 0 || portAt (position, true).node != 0;
        setMouseCursor (overPort ? juce::MouseCursor::CrosshairCursor
                                 : nodeAt (position) != nullptr ? juce::MouseCursor::DraggingHandCursor
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
            if (const auto* node = graph.find (selectedNode); node != nullptr && canRemove (*node))
            {
                removeNodeNow (*node);
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
            if (! isShown (*it))
                continue;
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
            if (isShown (*it) && nodeBounds (*it).contains (position))
                return &*it;
        return nullptr;
    }

    int cableAt (juce::Point<float> position) const
    {
        const auto& cables = graph.cables();
        const auto feedback = graph.feedbackCables();
        auto best = -1;
        auto bestDistance = 6.0f;
        for (size_t c = 0; c < cables.size(); ++c)
        {
            const auto path = cablePathOf (cables[c], feedback[c]);
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
    //==========================================================================
    // Cable motion. Each cable carries a sideways/vertical offset that is
    // zero at rest; the middle of the cable is displaced by it (nothing at
    // the ports). Moving a cable's ends kicks the offset against the motion,
    // and a spring with some damping brings it back, overshooting a little.
    struct Swing
    {
        juce::Point<float> offset, velocity, lastMid;
        bool seen = false;
        bool moving() const { return offset.x != 0.0f || offset.y != 0.0f || velocity.x != 0.0f || velocity.y != 0.0f; }
    };
    using SwingKey = std::array<int, 4>;
    static constexpr SwingKey pendingKey { -1, -1, -1, -1 };

    static SwingKey swingKey (const PatchGraph::Cable& cable)
    {
        return { (int) cable.from.node, cable.from.port, (int) cable.to.node, cable.to.port };
    }

    juce::Point<float> swingOf (const PatchGraph::Cable& cable) const
    {
        const auto found = swings.find (swingKey (cable));
        return found != swings.end() ? found->second.offset : juce::Point<float>();
    }

    void wakeCables()
    {
        if (motion && ! isTimerRunning() && isShowing())
            startTimerHz (60);
    }

    // One step for one cable whose ends are `from` and `to` (view coordinates).
    // Returns whether it is still moving.
    bool stepSwing (Swing& swing, juce::Point<float> from, juce::Point<float> to, float dt)
    {
        constexpr float stiffness = 150.0f, damping = 7.0f, kick = 0.55f, reach = 40.0f;
        const auto mid = (from + to) * 0.5f - origin; // (panning the view moves everything together: no swing)
        if (! swing.seen)
        {
            swing.lastMid = mid;
            swing.seen = true;
        }
        const auto moved = mid - swing.lastMid;
        swing.lastMid = mid;
        swing.offset -= moved * kick;

        const auto length = swing.offset.getDistanceFromOrigin();
        if (length > reach)
            swing.offset = swing.offset * (reach / length);

        swing.velocity += (swing.offset * -stiffness - swing.velocity * damping) * dt;
        swing.offset += swing.velocity * dt;

        if (swing.offset.getDistanceFromOrigin() < 0.05f && swing.velocity.getDistanceFromOrigin() < 0.5f)
        {
            swing.offset = {};
            swing.velocity = {};
            return false;
        }
        return true;
    }

    void timerCallback() override
    {
        if (! isShowing())
        {
            swings.clear();
            stopTimer();
            return;
        }

        if (! advanceMotion (juce::jmin (frameSeconds(), 0.04f)))
            stopTimer(); // (the next drag step wakes it again)
    }

public:
    // One physics step of dt seconds; whether any cable is still moving.
    // (The timer calls this; the tests step it by hand.)
    bool advanceMotion (float dt)
    {
        auto active = false;
        std::map<SwingKey, Swing> next;
        for (const auto& cable : graph.cables())
        {
            const auto key = swingKey (cable);
            auto swing = swings.count (key) ? swings[key] : Swing();
            active |= stepSwing (swing, portPosition (cable.from, false), portPosition (cable.to, true), dt);
            if (swing.seen)
                next[key] = swing;
        }
        if (drag.kind == DragKind::cable)
        {
            auto swing = swings.count (pendingKey) ? swings[pendingKey] : Swing();
            active |= stepSwing (swing, portPosition (drag.from, false), drag.mouse, dt);
            next[pendingKey] = swing;
        }
        const auto wasMoving = std::any_of (swings.begin(), swings.end(), [] (const auto& entry) { return entry.second.moving(); });
        swings = std::move (next);

        if (active || wasMoving)
            repaint();
        return active;
    }

    // Whether any cable is swinging (so a frame timer is wanted).
    bool isAnimating() const
    {
        return std::any_of (swings.begin(), swings.end(), [] (const auto& entry) { return entry.second.moving(); });
    }

    // The largest swing offset of any cable, in graph pixels (0 at rest).
    float largestSwing() const
    {
        auto largest = 0.0f;
        for (const auto& entry : swings)
            largest = juce::jmax (largest, entry.second.offset.getDistanceFromOrigin());
        return largest;
    }

    // Cable sag and swing can be turned off (the cables then draw the plain
    // curves of step A and no timer ever runs).
    void setMotion (bool on)
    {
        motion = on;
        swings.clear();
        stopTimer();
        repaint();
    }

private:

    // The route with its sag and swing applied: a smooth curve through points
    // sampled along it, pushed down by the sag and sideways by the swing,
    // most in the middle and not at all at the ports.
    static juce::Path displaced (const juce::Path& route, juce::Point<float> push)
    {
        if (push.x == 0.0f && push.y == 0.0f)
            return route;

        const auto length = route.getLength();
        const auto count = juce::jlimit (8, 48, juce::roundToInt (length / 8.0f));
        std::vector<juce::Point<float>> points;
        points.reserve ((size_t) count + 1);
        for (int i = 0; i <= count; ++i)
        {
            const auto along = (float) i / (float) count;
            points.push_back (route.getPointAlongPath (length * along) + push * std::sin (juce::MathConstants<float>::pi * along));
        }

        juce::Path path;
        path.startNewSubPath (points.front());
        for (size_t i = 1; i + 1 < points.size(); ++i)
            path.quadraticTo (points[i], (points[i] + points[i + 1]) * 0.5f);
        path.lineTo (points.back());
        return path;
    }

    // A forward cable that is not routed round the boxes hangs: the sag
    // grows with its length up to 14 px.
    float sagFor (juce::Point<float> from, juce::Point<float> to) const { return motion ? juce::jmin (14.0f, from.getDistanceFrom (to) * 0.06f) : 0.0f; }

    juce::Path cablePathOf (const PatchGraph::Cable& cable, bool loop) const
    {
        const auto from = portPosition (cable.from, false), to = portPosition (cable.to, true);
        return cablePath (from, to, loop, swingOf (cable));
    }

    std::map<SwingKey, Swing> swings;
    bool motion = true;

    enum class DragKind { none, node, cable, pan, loopLevel };

    bool isShown (const PatchGraph::Node& node) const { return showInputNode || ! node.isInput(); }
    bool canRemove (const PatchGraph::Node& node) const { return ! node.isFixed() && nodesRemovable && (! removable || removable (node)); }

    struct Drag
    {
        DragKind kind = DragKind::none;
        PatchGraph::NodeId node = 0;
        juce::Point<float> grabOffset;
        bool moved = false;
        PatchGraph::Port from;
        int pickedCable = -1; // a cable lifted off an input port, or -1 for a new one
        float pickedGain = 1.0f;
        float pickedDelay = 0.0f;
        juce::Point<float> mouse;
    };

    void removeNodeNow (const PatchGraph::Node& node)
    {
        const auto id = node.id;
        selectedNode = 0;
        if (onRemove)
            return onRemove (node);
        if (graph.removeNode (id))
            edited (Edit::nodes);
    }

    // A loop's level from its tag: live, inside one gesture.
    void setLoopLevel (int index, float level)
    {
        if (! juce::isPositiveAndBelow (index, (int) graph.cables().size()) || graph.cables()[(size_t) index].gain == level)
            return;
        if (! drag.moved && onGesture)
            onGesture (true);
        drag.moved = true;
        graph.setCableGain (index, level);
        edited (Edit::loop);
    }

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
        drag.pickedDelay = picked >= 0 ? graph.cables()[(size_t) picked].delayMs : 0.0f;
        drag.mouse = position;
        selectedCable = -1;
        repaint();
    }

    void finishCableDrag (PatchGraph::Port to)
    {
        if (acceptGraph)
        {
            // The edit on a copy first: refused, nothing changes.
            auto trial = graph;
            auto edits = false;
            if (drag.pickedCable >= 0 && drag.pickedCable < (int) trial.cables().size())
            {
                const auto old = trial.cables()[(size_t) drag.pickedCable];
                if (to.node != 0 && to == old.to)
                    return;
                trial.disconnect (old.from, old.to);
                edits = true;
            }
            edits = (to.node != 0 && trial.connect (drag.from, to, drag.pickedGain) == PatchGraph::ConnectResult::ok) || edits;
            if (edits && ! acceptGraph (trial))
            {
                if (onRefused)
                    onRefused();
                return;
            }
        }

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
            graph.setCableDelay (graph.findCable (drag.from, to), drag.pickedDelay);
            selectCable (graph.findCable (drag.from, to));
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
            if (canRemove (*node))
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
                    if (onBypass)
                        return onBypass (*target);
                    graph.setBypassed (id, ! target->bypassed);
                    edited (Edit::nodes);
                }
                else if (result == removeNode)
                {
                    removeNodeNow (*target);
                }
            });
            return;
        }

        if (const auto c = cableAt (position); c >= 0)
        {
            selectCable (c);
            const auto cable = graph.cables()[(size_t) c];
            if (graph.feedbackCables()[(size_t) c])
                menu.addItem (-1, "Loop: " + loopDelayText (cable) + " delay (set it below)", false);
            // (A loop's cable stays under 100 %, or a loop through gain would grow.)
            const bool loop = graph.feedbackCables()[(size_t) c];
            const float levels[] = { 1.0f, 0.7f, 0.5f, 0.25f };
            if (cableLevels && (! hasLevel || hasLevel (cable)))
            {
                juce::PopupMenu gains;
                for (int i = loop ? 1 : 0; i < 4; ++i)
                    gains.addItem (gainBase + i, juce::String (juce::roundToInt (levels[i] * 100.0f)) + " %", true, cable.gain == levels[i]);
                menu.addSubMenu ("Level", gains);
                menu.addSeparator();
            }
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

    juce::Path cablePath (juce::Point<float> from, juce::Point<float> to, bool loop = false, juce::Point<float> swing = {}) const
    {
        juce::Path path;
        path.startNewSubPath (from);
        const auto dx = to.x - from.x;
        if (dx >= 20.0f)
        {
            const auto pull = juce::jmax (30.0f, dx * 0.5f);
            path.cubicTo (from.translated (pull, 0.0f), to.translated (-pull, 0.0f), to);

            // A long cable that would run behind a box goes round the patch
            // instead: out, along a lane above or below every box, and in.
            if (dx >= 200.0f && passesBehindNode (path, from, to))
            {
                const auto content = contentBounds().translated (origin.x, origin.y);
                const auto middle = (from.y + to.y) * 0.5f;
                const auto above = middle - content.getY() < content.getBottom() - middle;
                // (Cables from different heights keep apart on the lane.)
                const auto spread = (float) (juce::roundToInt (from.y / grid) % 4) * 8.0f;
                const auto lane = above ? content.getY() - 16.0f - spread : content.getBottom() + 16.0f + spread;
                juce::Path round;
                round.startNewSubPath (from);
                round.cubicTo (from.translated (40.0f, 0.0f), { from.x + 40.0f, lane }, { from.x + 80.0f, lane });
                round.lineTo (to.x - 80.0f, lane);
                round.cubicTo ({ to.x - 40.0f, lane }, to.translated (-40.0f, 0.0f), to);
                return displaced (round, swing);
            }

            // (a loop cable runs under the boxes already, so it does not hang)
            return displaced (path, swing + juce::Point<float> (0.0f, loop ? 0.0f : sagFor (from, to)));
        }
        else if (! loop && std::abs (to.y - from.y) >= nodeHeight)
        {
            // A cable to a box that sits a little to the left, above or
            // below: one S-bend between them (under every box it ran around
            // the whole patch and read as a loop: ilana's Smear to EQ).
            const auto pull = 60.0f - dx * 0.5f;
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
        return displaced (path, swing);
    }

    // Whether a cable's curve crosses a box other than the two it joins.
    bool passesBehindNode (const juce::Path& path, juce::Point<float> from, juce::Point<float> to) const
    {
        const auto length = path.getLength();
        for (const auto& node : graph.nodes())
        {
            if (! isShown (node))
                continue;
            const auto box = nodeBounds (node).reduced (2.0f);
            if (box.expanded (8.0f).contains (from) || box.expanded (8.0f).contains (to))
                continue;
            for (int step = 1; step < 32; ++step)
                if (box.contains (path.getPointAlongPath (length * (float) step / 32.0f)))
                    return true;
        }
        return false;
    }

    void paintCable (juce::Graphics& g, const PatchGraph::Cable& cable, bool loop, bool selected) const
    {
        using namespace IlanaTheme;
        const auto path = cablePathOf (cable, loop);
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

            // The tag: level | delay; the accent while the guard holds the loop.
            const auto ringing = loopGuardOf && loopGuardOf (cable) < 0.98f;
            const auto box = loopTagBounds (cable);
            g.setColour (Ui::raised);
            g.fillRoundedRectangle (box, 10.0f);
            g.setColour (ringing ? accent() : selected ? colour : Ui::line);
            g.drawRoundedRectangle (box.reduced (0.5f), 10.0f, ringing || selected ? 1.5f : 1.0f);
            auto halves = box.reduced (6.0f, 0.0f);
            const auto left = halves.removeFromLeft (halves.getWidth() * 0.42f);
            g.setFont (font (TextSize::tiny, true, true));
            g.setColour (ringing ? accent() : Ui::text);
            g.drawText (juce::String (juce::roundToInt (cable.gain * 100.0f)) + "%", left, juce::Justification::centred, false);
            g.setColour (Ui::line);
            g.fillRect (left.getRight(), box.getY() + 5.0f, 1.0f, box.getHeight() - 10.0f);
            g.setColour (Ui::text2);
            g.drawText (loopDelayText (cable), halves, juce::Justification::centred, false);
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

            juce::String status = statusOf ? statusOf (node) : node.bypassed ? "BYPASSED" : juce::String();
            if (status.isEmpty() && cpuOf && ! statusOf)
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
            if (! isShown (node))
                continue;
            const auto width = node.isFixed() ? fixedWidth : nodeWidth;
            const juce::Rectangle<float> box (node.x, node.y, width, nodeHeight);
            area = area.isEmpty() ? box : area.getUnion (box);
        }
        return area;
    }

    // The view's area in unzoomed coordinates.
    juce::Rectangle<float> viewArea() const { return getLocalBounds().toFloat() / zoom; }

    PatchGraph::Graph& graph;
    float zoom = 1.0f;
    juce::Point<float> origin { 20.0f, 20.0f };
    PatchGraph::NodeId selectedNode = 0;
    int selectedCable = -1;
    PatchGraph::Port selectedFrom, selectedTo; // the selected cable's ends (it survives a refresh)
    float loopLatencyMs = 1.3f;
    Drag drag;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatchCableView)
};
