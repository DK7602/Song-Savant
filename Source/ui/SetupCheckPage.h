// Song Savant - Phase 0 screen: scan the setup, try the drag tests in Cubase, send the report.
#pragma once

#include "Widgets.h"

class SongSavantProcessor;

namespace ss::ui
{
    class SetupCheckPage : public juce::Component,
                           public juce::FileDragAndDropTarget,
                           public juce::TextDragAndDropTarget,
                           private juce::Timer
    {
    public:
        explicit SetupCheckPage (SongSavantProcessor&);
        ~SetupCheckPage() override;

        void paint (juce::Graphics&) override;
        void paintOverChildren (juce::Graphics&) override;
        void resized() override;

        void refreshFromResult();   // also called by the tests

        // things dragged from Cubase (or anywhere) onto Song Savant: noted in the report
        bool isInterestedInFileDrag (const juce::StringArray&) override  { return true; }
        void fileDragEnter (const juce::StringArray&, int, int) override  { setDropHover (true); }
        void fileDragExit (const juce::StringArray&) override             { setDropHover (false); }
        void filesDropped (const juce::StringArray& files, int, int) override;
        bool isInterestedInTextDrag (const juce::String&) override        { return true; }
        void textDragEnter (const juce::String&, int, int) override       { setDropHover (true); }
        void textDragExit (const juce::String&) override                  { setDropHover (false); }
        void textDropped (const juce::String& text, int, int) override;

    private:
        void timerCallback() override;
        void updateProgress();
        void chooseExtraFolder();
        void saveReportQuietly();
        void updateButtons();
        void setDropHover (bool);
        void noteDrop (const juce::String& line);

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

        DragChip userChip { "Your preset file", DragChip::Kind::preset },
                 factoryChip { "Factory preset", DragChip::Kind::preset },
                 midiChip { "MIDI clip", DragChip::Kind::midi };
        ResultPicker userResult, factoryResult, midiResult;

        juce::File midiFile;
        int seenGeneration = -1;
        juce::String reportStatus, extraFoldersText, dropLine;
        bool dropHover = false;
        std::unique_ptr<juce::FileChooser> chooser;

        // layout, kept for paint()
        juce::Rectangle<float> scanPanel, dragPanel, reportPanel;
        juce::Rectangle<float> scanIntro, dragIntro, reportIntro, extraLine, reportLine, dropArea;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SetupCheckPage)
    };
}
