// Song Savant - the welcome, shown the first time Song Savant opens (and from the manual's Welcome): what it does in
// three steps, LET'S GO, and a way into the manual.
#pragma once

#include "Widgets.h"

namespace ss::ui
{
    class WelcomePanel : public juce::Component
    {
    public:
        WelcomePanel();
        std::function<void()> onManual, onClose;
        void show();
        void paint (juce::Graphics&) override;
        void resized() override;
        bool keyPressed (const juce::KeyPress&) override;

        static constexpr const char* seenKey = "welcomeSeen";   // the setting: shown once

    private:
        void close();
        PillButton goBtn { "LET'S GO", PillButton::Style::gold },
                   manualBtn { "OPEN THE MANUAL", PillButton::Style::outline };
        juce::Rectangle<float> card, stepsArea;
        juce::Image owl;
    };
}
