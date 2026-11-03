#include "SetupCheckPage.h"
#include "PluginProcessor.h"

namespace ss::ui
{
    namespace
    {
        const juce::String scanText = "Song Savant looks at your plug-ins, preset files, sound libraries and preset databases to learn "
                                      "what it can read. It only reads: nothing on your computer is changed or moved. "
                                      "It takes a minute or two.";
        const juce::String reportText = "If a sound or instrument you own doesn't show up in the LIBRARY or on the cards, the report "
                                        "shows what Song Savant could and couldn't read on this computer. It's saved after every scan. "
                                        "Attach it (or paste it) when you ask for help. Your user name is never in it.";
        const juce::StringArray tips {
            "Just installed something? Click REFRESH LIBRARY on the LIBRARY page so Song Savant reads your sounds again.",
            "Steinberg sounds come from MediaBay: open Cubase's MediaBay once so it lists new libraries, then refresh.",
            "Instruments are found as VST3 plug-ins. One that's only installed as VST2 or AAX isn't listed.",
            "Sounds on another drive? ALSO LOOK IN A FOLDER..., then SCAN AGAIN, so the report shows what's there."
        };
        const auto tipFont = [] { return sans (14.0f); };
    }

    SetupCheckPage::SetupCheckPage (SongSavantProcessor& p) : proc (p)
    {
        for (auto* b : { &scanBtn, &stopBtn, &folderBtn, &openBtn, &showBtn, &copyBtn })
            addAndMakeVisible (b);
        addAndMakeVisible (progress);
        summaryView.setViewedComponent (&summary, false);
        summaryView.setScrollBarsShown (true, false);
        summaryView.setScrollBarThickness (8);
        addAndMakeVisible (summaryView);

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
            updateButtons();
            return;
        }
        summary.setItems (summarise (*r));
        updateProgress();
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
        // the background and the header (logo, pages) are drawn by the main view
        for (auto r : { scanPanel, reportPanel }) drawPanel (g, r);

        auto titleOf = [] (juce::Rectangle<float> panel) { return panel.reduced (22.0f, 0.0f).withTrimmedTop (18.0f).withHeight (30.0f); };
        drawSectionTitle (g, titleOf (scanPanel), 1, "Scan your setup");
        drawSectionTitle (g, titleOf (reportPanel), 2, "Something missing?");

        drawParagraph (g, scanText, scanIntro, sans (14.5f), p.muted);
        drawParagraph (g, reportText, reportIntro, sans (14.5f), p.muted);

        // what to try before sending it
        g.setColour (p.gold);
        g.setFont (sans (12.5f, true));
        g.drawText ("TRY THESE FIRST", tipsTitle, juce::Justification::centredLeft, true);
        for (size_t i = 0; i < tipAreas.size() && (int) i < tips.size(); ++i)
        {
            const auto a = tipAreas[i];
            g.setColour (p.gold);
            g.fillEllipse (a.getX(), a.getY() + 7.0f, 5.0f, 5.0f);
            drawParagraph (g, tips[(int) i], a.withTrimmedLeft (16.0f), tipFont(), p.text);
        }

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
        auto r = getLocalBounds().toFloat().reduced (24.0f, 0.0f).withTrimmedBottom (16.0f);

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

        // 2. the report, and what to try before sending it
        {
            reportPanel = right;
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
            in.removeFromTop (18.0f);
            tipsTitle = in.removeFromTop (22.0f);
            in.removeFromTop (6.0f);
            tipAreas.clear();
            for (auto& tip : tips)
            {
                const float h = paragraphHeight (tip, in.getWidth() - 16.0f, tipFont());
                tipAreas.push_back (in.removeFromTop (h));
                in.removeFromTop (10.0f);
            }
        }
    }
}
