// Song Savant - the window: the Library (Phase 1) and the Setup Check (Phase 0). Palettes come next.
#pragma once

#include "PluginProcessor.h"
#include "ui/MainView.h"

class SongSavantEditor : public juce::AudioProcessorEditor
{
public:
    explicit SongSavantEditor (SongSavantProcessor&);
    void resized() override;
    // As the app (not a plug-in, whose window is the DAW's), the window gets a maximise button between minimise and close
    void parentHierarchyChanged() override;
    void paint (juce::Graphics&) override {}

    ss::ui::MainView& getView()   { return view; }
    float getZoom() const         { return view.getTransform().getScaleFactor(); }

private:
    ss::ui::MainView view;
    bool windowButtonsSet = false;
};
