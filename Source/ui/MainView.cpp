#include "MainView.h"
#include "PluginProcessor.h"

namespace ss::ui
{
    MainView::MainView (SongSavantProcessor& p) : proc (p), songPage (p), libraryPage (p), setupPage (p), drumMapsPage (p), masterclassPage (p)
    {
        setLookAndFeel (&lookAndFeel);
        owl = owlImage();
        wordmark = wordmarkImage();
        for (auto* t : { &songTab, &masterclassTab, &libraryTab, &drumMapsTab, &setupTab })
        {
            t->setRadioGroupId (1);
            t->setComponentID ("tab");
            addAndMakeVisible (t);
        }
        themeBtn.setTooltip ("Light or dark");
        themeBtn.onClick = [this] { setLightTheme (! isLightTheme()); applyTheme(); };
        addAndMakeVisible (themeBtn);
        songTab.onClick = [this] { showPage (song); };
        libraryTab.onClick = [this] { showPage (library); };
        setupTab.onClick = [this] { showPage (setupCheck); };
        drumMapsTab.onClick = [this] { showPage (drumMaps); };
        masterclassTab.onClick = [this] { showPage (masterclass); };
        masterclassPage.onUseForSong = [this] { songPage.showForm(); showPage (song); };
        addChildComponent (songPage);
        addChildComponent (libraryPage);
        addChildComponent (setupPage);
        addChildComponent (drumMapsPage);
        addChildComponent (masterclassPage);
        showPage (proc.page);
    }

    MainView::~MainView()   { setLookAndFeel (nullptr); }

    void MainView::applyTheme()
    {
        lookAndFeel.applyColours();
        owl = owlImage();
        wordmark = wordmarkImage();
        sendLookAndFeelChange();
        songPage.themeChanged();
        libraryPage.themeChanged();
        drumMapsPage.themeChanged();
        masterclassPage.themeChanged();
        repaint();
    }

    void MainView::showPage (int which)
    {
        page = juce::jlimit ((int) song, (int) masterclass, which);
        proc.page = page;
        songPage.setVisible (page == song);
        libraryPage.setVisible (page == library);
        setupPage.setVisible (page == setupCheck);
        drumMapsPage.setVisible (page == drumMaps);
        masterclassPage.setVisible (page == masterclass);
        songTab.setToggleState (page == song, juce::dontSendNotification);
        libraryTab.setToggleState (page == library, juce::dontSendNotification);
        setupTab.setToggleState (page == setupCheck, juce::dontSendNotification);
        drumMapsTab.setToggleState (page == drumMaps, juce::dontSendNotification);
        masterclassTab.setToggleState (page == masterclass, juce::dontSendNotification);
        repaint();
    }

    void MainView::paint (juce::Graphics& g)
    {
        const auto& p = pal();
        drawBackground (g, getLocalBounds().toFloat());

        // the owl and the name, smaller when the window is narrow so the tabs fit
        auto h = headerArea;
        if (owl.isValid() && wordmark.isValid())
        {
            const float owlRatio = (float) owl.getWidth() / (float) owl.getHeight();
            const float wordRatio = (float) wordmark.getWidth() / (float) wordmark.getHeight();
            const float natural = h.getHeight() * owlRatio + 10.0f + h.getHeight() * 0.86f * wordRatio;
            const float scale = juce::jmin (1.0f, h.getWidth() / natural);
            h = h.withSizeKeepingCentre (h.getWidth(), h.getHeight() * scale).withX (headerArea.getX());
            auto o = h.removeFromLeft (h.getHeight() * owlRatio);
            g.drawImage (owl, o, juce::RectanglePlacement::centred);
            h.removeFromLeft (10.0f * scale);
            const float wh = h.getHeight() * 0.86f;
            const float ww = wh * wordRatio;
            g.drawImage (wordmark, h.removeFromLeft (ww).withSizeKeepingCentre (ww, wh), juce::RectanglePlacement::centred);
        }
        g.setColour (p.muted);
        g.setFont (sans (13.0f));
        g.drawText ("version " SS_VERSION, subtitleArea, juce::Justification::centredRight);
    }

    void MainView::resized()
    {
        auto r = getLocalBounds();
        auto header = r.removeFromTop (124).reduced (24, 16);
        // each tab as wide as its name, right to left
        const auto font = sans (14.0f, true);
        auto tabWidth = [&font] (PillButton& b) { return (int) juce::GlyphArrangement::getStringWidth (font, b.getButtonText()) + 40; };
        const int gapW = 8;
        int total = 40 + 12;
        for (auto* t : { &songTab, &masterclassTab, &libraryTab, &drumMapsTab, &setupTab }) total += tabWidth (*t) + gapW;
        auto tabs = header.removeFromRight (total).withSizeKeepingCentre (total, 40).translated (0, -8);
        for (auto* t : { &setupTab, &drumMapsTab, &libraryTab, &masterclassTab, &songTab })
        {
            t->setBounds (tabs.removeFromRight (tabWidth (*t)));
            tabs.removeFromRight (gapW);
        }
        tabs.removeFromRight (4);
        themeBtn.setBounds (tabs.removeFromRight (40));
        headerArea = header.withTrimmedRight (16).toFloat();
        subtitleArea = juce::Rectangle<float> ((float) setupTab.getRight() - 300.0f, (float) setupTab.getBottom() + 4.0f, 300.0f, 18.0f);
        songPage.setBounds (r);
        libraryPage.setBounds (r);
        setupPage.setBounds (r);
        drumMapsPage.setBounds (r);
        masterclassPage.setBounds (r);
    }
}
