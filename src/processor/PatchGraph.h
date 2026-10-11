#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

// The shared patch graph (roadmap step A): a node-and-cable model that the
// FX patcher (step 17) and the reroutable oscillators (step 16) both build
// on, and the compiler that turns it into a flat list of operations for the
// audio thread (docs/ROUTING-CPU.md, rules A1-A7 and A12).
//
//   Graph        nodes with numbered input and output ports, cables from an
//                output port to an input port (each with a gain). Two fixed
//                nodes: Input (the signal coming in, no inputs) and Output
//                (what leaves, no outputs). Edited on the message thread.
//   compile()    the graph to a Plan: a topological order, a buffer for
//                every signal (pooled by liveness: a buffer is reused once its
//                last reader has run), in-place processing where a node is
//                the only reader of its input, dead branches left out, and a
//                one-chunk delay on every cable that closes a loop. Runs on
//                the message thread; PatchGraphRunner.h swaps the plan in.
//
// No JUCE here: the model and the compiler are plain C++ so the voice side
// (step 16) can use them with its own fixed-size buses.
namespace PatchGraph
{
using NodeId = std::uint32_t;

inline constexpr NodeId inputNodeId = 1;
inline constexpr NodeId outputNodeId = 2;
inline constexpr int inputKind = -1;
inline constexpr int outputKind = -2;
inline constexpr int maxPorts = 4;
inline constexpr int maxNodes = 64;
inline constexpr int maxBuffers = maxNodes + 24; // a fully parallel graph needs one per branch
inline constexpr int maxFeedbackCables = 16;

struct Port
{
    NodeId node = 0;
    int port = 0;

    bool operator== (const Port& other) const { return node == other.node && port == other.port; }
    bool operator!= (const Port& other) const { return ! (*this == other); }
};

struct Node
{
    NodeId id = 0;
    int kind = 0;           // what the node is, for its owner (an FX type, an oscillator...)
    int ref = 0;            // the owner's handle for it (an FX slot, an instance number...)
    int numInputs = 1;
    int numOutputs = 1;
    bool bypassed = false;  // input port n goes straight to output port n, the node doesn't run
    bool keepAlive = false; // runs even when nothing downstream hears it (meters, tails)
    float x = 0.0f, y = 0.0f; // position in the cable view, in design pixels
    std::string label;

    bool isInput() const { return id == inputNodeId; }
    bool isOutput() const { return id == outputNodeId; }
    bool isFixed() const { return isInput() || isOutput(); }
};

struct Cable
{
    Port from; // an output port
    Port to;   // an input port
    float gain = 1.0f;
};

enum class ConnectResult
{
    ok,
    noSuchPort, // a node or port that doesn't exist, or the wrong direction
    duplicate,  // the same two ports are already joined
    full        // too many cables
};

class Graph
{
public:
    Graph()
    {
        nodeList.push_back (makeFixed (inputNodeId, inputKind, 0, 1, "INPUT"));
        nodeList.push_back (makeFixed (outputNodeId, outputKind, 1, 0, "OUTPUT"));
    }

    const std::vector<Node>& nodes() const { return nodeList; }
    const std::vector<Cable>& cables() const { return cableList; }

    // Bumped on every edit, so a view or a compiler can tell when to redo work.
    std::uint64_t revision() const { return revisionCount; }

    const Node* find (NodeId id) const
    {
        for (const auto& node : nodeList)
            if (node.id == id)
                return &node;
        return nullptr;
    }

    Node* find (NodeId id)
    {
        return const_cast<Node*> (static_cast<const Graph&> (*this).find (id));
    }

    // A new node; 0 when the graph is full. Ids are never reused within a
    // graph, so a cable or a saved reference can't land on a newer node.
    NodeId addNode (int kind, int numInputs, int numOutputs, float x = 0.0f, float y = 0.0f, std::string label = {}, int ref = 0)
    {
        if ((int) nodeList.size() >= maxNodes)
            return 0;

        Node node;
        node.id = nextId++;
        node.kind = kind;
        node.ref = ref;
        node.numInputs = std::clamp (numInputs, 0, maxPorts);
        node.numOutputs = std::clamp (numOutputs, 0, maxPorts);
        node.x = x;
        node.y = y;
        node.label = std::move (label);
        nodeList.push_back (std::move (node));
        ++revisionCount;
        return nodeList.back().id;
    }

    // Puts a node back with a known id (loading a saved graph). False when
    // the id is taken or invalid.
    bool restoreNode (const Node& node)
    {
        if (node.id <= outputNodeId || find (node.id) != nullptr || (int) nodeList.size() >= maxNodes)
            return false;

        auto copy = node;
        copy.numInputs = std::clamp (copy.numInputs, 0, maxPorts);
        copy.numOutputs = std::clamp (copy.numOutputs, 0, maxPorts);
        nodeList.push_back (std::move (copy));
        nextId = std::max (nextId, node.id + 1);
        ++revisionCount;
        return true;
    }

    // Removes a node and every cable on it. The fixed Input and Output stay.
    bool removeNode (NodeId id)
    {
        if (id == inputNodeId || id == outputNodeId)
            return false;

        const auto it = std::find_if (nodeList.begin(), nodeList.end(), [id] (const Node& n) { return n.id == id; });
        if (it == nodeList.end())
            return false;

        nodeList.erase (it);
        cableList.erase (std::remove_if (cableList.begin(), cableList.end(),
                                         [id] (const Cable& c) { return c.from.node == id || c.to.node == id; }),
                         cableList.end());
        ++revisionCount;
        return true;
    }

    bool isOutputPort (Port port) const
    {
        const auto* node = find (port.node);
        return node != nullptr && port.port >= 0 && port.port < node->numOutputs;
    }

    bool isInputPort (Port port) const
    {
        const auto* node = find (port.node);
        return node != nullptr && port.port >= 0 && port.port < node->numInputs;
    }

    ConnectResult canConnect (Port from, Port to) const
    {
        if (! isOutputPort (from) || ! isInputPort (to))
            return ConnectResult::noSuchPort;
        if (findCable (from, to) >= 0)
            return ConnectResult::duplicate;
        if ((int) cableList.size() >= maxNodes * maxPorts)
            return ConnectResult::full;
        return ConnectResult::ok;
    }

    // Joins an output port to an input port. Loops are allowed: the compiler
    // gives the cable that closes one a delay (see feedbackCables).
    ConnectResult connect (Port from, Port to, float gain = 1.0f)
    {
        const auto result = canConnect (from, to);
        if (result != ConnectResult::ok)
            return result;

        cableList.push_back ({ from, to, gain });
        ++revisionCount;
        return ConnectResult::ok;
    }

    bool disconnect (Port from, Port to)
    {
        const auto index = findCable (from, to);
        if (index < 0)
            return false;

        cableList.erase (cableList.begin() + index);
        ++revisionCount;
        return true;
    }

    // Removes every cable into one input port (a cable dragged off a port).
    int disconnectInput (Port to)
    {
        const auto before = cableList.size();
        cableList.erase (std::remove_if (cableList.begin(), cableList.end(), [to] (const Cable& c) { return c.to == to; }),
                         cableList.end());
        const auto removed = (int) (before - cableList.size());
        if (removed > 0)
            ++revisionCount;
        return removed;
    }

    int findCable (Port from, Port to) const
    {
        for (size_t i = 0; i < cableList.size(); ++i)
            if (cableList[i].from == from && cableList[i].to == to)
                return (int) i;
        return -1;
    }

    bool setCableGain (int index, float gain)
    {
        if (index < 0 || index >= (int) cableList.size())
            return false;
        cableList[(size_t) index].gain = gain;
        ++revisionCount;
        return true;
    }

    bool setBypassed (NodeId id, bool bypassed)
    {
        auto* node = find (id);
        if (node == nullptr || node->isFixed() || node->bypassed == bypassed)
            return false;
        node->bypassed = bypassed;
        ++revisionCount;
        return true;
    }

    bool setKeepAlive (NodeId id, bool keep)
    {
        auto* node = find (id);
        if (node == nullptr || node->isFixed() || node->keepAlive == keep)
            return false;
        node->keepAlive = keep;
        ++revisionCount;
        return true;
    }

    // Moving a node changes only the view, so it doesn't bump the revision
    // the compiler watches.
    bool moveNode (NodeId id, float x, float y)
    {
        auto* node = find (id);
        if (node == nullptr)
            return false;
        node->x = x;
        node->y = y;
        return true;
    }

    // Which cables close a loop: a depth-first walk from Input (then from
    // every node it didn't reach, in node order), following cables in the
    // order they were made; a cable back to a node still on the walk's path
    // is a feedback cable. Removing them leaves no loop, and the answer is the
    // same every time for the same graph.
    std::vector<bool> feedbackCables() const
    {
        std::vector<bool> feedback (cableList.size(), false);
        std::vector<int> state (nodeList.size(), 0); // 0 not seen, 1 on the path, 2 done

        struct Frame { int node; size_t nextCable; };
        std::vector<Frame> stack;

        const auto walkFrom = [&] (int start)
        {
            stack.push_back ({ start, 0 });
            state[(size_t) start] = 1;
            while (! stack.empty())
            {
                auto& frame = stack.back();
                const auto id = nodeList[(size_t) frame.node].id;
                auto pushed = false;
                while (frame.nextCable < cableList.size())
                {
                    const auto c = frame.nextCable++;
                    if (cableList[c].from.node != id)
                        continue;
                    const auto target = indexOf (cableList[c].to.node);
                    if (target < 0)
                        continue;
                    if (state[(size_t) target] == 1)
                    {
                        feedback[c] = true;
                    }
                    else if (state[(size_t) target] == 0)
                    {
                        state[(size_t) target] = 1;
                        stack.push_back ({ target, 0 });
                        pushed = true;
                        break;
                    }
                }
                if (! pushed)
                {
                    state[(size_t) stack.back().node] = 2;
                    stack.pop_back();
                }
            }
        };

        walkFrom (indexOf (inputNodeId));
        for (size_t i = 0; i < nodeList.size(); ++i)
            if (state[i] == 0)
                walkFrom ((int) i);
        return feedback;
    }

    int indexOf (NodeId id) const
    {
        for (size_t i = 0; i < nodeList.size(); ++i)
            if (nodeList[i].id == id)
                return (int) i;
        return -1;
    }

    void clear()
    {
        *this = Graph();
    }

private:
    static Node makeFixed (NodeId id, int kind, int ins, int outs, const char* label)
    {
        Node node;
        node.id = id;
        node.kind = kind;
        node.numInputs = ins;
        node.numOutputs = outs;
        node.label = label;
        return node;
    }

    std::vector<Node> nodeList;
    std::vector<Cable> cableList;
    NodeId nextId = outputNodeId + 1;
    std::uint64_t revisionCount = 1;
};

//==============================================================================
// The compiled form. Buffer 0 is the host's: the graph's input arrives in it
// and the output must be left in it. Every other index is a scratch buffer
// the runner allocates once per plan.
struct Op
{
    enum Type : std::uint8_t
    {
        clear,         // dst = 0
        copy,          // dst = src * gain
        mix,           // dst += src * gain
        process,       // run node `node` on its port buffers (plan.nodes[node])
        feedbackRead,  // dst = what ring `ring` was given one chunk ago
        feedbackWrite  // ring `ring` takes src (read before write, so the delay is exactly one chunk)
    };

    Type type = clear;
    std::int16_t dst = -1;
    std::int16_t src = -1;
    std::int16_t node = -1;
    std::int16_t ring = -1;
    float gain = 1.0f;
};

struct CompiledNode
{
    NodeId id = 0;
    int kind = 0;
    int ref = 0;
    int numInputs = 0;
    int numOutputs = 0;
    bool inPlace = false;    // one input, one output, the same buffer
    bool audible = true;     // false: kept alive only (meters, tails); its output is not heard
    std::int16_t inputs[maxPorts] = { -1, -1, -1, -1 };
    std::int16_t outputs[maxPorts] = { -1, -1, -1, -1 };
};

struct FeedbackLoop
{
    int cable = -1;        // index in graph.cables()
    Port from, to;
};

struct Plan
{
    std::vector<Op> ops;
    std::vector<CompiledNode> nodes; // in the order they run
    std::vector<FeedbackLoop> loops;
    int numBuffers = 1;              // including buffer 0
    int chunkSize = 0;               // 0: the whole host block in one pass
    bool ok = true;
    std::string error;
    std::uint64_t revision = 0;      // the graph's revision it came from

    bool hasFeedback() const { return ! loops.empty(); }

    int processCount() const
    {
        return (int) std::count_if (ops.begin(), ops.end(), [] (const Op& op) { return op.type == Op::process; });
    }

    int copyCount() const
    {
        return (int) std::count_if (ops.begin(), ops.end(), [] (const Op& op) { return op.type == Op::copy || op.type == Op::mix; });
    }
};

struct CompileOptions
{
    // A graph with a loop renders in chunks of this many samples (ROUTING-CPU
    // A12: 64 at 48 kHz is 1.3 ms of loop delay; 32 is the floor).
    int feedbackChunk = 64;

    // Whether a one-in, one-out node of this kind can work in place. Every
    // effect in the rack does; a node that can't gets its own output buffer.
    bool (*inPlace) (int kind) = nullptr;
};

namespace detail
{
class BufferPool
{
public:
    int take()
    {
        if (! free.empty())
        {
            // The lowest free index, so plans come out the same every time.
            const auto it = std::min_element (free.begin(), free.end());
            const auto index = *it;
            free.erase (it);
            return index;
        }
        return count++;
    }

    void give (int index)
    {
        if (index >= 0 && std::find (free.begin(), free.end(), index) == free.end())
            free.push_back (index);
    }

    void markTaken (int index)
    {
        free.erase (std::remove (free.begin(), free.end(), index), free.end());
    }

    int size() const { return count; }

private:
    std::vector<int> free;
    int count = 1; // buffer 0 is the host's
};
} // namespace detail

// The graph to a plan (message thread). Never fails on a well-formed graph
// below the size limits; plan.ok is false and plan.error says why otherwise.
inline Plan compile (const Graph& graph, const CompileOptions& options = {})
{
    Plan plan;
    plan.revision = graph.revision();
    const auto& nodes = graph.nodes();
    const auto& cables = graph.cables();
    const auto numNodes = nodes.size();
    const auto feedback = graph.feedbackCables();

    const auto fail = [&plan] (std::string why)
    {
        plan.ok = false;
        plan.error = std::move (why);
        plan.ops.clear();
        plan.nodes.clear();
        plan.numBuffers = 1;
        // A plan that failed still renders: the input passes straight through.
        return plan;
    };

    // 1. Which nodes are heard: everything with a path to Output (through
    //    any cable, feedback ones too), plus nodes kept alive and whatever
    //    feeds them. The rest is dead and left out (ROUTING-CPU A6).
    std::vector<bool> heard (numNodes, false), live (numNodes, false);
    {
        const auto mark = [&] (std::vector<bool>& set, int start)
        {
            std::vector<int> todo { start };
            set[(size_t) start] = true;
            while (! todo.empty())
            {
                const auto n = todo.back();
                todo.pop_back();
                for (const auto& cable : cables)
                {
                    if (cable.to.node != nodes[(size_t) n].id)
                        continue;
                    const auto source = graph.indexOf (cable.from.node);
                    if (source >= 0 && ! set[(size_t) source])
                    {
                        set[(size_t) source] = true;
                        todo.push_back (source);
                    }
                }
            }
        };

        mark (heard, graph.indexOf (outputNodeId));
        live = heard;
        for (size_t n = 0; n < numNodes; ++n)
            if (nodes[n].keepAlive && ! live[n])
                mark (live, (int) n);
    }

    // 2. Run order: a topological sort over the cables that aren't feedback,
    //    ties broken by node order (so a rack's slots run in slot order).
    std::vector<int> order;
    {
        std::vector<int> pending (numNodes, 0);
        for (size_t c = 0; c < cables.size(); ++c)
        {
            if (feedback[c])
                continue;
            const auto from = graph.indexOf (cables[c].from.node);
            const auto to = graph.indexOf (cables[c].to.node);
            if (from >= 0 && to >= 0 && live[(size_t) from] && live[(size_t) to])
                ++pending[(size_t) to];
        }

        std::vector<bool> placed (numNodes, false);
        for (;;)
        {
            // The first ready node in node order; Output only when nothing
            // else is ready, so it always runs last.
            auto next = -1;
            for (size_t n = 0; n < numNodes && next < 0; ++n)
                if (live[n] && ! placed[n] && pending[n] == 0 && ! nodes[n].isOutput())
                    next = (int) n;
            if (next < 0)
            {
                const auto out = graph.indexOf (outputNodeId);
                if (out >= 0 && live[(size_t) out] && ! placed[(size_t) out] && pending[(size_t) out] == 0)
                    next = out;
            }
            if (next < 0)
                break;

            placed[(size_t) next] = true;
            order.push_back (next);
            for (size_t c = 0; c < cables.size(); ++c)
            {
                if (feedback[c] || cables[c].from.node != nodes[(size_t) next].id)
                    continue;
                const auto to = graph.indexOf (cables[c].to.node);
                if (to >= 0 && live[(size_t) to])
                    --pending[(size_t) to];
            }
        }

        if ((int) order.size() != (int) std::count (live.begin(), live.end(), true))
            return fail ("the graph has a loop the compiler could not break");
    }

    // 3. How many reads each signal (a node's output port) still has
    //    waiting, so its buffer can go back to the pool after the last one.
    //    Feedback cables read their source as soon as it is made, so they
    //    don't count here.
    const auto key = [] (int nodeIndex, int port) { return nodeIndex * maxPorts + port; };
    std::vector<int> readsLeft (numNodes * maxPorts, 0);
    std::vector<int> bufferOf (numNodes * maxPorts, -1);
    for (size_t c = 0; c < cables.size(); ++c)
    {
        if (feedback[c])
            continue;
        const auto from = graph.indexOf (cables[c].from.node);
        const auto to = graph.indexOf (cables[c].to.node);
        if (from >= 0 && to >= 0 && live[(size_t) to])
            ++readsLeft[(size_t) key (from, cables[c].from.port)];
    }

    detail::BufferPool pool;
    std::vector<int> ringBuffer; // per loop: the buffer holding last chunk's signal

    // Every loop gets a ring and a buffer that holds what it read, for the
    // whole chunk (its reader may run at any point in the order).
    for (size_t c = 0; c < cables.size(); ++c)
    {
        if (! feedback[c])
            continue;
        const auto from = graph.indexOf (cables[c].from.node);
        const auto to = graph.indexOf (cables[c].to.node);
        if (from < 0 || to < 0 || ! live[(size_t) to] || ! live[(size_t) from])
        {
            ringBuffer.push_back (-1);
            continue;
        }
        if ((int) plan.loops.size() >= maxFeedbackCables)
            return fail ("too many feedback cables");

        const auto buffer = pool.take();
        Op read;
        read.type = Op::feedbackRead;
        read.dst = (std::int16_t) buffer;
        read.ring = (std::int16_t) plan.loops.size();
        plan.ops.push_back (read);
        plan.loops.push_back ({ (int) c, cables[c].from, cables[c].to });
        ringBuffer.push_back (buffer);
    }
    // (ringBuffer is indexed by feedback cable in cable order; map cable -> slot)
    std::vector<int> ringOfCable (cables.size(), -1), ringBufferOfCable (cables.size(), -1);
    {
        auto f = 0;
        auto ring = 0;
        for (size_t c = 0; c < cables.size(); ++c)
        {
            if (! feedback[c])
                continue;
            ringBufferOfCable[c] = ringBuffer[(size_t) f++];
            if (ringBufferOfCable[c] >= 0)
                ringOfCable[c] = ring++;
        }
    }

    // Where each input port's sum is being built. A signal is added into
    // every port it feeds as soon as it exists (in cable order), so its
    // buffer goes back to the pool right away and a wide parallel graph
    // needs a few buffers, not one per branch. A port's sum is therefore its
    // cables in the order their sources run, then its feedback cables.
    std::vector<int> sumOf (numNodes * maxPorts, -1);

    // A port read straight from its source, with no sum: one plain cable at
    // unity into a node that only reads its input (not in place).
    const auto inPlaceKind = [&options] (const Node& node)
    {
        return node.numInputs == 1 && node.numOutputs == 1 && (options.inPlace == nullptr || options.inPlace (node.kind));
    };
    std::vector<int> directFrom (numNodes * maxPorts, -1); // cable index, or -1
    // A port with one plain cable takes it when its node runs (so a signal
    // feeding many branches is copied into each just before it runs, and the
    // last one works on it in place); a port with several sums them as they come.
    std::vector<int> singleFrom (numNodes * maxPorts, -1); // cable index, or -1
    for (size_t n = 0; n < numNodes; ++n)
    {
        if (! live[n])
            continue;
        for (int port = 0; port < nodes[n].numInputs; ++port)
        {
            auto count = 0, only = -1;
            for (size_t c = 0; c < cables.size(); ++c)
                if (! feedback[c] && cables[c].to.node == nodes[n].id && cables[c].to.port == port)
                {
                    ++count;
                    only = (int) c;
                }
            if (count == 1)
                singleFrom[(size_t) key ((int) n, port)] = only;
        }
    }
    for (size_t n = 0; n < numNodes; ++n)
    {
        if (! live[n] || nodes[n].isOutput() || inPlaceKind (nodes[n]))
            continue;
        for (int port = 0; port < nodes[n].numInputs; ++port)
        {
            auto count = 0, only = -1;
            for (size_t c = 0; c < cables.size(); ++c)
                if (cables[c].to.node == nodes[n].id && cables[c].to.port == port)
                {
                    ++count;
                    only = (int) c;
                }
            if (count == 1 && ! feedback[(size_t) only] && cables[(size_t) only].gain == 1.0f)
                directFrom[(size_t) key ((int) n, port)] = only;
        }
    }

    const auto emit = [&plan] (Op::Type type, int dst, int src, float gain)
    {
        Op op;
        op.type = type;
        op.dst = (std::int16_t) dst;
        op.src = (std::int16_t) src;
        op.gain = gain;
        plan.ops.push_back (op);
    };

    // One read of a signal done: its buffer goes back once nobody else
    // will read it (unless `kept`, when the reader now owns it).
    const auto readDone = [&] (int nodeIndex, int port, bool kept)
    {
        auto& left = readsLeft[(size_t) key (nodeIndex, port)];
        if (--left == 0 && ! kept)
            pool.give (bufferOf[(size_t) key (nodeIndex, port)]);
    };

    // Adds `buffer` (at `gain`) into a port's sum, starting the sum if it is
    // the first. `adoptable`: the buffer is free after this, so a first
    // feed at unity becomes the sum itself.
    const auto addInto = [&] (int destination, int buffer, float gain, bool adoptable)
    {
        auto& sum = sumOf[(size_t) destination];
        if (sum < 0)
        {
            if (adoptable && gain == 1.0f)
            {
                sum = buffer;
                return true;
            }
            sum = pool.take();
            emit (Op::copy, sum, buffer, gain);
            return false;
        }
        emit (Op::mix, sum, buffer, gain);
        return false;
    };

    for (const auto n : order)
    {
        const auto& node = nodes[(size_t) n];
        CompiledNode compiled;
        compiled.id = node.id;
        compiled.kind = node.kind;
        compiled.ref = node.ref;
        compiled.numInputs = node.numInputs;
        compiled.numOutputs = node.numOutputs;
        compiled.audible = heard[(size_t) n];
        compiled.inPlace = inPlaceKind (node);

        if (! node.isInput())
        {
            std::vector<std::pair<int, int>> directReads; // (node index, port)
            for (int port = 0; port < node.numInputs; ++port)
            {
                const auto k = (size_t) key (n, port);
                if (directFrom[k] >= 0)
                {
                    const auto& cable = cables[(size_t) directFrom[k]];
                    const auto from = graph.indexOf (cable.from.node);
                    compiled.inputs[port] = (std::int16_t) bufferOf[(size_t) key (from, cable.from.port)];
                    directReads.push_back ({ from, cable.from.port });
                    continue;
                }

                if (singleFrom[k] >= 0)
                {
                    const auto& cable = cables[(size_t) singleFrom[k]];
                    const auto from = graph.indexOf (cable.from.node);
                    const auto source = (size_t) key (from, cable.from.port);
                    const auto adopted = addInto ((int) k, bufferOf[source], cable.gain, readsLeft[source] == 1);
                    readDone (from, cable.from.port, adopted);
                }

                // Feedback cables join the sum now, in cable order.
                for (size_t c = 0; c < cables.size(); ++c)
                {
                    if (! feedback[c] || ringBufferOfCable[c] < 0 || cables[c].to.node != node.id || cables[c].to.port != port)
                        continue;
                    if (! addInto ((int) k, ringBufferOfCable[c], cables[c].gain, true))
                        pool.give (ringBufferOfCable[c]);
                }

                if (sumOf[k] < 0)
                {
                    sumOf[k] = pool.take();
                    emit (Op::clear, sumOf[k], -1, 1.0f);
                }
                compiled.inputs[port] = (std::int16_t) sumOf[k];
            }

            for (const auto& r : directReads)
                readDone (r.first, r.second, false);
            // (A source read directly whose last read this was went back to
            // the pool; the node still reads it, so hold it until it has run.)
            for (int port = 0; port < node.numInputs; ++port)
                pool.markTaken (compiled.inputs[port]);
        }

        if (node.isOutput())
        {
            if (compiled.inputs[0] != 0)
                emit (Op::copy, 0, compiled.inputs[0], 1.0f);
            compiled.audible = true;
            plan.nodes.push_back (compiled);
            continue;
        }

        // Outputs: in place, the input buffer; a bypassed node copies input
        // n to output n; otherwise fresh buffers.
        for (int port = 0; port < node.numOutputs; ++port)
        {
            std::int16_t out = 0;
            if (node.isInput())
            {
                out = 0;
                pool.markTaken (0);
            }
            else if (compiled.inPlace)
            {
                out = compiled.inputs[port];
            }
            else
            {
                out = (std::int16_t) pool.take();
                if (node.bypassed)
                    emit (port < node.numInputs ? Op::copy : Op::clear, out, port < node.numInputs ? compiled.inputs[port] : -1, 1.0f);
            }
            compiled.outputs[port] = out;
        }

        // A node that only read its inputs gives them back once it has run,
        // unless a source read directly still has other readers.
        if (! compiled.inPlace && ! node.isInput())
            for (int port = 0; port < node.numInputs; ++port)
            {
                const auto in = compiled.inputs[port];
                auto stillOwned = false;
                for (size_t k = 0; k < bufferOf.size(); ++k)
                    stillOwned = stillOwned || (bufferOf[k] == in && readsLeft[k] > 0);
                if (! stillOwned)
                    pool.give (in);
            }

        if (! node.isInput() && ! node.bypassed)
        {
            Op op;
            op.type = Op::process;
            op.node = (std::int16_t) plan.nodes.size();
            plan.ops.push_back (op);
        }

        for (int port = 0; port < node.numOutputs; ++port)
        {
            const auto out = compiled.outputs[port];
            const auto valueKey = (size_t) key (n, port);
            bufferOf[valueKey] = out;

            // Loops take the signal the moment it exists.
            for (size_t c = 0; c < cables.size(); ++c)
            {
                if (! feedback[c] || ringOfCable[c] < 0 || cables[c].from.node != node.id || cables[c].from.port != port)
                    continue;
                Op op;
                op.type = Op::feedbackWrite;
                op.src = (std::int16_t) out;
                op.ring = (std::int16_t) ringOfCable[c];
                plan.ops.push_back (op);
            }

            if (readsLeft[valueKey] == 0)
            {
                pool.give (out);
                continue;
            }

            // Add it into every port it feeds that builds a sum.
            for (size_t c = 0; c < cables.size(); ++c)
            {
                const auto& cable = cables[c];
                if (feedback[c] || cable.from.node != node.id || cable.from.port != port)
                    continue;
                const auto to = graph.indexOf (cable.to.node);
                if (to < 0 || ! live[(size_t) to])
                    continue;
                const auto destination = key (to, cable.to.port);
                if (directFrom[(size_t) destination] >= 0 || singleFrom[(size_t) destination] >= 0)
                    continue; // taken when that node runs
                const auto adopted = addInto (destination, out, cable.gain, readsLeft[valueKey] == 1);
                readDone (n, port, adopted);
            }
        }

        plan.nodes.push_back (compiled);
    }

    if (graph.indexOf (outputNodeId) >= 0 && ! live[(size_t) graph.indexOf (outputNodeId)])
        return fail ("no output node");

    plan.numBuffers = pool.size();
    if (plan.numBuffers > maxBuffers)
        return fail ("the graph needs too many buffers");

    plan.chunkSize = plan.hasFeedback() ? std::max (32, options.feedbackChunk) : 0;
    return plan;
}
} // namespace PatchGraph
