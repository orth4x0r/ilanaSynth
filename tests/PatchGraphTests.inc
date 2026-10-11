// The shared patch graph (roadmap step A): the model, the compiler, the
// runner and the preset migration (src/processor/PatchGraph*.h). Included by
// TableTest.cpp; ILANA_GRAPH_TEST=1 runs only these (ILANA_GRAPH_PRESETS=N
// checks only the first N factory presets).

#include <map>
#include <tuple>
#include <thread>

#include "processor/PatchGraphRunner.h"
#include "processor/PatchGraphState.h"

namespace PatchGraphLab
{
using namespace PatchGraph;

// A stand-in node for the tests: a soft clip with a one-pole low-pass, its
// own state per node id, so a node run twice on the same buffer, or two
// nodes sharing one buffer by mistake, shows. Nodes with more ports mix
// their inputs differently into each output.
struct FakeNodes
{
    std::map<NodeId, std::array<float, 2>> lowpass;

    void run (const CompiledNode& node, NodeBuffers& io, int numSamples)
    {
        auto& state = lowpass[node.id];
        const auto drive = 1.0f + 0.37f * (float) (node.kind % 5);
        const auto coefficient = 0.15f + 0.1f * (float) (node.kind % 3);
        const auto numChannels = io.numOutputs > 0 ? io.out[0]->getNumChannels() : 0;
        for (int c = 0; c < numChannels; ++c)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                auto x = 0.0f;
                for (int p = 0; p < io.numInputs; ++p)
                    x += io.in[p]->getSample (c, i) * (1.0f - 0.25f * (float) p);
                state[(size_t) c] += coefficient * (std::tanh (drive * x) - state[(size_t) c]);
                for (int p = 0; p < io.numOutputs; ++p)
                    io.out[p]->setSample (c, i, state[(size_t) c] * (1.0f + 0.5f * (float) p) + (p == 1 ? 0.1f * x : 0.0f));
            }
        }
        if (io.numOutputs == 0)
            for (int p = 0; p < io.numInputs; ++p)
                for (int c = 0; c < io.in[p]->getNumChannels(); ++c)
                    for (int i = 0; i < numSamples; ++i)
                        state[(size_t) c] += io.in[p]->getSample (c, i);
    }
};

// The plain way to run a graph, as a check on the compiler: every signal in
// its own buffer, nodes in the compiled order, each input the sum of its
// cables in cable order, and a delay line of exactly one chunk on every
// feedback cable. No pooling, no in-place work.
struct Reference
{
    const Graph& graph;
    PatchGraph::Plan plan;
    std::map<std::pair<NodeId, int>, juce::AudioBuffer<float>> signals;
    std::map<int, std::array<std::vector<float>, 2>> history; // per feedback cable: everything it carried
    std::int64_t now = 0;
    FakeNodes nodes;

    Reference (const Graph& g, const PatchGraph::Plan& p) : graph (g), plan (p) {}

    void processChunk (juce::AudioBuffer<float>& io)
    {
        const auto numChannels = io.getNumChannels();
        const auto length = io.getNumSamples();
        const auto feedback = graph.feedbackCables();
        const auto& cables = graph.cables();

        // A feedback cable gives back what it carried exactly one loop
        // chunk ago.
        const auto delay = (std::int64_t) plan.chunkSize;
        std::map<int, juce::AudioBuffer<float>> readThisChunk;
        for (size_t c = 0; c < cables.size(); ++c)
        {
            if (! feedback[c])
                continue;
            auto& line = readThisChunk[(int) c];
            line.setSize (numChannels, length);
            auto& past = history[(int) c];
            for (int ch = 0; ch < numChannels; ++ch)
                for (int i = 0; i < length; ++i)
                {
                    const auto at = now + i - delay;
                    line.setSample (ch, i, at >= 0 && at < (std::int64_t) past[(size_t) ch].size() ? past[(size_t) ch][(size_t) at] : 0.0f);
                }
        }

        signals.clear();
        for (const auto& compiled : plan.nodes)
        {
            const auto* node = graph.find (compiled.id);
            std::vector<juce::AudioBuffer<float>> ins ((size_t) node->numInputs);
            for (int port = 0; port < node->numInputs; ++port)
            {
                ins[(size_t) port].setSize (numChannels, length);
                ins[(size_t) port].clear();
                // The compiler's order: plain cables in the order their
                // sources run (then by port, then cable order), feedback
                // cables after them in cable order.
                std::vector<std::tuple<int, int, int>> order;
                for (size_t c = 0; c < cables.size(); ++c)
                {
                    const auto& cable = cables[c];
                    if (cable.to.node != node->id || cable.to.port != port)
                        continue;
                    auto position = 1 << 20;
                    if (! feedback[c])
                        for (size_t i = 0; i < plan.nodes.size(); ++i)
                            if (plan.nodes[i].id == cable.from.node)
                                position = (int) i;
                    order.emplace_back (feedback[c] ? (1 << 21) : position, feedback[c] ? 0 : cable.from.port, (int) c);
                }
                std::sort (order.begin(), order.end());
                for (const auto& entry : order)
                {
                    const auto c = (size_t) std::get<2> (entry);
                    const auto& cable = cables[c];
                    const juce::AudioBuffer<float>* source = nullptr;
                    if (feedback[c])
                        source = &readThisChunk[(int) c];
                    else if (signals.count (std::make_pair (cable.from.node, cable.from.port)) != 0)
                        source = &signals[std::make_pair (cable.from.node, cable.from.port)];
                    if (source == nullptr)
                        continue;
                    for (int ch = 0; ch < numChannels; ++ch)
                        ins[(size_t) port].addFrom (ch, 0, *source, ch, 0, length, cable.gain);
                }
            }

            if (node->isInput())
            {
                signals[std::make_pair (node->id, 0)].makeCopyOf (io);
            }
            else if (node->isOutput())
            {
                io.makeCopyOf (ins[0]);
            }
            else
            {
                std::vector<juce::AudioBuffer<float>> outs ((size_t) node->numOutputs);
                for (auto& o : outs)
                {
                    o.setSize (numChannels, length);
                    o.clear();
                }
                if (node->bypassed)
                {
                    for (int p = 0; p < node->numOutputs; ++p)
                        if (p < node->numInputs)
                            outs[(size_t) p].makeCopyOf (ins[(size_t) p]);
                }
                else
                {
                    NodeBuffers bufs;
                    bufs.numInputs = node->numInputs;
                    bufs.numOutputs = node->numOutputs;
                    // In place for a one-in, one-out node, as the runner does.
                    if (compiled.inPlace)
                    {
                        outs[0].makeCopyOf (ins[0]);
                        bufs.in[0] = bufs.out[0] = &outs[0];
                    }
                    else
                    {
                        for (int p = 0; p < node->numInputs; ++p)
                            bufs.in[p] = &ins[(size_t) p];
                        for (int p = 0; p < node->numOutputs; ++p)
                            bufs.out[p] = &outs[(size_t) p];
                    }
                    nodes.run (compiled, bufs, length);
                }
                for (int p = 0; p < node->numOutputs; ++p)
                    signals[std::make_pair (node->id, p)] = outs[(size_t) p];
            }
        }

        for (size_t c = 0; c < cables.size(); ++c)
        {
            if (! feedback[c])
                continue;
            const auto key = std::make_pair (cables[c].from.node, cables[c].from.port);
            auto& past = history[(int) c];
            for (int ch = 0; ch < numChannels; ++ch)
            {
                past[(size_t) ch].resize ((size_t) now, 0.0f);
                for (int i = 0; i < length; ++i)
                {
                    auto x = signals.count (key) != 0 ? signals[key].getSample (ch, i) : 0.0f;
                    x = std::abs (x) < 1.0e-20f ? 0.0f : juce::jlimit (-8.0f, 8.0f, x);
                    past[(size_t) ch].push_back (x);
                }
            }
        }
        now += length;
    }
};

inline float signal (int channel, int n)
{
    return (float) (0.5 * std::sin (0.031 * n + channel) + 0.3 * std::sin (0.0071 * n * (1 + channel)));
}

// A random graph: `count` nodes (some with two ports), random cables, a few
// of them closing loops, a few nodes bypassed or kept alive, a few left dead.
inline Graph randomGraph (juce::Random& random, int count, bool loops)
{
    Graph graph;
    std::vector<NodeId> ids { inputNodeId };
    for (int n = 0; n < count; ++n)
    {
        const auto ports = random.nextInt (5) == 0 ? 2 : 1;
        const auto id = graph.addNode (random.nextInt (20), ports, ports);
        if (random.nextInt (8) == 0)
            graph.setBypassed (id, true);
        if (random.nextInt (10) == 0)
            graph.setKeepAlive (id, true);
        ids.push_back (id);
    }

    // A chain-ish backbone so most nodes are heard, then extra cables.
    for (size_t i = 1; i < ids.size(); ++i)
    {
        const auto from = ids[(size_t) random.nextInt ((int) i)];
        graph.connect ({ from, 0 }, { ids[i], 0 }, random.nextInt (3) == 0 ? 0.5f + random.nextFloat() : 1.0f);
    }
    for (int extra = 0; extra < count; ++extra)
    {
        const auto a = ids[(size_t) random.nextInt ((int) ids.size())];
        const auto b = ids[(size_t) (1 + random.nextInt ((int) ids.size() - 1))];
        const auto* from = graph.find (a);
        const auto* to = graph.find (b);
        const auto forward = graph.indexOf (a) < graph.indexOf (b);
        if (! loops && ! forward)
            continue;
        graph.connect ({ a, random.nextInt (juce::jmax (1, from->numOutputs)) }, { b, random.nextInt (juce::jmax (1, to->numInputs)) },
                       forward ? 1.0f : 0.3f + 0.4f * random.nextFloat());
    }
    for (size_t i = 1; i < ids.size(); ++i)
        if (random.nextInt (3) == 0)
            graph.connect ({ ids[i], graph.find (ids[i])->numOutputs - 1 }, { outputNodeId, 0 }, random.nextInt (2) == 0 ? 1.0f : 0.7f);
    graph.connect ({ ids.back(), 0 }, { outputNodeId, 0 });
    return graph;
}

// Runs `graph` through the runner and through the reference over `blocks`
// host blocks of `blockSize`; returns the worst sample difference.
inline float compareWithReference (const Graph& graph, int blockSize, int blocks, juce::String* error = nullptr)
{
    const auto plan = compile (graph);
    if (! plan.ok)
    {
        if (error != nullptr)
            *error = plan.error;
        return 1.0e9f;
    }

    Runner runner;
    runner.prepare (48000.0, blockSize, 2);
    runner.publish (plan);
    FakeNodes fake;
    Reference reference (graph, plan);

    auto worst = 0.0f;
    juce::AudioBuffer<float> a (2, blockSize), b (2, blockSize);
    for (int block = 0; block < blocks; ++block)
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < blockSize; ++i)
                a.setSample (c, i, signal (c, block * blockSize + i));
        b.makeCopyOf (a);

        runner.process (a, [&fake] (const CompiledNode& node, NodeBuffers& io, int n) { fake.run (node, io, n); });

        const auto chunk = plan.chunkSize > 0 ? plan.chunkSize : blockSize;
        for (int start = 0; start < blockSize; start += chunk)
        {
            const auto length = juce::jmin (chunk, blockSize - start);
            juce::AudioBuffer<float> piece (2, length);
            for (int c = 0; c < 2; ++c)
                piece.copyFrom (c, 0, b, c, start, length);
            reference.processChunk (piece);
            for (int c = 0; c < 2; ++c)
                b.copyFrom (c, start, piece, c, 0, length);
        }

        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < blockSize; ++i)
                worst = juce::jmax (worst, std::abs (a.getSample (c, i) - b.getSample (c, i)));
    }
    return worst;
}

inline void setRackParam (IlanaSynthAudioProcessor& processor, const juce::String& id, float value)
{
    if (auto* parameter = processor.apvts.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

// The rack rendered by the fixed loop (processEffects) and by its migrated
// graph (processEffectsGraph) on two processors set up the same way.
// Returns the worst sample difference; `control` gets the difference of two
// fixed-loop renders, which is not zero only for an effect that isn't
// repeatable run to run.
inline float compareRack (const std::function<void (IlanaSynthAudioProcessor&)>& setUp, float* control = nullptr)
{
    constexpr int blockSize = 256, blocks = 24;
    const auto render = [&] (bool viaGraph)
    {
        IlanaSynthAudioProcessor processor;
        setUp (processor);
        processor.prepareToPlay (48000.0, blockSize);
        Runner runner;
        if (viaGraph)
        {
            runner.setSettings ({ false, false });
            runner.prepare (48000.0, blockSize, 2);
            runner.publish (compile (processor.getFxRackGraph()));
        }
        juce::AudioBuffer<float> buffer (2, blockSize);
        std::vector<float> out;
        for (int block = 0; block < blocks; ++block)
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < blockSize; ++i)
                    buffer.setSample (c, i, signal (c, block * blockSize + i));
            if (viaGraph)
                processor.processEffectsGraph (buffer, runner);
            else
                processor.processEffects (buffer);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < blockSize; ++i)
                    out.push_back (buffer.getSample (c, i));
        }
        return out;
    };

    const auto fixed = render (false);
    const auto graph = render (true);
    auto worst = 0.0f;
    for (size_t i = 0; i < fixed.size(); ++i)
        worst = juce::jmax (worst, std::abs (fixed[i] - graph[i]));
    if (control != nullptr)
    {
        *control = 0.0f;
        if (worst > 0.0f)
        {
            const auto again = render (false);
            for (size_t i = 0; i < fixed.size(); ++i)
                *control = juce::jmax (*control, std::abs (fixed[i] - again[i]));
        }
    }
    return worst;
}
} // namespace PatchGraphLab

void runPatchGraphTests (bool everyPreset = true)
{
    using namespace PatchGraphLab;

    // The model: ports, directions, duplicates, removal.
    {
        Graph graph;
        const auto a = graph.addNode (1, 1, 1);
        const auto b = graph.addNode (2, 2, 1);
        check (graph.connect ({ inputNodeId, 0 }, { a, 0 }) == ConnectResult::ok, "graph: Input -> A connects");
        check (graph.connect ({ inputNodeId, 0 }, { a, 0 }) == ConnectResult::duplicate, "graph: the same cable twice is refused");
        check (graph.connect ({ a, 0 }, { b, 2 }) == ConnectResult::noSuchPort, "graph: a port past the node's last is refused");
        check (graph.connect ({ outputNodeId, 0 }, { a, 0 }) == ConnectResult::noSuchPort, "graph: Output has no output port");
        check (graph.connect ({ a, 0 }, { inputNodeId, 0 }) == ConnectResult::noSuchPort, "graph: Input has no input port");
        check (graph.connect ({ a, 0 }, { b, 1 }) == ConnectResult::ok, "graph: A -> B's second input connects");
        const auto revision = graph.revision();
        graph.moveNode (a, 40.0f, 10.0f);
        check (graph.revision() == revision, "graph: moving a node doesn't count as an edit");
        check (graph.removeNode (a) && graph.cables().empty(), "graph: removing a node removes its cables");
        check (! graph.removeNode (inputNodeId) && ! graph.removeNode (outputNodeId), "graph: Input and Output stay");
        const auto c = graph.addNode (3, 1, 1);
        check (c != a && c > b, "graph: node ids are never reused");
    }

    // Loops: the cable that closes one is the feedback cable.
    {
        Graph graph;
        const auto a = graph.addNode (1, 1, 1);
        const auto b = graph.addNode (1, 1, 1);
        graph.connect ({ inputNodeId, 0 }, { a, 0 });
        graph.connect ({ a, 0 }, { b, 0 });
        graph.connect ({ b, 0 }, { a, 0 });
        graph.connect ({ b, 0 }, { b, 0 });
        graph.connect ({ b, 0 }, { outputNodeId, 0 });
        const auto feedback = graph.feedbackCables();
        check (! feedback[0] && ! feedback[1] && feedback[2] && feedback[3] && ! feedback[4],
               "graph: B -> A and B -> B close loops, the forward cables don't");
        const auto plan = compile (graph);
        check (plan.ok && plan.loops.size() == 2 && plan.chunkSize == 64, "compile: two loops, rendered in 64-sample chunks");
    }

    // Compiled shapes: a plain chain costs no copies; parallel shares the
    // input; dead, kept-alive and bypassed nodes.
    {
        Graph chain;
        auto previous = inputNodeId;
        for (int n = 0; n < 4; ++n)
        {
            const auto id = chain.addNode (n, 1, 1);
            chain.connect ({ previous, 0 }, { id, 0 });
            previous = id;
        }
        chain.connect ({ previous, 0 }, { outputNodeId, 0 });
        const auto plan = compile (chain);
        check (plan.ok && plan.processCount() == 4 && plan.copyCount() == 0 && plan.numBuffers == 1 && plan.chunkSize == 0,
               "compile: a chain of 4 runs in place in the host's buffer, no copies (" + juce::String (plan.copyCount()) + " copies, "
                   + juce::String (plan.numBuffers) + " buffers)");

        Graph parallel;
        std::vector<NodeId> ids;
        for (int n = 0; n < 6; ++n)
        {
            ids.push_back (parallel.addNode (n, 1, 1));
            parallel.connect ({ inputNodeId, 0 }, { ids.back(), 0 });
            parallel.connect ({ ids.back(), 0 }, { outputNodeId, 0 }, 1.0f / 6.0f);
        }
        const auto dead = parallel.addNode (9, 1, 1);
        parallel.connect ({ inputNodeId, 0 }, { dead, 0 });
        const auto kept = parallel.addNode (9, 1, 1);
        parallel.connect ({ inputNodeId, 0 }, { kept, 0 });
        parallel.setKeepAlive (kept, true);
        parallel.setBypassed (ids[2], true);
        const auto p = compile (parallel);
        auto runsDead = false, runsKept = false;
        for (const auto& node : p.nodes)
        {
            runsDead = runsDead || node.id == dead;
            runsKept = runsKept || (node.id == kept && ! node.audible);
        }
        check (p.ok && ! runsDead && runsKept, "compile: a dead branch is left out, a kept-alive one runs unheard");
        check (p.processCount() == 6, "compile: 5 working branches plus the kept one run, the bypassed one doesn't (" + juce::String (p.processCount()) + ")");
        check (p.numBuffers <= 3, "compile: parallel branches share pooled buffers (" + juce::String (p.numBuffers) + " buffers for 7 branches)");
    }

    // The runner against the plain reference on random graphs: every buffer
    // reuse, in-place run, bypass, kept-alive node and loop must give the
    // same samples. Block sizes that don't divide into the loop chunk too.
    {
        juce::Random random (0x51a7e);
        auto worst = 0.0f;
        auto loopsSeen = 0, failed = 0;
        juce::String firstError;
        for (int trial = 0; trial < 300; ++trial)
        {
            const auto graph = randomGraph (random, 2 + random.nextInt (20), trial % 2 == 1);
            loopsSeen += compile (graph).hasFeedback() ? 1 : 0;
            const int sizes[] = { 512, 100, 64, 37 };
            juce::String error;
            const auto difference = compareWithReference (graph, sizes[trial % 4], 6, &error);
            if (difference > 0.0f && firstError.isEmpty())
                firstError = "trial " + juce::String (trial) + ": " + (error.isNotEmpty() ? error : juce::String (difference, 9));
            failed += difference > 0.0f ? 1 : 0;
            worst = juce::jmax (worst, difference);
        }
        check (failed == 0, "runner: 300 random graphs match the plain reference sample for sample (" + juce::String (failed)
                                + " differ; first: " + firstError + ")");
        check (loopsSeen > 50, "runner: the random graphs include loops (" + juce::String (loopsSeen) + ")");
    }

    // A loop delays by exactly one chunk: an impulse through A with A -> A
    // at 0.5 comes back at 64 and 128 samples, halved each time.
    {
        Graph graph;
        const auto a = graph.addNode (0, 1, 1);
        graph.connect ({ inputNodeId, 0 }, { a, 0 });
        graph.connect ({ a, 0 }, { a, 0 }, 0.5f);
        graph.connect ({ a, 0 }, { outputNodeId, 0 });
        Runner runner;
        runner.prepare (48000.0, 100, 2);
        runner.publish (compile (graph));
        juce::AudioBuffer<float> buffer (2, 100);
        std::vector<float> out;
        for (int block = 0; block < 3; ++block)
        {
            buffer.clear();
            if (block == 0)
                buffer.setSample (0, 0, 1.0f);
            runner.process (buffer, [] (const CompiledNode&, NodeBuffers&, int) {}); // a wire
            for (int i = 0; i < 100; ++i)
                out.push_back (buffer.getSample (0, i));
        }
        auto others = 0.0f;
        for (size_t i = 0; i < out.size(); ++i)
            if (i % 64 != 0)
                others = juce::jmax (others, std::abs (out[i]));
        check (out[0] == 1.0f && out[64] == 0.5f && out[128] == 0.25f && out[192] == 0.125f && out[256] == 0.0625f && others == 0.0f,
               "runner: a loop comes back after exactly 64 samples across 100-sample blocks (" + juce::String (out[0]) + ", " + juce::String (out[64])
                   + ", " + juce::String (out[128]) + ", " + juce::String (out[192]) + ", others " + juce::String (others) + ")");
    }

    // A runaway loop stays finite.
    {
        Graph graph;
        const auto a = graph.addNode (0, 1, 1);
        graph.connect ({ inputNodeId, 0 }, { a, 0 });
        graph.connect ({ a, 0 }, { a, 0 }, 4.0f);
        graph.connect ({ a, 0 }, { outputNodeId, 0 });
        Runner runner;
        runner.prepare (48000.0, 256, 2);
        runner.publish (compile (graph));
        juce::AudioBuffer<float> buffer (2, 256);
        auto finite = true;
        auto peak = 0.0f;
        for (int block = 0; block < 200; ++block)
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 256; ++i)
                    buffer.setSample (c, i, 0.5f);
            runner.process (buffer, [] (const CompiledNode&, NodeBuffers&, int) {});
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 256; ++i)
                {
                    finite = finite && std::isfinite (buffer.getSample (c, i));
                    peak = juce::jmax (peak, std::abs (buffer.getSample (c, i)));
                }
        }
        check (finite && peak <= 0.5f + 4.0f * 8.0f, "runner: a loop at gain 4 stays finite (peak " + juce::String (peak) + ")");
    }

    // The silence skip: a node whose input has been silent for longer than
    // its tail stops running and gives silence.
    {
        Graph graph;
        const auto a = graph.addNode (0, 1, 1);
        graph.connect ({ inputNodeId, 0 }, { a, 0 });
        graph.connect ({ a, 0 }, { outputNodeId, 0 });
        Runner runner;
        runner.setSettings ({ true, true });
        runner.setTailFunction ([] (const CompiledNode&, double) { return 1000; });
        runner.prepare (48000.0, 256, 2);
        runner.publish (compile (graph));
        auto runs = 0;
        juce::AudioBuffer<float> buffer (2, 256);
        for (int block = 0; block < 20; ++block)
        {
            buffer.clear();
            if (block == 0)
                buffer.setSample (0, 3, 1.0f);
            runner.process (buffer, [&runs] (const CompiledNode&, NodeBuffers&, int) { ++runs; });
        }
        const auto* stats = runner.statsFor (a);
        check (runs == 5 && stats != nullptr && stats->sleeping.load(), "runner: a silent node sleeps once its 1000-sample tail is spent (ran "
                                                                             + juce::String (runs) + " of 20 blocks)");
    }

    // Publishing new plans while the audio thread runs: no crash, no lost
    // plan, the last one wins.
    {
        Runner runner;
        runner.setSettings ({ true, false });
        runner.prepare (48000.0, 128, 2);
        std::atomic<bool> stop { false };
        std::atomic<int> blocks { 0 };
        std::thread audio ([&]
        {
            juce::AudioBuffer<float> buffer (2, 128);
            FakeNodes fake;
            while (! stop.load())
            {
                buffer.clear();
                runner.process (buffer, [&fake] (const CompiledNode& node, NodeBuffers& io, int n) { fake.run (node, io, n); });
                ++blocks;
            }
        });
        juce::Random random (7);
        Graph last;
        for (int i = 0; i < 300; ++i)
        {
            last = randomGraph (random, 1 + random.nextInt (12), true);
            runner.publish (compile (last));
            std::this_thread::sleep_for (std::chrono::microseconds (200));
        }
        while (blocks.load() < 50)
            std::this_thread::yield();
        std::this_thread::sleep_for (std::chrono::milliseconds (20));
        stop = true;
        audio.join();
        runner.collectGarbage();
        check (runner.publishedPlan() != nullptr && runner.publishedPlan()->revision == last.revision(),
               "runner: 300 plans published while audio ran, " + juce::String (blocks.load()) + " blocks, no crash");
    }

    // Saving and loading: the same graph back; damage is dropped, not trusted.
    {
        juce::Random random (11);
        auto graph = randomGraph (random, 10, true);
        graph.setCableGain (0, 0.25f);
        graph.moveNode (outputNodeId, 300.0f, 40.0f);
        const auto tree = toValueTree (graph);
        const auto back = fromValueTree (tree);
        auto same = back.nodes().size() == graph.nodes().size() && back.cables().size() == graph.cables().size();
        for (size_t i = 0; same && i < graph.cables().size(); ++i)
            same = back.cables()[i].from == graph.cables()[i].from && back.cables()[i].to == graph.cables()[i].to
                   && back.cables()[i].gain == graph.cables()[i].gain;
        for (size_t i = 0; same && i < graph.nodes().size(); ++i)
            same = back.nodes()[i].id == graph.nodes()[i].id && back.nodes()[i].kind == graph.nodes()[i].kind
                   && back.nodes()[i].bypassed == graph.nodes()[i].bypassed && back.nodes()[i].x == graph.nodes()[i].x;
        check (same, "state: a graph saved and loaded is the same graph");

        auto damaged = tree.createCopy();
        juce::ValueTree badCable (cableTag);
        badCable.setProperty ("from", 999, nullptr);
        badCable.setProperty ("to", 2, nullptr);
        damaged.appendChild (badCable, nullptr);
        juce::ValueTree badNode (nodeTag);
        badNode.setProperty ("id", 0, nullptr);
        damaged.appendChild (badNode, nullptr);
        juce::ValueTree nanGain (cableTag);
        nanGain.setProperty ("from", 1, nullptr);
        nanGain.setProperty ("to", 2, nullptr);
        nanGain.setProperty ("gain", std::numeric_limits<double>::quiet_NaN(), nullptr);
        damaged.appendChild (nanGain, nullptr);
        const auto loaded = fromValueTree (damaged);
        auto finiteGains = true;
        for (const auto& cable : loaded.cables())
            finiteGains = finiteGains && std::isfinite (cable.gain);
        check (loaded.nodes().size() == graph.nodes().size() && finiteGains && compile (loaded).ok,
               "state: a damaged graph loads without the bad parts and still compiles");
    }

    // Migration, random racks: series and parallel, bypass, solo and MIX,
    // through the real effects. The migrated graph renders the fixed rack
    // sample for sample.
    {
        juce::Random random (0xfe11);
        auto failed = 0, unrepeatable = 0;
        juce::String first;
        for (int trial = 0; trial < 40; ++trial)
        {
            std::array<int, IlanaSynthAudioProcessor::numFxSlots> types {};
            std::array<bool, IlanaSynthAudioProcessor::numFxSlots> bypass {}, solo {};
            std::array<float, IlanaSynthAudioProcessor::numFxSlots> mix {};
            for (int s = 0; s < IlanaSynthAudioProcessor::numFxSlots; ++s)
            {
                types[(size_t) s] = random.nextInt (3) == 0 ? 0 : 1 + random.nextInt (41);
                bypass[(size_t) s] = random.nextInt (8) == 0;
                solo[(size_t) s] = random.nextInt (12) == 0;
                mix[(size_t) s] = random.nextInt (3) == 0 ? random.nextFloat() : 1.0f;
            }
            const auto parallel = trial % 2 == 1;
            float control = 0.0f;
            const auto difference = compareRack ([&] (IlanaSynthAudioProcessor& p)
            {
                for (int s = 0; s < IlanaSynthAudioProcessor::numFxSlots; ++s)
                {
                    p.assignFxSlot (s + 1, types[(size_t) s]);
                    const auto prefix = "fx_slot" + juce::String (s + 1);
                    setRackParam (p, prefix + "_bypass", bypass[(size_t) s] ? 1.0f : 0.0f);
                    setRackParam (p, prefix + "_solo", solo[(size_t) s] ? 1.0f : 0.0f);
                    setRackParam (p, prefix + "_mix", mix[(size_t) s]);
                }
                setRackParam (p, "fx_routing", parallel ? 1.0f : 0.0f);
            }, &control);
            if (difference > 0.0f && control == 0.0f)
            {
                ++failed;
                if (first.isEmpty())
                    first = "trial " + juce::String (trial) + " (" + (parallel ? "parallel" : "series") + "): " + juce::String (difference, 9);
            }
            unrepeatable += difference > 0.0f && control > 0.0f ? 1 : 0;
        }
        check (failed == 0, "migration: 40 random racks render the same through their graph (" + juce::String (failed) + " differ; " + first + ")");
        if (unrepeatable > 0)
            std::cout << "  (" << unrepeatable << " random racks are not repeatable run to run, so not compared)" << std::endl;
    }

    // Migration, every factory preset, in its own routing and forced to
    // parallel: the graph renders the fixed rack sample for sample.
    {
        IlanaSynthAudioProcessor names;
        const auto presets = names.getFactoryPresetNames();
        auto limit = presets.size();
        const auto only = juce::SystemStats::getEnvironmentVariable ("ILANA_GRAPH_PRESETS", "").getIntValue();
        if (only > 0)
            limit = juce::jmin (limit, only);

        auto failed = 0, unrepeatable = 0, withFx = 0, checked = 0;
        juce::String first, unrepeatableNames;
        // (The whole suite checks every sixth preset; ILANA_GRAPH_TEST all of them.)
        const auto stride = everyPreset ? 1 : 6;
        for (int index = 0; index < limit; index += stride)
        {
            ++checked;
            for (const auto forceParallel : { false, true })
            {
                auto hasFx = false;
                float control = 0.0f;
                const auto difference = compareRack ([&] (IlanaSynthAudioProcessor& p)
                {
                    p.loadFactoryPreset (index);
                    if (forceParallel)
                        setRackParam (p, "fx_routing", 1.0f);
                    for (int s = 1; s <= IlanaSynthAudioProcessor::numFxSlots; ++s)
                        hasFx = hasFx || p.apvts.getRawParameterValue ("fx_slot" + juce::String (s))->load() > 0.5f;
                }, &control);
                if (! forceParallel && hasFx)
                    ++withFx;
                if (difference > 0.0f && control == 0.0f)
                {
                    ++failed;
                    if (first.isEmpty())
                        first = presets[index] + (forceParallel ? " (parallel)" : "") + ": " + juce::String (difference, 9);
                }
                if (difference > 0.0f && control > 0.0f)
                {
                    ++unrepeatable;
                    unrepeatableNames << (unrepeatableNames.isEmpty() ? "" : ", ") << presets[index];
                }
                if (! hasFx)
                    break; // an empty rack is the same either way
            }
        }
        check (failed == 0, "migration: " + juce::String (checked) + " factory presets (" + juce::String (withFx)
                                + " with effects, each also forced to parallel) render the same through their graph ("
                                + juce::String (failed) + " differ; " + first + ")");
        if (unrepeatable > 0)
            std::cout << "  (" << unrepeatable << " preset renders are not repeatable run to run, so not compared: " << unrepeatableNames << ")" << std::endl;
    }
}
