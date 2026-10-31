// Song Savant - the window: the Library (Phase 1) and the Setup Check (Phase 0). Palettes come next.
#pragma once

#include "PluginProcessor.h"
#include "ui/MainView.h"

class SongSavantEditor : public juce::AudioProcessorEditor
{
public:
    explicit SongSavantEditor (SongSavantProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override {}

    ss::ui::MainView& getView()   { return view; }

private:
    ss::ui::MainView view;
};
