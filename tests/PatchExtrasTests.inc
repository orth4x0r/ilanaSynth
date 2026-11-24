// Patch extras (Fable A3): every row of PatchExtras::rows() must survive a
// saved state, come back on undo, and light the EDITED marker.
// Included by TableTest.cpp; ILANA_TEST_ONLY=extras runs only these.

void runPatchExtrasTests()
{
    for (const auto& row : PatchExtras::rows())
    {
        const juce::String name (row.name);
        IlanaSynthAudioProcessor processor;
        const auto fresh = row.read (processor);
        const auto hashBefore = processor.getPatchDataHash();

        processor.performEdit ("PatchExtras " + name, [&] { row.change (processor); });
        const auto changed = row.read (processor);
        check (changed != fresh, "extras: " + name + ": the change takes");
        if (row.marksEdited)
            check (processor.getPatchDataHash() != hashBefore, "extras: " + name + ": the EDITED marker notices it");

        juce::MemoryBlock state;
        processor.getStateInformation (state);
        IlanaSynthAudioProcessor reloaded;
        reloaded.setStateInformation (state.getData(), (int) state.getSize());
        check (row.read (reloaded) == changed, "extras: " + name + ": a saved state brings it back (" + row.read (reloaded) + ")");

        processor.getUndoManager().undo();
        check (row.read (processor) == fresh, "extras: " + name + ": undo puts it back");
        processor.getUndoManager().redo();
        check (row.read (processor) == changed, "extras: " + name + ": redo makes it again");
    }
}
