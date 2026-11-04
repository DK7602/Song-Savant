#include "PluginEditor.h"

SongSavantEditor::SongSavantEditor (SongSavantProcessor& p) : juce::AudioProcessorEditor (p), view (p)
{
    addAndMakeVisible (view);
    setResizable (true, true);
    setResizeLimits (1060, 780, 3840, 2400);       // (room to fill a big screen when the app is maximised)
    setSize (1240, 820);
    if (p.catalog.catalog() == nullptr && ! p.catalog.isBuilding())
        p.startCatalog();
}

void SongSavantEditor::resized()   { view.setBounds (getLocalBounds()); }

void SongSavantEditor::parentHierarchyChanged()
{
    // JUCE's app window only asks for minimise and close: add maximise (full screen), between them. A double-click on
    // the title bar then maximises too. Inside a DAW there's no JUCE window around the editor, so nothing changes there.
    if (windowButtonsSet) return;
    if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
    {
        windowButtonsSet = true;
        window->setTitleBarButtonsRequired (juce::DocumentWindow::allButtons, false);
    }
}
