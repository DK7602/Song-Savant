#include "WelcomePanel.h"

namespace ss::ui
{
    namespace
    {
        struct Step { const char* title; const char* text; };
        const Step steps[] = {
            { "Describe your song",
              "On the SONG page, say what you're making in your own words (\"modern country like Morgan Wallen, warm, 120 BPM in A minor\") "
              "and click FIND MY SOUNDS." },
            { "Pick your sounds",
              "You get a card for every part, with a sound you already own and alternatives. Lock the ones you love, then COPY NAME "
              "and paste it in your DAW to load each one." },
            { "Hear it and build it",
              "PLAY the starter parts, drag them into your DAW, send the drums to Groove Grabber, lay the song out on ARRANGE, and "
              "learn the style on MASTERCLASS." } };
    }

    WelcomePanel::WelcomePanel()
    {
        setWantsKeyboardFocus (true);
        goBtn.onClick = [this] { close(); };
        addAndMakeVisible (goBtn);
        manualBtn.onClick = [this] { close(); if (onManual) onManual(); };
        addAndMakeVisible (manualBtn);
    }

    void WelcomePanel::show()
    {
        owl = owlImage();
        setVisible (true);
        toFront (true);
        resized();
        grabKeyboardFocus();
    }

    void WelcomePanel::close()
    {
        writeFlag (seenKey, true);
        setVisible (false);
        if (onClose) onClose();
    }

    bool WelcomePanel::keyPressed (const juce::KeyPress& k)
    {
        if (k == juce::KeyPress::escapeKey || k == juce::KeyPress::returnKey) { close(); return true; }
        return false;
    }

    void WelcomePanel::resized()
    {
        card = getLocalBounds().toFloat().withSizeKeepingCentre (juce::jmin (860.0f, (float) getWidth() - 60.0f),
                                                                   juce::jmin (600.0f, (float) getHeight() - 60.0f));
        auto r = card.reduced (40.0f, 30.0f);
        auto buttons = r.removeFromBottom (50.0f);
        goBtn.setBounds (buttons.removeFromRight (180.0f).toNearestInt());
        buttons.removeFromRight (14.0f);
        manualBtn.setBounds (buttons.removeFromRight (220.0f).toNearestInt());
        r.removeFromBottom (14.0f);
        r.removeFromTop (150.0f);
        stepsArea = r;
    }

    void WelcomePanel::paint (juce::Graphics& g)
    {
        const auto& p = pal();
        g.fillAll (juce::Colours::black.withAlpha (isLightTheme() ? 0.35f : 0.6f));
        drawBackground (g, card);
        drawPanel (g, card, 20.0f);

        auto top = card.reduced (40.0f, 30.0f).removeFromTop (130.0f);
        if (owl.isValid())
        {
            auto o = top.removeFromLeft (top.getHeight() * (float) owl.getWidth() / (float) owl.getHeight());
            g.drawImage (owl, o, juce::RectanglePlacement::centred);
            top.removeFromLeft (24.0f);
        }
        g.setColour (p.text);
        g.setFont (serif (36.0f));
        g.drawText ("Welcome to Song Savant", top.removeFromTop (top.getHeight() * 0.55f), juce::Justification::bottomLeft, true);
        g.setColour (p.gold);
        g.setFont (sans (17.0f, true));
        g.drawText ("Your instruments. Your song. The right sounds.", top.removeFromTop (30.0f), juce::Justification::centredLeft, true);
        g.setColour (p.muted);
        g.setFont (sans (14.0f));
        g.drawText ("It never suggests a sound you don't own, and it never changes your files.", top, juce::Justification::topLeft, true);

        // the three steps
        auto area = stepsArea;
        const float rowH = area.getHeight() / 3.0f;
        for (int i = 0; i < 3; ++i)
        {
            auto row = area.removeFromTop (rowH).reduced (0.0f, 6.0f);
            auto num = row.removeFromLeft (52.0f).withSizeKeepingCentre (40.0f, 40.0f).withY (row.getY() + 2.0f);
            g.setColour (p.gold.withAlpha (0.18f));
            g.fillEllipse (num);
            g.setColour (p.gold);
            g.drawEllipse (num, 1.5f);
            g.setFont (serif (20.0f, true));
            g.drawText (juce::String (i + 1), num, juce::Justification::centred);
            row.removeFromLeft (12.0f);
            g.setColour (p.text);
            g.setFont (sans (18.0f, true));
            g.drawText (steps[i].title, row.removeFromTop (26.0f), juce::Justification::centredLeft, true);
            drawParagraph (g, steps[i].text, row, sans (14.5f), p.muted);
        }
    }
}
