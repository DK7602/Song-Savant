// Song Savant - the setup check: scan what's on this computer, and the report to send when something you own
// doesn't show up. (Until 1.0.2 it also had Phase 0's drag test with Cubase: that question is answered.)
#pragma once

#include "Widgets.h"

class SongSavantProcessor;

namespace ss::ui
{
    class SetupCheckPage : public juce::Component,
                           private juce::Timer
    {
    public:
        explicit SetupCheckPage (SongSavantProcessor&);
        ~SetupCheckPage() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        void refreshFromResult();   // also called by the tests

    private:
        void timerCallback() override;
        void updateProgress();
        void chooseExtraFolder();
        void saveReportQuietly();
        void updateButtons();

        SongSavantProcessor& proc;

        PillButton scanBtn { "SCAN MY SETUP", PillButton::Style::gold },
                   stopBtn { "STOP", PillButton::Style::outline },
                   folderBtn { "ALSO LOOK IN A FOLDER...", PillButton::Style::outline },
                   openBtn { "OPEN REPORT", PillButton::Style::gold },
                   showBtn { "SHOW FILE", PillButton::Style::outline },
                   copyBtn { "COPY TEXT", PillButton::Style::outline };

        ProgressStrip progress;
        SummaryList summary;
        juce::Viewport summaryView;

        int seenGeneration = -1;
        juce::String reportStatus, extraFoldersText;
        std::unique_ptr<juce::FileChooser> chooser;

        // layout, kept for paint()
        juce::Rectangle<float> scanPanel, reportPanel;
        juce::Rectangle<float> scanIntro, reportIntro, extraLine, reportLine, tipsTitle;
        std::vector<juce::Rectangle<float>> tipAreas;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SetupCheckPage)
    };
}
