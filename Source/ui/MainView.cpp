#include "MainView.h"
#include "PluginProcessor.h"

namespace ss::ui
{
    MainView::MainView (SongSavantProcessor& p) : proc (p), songPage (p), libraryPage (p), setupPage (p)
    {
        setLookAndFeel (&lookAndFeel);
        owl = owlImage();
        wordmark = wordmarkImage();
        for (auto* t : { &songTab, &libraryTab, &setupTab })
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
        addChildComponent (songPage);
        addChildComponent (libraryPage);
        addChildComponent (setupPage);
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
        repaint();
    }

    void MainView::showPage (int which)
    {
        page = juce::jlimit ((int) song, (int) setupCheck, which);
        proc.page = page;
        songPage.setVisible (page == song);
        libraryPage.setVisible (page == library);
        setupPage.setVisible (page == setupCheck);
        songTab.setToggleState (page == song, juce::dontSendNotification);
        libraryTab.setToggleState (page == library, juce::dontSendNotification);
        setupTab.setToggleState (page == setupCheck, juce::dontSendNotification);
        repaint();
    }

    void MainView::paint (juce::Graphics& g)
    {
        const auto& p = pal();
        drawBackground (g, getLocalBounds().toFloat());

        auto h = headerArea;
        if (owl.isValid())
        {
            auto o = h.removeFromLeft (h.getHeight() * (float) owl.getWidth() / (float) owl.getHeight());
            g.drawImage (owl, o, juce::RectanglePlacement::centred);
        }
        h.removeFromLeft (10.0f);
        if (wordmark.isValid())
        {
            const float wh = h.getHeight() * 0.86f;
            const float ww = wh * (float) wordmark.getWidth() / (float) wordmark.getHeight();
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
        headerArea = header.toFloat();
        auto tabs = header.removeFromRight (540).withSizeKeepingCentre (540, 40).translated (0, -8);
        setupTab.setBounds (tabs.removeFromRight (170));
        tabs.removeFromRight (12);
        libraryTab.setBounds (tabs.removeFromRight (140));
        tabs.removeFromRight (12);
        songTab.setBounds (tabs.removeFromRight (120));
        tabs.removeFromRight (16);
        themeBtn.setBounds (tabs.removeFromRight (40));
        subtitleArea = juce::Rectangle<float> ((float) setupTab.getRight() - 300.0f, (float) setupTab.getBottom() + 4.0f, 300.0f, 18.0f);
        songPage.setBounds (r);
        libraryPage.setBounds (r);
        setupPage.setBounds (r);
    }
}
