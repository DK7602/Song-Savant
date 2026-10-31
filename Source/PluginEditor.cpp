#include "PluginEditor.h"

SongSavantEditor::SongSavantEditor (SongSavantProcessor& p) : juce::AudioProcessorEditor (p), page (p)
{
    addAndMakeVisible (page);
    setResizable (true, true);
    setResizeLimits (1060, 780, 2400, 1600);
    setSize (1140, 800);
}

void SongSavantEditor::resized()   { page.setBounds (getLocalBounds()); }
