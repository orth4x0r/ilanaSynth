#include "PatchExtras.h"
#include "../PluginProcessor.h"

namespace PatchExtras
{
namespace
{
juce::String curveText (const LfoCurve& curve)
{
    juce::String text;
    for (const auto& point : curve.points)
        text << point.x << "," << point.y << "," << point.tension << ";";
    return text;
}

LfoCurve oddCurve()
{
    LfoCurve curve;
    curve.points = { { 0.0f, -0.5f, 0.0f }, { 0.4f, 0.9f, 0.3f }, { 1.0f, -0.5f, 0.0f } };
    return curve;
}

std::vector<std::vector<float>> oddFrames()
{
    std::vector<std::vector<float>> frames (2, std::vector<float> ((size_t) WavetableDoc::frameSize));
    for (size_t frame = 0; frame < frames.size(); ++frame)
        for (size_t i = 0; i < frames[frame].size(); ++i)
            frames[frame][i] = (float) std::sin (juce::MathConstants<double>::twoPi * (double) ((frame + 2) * i) / (double) frames[frame].size());
    return frames;
}
} // namespace

const std::vector<Row>& rows()
{
    using P = IlanaSynthAudioProcessor;
    static const std::vector<Row> table {
        { "macro names",
          [] (P& p) { p.setMacroName (1, "PatchExtras"); },
          [] (const P& p) { return p.getMacroName (1); } },
        { "LFO draw steps",
          [] (P& p) { p.setLfoCustomPoint (0, 3, 0.77f); },
          [] (const P& p) { return juce::String (p.getLfoCustomPoint (0, 3), 3); } },
        { "LFO curves",
          [] (P& p) { p.setLfoCurve (1, oddCurve()); },
          [] (const P& p) { return curveText (p.getLfoCurve (1)); } },
        { "mod remaps",
          [] (P& p) { p.setModRemap (0, oddCurve()); },
          [] (const P& p) { return curveText (p.getModRemap (0)); } },
        { "section colours",
          [] (P& p) { p.setSectionColour (1, 0xff123456u); },
          [] (const P& p) { return juce::String::toHexString ((int) p.getSectionColour (1)); } },
        { "patch tables",
          [] (P& p) { p.setUserTable (0, WavetableDoc::fromFrames ("PatchExtras", oddFrames())); },
          [] (const P& p)
          {
              const auto doc = p.getUserTableDocPtr (0);
              return doc == nullptr ? juce::String ("none") : doc->name + ":" + juce::String ((int) doc->frames.size());
          } },
        { "tuning",
          [] (P& p)
          {
              juce::String error;
              p.loadTuningScale ("! odd.scl\nPatchExtras\n 3\n!\n 400.0\n 700.0\n 2/1\n", error);
          },
          [] (const P& p) { return p.getTuningState().getScaleText(); } },
    };
    return table;
}
} // namespace PatchExtras
