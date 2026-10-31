// Song Savant - Phase 0 screen: scan the setup, try the drag tests in Cubase, send the report.
#pragma once

#include "Widgets.h"

class SongSavantProcessor;

namespace ss::ui
{
    class SetupCheckPage : public juce::Component, private juce::Timer
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
        juce::Image owl, wordmark;

        PillButton scanBtn { "SCAN MY SETUP", PillButton::Style::gold },
                   stopBtn { "STOP", PillButton::Style::outline },
                   folderBtn { "ALSO LOOK IN A FOLDER...", PillButton::Style::outline },
                   openBtn { "OPEN REPORT", PillButton::Style::gold },
                   showBtn { "SHOW FILE", PillButton::Style::outline },
                   copyBtn { "COPY TEXT", PillButton::Style::outline };

        ProgressStrip progress;
        SummaryList summary;
        juce::Viewport summaryView;

        DragChip userChip { "Your preset file", DragChip::Kind::preset },
                 factoryChip { "Factory preset", DragChip::Kind::preset },
                 midiChip { "MIDI clip", DragChip::Kind::midi };
        ResultPicker userResult, factoryResult, midiResult;

        juce::File midiFile;
        int seenGeneration = -1;
        juce::String reportStatus, extraFoldersText;
        std::unique_ptr<juce::FileChooser> chooser;

        // layout, kept for paint()
        juce::Rectangle<float> headerArea, scanPanel, dragPanel, reportPanel;
        juce::Rectangle<float> scanIntro, dragIntro, reportIntro, extraLine, reportLine;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SetupCheckPage)
    };
}
