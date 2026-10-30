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
//                                be moved to another input or dropped off; a
//                                port with no cable starts one backwards:
//                                drop it on an output port (ilana 2026-10-15)
//   drag a node                  moves it on the 10 px grid
//   click a cable or node        selects it; Delete or Backspace removes it
//   right-click                  a cable: remove, its gain, its loop delay;
//                                a node: bypass, remove, the owner's items;
//                                empty space: the owner's add menu
//   drag (or scroll) a loop's    its level (ilana 2026-10-11: every loop
//   tag up and down              gets a level she can reach on the cable;
//                                the wheel only once the tag is selected)
//   drop a box on a cable        lifts it out of where it was (its inputs
//                                joined to its outputs) and puts it in the
//                                cable (ilana 2026-10-12: "reorder freely")
//
// Navigation (ilana 2026-10-12: "better patcher navigation and zooming and
// touchpad support"), the convention of Figma and most canvas apps:
//   scroll, two-finger swipe     pans (shift turns a mouse wheel sideways)
//   ctrl / cmd + scroll, pinch   zooms around the pointer (a Windows
//                                touchpad's pinch arrives as ctrl + wheel)
//   drag empty space, middle     pans
//   drag, space + drag
//   F, 0, + and -                fit, 100 %, zoom in and out
// The view's position and zoom glide to a new place (FIT, the buttons) on
// the same frame timer as the cables, so nothing ticks while it is still.
// While part of the patch is off screen a small map in the corner shows
// where the view is; click or drag in it to move there.
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

    // The zoom range the user can reach (1 = design size).
    static constexpr float minViewZoom = 0.3f, maxViewZoom = 2.5f;

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
    // A cable the graph itself won't take (duplicate: the two ports are already
    // joined; full: too many cables). Nothing changes; the page says why.
    std::function<void (PatchGraph::ConnectResult)> onConnectRefused;
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
    // Optional: the user moved or zoomed the view (scroll, pinch, drag, keys;
    // not the owner's own fits), so the owner stops refitting it by itself.
    std::function<void()> onNavigated;
    // Optional: F (or the owner's FIT button) fits the patch the owner's way;
    // unset, fitContent().
    std::function<void()> onFitRequested;
    // Optional: whether a box may be dropped into a cable (unset: any box the
    // user can move, into any forward cable not already on it).
    std::function<bool (const PatchGraph::Node&, const PatchGraph::Cable&)> canSplice;
    // A drag on a loop's tag: true at its start, false at its end; the Edit::loop
    // edits come between (the owner makes them one undo step).
    std::function<void (bool)> onGesture;
    // The highest level a loop cable takes (the owner's limit).
    float maxLoopLevel = 0.7f;
    // Optional: modulation (the PATCH tab's LFO boxes). A cable out of a node
    // for which isModSource is true is a modulation cable: it lands on the
    // top edge of the box it goes to (not on its audio input), is drawn thin
    // with a tag (modTagOf) by its end, and is dropped anywhere on a box.
    // decorateNode paints into a node's box after its name (e.g. a wave).
    std::function<bool (const PatchGraph::Node&)> isModSource;
    std::function<juce::String (const PatchGraph::Cable&)> modTagOf;
    std::function<void (juce::Graphics&, const PatchGraph::Node&, juce::Rectangle<float>)> decorateNode;
    std::function<bool (const PatchGraph::Node&)> bypassable;

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
        stopGlide();
        origin = viewArea().getCentre() - content.getCentre();
        origin = { std::round (origin.x / grid) * grid, std::round (origin.y / grid) * grid };
        repaint();
    }

    // The graph point under a point of this view (view pixels).
    juce::Point<float> graphPointAt (juce::Point<float> viewPosition) const { return viewPosition / zoom - origin; }

    // Picks a box up as if it had been pressed on: the mouse events that follow
    // (mouseDrag, mouseUp, forwarded by the owner) drag it, and it drops into a
    // cable like any other. For the owner's row of effects to drag from.
    bool beginNodeDrag (PatchGraph::NodeId id)
    {
        const auto* node = graph.find (id);
        if (node == nullptr || node->isFixed() || (movable && ! movable (*node)))
            return false;
        finishGlide();
        selectedNode = id;
        selectedCable = -1;
        drag = {};
        drag.kind = DragKind::node;
        drag.node = id;
        drag.grabOffset = { nodeWidth * 0.5f, nodeHeight * 0.5f };
        drag.start = { node->x, node->y };
        repaint();
        return true;
    }

    // Zoom: the whole view drawn larger (1 = design size). Positions in the
    // hit tests and geometry below are in unzoomed view coordinates.
    void setZoom (float newZoom)
    {
        stopGlide();
        zoom = juce::jlimit (minViewZoom, maxViewZoom, newZoom);
        repaint();
    }
    float getZoom() const { return zoom; }

    // Zooms to newZoom keeping the graph point under `at` (view pixels) where
    // it is; animated: glides there.
    void zoomAround (float newZoom, juce::Point<float> at, bool animated = false)
    {
        const auto fromZoom = animated && gliding ? glideZoom : zoom;
        const auto fromOrigin = animated && gliding ? glideOrigin : origin;
        newZoom = juce::jlimit (minViewZoom, maxViewZoom, newZoom);
        const auto pinned = at / fromZoom - fromOrigin; // the graph point under `at`
        moveCamera (newZoom, at / newZoom - pinned, animated);
    }

    // One step in or out (the buttons and keys), around the middle of the view.
    void zoomStep (int direction)
    {
        const auto current = gliding ? glideZoom : zoom;
        zoomAround (current * std::pow (1.25f, (float) direction), getLocalBounds().toFloat().getCentre(), true);
        navigated();
    }

    // Back to the design size, around the middle of the view.
    void zoomToActualSize()
    {
        zoomAround (1.0f, getLocalBounds().toFloat().getCentre(), true);
        navigated();
    }

    // Moves the view by a distance in view pixels.
    void panBy (juce::Point<float> delta)
    {
        const auto base = gliding ? glideOrigin : origin;
        const auto z = gliding ? glideZoom : zoom;
        moveCamera (z, base + delta / z, false);
    }

    // The zoom it is at or gliding to.
    float getTargetZoom() const { return gliding ? glideZoom : zoom; }
    bool isGliding() const { return gliding; }
    // Ends a glide where it was going.
    void finishGlide()
    {
        if (gliding)
        {
            zoom = glideZoom;
            origin = glideOrigin;
            gliding = false;
            repaint();
        }
    }

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

    // The same, into a part of the view (view coordinates, e.g. what an open
    // window leaves free), with its own margins and zoom steps.
    void fitContentInto (juce::Rectangle<int> area, float maxZoom, float minZoom, float padX, float padY, float step, bool animated = false)
    {
        const auto content = contentBounds();
        if (content.isEmpty() || area.getWidth() <= 0 || area.getHeight() <= 0)
            return;
        const auto fit = juce::jmin ((float) area.getWidth() / (content.getWidth() + padX),
                                     (float) area.getHeight() / (content.getHeight() + padY));
        const auto newZoom = juce::jlimit (minZoom, juce::jmax (minZoom, maxZoom), std::floor (fit / step) * step);
        auto newOrigin = area.toFloat().getCentre() / newZoom - content.getCentre();
        newOrigin = { std::round (newOrigin.x / grid) * grid, std::round (newOrigin.y / grid) * grid };
        moveCamera (newZoom, newOrigin, animated);
    }

    // The nodes' box in unzoomed coordinates (before the origin).
    juce::Rectangle<float> getContentArea() const { return contentBounds(); }

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
        const auto slide = slideOf (node.id);
        return { origin.x + node.x + slide.x, origin.y + node.y + slide.y, width, nodeHeight };
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

    // Where a modulation cable lands on a box: the middle of its top edge.
    // Where a modulation cable from `from` lands on box `id`: the middle of
    // its top edge, or of its bottom edge when the cable comes from below.
    juce::Point<float> modPortPosition (PatchGraph::NodeId id, juce::Point<float> from = {}) const
    {
        const auto* node = graph.find (id);
        if (node == nullptr)
            return {};
        const auto bounds = nodeBounds (*node);
        return { bounds.getCentreX(), from.y > bounds.getBottom() ? bounds.getBottom() : bounds.getY() };
    }

    bool isModCable (const PatchGraph::Cable& cable) const
    {
        const auto* from = graph.find (cable.from.node);
        return isModSource && from != nullptr && isModSource (*from);
    }

    // The box a cable's drag ends on: an input port, or for a modulation
    // cable any box that isn't a fixed one or another source of modulation.
    PatchGraph::Port dropTargetAt (juce::Point<float> position) const
    {
        if (drag.reverse)
        {
            const auto out = portAt (position, false);
            return out.node != drag.from.node ? out : PatchGraph::Port();
        }
        const auto* from = graph.find (drag.from.node);
        if (from != nullptr && isModSource && isModSource (*from))
        {
            const auto* target = nodeAt (position);
            if (target == nullptr || target->isFixed() || isModSource (*target) || target->numInputs <= 0)
                return {};
            return { target->id, 0 };
        }
        return portAt (position, true);
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
        // (A tag zoomed far out stays at least 30 x 16 screen pixels to hit.)
        const auto least = juce::Point<float> (30.0f, 16.0f) / zoom;
        for (size_t c = 0; c < graph.cables().size(); ++c)
            if (feedback[c])
            {
                const auto bounds = loopTagBounds (graph.cables()[c]);
                if (bounds.withSizeKeepingCentre (juce::jmax (bounds.getWidth(), least.x), juce::jmax (bounds.getHeight(), least.y)).contains (position))
                    return (int) c;
            }
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
        if (drag.kind == DragKind::node && juce::isPositiveAndBelow (drag.spliceCable, (int) cables.size()))
        {
            // The cable a dragged box would drop into: a soft glow under a bright line.
            const auto path = cablePathOf (cables[(size_t) drag.spliceCable], false);
            const auto* box = graph.find (drag.node);
            const auto colour = box != nullptr ? colourFor (*box) : Ui::text;
            g.setColour (colour.withAlpha (0.22f));
            g.strokePath (path, juce::PathStrokeType (10.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (colour);
            g.strokePath (path, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        for (const auto& node : graph.nodes())
            if (isShown (node))
                paintNode (g, node);

        if (drag.kind == DragKind::cable)
        {
            const auto from = portPosition (drag.from, drag.reverse);
            const auto* source = graph.find (drag.from.node);
            const auto colour = source != nullptr ? colourFor (*source) : Ui::text2;
            g.setColour (colour.withAlpha (0.9f));
            const auto pending = swings.find (pendingKey);
            g.strokePath (cablePath (drag.reverse ? drag.mouse : from, drag.reverse ? from : drag.mouse, false, pending != swings.end() ? pending->second.offset : juce::Point<float>()), juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        g.addTransform (juce::AffineTransform::scale (1.0f / zoom));
        paintMap (g);
    }

    //==========================================================================
    // The map: the whole patch small in the bottom-right corner, with the part
    // in view framed, while some of it is off screen (view pixels).
    juce::Rectangle<float> mapBounds() const
    {
        const auto content = contentBounds();
        if (content.isEmpty() || contentFits() || getWidth() < 360 || getHeight() < 200)
            return {};
        const auto world = mapWorld();
        const auto scale = juce::jmin (mapMaxWidth / world.getWidth(), mapMaxHeight / world.getHeight());
        const auto size = juce::Point<float> (world.getWidth() * scale, world.getHeight() * scale) + juce::Point<float> (2.0f * mapPad, 2.0f * mapPad);
        return { (float) getWidth() - mapMargin - size.x, (float) getHeight() - mapMargin - size.y, size.x, size.y };
    }

    // Moves the view so the map point `at` (view pixels) is in its middle.
    void centreOnMapPoint (juce::Point<float> at)
    {
        const auto map = mapBounds();
        if (map.isEmpty())
            return;
        const auto world = mapWorld();
        const auto inner = map.reduced (mapPad);
        const auto scale = inner.getWidth() / world.getWidth();
        const auto target = world.getPosition() + (at - inner.getPosition()) / scale; // graph coordinates
        moveCamera (zoom, viewArea().getCentre() - target, false);
    }

    //==========================================================================
    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        finishGlide();
        const auto position = e.position / zoom;

        // Middle button or space held: pan from anywhere.
        if (e.mods.isMiddleButtonDown() || (spaceHeld && e.mods.isLeftButtonDown()))
        {
            startPan (e.position);
            return;
        }

        if (const auto map = mapBounds(); map.contains (e.position) && ! e.mods.isPopupMenu())
        {
            drag = {};
            drag.kind = DragKind::map;
            centreOnMapPoint (e.position);
            navigated();
            return;
        }

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
                if (cables[(size_t) c].to == in && ! isModCable (cables[(size_t) c]))
                {
                    startCableDrag (cables[(size_t) c].from, c, position);
                    return;
                }
            startCableDrag (in, -1, position);
            drag.reverse = true;
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
            drag.start = { node->x, node->y };
            repaint();
            return;
        }

        selectCable (cableAt (position));
        selectedNode = 0;
        if (selectedCable < 0)
            startPan (e.position); // empty space: drag to look around a big patch
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const auto position = e.position / zoom;
        if (const auto* node = nodeAt (position))
        {
            if (! node->isFixed() && onOpen)
                onOpen (*node);
        }
        else if (cableAt (position) < 0 && loopTagAt (position) < 0 && ! mapBounds().contains (e.position))
        {
            // Empty space: the whole patch back in view.
            if (onFitRequested)
                onFitRequested();
            else
                fitContent();
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (drag.kind == DragKind::node)
        {
            const auto target = e.position / zoom - drag.grabOffset - origin;
            const juce::Point<float> snapped { std::round (target.x / grid) * grid, std::round (target.y / grid) * grid };
            // A click that wobbles a pixel is not a move (no edit, no undo step).
            if (! drag.moved && (e.getDistanceFromDragStart() <= 2 || snapped == drag.start))
                return;
            graph.moveNode (drag.node, snapped.x, snapped.y);
            drag.moved = true;
            if (const auto target = spliceTargetAt (e.position / zoom); target != drag.spliceCable)
            {
                drag.spliceCable = target;
                previewSplice();
            }
            wakeCables();
            repaint();
        }
        else if (drag.kind == DragKind::map)
        {
            centreOnMapPoint (e.position);
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
            // (Not snapped: the grid follows the view, a snapped pan would stutter.)
            origin = drag.grabOffset + e.position / zoom;
            if (! drag.moved && e.getDistanceFromDragStart() > 2)
                navigated();
            drag.moved = drag.moved || e.getDistanceFromDragStart() > 2;
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
        if (drag.kind == DragKind::node && drag.moved)
        {
            const auto spliced = drag.spliceCable >= 0 && spliceInto (drag.node, drag.spliceCable);
            if (! spliced)
            {
                settleSlides ({});
                if (onEdited)
                    onEdited (Edit::layout);
            }
        }
        if (drag.kind == DragKind::pan || drag.kind == DragKind::map)
        {
            drag = {};
            updateCursor (e.position);
            repaint();
            return;
        }

        if (drag.kind == DragKind::cable)
            finishCableDrag (dropTargetAt (e.position / zoom));

        drag = {};
        repaint();
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        // The deltas, not their signs: a mouse notch (about 0.23 on Windows)
        // and a touchpad's stream of small events move the view alike.
        const auto horizontal = std::abs (wheel.deltaX) > std::abs (wheel.deltaY);
        const auto tag = loopTagAt (e.position / zoom);
        const auto onTag = tag >= 0 && tag == selectedCable && nodeAt (e.position / zoom) == nullptr;
        if (e.mods.isCommandDown() || e.mods.isCtrlDown())
        {
            const auto delta = juce::jlimit (-1.0f, 1.0f, horizontal ? -wheel.deltaX : wheel.deltaY);
            zoomAround (zoom * std::exp (delta * wheelZoomRate), e.position);
            navigated();
            return;
        }
        if (! onTag)
        {
            auto delta = juce::Point<float> (wheel.deltaX, wheel.deltaY);
            if (e.mods.isShiftDown() && wheel.deltaX == 0.0f)
                delta = { wheel.deltaY, 0.0f }; // a mouse wheel, sideways
            if (wheel.isReversed)
                delta = -delta;
            delta.x = juce::jlimit (-1.0f, 1.0f, delta.x);
            delta.y = juce::jlimit (-1.0f, 1.0f, delta.y);
            panBy (delta * wheelPanRate);
            navigated();
            return;
        }
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

    // A pinch on a Mac touchpad (or a touch screen): zoom around the fingers.
    void mouseMagnify (const juce::MouseEvent& e, float scaleFactor) override
    {
        if (scaleFactor <= 0.0f || ! std::isfinite (scaleFactor))
            return;
        zoomAround (zoom * scaleFactor, e.position);
        navigated();
    }

    void mouseMove (const juce::MouseEvent& e) override { updateCursor (e.position); }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (hoverNode != 0 || hoverCable >= 0 || hoverPort.node != 0)
        {
            hoverNode = 0;
            hoverCable = -1;
            hoverPort = {};
            repaint();
        }
    }

    bool keyStateChanged (bool) override
    {
        const auto held = juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::spaceKey);
        if (held != spaceHeld)
        {
            spaceHeld = held;
            setMouseCursor (held ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
        }
        return held;
    }

    void focusLost (FocusChangeType) override { spaceHeld = false; }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (! key.getModifiers().isCommandDown() && ! key.getModifiers().isCtrlDown() && ! key.getModifiers().isAltDown())
        {
            // (The typed character, or the key's code when none comes with it.)
            const auto code = key.getKeyCode();
            const auto c = key.getTextCharacter() != 0 ? key.getTextCharacter()
                         : code == juce::KeyPress::numberPadAdd ? (juce::juce_wchar) '+'
                         : code == juce::KeyPress::numberPadSubtract ? (juce::juce_wchar) '-'
                         : code == juce::KeyPress::numberPad0 ? (juce::juce_wchar) '0'
                                                              : (juce::juce_wchar) code;
            if (c == 'f' || c == 'F')
            {
                if (onFitRequested)
                    onFitRequested();
                else
                    fitContent();
                return true;
            }
            if (c == '0')
                return zoomToActualSize(), true;
            if (c == '+' || c == '=')
                return zoomStep (1), true;
            if (c == '-' || c == '_')
                return zoomStep (-1), true;
            if (key == juce::KeyPress::spaceKey)
                return true; // (held: pans)
        }
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
        const auto reach = (portRadius + 4.0f) / zoom; // (screen pixels, whatever the zoom)
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
        auto bestDistance = 6.0f / zoom; // (6 screen pixels)
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
        if (motion && IlanaAnim::fullMotion() && ! isTimerRunning() && isShowing())
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
            finishGlide();
            stopTimer();
            return;
        }

        const auto dt = juce::jmin (frameSeconds(), 0.04f);
        const auto camera = stepGlide (dt);
        const auto sliding = stepSlides (dt);
        if (! advanceMotion (dt) && ! camera && ! sliding)
            stopTimer(); // (the next drag step or glide wakes it again)
    }

    //==========================================================================
    // The camera: where the view looks and how close. A glide eases the
    // middle of the view and the zoom (in log steps, so zooming in and out
    // feel the same) towards the target, about 90 % of the way in 160 ms.
    void moveCamera (float newZoom, juce::Point<float> newOrigin, bool animated)
    {
        newZoom = juce::jlimit (minViewZoom, maxViewZoom, newZoom);
        if (! std::isfinite (newOrigin.x) || ! std::isfinite (newOrigin.y))
            return;
        if (animated && isShowing())
        {
            glideZoom = newZoom;
            glideOrigin = newOrigin;
            gliding = true;
            if (! isTimerRunning())
                startTimerHz (60);
            return;
        }
        gliding = false;
        zoom = newZoom;
        origin = newOrigin;
        repaint();
    }

    void stopGlide() { gliding = false; }

public:
    // One glide step of dt seconds; whether it is still moving (the tests
    // step it by hand).
    bool stepGlide (float dt)
    {
        if (! gliding)
            return false;
        const auto centre = getLocalBounds().toFloat().getCentre();
        const auto from = centre / zoom - origin, to = centre / glideZoom - glideOrigin; // graph points in the middle
        const auto amount = 1.0f - std::exp (-dt * 14.0f);
        const auto logZoom = std::log (zoom) + (std::log (glideZoom) - std::log (zoom)) * amount;
        const auto middle = from + (to - from) * amount;
        zoom = std::exp (logZoom);
        origin = centre / zoom - middle;
        if (std::abs (std::log (glideZoom / zoom)) < 0.002f && (to - middle).getDistanceFromOrigin() < 0.3f)
        {
            zoom = glideZoom;
            origin = glideOrigin;
            gliding = false;
        }
        repaint();
        return gliding;
    }

private:

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
            active |= stepSwing (swing, portPosition (cable.from, false), cableEnd (cable), dt);
            if (swing.seen)
                next[key] = swing;
        }
        if (drag.kind == DragKind::cable)
        {
            auto swing = swings.count (pendingKey) ? swings[pendingKey] : Swing();
            active |= drag.reverse ? stepSwing (swing, drag.mouse, portPosition (drag.from, true), dt) : stepSwing (swing, portPosition (drag.from, false), drag.mouse, dt);
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

    juce::Point<float> cableEnd (const PatchGraph::Cable& cable) const
    {
        return isModCable (cable) ? modPortPosition (cable.to.node, portPosition (cable.from, false)) : portPosition (cable.to, true);
    }

    juce::Path cablePathOf (const PatchGraph::Cable& cable, bool loop) const
    {
        const auto from = portPosition (cable.from, false), to = cableEnd (cable);
        if (isModCable (cable))
            return modCablePath (from, to, swingOf (cable));
        return cablePath (from, to, loop, swingOf (cable));
    }

    // Out of the LFO's port to the right, then down into the top of the box
    // it moves (or up into its bottom).
    juce::Path modCablePath (juce::Point<float> from, juce::Point<float> to, juce::Point<float> swing) const
    {
        juce::Path path;
        path.startNewSubPath (from);
        const auto pullX = juce::jlimit (30.0f, 120.0f, std::abs (to.x - from.x) * 0.5f);
        const auto pullY = juce::jlimit (30.0f, 120.0f, std::abs (to.y - from.y) * 0.6f);
        const auto up = from.y > to.y;
        // (A box to the left: turn straight up or down out of the port, no hook.)
        const auto start = to.x >= from.x ? from.translated (pullX, 0.0f) : from.translated (16.0f, up ? -pullY : pullY);
        path.cubicTo (start, to.translated (0.0f, up ? pullY : -pullY), to);
        return displaced (path, swing + juce::Point<float> (0.0f, sagFor (from, to) * 0.5f));
    }

    std::map<SwingKey, Swing> swings;
    bool motion = true;

    enum class DragKind { none, node, cable, pan, loopLevel, map };

    //==========================================================================
    // Navigation helpers.
    static constexpr float wheelPanRate = 360.0f;   // view pixels for a delta of 1 (a Windows notch: about 85)
    static constexpr float wheelZoomRate = 0.65f;   // e^(0.65 x delta): a Windows notch about x1.16
    static constexpr float mapMaxWidth = 180.0f, mapMaxHeight = 110.0f, mapPad = 8.0f, mapMargin = 12.0f;

    void startPan (juce::Point<float> screen)
    {
        drag = {};
        drag.kind = DragKind::pan;
        drag.grabOffset = origin - screen / zoom;
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }

    void navigated()
    {
        if (onNavigated)
            onNavigated();
    }

    void updateCursor (juce::Point<float> screen)
    {
        setHover (screen / zoom);
        if (spaceHeld)
            return setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        if (mapBounds().contains (screen))
            return setMouseCursor (juce::MouseCursor::PointingHandCursor);
        const auto position = screen / zoom;
        if (const auto tag = loopTagAt (position); tag >= 0 && nodeAt (position) == nullptr)
            return setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        const auto overPort = portAt (position, false).node != 0 || portAt (position, true).node != 0;
        setMouseCursor (overPort ? juce::MouseCursor::CrosshairCursor
                                 : nodeAt (position) != nullptr ? juce::MouseCursor::DraggingHandCursor
                                                                  : juce::MouseCursor::NormalCursor);
    }

    // What the map shows (graph coordinates): the patch and the view.
    juce::Rectangle<float> mapWorld() const
    {
        return contentBounds().getUnion (viewArea() - origin).expanded (20.0f);
    }

    void paintMap (juce::Graphics& g) const
    {
        using namespace IlanaTheme;
        const auto map = mapBounds();
        if (map.isEmpty())
            return;
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.fillRoundedRectangle (map.translated (0.0f, 2.0f), 8.0f);
        g.setColour (Ui::panel.withAlpha (0.94f));
        g.fillRoundedRectangle (map, 8.0f);
        g.setColour (Ui::line);
        g.drawRoundedRectangle (map.reduced (0.5f), 8.0f, 1.0f);

        const auto world = mapWorld();
        const auto inner = map.reduced (mapPad);
        const auto scale = inner.getWidth() / world.getWidth();
        const auto toMap = [&] (juce::Rectangle<float> box)
        {
            return juce::Rectangle<float> (inner.getX() + (box.getX() - world.getX()) * scale, inner.getY() + (box.getY() - world.getY()) * scale,
                                           box.getWidth() * scale, box.getHeight() * scale);
        };
        for (const auto& node : graph.nodes())
            if (isShown (node))
            {
                const auto box = toMap ({ node.x, node.y, node.isFixed() ? fixedWidth : nodeWidth, nodeHeight });
                g.setColour (colourFor (node).withAlpha (node.bypassed ? 0.3f : 0.75f));
                g.fillRoundedRectangle (box, juce::jmin (2.0f, box.getHeight() * 0.5f));
            }
        const auto seen = toMap (viewArea() - origin).getIntersection (inner);
        g.setColour (Ui::text.withAlpha (0.06f));
        g.fillRoundedRectangle (seen, 3.0f);
        g.setColour (Ui::text2);
        g.drawRoundedRectangle (seen.reduced (0.5f), 3.0f, 1.2f);
    }

    //==========================================================================
    // Dropping a box into a cable. The box is lifted first (each cable into
    // it joined to each cable out of it, so the chain it leaves stays whole),
    // then the cable runs through it. Only one-in, one-out boxes off any loop.
    static bool spliceGraph (PatchGraph::Graph& g, PatchGraph::NodeId id, PatchGraph::Port cableFrom, PatchGraph::Port cableTo)
    {
        const auto targetIndex = g.findCable (cableFrom, cableTo);
        if (targetIndex < 0)
            return false;
        const auto gain = g.cables()[(size_t) targetIndex].gain;
        std::vector<PatchGraph::Cable> ins, outs;
        for (const auto& cable : g.cables())
        {
            if (cable.to.node == id)
                ins.push_back (cable);
            if (cable.from.node == id)
                outs.push_back (cable);
        }
        for (const auto& cable : ins)
            g.disconnect (cable.from, cable.to);
        for (const auto& cable : outs)
            g.disconnect (cable.from, cable.to);
        for (const auto& in : ins)
            for (const auto& out : outs)
                if (g.findCable (in.from, out.to) < 0)
                    g.connect (in.from, out.to, in.gain * out.gain);
        g.disconnect (cableFrom, cableTo);
        return g.connect (cableFrom, { id, 0 }, gain) == PatchGraph::ConnectResult::ok
               && g.connect ({ id, 0 }, cableTo, 1.0f) == PatchGraph::ConnectResult::ok;
    }

    static int countLoops (const PatchGraph::Graph& g)
    {
        const auto loops = g.feedbackCables();
        return (int) std::count (loops.begin(), loops.end(), true);
    }

    // The cable a box dragged to `position` would drop into, or -1.
    int spliceTargetAt (juce::Point<float> position) const
    {
        const auto* node = graph.find (drag.node);
        if (node == nullptr || node->isFixed() || node->numInputs != 1 || node->numOutputs != 1)
            return -1;
        const auto& cables = graph.cables();
        const auto feedback = graph.feedbackCables();
        for (size_t c = 0; c < cables.size(); ++c)
            if (feedback[c] && (cables[c].from.node == node->id || cables[c].to.node == node->id))
                return -1; // (lifting it would drop its loop)
        // (The cable it is over keeps it within a wider reach, so the boxes
        // sliding apart to make room can't shake it loose.)
        if (juce::isPositiveAndBelow (drag.spliceCable, (int) cables.size()) && ! feedback[(size_t) drag.spliceCable])
        {
            juce::Point<float> nearest;
            cablePathOf (cables[(size_t) drag.spliceCable], false).getNearestPoint (position, nearest);
            if (nearest.getDistanceFrom (position) < 30.0f)
                return drag.spliceCable;
        }
        auto best = -1;
        auto bestDistance = 16.0f;
        for (size_t c = 0; c < cables.size(); ++c)
        {
            const auto& cable = cables[c];
            if (feedback[c] || cable.from.node == node->id || cable.to.node == node->id)
                continue;
            juce::Point<float> nearest;
            cablePathOf (cable, false).getNearestPoint (position, nearest);
            const auto distance = nearest.getDistanceFrom (position);
            if (distance < bestDistance && (! canSplice || canSplice (*node, cable)))
            {
                auto trial = graph;
                if (spliceGraph (trial, node->id, cable.from, cable.to) && countLoops (trial) <= countLoops (graph)
                    && (! acceptGraph || acceptGraph (trial)))
                {
                    bestDistance = distance;
                    best = (int) c;
                }
            }
        }
        return best;
    }

    bool spliceInto (PatchGraph::NodeId id, int cableIndex)
    {
        if (! juce::isPositiveAndBelow (cableIndex, (int) graph.cables().size()))
            return false;
        const auto cable = graph.cables()[(size_t) cableIndex];
        auto trial = graph;
        if (! spliceGraph (trial, id, cable.from, cable.to) || (acceptGraph && ! acceptGraph (trial)))
            return false;
        const auto before = graph;
        spliceGraph (graph, id, cable.from, cable.to);
        makeRoomFor (graph, id, cable.from.node, cable.to.node, drag.start);
        std::map<PatchGraph::NodeId, juce::Point<float>> moved;
        for (const auto& node : graph.nodes())
            if (const auto* was = before.find (node.id); was != nullptr && node.id != id && (node.x != was->x || node.y != was->y))
                moved[node.id] = { node.x - was->x, node.y - was->y };
        settleSlides (moved);
        selectedNode = id;
        selectedCable = -1;
        wakeCables();
        edited (Edit::cables);
        return true;
    }

    // A dropped box tidied into its new place. First the hole it left: when
    // nothing else stands in its old column, the boxes right of it close up
    // (the nearest lands where it was). Then it goes one gap right of the
    // box that now feeds it, on that box's row, and everything from the box
    // it now feeds onwards moves right as far as it takes (10 px grid).
    void makeRoomFor (PatchGraph::Graph& g, PatchGraph::NodeId id, PatchGraph::NodeId from, PatchGraph::NodeId to, juce::Point<float> was) const
    {
        constexpr float gap = 30.0f;
        const auto width = [] (const PatchGraph::Node& node) { return node.isFixed() ? fixedWidth : nodeWidth; };
        const auto canMove = [this] (const PatchGraph::Node& node) { return ! node.isInput() && (! movable || movable (node)); };

        auto columnFree = true;
        auto nearest = std::numeric_limits<float>::max();
        for (const auto& node : g.nodes())
        {
            if (node.id == id || ! isShown (node))
                continue;
            if (node.x < was.x + nodeWidth && node.x + width (node) > was.x)
                columnFree = false;
            if (node.x > was.x)
                nearest = juce::jmin (nearest, node.x);
        }
        if (columnFree && nearest < std::numeric_limits<float>::max())
        {
            auto movableRight = true;
            for (const auto& node : g.nodes())
                if (node.id != id && node.x > was.x && ! canMove (node))
                    movableRight = false;
            if (movableRight)
            {
                const auto back = std::floor ((nearest - was.x) / grid) * grid;
                for (const auto& node : std::vector<PatchGraph::Node> (g.nodes()))
                    if (node.id != id && node.x > was.x)
                        g.moveNode (node.id, node.x - back, node.y);
            }
        }

        const auto* source = g.find (from);
        const auto* target = g.find (to);
        if (source == nullptr || target == nullptr || g.find (id) == nullptr)
            return;
        const auto x = std::round ((source->x + width (*source) + gap) / grid) * grid;
        const auto y = source->y;
        const auto targetX = target->x;
        g.moveNode (id, x, y);
        const auto shift = std::ceil ((x + nodeWidth + gap - targetX) / grid) * grid;
        if (shift <= 0.0f)
            return;
        for (const auto& node : std::vector<PatchGraph::Node> (g.nodes()))
            if (node.id != id && node.x >= targetX && canMove (node))
                g.moveNode (node.id, node.x + shift, node.y);
    }

    //==========================================================================
    // Slides: a box's drawn place eases to where it is going (the boxes
    // making room while a dragged box hovers a cable, and after a drop), so
    // nothing jumps. Display offsets only; the graph holds the real places.
    juce::Point<float> slideOf (PatchGraph::NodeId id) const
    {
        const auto found = slides.find (id);
        return found != slides.end() ? found->second.offset : juce::Point<float>();
    }

    struct Slide
    {
        juce::Point<float> offset, target;
    };

    // The room a dropped box would make, shown before the drop.
    void previewSplice()
    {
        std::map<PatchGraph::NodeId, juce::Point<float>> targets;
        if (drag.spliceCable >= 0 && drag.spliceCable < (int) graph.cables().size())
        {
            const auto cable = graph.cables()[(size_t) drag.spliceCable];
            auto trial = graph;
            trial.moveNode (drag.node, drag.start.x, drag.start.y);
            if (spliceGraph (trial, drag.node, cable.from, cable.to))
            {
                makeRoomFor (trial, drag.node, cable.from.node, cable.to.node, drag.start);
                for (const auto& node : trial.nodes())
                    if (const auto* now = graph.find (node.id); now != nullptr && node.id != drag.node)
                        if (node.x != now->x || node.y != now->y)
                            targets[node.id] = { node.x - now->x, node.y - now->y };
            }
        }
        for (auto& [id, slide] : slides)
            slide.target = targets.count (id) ? targets[id] : juce::Point<float>();
        for (const auto& [id, target] : targets)
            slides[id].target = target;
        wakeSlides();
    }

    // After the graph moved boxes by `moved`: what was drawn stays put and
    // eases on from there.
    void settleSlides (const std::map<PatchGraph::NodeId, juce::Point<float>>& moved)
    {
        for (const auto& [id, by] : moved)
        {
            auto& slide = slides[id];
            slide.offset -= by;
            slide.target = {};
        }
        for (auto& [id, slide] : slides)
            if (! moved.count (id))
                slide.target = {};
        wakeSlides();
    }

    void wakeSlides()
    {
        if (! isShowing())
        {
            slides.clear(); // (nothing to animate off screen: straight there)
            repaint();
            return;
        }
        if (! isTimerRunning())
            startTimerHz (60);
    }

public:
    // One slide step of dt seconds; whether any box is still sliding.
    bool stepSlides (float dt)
    {
        if (slides.empty())
            return false;
        const auto amount = 1.0f - std::exp (-dt * 16.0f);
        for (auto it = slides.begin(); it != slides.end();)
        {
            auto& slide = it->second;
            slide.offset += (slide.target - slide.offset) * amount;
            if (slide.offset.getDistanceFrom (slide.target) < 0.25f)
                slide.offset = slide.target;
            if (slide.offset == juce::Point<float>() && slide.target == juce::Point<float>())
                it = slides.erase (it);
            else
                ++it;
        }
        repaint();
        return std::any_of (slides.begin(), slides.end(), [] (const auto& entry) { return entry.second.offset != entry.second.target; });
    }

private:
    std::map<PatchGraph::NodeId, Slide> slides;

    // What the pointer is over (drawn a little brighter).
    PatchGraph::NodeId hoverNode = 0;
    int hoverCable = -1;
    PatchGraph::Port hoverPort;
    bool hoverInput = false;

    void setHover (juce::Point<float> position)
    {
        const auto out = portAt (position, false), in = portAt (position, true);
        const auto port = out.node != 0 ? out : in;
        const auto* node = port.node == 0 ? nodeAt (position) : nullptr;
        const auto cable = port.node == 0 && node == nullptr ? cableAt (position) : -1;
        const auto nodeId = node != nullptr ? node->id : (PatchGraph::NodeId) 0;
        if (port == hoverPort && (port.node == 0 || hoverInput == (out.node == 0)) && nodeId == hoverNode && cable == hoverCable)
            return;
        hoverPort = port;
        hoverInput = out.node == 0;
        hoverNode = nodeId;
        hoverCable = cable;
        repaint();
    }

    bool gliding = false, spaceHeld = false;
    float glideZoom = 1.0f;
    juce::Point<float> glideOrigin;

    bool isShown (const PatchGraph::Node& node) const { return showInputNode || ! node.isInput(); }
    bool canRemove (const PatchGraph::Node& node) const { return ! node.isFixed() && nodesRemovable && (! removable || removable (node)); }

    struct Drag
    {
        DragKind kind = DragKind::none;
        PatchGraph::NodeId node = 0;
        juce::Point<float> grabOffset;
        bool moved = false;
        bool reverse = false; // a new cable dragged from an input port to an output port (from is the input)
        PatchGraph::Port from;
        int pickedCable = -1; // a cable lifted off an input port, or -1 for a new one
        float pickedGain = 1.0f;
        float pickedDelay = 0.0f;
        juce::Point<float> mouse;
        int spliceCable = -1; // a dragged box over a cable it would drop into
        juce::Point<float> start; // where a dragged box was
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
        // Dragged backwards, from an input port: the port it lands on is the cable's source.
        if (drag.reverse)
        {
            if (to.node == 0)
                return;
            const auto input = drag.from;
            drag.reverse = false;
            drag.from = to;
            to = input;
        }

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
            if (to.node != 0)
            {
                const auto result = trial.connect (drag.from, to, drag.pickedGain);
                if (result == PatchGraph::ConnectResult::duplicate || result == PatchGraph::ConnectResult::full)
                {
                    // (A picked cable stays where it was, not lost.)
                    if (onConnectRefused)
                        onConnectRefused (result);
                    return;
                }
                edits = result == PatchGraph::ConnectResult::ok || edits;
            }
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
                if (! bypassable || bypassable (*node))
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
                    gains.addItem (gainBase + i, juce::String (juce::roundToInt (levels[i] * 100.0f)) + "%", true, cable.gain == levels[i]);
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
            // (Nor the box being dragged: over a cable, the cable stays under
            // it, lit up as where it drops.)
            if (! isShown (node) || (drag.kind == DragKind::node && node.id == drag.node))
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
        const auto hovered = ! selected && drag.kind == DragKind::none && hoverCable >= 0 && hoverCable < (int) graph.cables().size()
                             && graph.cables()[(size_t) hoverCable].from == cable.from && graph.cables()[(size_t) hoverCable].to == cable.to;
        colour = colour.withAlpha (selected || hovered ? 1.0f : 0.35f + 0.5f * juce::jlimit (0.0f, 1.0f, std::abs (cable.gain)));
        // A cable out of a bypassed or unused box carries nothing: drawn faint, like the box (UI review 27).
        if (source != nullptr && source->bypassed && ! selected && ! hovered)
            colour = colour.withMultipliedAlpha (0.4f);
        const auto width = selected ? 2.6f : hovered ? 2.4f : 2.0f;

        if (isModCable (cable))
        {
            // Thinner, with a dot where it lands, and its tag (the knobs it moves) above the box.
            const auto end = cableEnd (cable);
            g.setColour (colour.withAlpha (selected ? 1.0f : 0.75f));
            g.strokePath (path, juce::PathStrokeType (selected ? 2.2f : 1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (end));
            if (modTagOf)
                if (const auto tag = modTagOf (cable); tag.isNotEmpty())
                {
                    g.setFont (font (TextSize::tiny, true));
                    const auto textWidth = juce::jmin (nodeWidth + 20.0f, (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), tag) + 18.0f);
                    const auto below = portPosition (cable.from, false).y > end.y;
                    const auto box = juce::Rectangle<float> (textWidth, 18.0f).withCentre (end.translated (0.0f, below ? 17.0f : -17.0f));
                    g.setColour (Ui::raised);
                    g.fillRoundedRectangle (box, 9.0f);
                    g.setColour (selected ? colour : Ui::line);
                    g.drawRoundedRectangle (box.reduced (0.5f), 9.0f, 1.0f);
                    g.setColour (Ui::text2);
                    drawFitted (g, tag, box.reduced (8.0f, 0.0f), juce::Justification::centred, 1, TextSize::tiny);
                }
            return;
        }

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

    // A node's text keeps a minimum size on the screen (about 10 px) when the view is
    // zoomed out, up to 1.45 times its design size, so the node types stay readable (S7-10).
    float legibleSize (float designSize) const { return juce::jmin (designSize * 1.45f, juce::jmax (designSize, 10.0f / juce::jmax (0.05f, zoom))); }

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
            const auto hovered = ! selected && node.id == hoverNode && drag.kind == DragKind::none;
            g.setColour (selected ? colour : hovered ? colour.withAlpha (0.45f) : Ui::line);
            g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, selected ? 1.5f : 1.0f);

            auto inner = bounds.reduced (14.0f, 8.0f).toNearestInt();
            auto top = inner.removeFromTop (inner.getHeight() / 2);
            paintTag (g, { (float) top.getX() + 3.0f, (float) top.getCentreY() }, colour.withMultipliedAlpha (alpha));
            g.setColour (Ui::text.withAlpha (alpha));
            g.setFont (font (legibleSize (TextSize::body), true));
            drawFitted (g, juce::String (node.label), top.withTrimmedLeft (12), juce::Justification::centredLeft, 1, TextSize::minPassive);

            juce::String status = statusOf ? statusOf (node) : node.bypassed ? "BYPASSED" : juce::String();
            if (status.isEmpty() && cpuOf && ! statusOf)
                if (const auto cpu = cpuOf (node.id); cpu >= 0.0f)
                    status = juce::String (cpu * 100.0f, cpu < 0.1f ? 2 : 1) + "% CPU";
            g.setColour (Ui::text3);
            g.setFont (font (legibleSize (TextSize::tiny), false, true));
            drawFitted (g, status, inner.withTrimmedLeft (12), juce::Justification::centredLeft, 1, TextSize::minPassive);
            if (decorateNode)
                decorateNode (g, node, bounds);
        }

        // A box an LFO cable is being dragged over.
        if (drag.kind == DragKind::cable && dropTargetAt (drag.mouse).node == node.id)
            if (const auto* source = graph.find (drag.from.node); source != nullptr && isModSource && isModSource (*source))
            {
                g.setColour (colourFor (*source));
                g.drawRoundedRectangle (bounds.expanded (2.0f), 10.0f, 1.5f);
            }

        // Ports: filled while a cable is on them.
        const auto paintPort = [&] (PatchGraph::Port port, bool input)
        {
            const auto centre = portPosition (port, input);
            auto connected = false;
            for (const auto& cable : graph.cables())
                connected = connected || (input ? cable.to == port && ! isModCable (cable) : cable.from == port);
            const auto* dragFrom = graph.find (drag.from.node);
            const auto target = drag.kind == DragKind::cable && input != drag.reverse && portAt (drag.mouse, input) == port
                                && (drag.reverse ? port.node != drag.from.node : ! (isModSource && dragFrom != nullptr && isModSource (*dragFrom)));
            // (A port under the pointer grows a little: it can be grabbed.)
            const auto hovered = drag.kind == DragKind::none && hoverPort == port && hoverInput == input;
            const auto radius = portRadius + (target || hovered ? 1.5f : 0.0f);
            const auto disc = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);
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
