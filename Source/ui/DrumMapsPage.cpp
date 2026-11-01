#include "DrumMapsPage.h"
#include "PluginProcessor.h"

namespace ss::ui
{
    namespace
    {
        juce::Colour padColour (const juce::String& kind)
        {
            if (kind == "Kick" || kind == "808") return kindColour (Kind::drums);
            if (kind == "Snare" || kind == "Clap") return pal().gold;
            if (kind.contains ("Hat") || kind == "Cymbal") return kindColour (Kind::lead);
            if (kind == "Tom") return pal().purple;
            return pal().muted;
        }
    }

    int PadList::heightNeeded() const
    {
        return map == nullptr ? 0 : (int) (rowHeight * (float) map->pads.size()) + 4;
    }

    int PadList::rowAt (float y) const
    {
        if (map == nullptr || y < 0) return -1;
        const int r = (int) (y / rowHeight);
        return r < (int) map->pads.size() ? r : -1;
    }

    void PadList::mouseUp (const juce::MouseEvent& e)
    {
        const int r = rowAt (e.position.y);
        if (r < 0) return;
        if (e.mods.isShiftDown() && anchor >= 0) { first = std::min (anchor, r); last = std::max (anchor, r); }
        else { anchor = first = last = r; }
        repaint();
        if (onPadsClicked)
            onPadsClicked (first, last, localAreaToGlobal (juce::Rectangle<int> (0, (int) ((float) r * rowHeight), getWidth(), (int) rowHeight)));
    }

    void PadList::paint (juce::Graphics& g)
    {
        if (map == nullptr) return;
        const auto& p = pal();
        auto r = getLocalBounds().toFloat();
        int i = 0;
        for (auto& pad : map->pads)
        {
            auto row = r.removeFromTop (rowHeight);
            const bool over = i == hover, picked = i >= first && i <= last;
            if (over || picked || i % 2 == 0)
            {
                g.setColour (picked ? p.gold.withAlpha (0.22f) : over ? p.purple.withAlpha (0.35f) : p.panelHi.withAlpha (0.45f));
                g.fillRoundedRectangle (row.reduced (0.0f, 1.0f), 6.0f);
            }
            ++i;
            if (over)
            {
                g.setColour (p.muted);
                g.setFont (sans (11.5f));
                g.drawText ("rename", row.withTrimmedRight (12.0f), juce::Justification::centredRight);
            }
            row.removeFromLeft (10.0f);
            g.setColour (p.text);
            g.setFont (sans (14.0f, true));
            g.drawText (noteName (pad.note), row.removeFromLeft (52.0f), juce::Justification::centredLeft);
            g.setColour (p.dim);
            g.setFont (sans (12.0f));
            g.drawText ("key " + juce::String (pad.note), row.removeFromLeft (62.0f), juce::Justification::centredLeft);
            const auto c = padColour (pad.kind);
            g.setColour (c);
            g.fillEllipse (row.removeFromLeft (14.0f).withSizeKeepingCentre (9.0f, 9.0f));
            row.removeFromLeft (8.0f);
            g.setColour (p.text);
            g.setFont (sans (15.0f));
            g.drawText (pad.name, row, juce::Justification::centredLeft, true);
        }
    }

    //==============================================================================
    DrumMapsPage::DrumMapsPage (SongSavantProcessor& p) : proc (p)
    {
        addAndMakeVisible (chooseBtn);
        chooseBtn.onClick = [this] { choosePreset(); };
        addChildComponent (showBtn);
        showBtn.onClick = [this] { if (savedFile.existsAsFile()) savedFile.revealToUser(); };
        addChildComponent (againBtn);
        againBtn.onClick = [this] { if (proc.kitMap.preset().existsAsFile()) mapPreset (proc.kitMap.preset()); };
        pads.onPadsClicked = [this] (int first, int last, juce::Rectangle<int> area) { renamePads (first, last, area); };
        daw = proc.dawName();
        padsView.setViewedComponent (&pads, false);
        padsView.setScrollBarsShown (true, false);
        addAndMakeVisible (padsView);
        timerCallback();          // a kit mapped earlier (while the window was closed) shows straight away
        startTimerHz (8);
    }

    DrumMapsPage::~DrumMapsPage()   { stopTimer(); }

    bool DrumMapsPage::isInterestedInFileDrag (const juce::StringArray& files)
    {
        for (auto& f : files) if (f.endsWithIgnoreCase (".vstpreset")) return true;
        return true;   // say why other files can't be used, rather than refusing them silently
    }

    void DrumMapsPage::filesDropped (const juce::StringArray& files, int, int)
    {
        dropHover = false;
        for (auto& f : files)
            if (f.endsWithIgnoreCase (".vstpreset")) { mapPreset (juce::File (f)); return; }
        dropProblem = "That's not a preset file. Save the kit as a preset in your DAW first (see step 2), then drop the .vstpreset file here.";
        repaint();
    }

    void DrumMapsPage::choosePreset()
    {
        auto start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("VST3 Presets");
        chooser = std::make_unique<juce::FileChooser> ("Choose a drum kit preset", start, "*.vstpreset");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            if (fc.getResult().existsAsFile()) mapPreset (fc.getResult());
        });
    }

    void DrumMapsPage::changeMap (const std::function<void (KitMap&)>& change)
    {
        auto current = proc.kitMap.result();
        if (current == nullptr) return;
        KitMap k = *current;
        change (k);
        proc.kitMap.setResult (proc.kitMap.preset(), k);   // saved again straight away
        pads.clearSelection();
        timerCallback();
    }

    void DrumMapsPage::renamePads (int first, int last, juce::Rectangle<int> area)
    {
        auto current = proc.kitMap.result();
        if (current == nullptr || first < 0 || last >= (int) current->pads.size()) return;
        const auto pad = current->pads[(size_t) first];
        const auto lastPad = current->pads[(size_t) last];
        const bool range = last > first;
        const int count = last - first + 1;

        juce::PopupMenu m;
        m.addSectionHeader (range ? noteName (pad.note) + " to " + noteName (lastPad.note) + "  (" + juce::String (count) + " keys)  are..."
                                  : noteName (pad.note) + "  (key " + juce::String (pad.note) + ")  is a...");
        int id = 1;
        for (auto& kind : padKinds()) m.addItem (id++, kind, true, ! range && ! pad.custom && kind == pad.kind);
        m.addSeparator();
        m.addItem (1000, "Type a name...");
        m.addItem (1001, range ? "Leave these keys out of the map" : "Leave this key out of the map");
        if (! range)
        {
            m.addSeparator();
            m.addItem (-1, "Tip: shift-click another pad to rename a whole range", false);
        }
        juce::Component::SafePointer<DrumMapsPage> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (area).withMinimumWidth (240), [safe, first, last, range, pad] (int r)
        {
            if (safe == nullptr) return;
            if (r <= 0) { safe->pads.clearSelection(); return; }
            if (r <= padKinds().size())
            {
                const auto kind = padKinds()[r - 1];
                safe->changeMap ([=] (KitMap& k) { setPadKind (k, first, last, kind); });
            }
            else if (r == 1001)
                safe->changeMap ([=] (KitMap& k) { removePads (k, first, last); });
            else if (r == 1000)
            {
                safe->nameWindow = std::make_unique<juce::AlertWindow> (range ? juce::String ("Name these keys") : "Name " + noteName (pad.note),
                                                                        range ? "What are these pads? (they'll be numbered: Conga, Conga 2...)"
                                                                              : "What should this pad be called?",
                                                                        juce::MessageBoxIconType::NoIcon);
                safe->nameWindow->addTextEditor ("name", range ? juce::String() : pad.name);
                safe->nameWindow->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
                safe->nameWindow->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                safe->nameWindow->enterModalState (true, juce::ModalCallbackFunction::create ([safe, first, last, range] (int result)
                {
                    if (safe == nullptr || safe->nameWindow == nullptr) return;
                    const auto typed = safe->nameWindow->getTextEditorContents ("name").trim();
                    safe->nameWindow->setVisible (false);
                    if (result == 1 && typed.isNotEmpty())
                        safe->changeMap ([=] (KitMap& k) { if (range) setPadKind (k, first, last, typed); else setPadName (k, first, typed); });
                    else
                        safe->pads.clearSelection();
                    juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->nameWindow.reset(); });
                }), false);
            }
        });
    }

    juce::String DrumMapsPage::loadSteps (const juce::String& mapName) const
    {
        const auto arrow = juce::String::fromUTF8 (" \xe2\x80\xba ");
        const auto name = mapName.isNotEmpty() ? mapName : juce::String ("your kit");
        if (daw.isEmpty() || daw == "Cubase" || daw == "Nuendo")
            return "When the map is ready, in " + (daw.isNotEmpty() ? daw : juce::String ("Cubase")) + ": open the MIDI menu" + arrow + "Drum Map Setup... "
                   "(or click the Drum Map field in the instrument track's Inspector" + arrow + "Drum Map Setup...). If an older " + name
                   + " is in the list, select it and click Remove. Click Load..., pick " + name + ".drm in Documents" + arrow + "Song Savant"
                   + arrow + "Drum Maps, then OK. Last, set the track's Drum Map field (in the Inspector) to " + name + ".";
        return "Song Savant makes Cubase drum maps (.drm) for now, so this step needs Cubase. Pad names for " + daw + " are planned.";
    }

    void DrumMapsPage::mapPreset (const juce::File& preset)
    {
        dropProblem.clear();
        savedFile = juce::File();
        proc.kitMap.start (preset, proc.locationProvider());
        refresh();
    }

    void DrumMapsPage::timerCallback()
    {
        if (proc.kitMap.generation() != seenGeneration)
        {
            seenGeneration = proc.kitMap.generation();
            savedFile = juce::File();
            if (auto m = proc.kitMap.result(); m != nullptr && m->ok() && ! m->oneSound)
            {
                savedFile = saveCubaseDrumMap (*m);       // saved straight away: one less thing to click
                detailsFile = saveHitDetails (*m);        // what it heard, key by key (to send back if names are wrong)
            }
            refresh();
        }
        if (proc.kitMap.isMapping()) repaint (statusArea.toNearestInt().expanded (4));
    }

    void DrumMapsPage::refresh()
    {
        auto m = proc.kitMap.isMapping() ? nullptr : proc.kitMap.result();
        pads.setMap (m != nullptr && m->ok() ? m : nullptr);
        pads.setSize (juce::jmax (10, padsView.getMaximumVisibleWidth()), pads.heightNeeded());
        showBtn.setVisible (savedFile.existsAsFile());
        againBtn.setVisible (m != nullptr && ! proc.kitMap.isMapping());
        chooseBtn.setEnabled (! proc.kitMap.isMapping());
        repaint();
    }

    void DrumMapsPage::paint (juce::Graphics& g)
    {
        const auto& p = pal();
        drawPanel (g, leftPanel);
        drawPanel (g, rightPanel);

        // left: how it works
        auto steps = stepsArea;
        g.setColour (p.text);
        g.setFont (serif (24.0f));
        g.drawText ("Map a drum kit", steps.removeFromTop (34.0f), juce::Justification::centredLeft);
        g.setColour (p.muted);
        g.setFont (sans (13.5f));
        steps.removeFromTop (drawParagraph (g, "For kits with no drum map in Cubase (Analog Lab, HALion, other plug-ins): Song Savant plays every key "
                                               "and names each pad, so the drum editor shows Kick, Snare, Hat... instead of note numbers.",
                                            steps, sans (13.5f), p.muted) + 12.0f);
        const char* lines[] = {
            "In Cubase, load the kit on an instrument track and pick the sound you want.",
            "In the plug-in window's top bar, click the small \xe2\x96\xbe next to the \xe2\x96\xb2 \xe2\x96\xbc arrows, choose Save Preset... and give it a name.",
            "Drag that preset onto this page (from MediaBay, or Documents \xe2\x80\xba VST3 Presets), or click CHOOSE A PRESET." };
        auto step = [&] (juce::Rectangle<float>& area, int number, const juce::String& text, float size)
        {
            auto row = area;
            auto num = row.removeFromLeft (30.0f).removeFromTop (24.0f);
            g.setColour (p.gold);
            g.drawEllipse (num.withSizeKeepingCentre (22.0f, 22.0f), 1.4f);
            g.setFont (sans (12.5f, true));
            g.drawText (juce::String (number), num, juce::Justification::centred);
            const float h = drawParagraph (g, text, row.withTrimmedLeft (6.0f), sans (size), p.text);
            area.removeFromTop (juce::jmax (26.0f, h) + 12.0f);
        };
        for (int i = 0; i < 3; ++i) step (steps, i + 1, juce::String::fromUTF8 (lines[i]), 14.0f);
        {
            auto m4 = proc.kitMap.result();
            auto s4 = step4Area;
            step (s4, 4, loadSteps (m4 != nullptr ? m4->name : juce::String()), 13.0f);
        }

        // the drop zone
        {
            juce::Path dash, outline;
            outline.addRoundedRectangle (dropArea, 14.0f);
            const float pattern[] = { 7.0f, 5.0f };
            juce::PathStrokeType (1.6f).createDashedStroke (dash, outline, pattern, 2);
            g.setColour ((dropHover ? p.gold : p.purple).withAlpha (dropHover ? 0.16f : 0.07f));
            g.fillPath (outline);
            g.setColour (dropHover ? p.gold : p.purple.withAlpha (0.8f));
            g.fillPath (dash);
            g.setColour (dropHover ? p.gold : p.text);
            g.setFont (sans (17.0f, true));
            g.drawText ("Drop a kit preset here", dropArea.withTrimmedBottom (dropArea.getHeight() * 0.45f), juce::Justification::centredBottom);
        }

        // what's happening
        {
            juce::String s;
            juce::Colour c = p.muted;
            auto m = proc.kitMap.result();
            if (proc.kitMap.isMapping())        { s = proc.kitMap.status(); c = p.gold; }
            else if (dropProblem.isNotEmpty())  { s = dropProblem; c = p.warn; }
            else if (m != nullptr && ! m->ok()) { s = "Couldn't map it: " + m->error; c = p.warn; }
            else if (m != nullptr)              { s = "Mapped " + m->name + " from " + m->plugin + "."; c = p.good; }
            else                                  s = "Takes about a minute: the plug-in is opened in the background (Song Savant Scanner) and every key from C0 to C6 is played.";
            drawParagraph (g, s, statusArea, sans (13.5f, proc.kitMap.isMapping()), c);
        }

        // right: the result
        const auto arrow = juce::String::fromUTF8 (" \xe2\x80\xba ");
        auto m = proc.kitMap.isMapping() ? nullptr : proc.kitMap.result();
        if (m == nullptr || ! m->ok())
        {
            g.setColour (p.dim);
            g.setFont (serif (20.0f));
            g.drawFittedText (proc.kitMap.isMapping() ? "Listening to every key..." : "Your kit's pads will show here.",
                              rightPanel.reduced (30.0f).toNearestInt(), juce::Justification::centred, 2);
            return;
        }
        g.setColour (p.text);
        g.setFont (serif (26.0f));
        g.drawText (m->name, titleArea, juce::Justification::centredLeft, true);
        juce::String sub = m->plugin + "  |  " + juce::String ((int) m->pads.size()) + (m->pads.size() == 1 ? " sound" : " sounds")
                           + "  |  click a pad to rename it, shift-click for a range";
        g.setColour (p.gold);
        g.setFont (sans (14.5f, true));
        g.drawText (sub, resultTextArea.withHeight (20.0f), juce::Justification::centredLeft, true);

        juce::String how;
        if (m->oneSound)
            how = "This preset plays one sound up and down the keyboard (like an 808 or a single drum), so it doesn't need a drum map.";
        else if (savedFile.existsAsFile())
            how = "Saved as " + savedFile.getFileName() + " in Documents" + arrow + "Song Savant" + arrow + "Drum Maps: load it as in step 4. "
                  "A name wrong? Click the pad (shift-click a second pad for a whole range). What Song Savant heard on each key is saved next to "
                  "it (" + detailsFile.getFileName() + ") if you want to send it in.";
        else
            how = "Couldn't save the drum map in Documents" + arrow + "Song Savant" + arrow + "Drum Maps.";
        drawParagraph (g, how, howArea, sans (13.0f), p.text);
    }

    void DrumMapsPage::paintOverChildren (juce::Graphics& g)
    {
        if (! dropHover) return;
        g.setColour (pal().gold.withAlpha (0.08f));
        g.fillRect (getLocalBounds());
    }

    void DrumMapsPage::resized()
    {
        auto r = getLocalBounds().toFloat().reduced (24.0f, 0.0f).withTrimmedBottom (20.0f);
        const float gap = 18.0f;
        leftPanel = r.removeFromLeft (juce::jlimit (380.0f, 520.0f, r.getWidth() * 0.42f));
        r.removeFromLeft (gap);
        rightPanel = r;

        {
            auto in = leftPanel.reduced (22.0f, 20.0f);
            stepsArea = in.removeFromTop (in.getHeight() * 0.42f);
            dropArea = in.removeFromTop (juce::jlimit (90.0f, 120.0f, in.getHeight() * 0.3f));
            chooseBtn.setBounds (dropArea.withTrimmedTop (dropArea.getHeight() * 0.55f).withSizeKeepingCentre (220.0f, 36.0f).toNearestInt());
            in.removeFromTop (10.0f);
            statusArea = in.removeFromTop (44.0f);
            in.removeFromTop (8.0f);
            step4Area = in;
        }
        {
            auto in = rightPanel.reduced (22.0f, 20.0f);
            titleArea = in.removeFromTop (36.0f);
            resultTextArea = in.removeFromTop (24.0f);
            in.removeFromTop (8.0f);
            auto bottom = in.removeFromBottom (juce::jmin (130.0f, in.getHeight() * 0.4f));
            auto buttons = bottom.removeFromBottom (40.0f);
            showBtn.setBounds (buttons.removeFromLeft (220.0f).toNearestInt());
            buttons.removeFromLeft (12.0f);
            againBtn.setBounds (buttons.removeFromLeft (160.0f).toNearestInt());
            bottom.removeFromBottom (8.0f);
            howArea = bottom;
            in.removeFromBottom (10.0f);
            padsView.setBounds (in.toNearestInt());
        }
        refresh();
    }
}
