// Song Savant - the window's frame: background, logo, the page tabs, and the current page.
#pragma once

#include "SongPage.h"
#include "LibraryPage.h"
#include "SetupCheckPage.h"

class SongSavantProcessor;

namespace ss::ui
{
    class MainView : public juce::Component
    {
    public:
        enum Page { song = 0, library = 1, setupCheck = 2 };

        explicit MainView (SongSavantProcessor&);
        ~MainView() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        void showPage (int);
        SongPage& getSong()              { return songPage; }
        LibraryPage& getLibrary()        { return libraryPage; }
        SetupCheckPage& getSetupCheck()  { return setupPage; }

    private:
        SongSavantProcessor& proc;
        SavantLookAndFeel lookAndFeel;
        juce::Image owl, wordmark;
        PillButton songTab { "SONG", PillButton::Style::subtle },
                   libraryTab { "LIBRARY", PillButton::Style::subtle },
                   setupTab { "SETUP CHECK", PillButton::Style::subtle };
        SongPage songPage;
        LibraryPage libraryPage;
        SetupCheckPage setupPage;
        juce::TooltipWindow tooltips { this, 500 };
        juce::Rectangle<float> headerArea, subtitleArea;
        int page = song;
    };
}
