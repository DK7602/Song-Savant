// Song Savant - colours, fonts and drawing helpers: deep navy with gold, royal blue and purple, soft glows.
// Matches the design in docs/design (dark theme; the light theme comes in Phase 1).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "core/SoundKinds.h"

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

    const Palette& pal();                   // the current theme's colours

    // Dark (navy, gold, blue, purple) or light (ivory and marble with the same accents); saved for next time
    bool isLightTheme();
    void setLightTheme (bool);

    // Small settings kept between runs (settings.xml in Song Savant's settings folder)
    bool readFlag (const juce::String& key, bool fallback = false);
    void writeFlag (const juce::String& key, bool);

    juce::Font sans (float height, bool bold = false);
    juce::Font serif (float height, bool bold = false);   // headings

    juce::Colour kindColour (Kind);                         // each kind of sound's colour (gold keys, blue bass...)
    void drawKindIcon (juce::Graphics&, Kind, juce::Rectangle<float>, juce::Colour);   // a simple line drawing

    juce::Image owlImage();
    juce::Image wordmarkImage();
    // The solid part of each logo picture, in the picture's own pixels: the owl's body (not its glow) and the title's
    // letters (not the underline flourish). The header sizes the pictures by these. Measured from the artwork in
    // Resources: measure again if the artwork changes (and rerun tools/make_light_logo.py for the light theme).
    juce::Rectangle<float> owlContent();
    juce::Rectangle<float> wordmarkContent();

    void drawBackground (juce::Graphics&, juce::Rectangle<float> area);
    void drawPanel (juce::Graphics&, juce::Rectangle<float> area, float radius = 18.0f);
    void drawSectionTitle (juce::Graphics&, juce::Rectangle<float> area, int number, const juce::String& title);

    // Colours for JUCE's own controls (text boxes, drop-downs, menus, scroll bars) to match the theme
    class SavantLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        SavantLookAndFeel();
        void applyColours();                 // after the theme changes
        juce::Font getComboBoxFont (juce::ComboBox&) override              { return sans (14.5f); }
        juce::Font getPopupMenuFont() override                              { return sans (15.0f); }
        void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
        void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
        void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;
        void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                            int thumbStart, int thumbSize, bool over, bool down) override;
    };

    // How much bigger to draw everything for a window of this size: 1 up to the size it opens at (1440 x 920), then
    // growing with the window, so a big or maximised window has bigger words, buttons and cards. The pages are laid out
    // at the window's size divided by this, never smaller than zoomBase (they fit down to 1060 x 780).
    constexpr int zoomBaseWidth = 1440, zoomBaseHeight = 820;
    float zoomFor (int width, int height);

    // Wrapped text; returns the height it used
    float drawParagraph (juce::Graphics&, const juce::String&, juce::Rectangle<float> area, const juce::Font&, juce::Colour);
    float paragraphHeight (const juce::String&, float width, const juce::Font&);
}
