// Song Savant - buttons, drag chips and small pieces used on the screens.
#pragma once

#include "Theme.h"
#include "core/Report.h"

namespace ss::ui
{
    // A pill-shaped button: gold (main action), outline (secondary) or subtle
    class PillButton : public juce::Button
    {
    public:
        enum class Style { gold, outline, subtle };
        PillButton (const juce::String& buttonText, Style);
        void paintButton (juce::Graphics&, bool over, bool down) override;
        Style style;
    };

    // The sun / moon button that switches between the light and dark themes
    class ThemeButton : public juce::Button
    {
    public:
        ThemeButton() : juce::Button ("theme")   { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
        void paintButton (juce::Graphics&, bool over, bool down) override;
    };

    // The setup check's button in the header: a clipboard with a tick (gold when its page is showing)
    class SetupButton : public juce::Button
    {
    public:
        SetupButton() : juce::Button ("setup")  { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
        void paintButton (juce::Graphics&, bool over, bool down) override;
    };

    // The manual's button in the header: a question mark in a gold circle
    class HelpButton : public juce::Button
    {
    public:
        HelpButton() : juce::Button ("manual")   { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
        void paintButton (juce::Graphics&, bool over, bool down) override;
    };

    // A round transport button: to the start, rewind, play (a stop square while it plays: the toggle state), pause,
    // fast forward, to the end. Play is the gold one.
    class TransportButton : public juce::Button
    {
    public:
        enum class Icon { toStart, rewind, play, pause, forward, toEnd };
        explicit TransportButton (Icon);
        void paintButton (juce::Graphics&, bool over, bool down) override;
        static void drawIcon (juce::Graphics&, Icon, bool stop, juce::Rectangle<float> area);   // (the cards use it too)
        Icon icon;
    };

    // Something the user drags out of Song Savant into their DAW (a preset file, a MIDI clip)
    class DragChip : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        enum class Kind { preset, midi };
        DragChip (const juce::String& title, Kind);

        void setFile (const juce::File&, const juce::String& label, const juce::String& unavailableReason);
        bool hasFile() const   { return file.existsAsFile(); }

        void paint (juce::Graphics&) override;
        void mouseEnter (const juce::MouseEvent&) override   { repaint(); }
        void mouseExit (const juce::MouseEvent&) override    { repaint(); }
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override      { dragging = false; }

    private:
        juce::String title, label, reason;
        Kind kind;
        juce::File file;
        bool dragging = false;
    };

    // "Worked" / "Didn't work" for one drag test
    class ResultPicker : public juce::Component
    {
    public:
        ResultPicker();
        void setValue (int v);   // 1 worked, -1 didn't, 0 not tried
        int getValue() const     { return value; }
        std::function<void (int)> onChange;
        void resized() override;

    private:
        PillButton worked { "WORKED", PillButton::Style::subtle }, failed { "DIDN'T WORK", PillButton::Style::subtle };
        int value = 0;
        void refresh();
    };

    // The scan summary: one row per thing we looked for
    class SummaryList : public juce::Component
    {
    public:
        void setItems (std::vector<SummaryItem>);
        void setPlaceholder (const juce::String& s)   { placeholder = s; repaint(); }
        int heightFor (int width) const;
        void paint (juce::Graphics&) override;

    private:
        static constexpr float titleWidth = 190.0f;
        static float rowHeight (const SummaryItem&, float width);
        std::vector<SummaryItem> items;
        juce::String placeholder;
    };

    // A thin progress bar with the current step written under it
    class ProgressStrip : public juce::Component
    {
    public:
        void set (float fraction, const juce::String& text, bool active);
        void paint (juce::Graphics&) override;

    private:
        float value = 0.0f;
        juce::String stage;
        bool running = false;
    };
}
