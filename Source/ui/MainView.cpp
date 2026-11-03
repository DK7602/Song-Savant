#include "MainView.h"
#include "PluginProcessor.h"

namespace ss::ui
{
    MainView::MainView (SongSavantProcessor& p) : proc (p), songPage (p), libraryPage (p), setupPage (p), drumMapsPage (p), masterclassPage (p), arrangePage (p), pianoRoll (p)
    {
        setLookAndFeel (&lookAndFeel);
        owl = owlImage();
        wordmark = wordmarkImage();
        for (auto* t : { &songTab, &arrangeTab, &masterclassTab, &libraryTab, &drumMapsTab })
        {
            t->setRadioGroupId (1);
            t->setComponentID ("tab");
            addAndMakeVisible (t);
        }
        themeBtn.setTooltip ("Light or dark");
        themeBtn.onClick = [this] { toggleTheme(); };
        addAndMakeVisible (themeBtn);
        songTab.onClick = [this] { showPage (song); };
        libraryTab.onClick = [this] { showPage (library); };
        setupBtn.setTooltip ("Setup check: what Song Savant found on this computer");
        setupBtn.onClick = [this] { showPage (setupCheck); };
        addAndMakeVisible (setupBtn);
        helpBtn.setTooltip ("The manual (F1)");
        helpBtn.onClick = [this] { showHelp(); };
        addAndMakeVisible (helpBtn);
        arrangeTab.onClick = [this] { showPage (arrange); };
        drumMapsTab.onClick = [this] { showPage (drumMaps); };
        masterclassTab.onClick = [this] { showPage (masterclass); };
        masterclassPage.onUseForSong = [this] { songPage.showForm(); showPage (song); };
        addChildComponent (songPage);
        addChildComponent (libraryPage);
        addChildComponent (setupPage);
        addChildComponent (drumMapsPage);
        addChildComponent (masterclassPage);
        addChildComponent (arrangePage);
        addChildComponent (pianoRoll);
        pianoRoll.onDone = [this] { showPage (page); };
        songPage.onEditPart = [this] (const juce::String& role) { showPianoRoll (role, -1); };
        arrangePage.onEditPattern = [this] (const juce::String& role, int section) { showPianoRoll (role, section); };
        // a MIDI pattern dropped on a card or a box: that part (and section) in the piano roll, with the pattern in it
        songPage.onDropPart = [this] (const juce::String& role, const juce::File& f) { if (showPianoRoll (role, -1)) pianoRoll.dropFile (f); };
        arrangePage.onDropPattern = [this] (const juce::String& role, int section, const juce::File& f)
        {
            if (showPianoRoll (role, section)) pianoRoll.dropFile (f);
        };
        addChildComponent (help);              // over everything, when it's open
        welcome.onManual = [this] { showHelp(); };
        addChildComponent (welcome);
        showPage (proc.page);
        // the very first time Song Savant opens: what it does, in three steps
        if (! readFlag (WelcomePanel::seenKey))
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MainView> (this)]
            {
                if (safe != nullptr) { safe->welcome.setBounds (safe->getLocalBounds()); safe->welcome.show(); }
            });
    }

    MainView::~MainView()
    {
        if (listeningTo != nullptr) listeningTo->removeKeyListener (this);
        setLookAndFeel (nullptr);
    }

    void MainView::toggleTheme()
    {
        setLightTheme (! isLightTheme());
        applyTheme();
    }

    // like Groove Grabber and the other apps: a click on the logo switches light / dark
    void MainView::mouseMove (const juce::MouseEvent& e)
    {
        const bool over = logoArea.contains (e.position);
        setMouseCursor (over ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        setTooltip (over ? "Light or dark" : juce::String());
    }

    void MainView::mouseExit (const juce::MouseEvent&)
    {
        setMouseCursor (juce::MouseCursor::NormalCursor);
        setTooltip ({});
    }

    void MainView::mouseUp (const juce::MouseEvent& e)
    {
        if (logoArea.contains (e.position) && logoArea.contains (e.mouseDownPosition)) toggleTheme();
    }

    void MainView::showHelp()
    {
        help.setBounds (getLocalBounds());
        help.show();
    }

    void MainView::parentHierarchyChanged()
    {
        // F1 opens the manual wherever the keyboard focus is: listen at the window's top
        auto* top = getTopLevelComponent();
        if (top == listeningTo.getComponent()) return;
        if (listeningTo != nullptr) listeningTo->removeKeyListener (this);
        listeningTo = top;
        if (top != nullptr) top->addKeyListener (this);
    }

    bool MainView::keyPressed (const juce::KeyPress& k, juce::Component*)
    {
        if (k.getKeyCode() == juce::KeyPress::F1Key)
        {
            if (help.isVisible()) help.setVisible (false);
            else showHelp();
            return true;
        }
        return false;
    }

    void MainView::applyTheme()
    {
        lookAndFeel.applyColours();
        owl = owlImage();
        wordmark = wordmarkImage();
        sendLookAndFeelChange();
        songPage.themeChanged();
        pianoRoll.themeChanged();
        libraryPage.themeChanged();
        drumMapsPage.themeChanged();
        masterclassPage.themeChanged();
        arrangePage.themeChanged();
        help.themeChanged();
        repaint();
    }

    bool MainView::showPianoRoll (const juce::String& role, int section)
    {
        if (! pianoRoll.open (role, section)) return false;
        // the page it came from stays lit in the header; DONE goes back to it
        for (auto* p : std::initializer_list<juce::Component*> { &songPage, &libraryPage, &setupPage, &drumMapsPage, &masterclassPage, &arrangePage })
            p->setVisible (false);
        pianoRoll.setVisible (true);
        pianoRoll.grabKeyboardFocus();
        return true;
    }

    void MainView::showPage (int which)
    {
        page = juce::jlimit ((int) song, (int) arrange, which);
        proc.page = page;
        songPage.setVisible (page == song);
        libraryPage.setVisible (page == library);
        setupPage.setVisible (page == setupCheck);
        drumMapsPage.setVisible (page == drumMaps);
        masterclassPage.setVisible (page == masterclass);
        arrangePage.setVisible (page == arrange);
        pianoRoll.setVisible (false);
        songTab.setToggleState (page == song, juce::dontSendNotification);
        libraryTab.setToggleState (page == library, juce::dontSendNotification);
        setupBtn.setToggleState (page == setupCheck, juce::dontSendNotification);
        arrangeTab.setToggleState (page == arrange, juce::dontSendNotification);
        drumMapsTab.setToggleState (page == drumMaps, juce::dontSendNotification);
        masterclassTab.setToggleState (page == masterclass, juce::dontSendNotification);
        repaint();
    }

    void MainView::paint (juce::Graphics& g)
    {
        const auto& p = pal();
        drawBackground (g, getLocalBounds().toFloat());

        // the owl and the name, the same size in both themes (the two sets of pictures are drawn differently, so they're
        // sized by their solid parts: the owl's body, the title's letters), smaller when the window is narrow
        const auto h = headerArea;
        if (owl.isValid() && wordmark.isValid())
        {
            const auto oc = owlContent(), wc = wordmarkContent();
            const float H = h.getHeight();
            // the dark design's proportions: the owl's body 0.9 of the header's height, the letters about 0.61 x 2.66 of it
            // (the light title's letters are taller and narrower: they get the same area)
            float so = 0.9f * H / oc.getHeight();
            float sw = std::sqrt (0.607f * H * 2.656f * H / (wc.getWidth() * wc.getHeight()));
            const float gap = 12.0f;
            const float natural = oc.getWidth() * so + gap + wc.getWidth() * sw;
            const float fit = juce::jmin (1.0f, h.getWidth() / natural);
            so *= fit;
            sw *= fit;
            const float cy = h.getCentreY();
            const float ox = h.getX();
            const juce::Rectangle<float> owlRect { ox - oc.getX() * so, cy - (float) owl.getHeight() * so * 0.5f,
                                                   (float) owl.getWidth() * so, (float) owl.getHeight() * so };
            g.drawImage (owl, owlRect, juce::RectanglePlacement::stretchToFit);
            const float wx = ox + oc.getWidth() * so + gap * fit;
            const juce::Rectangle<float> wordRect { wx - wc.getX() * sw, cy - (float) wordmark.getHeight() * sw * 0.5f,
                                                    (float) wordmark.getWidth() * sw, (float) wordmark.getHeight() * sw };
            g.drawImage (wordmark, wordRect, juce::RectanglePlacement::stretchToFit);
            logoArea = owlRect.getUnion (wordRect).getIntersection (h.expanded (0.0f, 8.0f));
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
        int total = 40 + 8 + 40 + 8 + 40 + 12;
        for (auto* t : { &songTab, &arrangeTab, &masterclassTab, &libraryTab, &drumMapsTab }) total += tabWidth (*t) + gapW;
        auto tabs = header.removeFromRight (total).withSizeKeepingCentre (total, 40).translated (0, -8);
        for (auto* t : { &drumMapsTab, &libraryTab, &masterclassTab, &arrangeTab, &songTab })
        {
            t->setBounds (tabs.removeFromRight (tabWidth (*t)));
            tabs.removeFromRight (gapW);
        }
        tabs.removeFromRight (4);
        setupBtn.setBounds (tabs.removeFromRight (40));
        tabs.removeFromRight (8);
        themeBtn.setBounds (tabs.removeFromRight (40));
        tabs.removeFromRight (8);
        helpBtn.setBounds (tabs.removeFromRight (40));
        headerArea = header.withTrimmedRight (16).toFloat();
        subtitleArea = juce::Rectangle<float> ((float) drumMapsTab.getRight() - 300.0f, (float) drumMapsTab.getBottom() + 4.0f, 300.0f, 18.0f);
        songPage.setBounds (r);
        libraryPage.setBounds (r);
        setupPage.setBounds (r);
        drumMapsPage.setBounds (r);
        masterclassPage.setBounds (r);
        arrangePage.setBounds (r);
        pianoRoll.setBounds (r);
        help.setBounds (getLocalBounds());
        welcome.setBounds (getLocalBounds());
    }
}
