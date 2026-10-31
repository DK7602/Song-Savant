#include "SetupCheckPage.h"
#include "PluginProcessor.h"
#include "core/MidiClip.h"
#include "core/Paths.h"

namespace ss::ui
{
    namespace
    {
        const juce::String scanText = "Song Savant looks at your plug-ins, preset files, sound libraries and preset databases to learn "
                                      "what it can read. It only reads: nothing on your computer is changed or moved. "
                                      "It takes a minute or two.";
        const juce::String dragText = "Open a project in Cubase. Drag each item below onto the empty area under your tracks, "
                                      "then tell us what happened.";
        const juce::String reportText = "The report is saved automatically after the scan and after each drag test. "
                                        "Send the file back so we can plan the next step.";
    }

    SetupCheckPage::SetupCheckPage (SongSavantProcessor& p) : proc (p)
    {
        owl = owlImage();
        wordmark = wordmarkImage();

        for (auto* b : { &scanBtn, &stopBtn, &folderBtn, &openBtn, &showBtn, &copyBtn })
            addAndMakeVisible (b);
        addAndMakeVisible (progress);
        summaryView.setViewedComponent (&summary, false);
        summaryView.setScrollBarsShown (true, false);
        summaryView.setScrollBarThickness (8);
        addAndMakeVisible (summaryView);
        for (auto* c : { &userChip, &factoryChip, &midiChip }) addAndMakeVisible (c);
        for (auto* c : { &userResult, &factoryResult, &midiResult }) addAndMakeVisible (c);

        summary.setPlaceholder ("Click SCAN MY SETUP to start.");

        scanBtn.onClick = [this] { proc.startScan(); updateButtons(); };
        stopBtn.onClick = [this] { proc.scan.cancel(); };
        folderBtn.onClick = [this] { chooseExtraFolder(); };
        openBtn.onClick = [this]
        {
            if (auto f = proc.saveReport(); f.existsAsFile()) f.startAsProcess();
        };
        showBtn.onClick = [this]
        {
            if (auto f = proc.saveReport(); f.existsAsFile()) f.revealToUser();
        };
        copyBtn.onClick = [this]
        {
            juce::SystemClipboard::copyTextToClipboard (proc.reportText());
            reportStatus = "Report copied. Paste it into a message.";
            repaint();
        };

        // the MIDI clip for the drag test: always available
        midiFile = paths::tempDir().getChildFile ("Drag Test").getChildFile ("Song Savant test chords.mid");
        if (writeTestChordClip (midiFile, 120.0))
            midiChip.setFile (midiFile, "4 bars of chords: C, G, Am, F at 120 BPM", {});
        else
            midiChip.setFile ({}, {}, "Couldn't write the test clip.");

        auto hook = [this] (ResultPicker& picker, int DragTests::* field)
        {
            picker.setValue (proc.dragTests.*field);
            picker.onChange = [this, field] (int v) { proc.dragTests.*field = v; saveReportQuietly(); };
        };
        hook (userResult, &DragTests::userPreset);
        hook (factoryResult, &DragTests::factoryPreset);
        hook (midiResult, &DragTests::midiClip);

        refreshFromResult();
        startTimerHz (15);
    }

    SetupCheckPage::~SetupCheckPage()   { stopTimer(); }

    void SetupCheckPage::updateButtons()
    {
        const bool busy = proc.scan.isScanning();
        const bool haveResult = proc.scan.result() != nullptr;
        scanBtn.setEnabled (! busy);
        scanBtn.setButtonText (haveResult ? "SCAN AGAIN" : "SCAN MY SETUP");
        stopBtn.setVisible (busy);
        folderBtn.setEnabled (! busy);
        for (auto* b : { &openBtn, &showBtn, &copyBtn }) b->setEnabled (haveResult && ! busy);

        const auto n = proc.extraFolders.size();
        extraFoldersText = n == 0 ? juce::String()
                                  : "Also looking in: " + proc.extraFolders.getFirst().getFileName()
                                        + (n > 1 ? " and " + juce::String (n - 1) + " more" : juce::String());
    }

    void SetupCheckPage::updateProgress()
    {
        const bool busy = proc.scan.isScanning();
        if (busy || proc.scan.result() == nullptr)
            progress.set (busy ? proc.scan.progress() : 0.0f, busy ? proc.scan.stage() : juce::String ("Ready."), busy);
        else
            progress.set (1.0f, proc.scan.stage().isNotEmpty() ? proc.scan.stage() : juce::String ("Done."), false);
    }

    void SetupCheckPage::timerCallback()
    {
        updateProgress();
        if (proc.scan.generation() != seenGeneration)
        {
            refreshFromResult();
            saveReportQuietly();
        }
        updateButtons();
    }

    void SetupCheckPage::refreshFromResult()
    {
        seenGeneration = proc.scan.generation();
        auto r = proc.scan.result();
        if (r == nullptr)
        {
            userChip.setFile ({}, {}, "Scan first: we'll pick one of your preset files.");
            factoryChip.setFile ({}, {}, "Scan first: we'll try to copy a preset out of a HALion library.");
            updateButtons();
            return;
        }
        summary.setItems (summarise (*r));
        updateProgress();
        userChip.setFile (r->userPreset, r->userPresetLabel,
                          "No preset files found. In Cubase, open HALion Sonic, save any sound as a preset, then scan again.");
        factoryChip.setFile (r->factoryPreset, r->factoryPresetLabel,
                             "Couldn't copy a preset out of a sound library (that's useful to know too).");
        resized();
        updateButtons();
        repaint();
    }

    void SetupCheckPage::saveReportQuietly()
    {
        if (auto f = proc.saveReport(); f.existsAsFile())
            reportStatus = "Saved: Documents" + juce::File::getSeparatorString() + "Song Savant" + juce::File::getSeparatorString() + f.getFileName();
        repaint();
    }

    void SetupCheckPage::chooseExtraFolder()
    {
        chooser = std::make_unique<juce::FileChooser> ("A folder with instruments or sound libraries (for example on another drive)",
                                                       juce::File::getSpecialLocation (juce::File::userHomeDirectory));
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [safe = juce::Component::SafePointer<SetupCheckPage> (this)] (const juce::FileChooser& fc)
        {
            if (safe == nullptr) return;
            auto f = fc.getResult();
            if (f.isDirectory())
            {
                safe->proc.extraFolders.addIfNotAlreadyThere (f);
                safe->updateButtons();
                safe->repaint();
            }
        });
    }

    //==============================================================================
    void SetupCheckPage::paint (juce::Graphics& g)
    {
        const auto& p = pal();
        drawBackground (g, getLocalBounds().toFloat());

        // header: owl + wordmark, then the page name
        {
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
                auto w = h.removeFromLeft (wh * (float) wordmark.getWidth() / (float) wordmark.getHeight()).withSizeKeepingCentre (wh * (float) wordmark.getWidth() / (float) wordmark.getHeight(), wh);
                g.drawImage (wordmark, w, juce::RectanglePlacement::centred);
            }
            auto right = h.removeFromRight (300.0f);
            g.setColour (p.gold);
            g.setFont (serif (26.0f));
            g.drawText ("Setup Check", right.removeFromTop (right.getHeight() * 0.58f), juce::Justification::bottomRight);
            g.setColour (p.muted);
            g.setFont (sans (13.5f));
            g.drawText ("Phase 0  |  version " SS_VERSION, right, juce::Justification::topRight);
        }

        for (auto r : { scanPanel, dragPanel, reportPanel }) drawPanel (g, r);

        auto titleOf = [] (juce::Rectangle<float> panel) { return panel.reduced (22.0f, 0.0f).withTrimmedTop (18.0f).withHeight (30.0f); };
        drawSectionTitle (g, titleOf (scanPanel), 1, "Scan your setup");
        drawSectionTitle (g, titleOf (dragPanel), 2, "Drag test with Cubase");
        drawSectionTitle (g, titleOf (reportPanel), 3, "Send the report");

        drawParagraph (g, scanText, scanIntro, sans (14.5f), p.muted);
        drawParagraph (g, dragText, dragIntro, sans (14.5f), p.muted);
        drawParagraph (g, reportText, reportIntro, sans (14.5f), p.muted);

        if (extraFoldersText.isNotEmpty())
        {
            g.setColour (p.muted);
            g.setFont (sans (13.0f));
            g.drawText (extraFoldersText, extraLine, juce::Justification::centredLeft, true);
        }
        g.setColour (reportStatus.isNotEmpty() ? p.good : p.dim);
        g.setFont (sans (13.5f));
        g.drawText (reportStatus.isNotEmpty() ? reportStatus : juce::String ("No report yet: scan first."), reportLine, juce::Justification::centredLeft, true);
    }

    void SetupCheckPage::resized()
    {
        auto r = getLocalBounds().toFloat().reduced (24.0f, 16.0f);
        headerArea = r.removeFromTop (92.0f);
        r.removeFromTop (12.0f);

        const float gap = 20.0f;
        auto left = r.removeFromLeft (r.getWidth() * 0.56f);
        r.removeFromLeft (gap);
        auto right = r;

        // 1. scan
        scanPanel = left;
        {
            auto in = scanPanel.reduced (22.0f).withTrimmedTop (44.0f);
            const float th = paragraphHeight (scanText, in.getWidth(), sans (14.5f));
            scanIntro = in.removeFromTop (th);
            in.removeFromTop (14.0f);
            auto buttons = in.removeFromTop (46.0f);
            scanBtn.setBounds (buttons.removeFromLeft (200.0f).toNearestInt());
            buttons.removeFromLeft (10.0f);
            stopBtn.setBounds (buttons.removeFromLeft (90.0f).toNearestInt());
            folderBtn.setBounds (buttons.removeFromRight (juce::jmin (240.0f, buttons.getWidth())).toNearestInt());
            extraLine = in.removeFromTop (22.0f);
            in.removeFromTop (4.0f);
            progress.setBounds (in.removeFromTop (34.0f).toNearestInt());
            in.removeFromTop (8.0f);
            summaryView.setBounds (in.toNearestInt());
            const int w = summaryView.getWidth() - 10;
            summary.setSize (w, juce::jmax (summary.heightFor (w), summaryView.getHeight()));
        }

        // 3. report (bottom right), sized to its content; 2. drag tests take the rest
        {
            const float w = right.getWidth() - 44.0f;
            const float h = 44.0f + paragraphHeight (reportText, w, sans (14.5f)) + 14.0f + 44.0f + 8.0f + 24.0f + 22.0f;
            reportPanel = right.removeFromBottom (h);
            right.removeFromBottom (gap);
            dragPanel = right;

            auto in = reportPanel.reduced (22.0f).withTrimmedTop (44.0f);
            reportIntro = in.removeFromTop (paragraphHeight (reportText, in.getWidth(), sans (14.5f)));
            in.removeFromTop (14.0f);
            auto buttons = in.removeFromTop (44.0f);
            const float bw = (buttons.getWidth() - 16.0f) / 3.0f;
            openBtn.setBounds (buttons.removeFromLeft (bw).toNearestInt());
            buttons.removeFromLeft (8.0f);
            showBtn.setBounds (buttons.removeFromLeft (bw).toNearestInt());
            buttons.removeFromLeft (8.0f);
            copyBtn.setBounds (buttons.toNearestInt());
            in.removeFromTop (8.0f);
            reportLine = in.removeFromTop (24.0f);
        }
        {
            auto in = dragPanel.reduced (22.0f).withTrimmedTop (44.0f);
            dragIntro = in.removeFromTop (paragraphHeight (dragText, in.getWidth(), sans (14.5f)));
            in.removeFromTop (10.0f);
            const float each = juce::jmin (118.0f, in.getHeight() / 3.0f);
            for (auto [chip, picker] : { std::pair<DragChip*, ResultPicker*> { &userChip, &userResult },
                                         std::pair<DragChip*, ResultPicker*> { &factoryChip, &factoryResult },
                                         std::pair<DragChip*, ResultPicker*> { &midiChip, &midiResult } })
            {
                auto slot = in.removeFromTop (each);
                chip->setBounds (slot.removeFromTop (slot.getHeight() - 40.0f).toNearestInt());
                slot.removeFromTop (5.0f);
                picker->setBounds (slot.removeFromTop (30.0f).withTrimmedLeft (slot.getWidth() * 0.35f).toNearestInt());
            }
        }
    }
}
