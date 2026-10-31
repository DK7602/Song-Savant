// Song Savant - the window. Phase 0 shows the setup check; Phase 1 adds the song form, palettes and browser.
#pragma once

#include "PluginProcessor.h"
#include "ui/SetupCheckPage.h"

class SongSavantEditor : public juce::AudioProcessorEditor
{
public:
    explicit SongSavantEditor (SongSavantProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override {}

    ss::ui::SetupCheckPage& getPage()   { return page; }

private:
    ss::ui::SetupCheckPage page;
    juce::TooltipWindow tooltips { this, 500 };
};
