// Song Savant - colours, fonts and drawing helpers: deep navy with gold, royal blue and purple, soft glows.
// Matches the design in docs/design (dark theme; the light theme comes in Phase 1).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ss::ui
{
    struct Palette
    {
        juce::Colour bgTop       { 0xff0a0820 };
        juce::Colour bgBottom    { 0xff120b2e };
        juce::Colour panel       { 0xc8141039 };
        juce::Colour panelHi     { 0xff1d1850 };
        juce::Colour gold        { 0xfff4c55b };
        juce::Colour goldDeep    { 0xffb9802a };
        juce::Colour goldText    { 0xff1a1235 };   // text on gold
        juce::Colour blue        { 0xff3d6bff };
        juce::Colour purple      { 0xff8a4dff };
        juce::Colour text        { 0xfff1edff };
        juce::Colour muted       { 0xffa7a2d0 };
        juce::Colour dim         { 0xff6e6a99 };
        juce::Colour good        { 0xff5be49b };
        juce::Colour warn        { 0xfff7b34c };
        juce::Colour bad         { 0xffff6b88 };
    };

    const Palette& pal();

    juce::Font sans (float height, bool bold = false);
    juce::Font serif (float height, bool bold = false);   // headings

    juce::Image owlImage();
    juce::Image wordmarkImage();

    void drawBackground (juce::Graphics&, juce::Rectangle<float> area);
    void drawPanel (juce::Graphics&, juce::Rectangle<float> area, float radius = 18.0f);
    void drawSectionTitle (juce::Graphics&, juce::Rectangle<float> area, int number, const juce::String& title);

    // Wrapped text; returns the height it used
    float drawParagraph (juce::Graphics&, const juce::String&, juce::Rectangle<float> area, const juce::Font&, juce::Colour);
    float paragraphHeight (const juce::String&, float width, const juce::Font&);
}
