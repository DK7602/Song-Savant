#include "PluginEditor.h"

SongSavantEditor::SongSavantEditor (SongSavantProcessor& p) : juce::AudioProcessorEditor (p), view (p)
{
    addAndMakeVisible (view);
    setResizable (true, true);
    setResizeLimits (1060, 780, 2400, 1600);
    setSize (1240, 820);
    if (p.catalog.catalog() == nullptr && ! p.catalog.isBuilding())
        p.startCatalog();
}

void SongSavantEditor::resized()   { view.setBounds (getLocalBounds()); }
