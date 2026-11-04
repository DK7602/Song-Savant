#include "PluginEditor.h"

SongSavantEditor::SongSavantEditor (SongSavantProcessor& p) : juce::AudioProcessorEditor (p), view (p)
{
    addAndMakeVisible (view);
    setResizable (true, true);
    setResizeLimits (1060, 780, 3840, 2400);       // (room to fill a big screen when the app is maximised)
    setSize (1440, 920);                            // (the same as Groove Grabber)
    if (p.catalog.catalog() == nullptr && ! p.catalog.isBuilding())
        p.startCatalog();
}

void SongSavantEditor::resized()
{
    // a window bigger than the one it opens at draws everything bigger (the words too): the pages are laid out at the
    // window's size divided by the zoom, then drawn zoomed to fill it
    const float zoom = ss::ui::zoomFor (getWidth(), getHeight());
    view.setTransform (zoom > 1.0f ? juce::AffineTransform::scale (zoom) : juce::AffineTransform());
    view.setBounds (0, 0, (int) std::ceil ((float) getWidth() / zoom), (int) std::ceil ((float) getHeight() / zoom));
}

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
