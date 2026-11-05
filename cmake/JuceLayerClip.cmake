# A one-line fix to JUCE 8.0.15, applied to its fetched source at configure
# time (idempotent: the marker comment says it is in).
#
# A component with alpha under 1 (every greyed-out control) is drawn through a
# transparency layer, and JUCE begins that layer before clipping to the
# component, so the layer is as big as the whole area being redrawn. With
# ~60 dimmed controls on PLAY, a frame that redraws most of the window (the
# fold slide, a tab switch) built ~60 window-sized layers: in software that is
# 60 image clears and blends, in Direct2D 60 full-size GPU layers. Clipping to
# the component first makes each layer the control's own size. Same pixels.
set(_juce_component "${juce_SOURCE_DIR}/modules/juce_gui_basics/components/juce_Component.cpp")
file(READ "${_juce_component}" _source)
string(REPLACE "\r\n" "\n" _source "${_source}") # (the file has Windows line ends)
string(FIND "${_source}" "ilanaSynth: a layer only as big as the component" _done)
if(_done EQUAL -1)
    set(_old "            OpaqueLayer transparentOpaqueLayer { this };\n            g.beginTransparencyLayer (getAlpha());")
    set(_new "            OpaqueLayer transparentOpaqueLayer { this };\n            Graphics::ScopedSaveState layerClip (g); // ilanaSynth: a layer only as big as the component\n            if (! isPaintingUnclipped())\n                g.reduceClipRegion (getLocalBounds());\n            g.beginTransparencyLayer (getAlpha());")
    string(FIND "${_source}" "${_old}" _at)
    if(_at EQUAL -1)
        message(WARNING "JuceLayerClip: JUCE's transparency layer code has changed; the fix was not applied (see cmake/JuceLayerClip.cmake)")
    else()
        string(REPLACE "${_old}" "${_new}" _source "${_source}")
        file(WRITE "${_juce_component}" "${_source}")
        message(STATUS "JuceLayerClip: applied")
    endif()
endif()
