// Song Savant - the window's frame: background, logo, the page tabs, and the current page.
#pragma once

#include "LibraryPage.h"
#include "SetupCheckPage.h"

class SongSavantProcessor;

namespace ss::ui
{
    class MainView : public juce::Component
    {
    public:
        enum Page { library = 0, setupCheck = 1 };

        explicit MainView (SongSavantProcessor&);
        ~MainView() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        void showPage (int);
        LibraryPage& getLibrary()        { return libraryPage; }
        SetupCheckPage& getSetupCheck()  { return setupPage; }

    private:
        SongSavantProcessor& proc;
        SavantLookAndFeel lookAndFeel;
        juce::Image owl, wordmark;
        PillButton libraryTab { "LIBRARY", PillButton::Style::subtle },
                   setupTab { "SETUP CHECK", PillButton::Style::subtle };
        LibraryPage libraryPage;
        SetupCheckPage setupPage;
        juce::TooltipWindow tooltips { this, 500 };
        juce::Rectangle<float> headerArea, subtitleArea;
        int page = library;
    };
}
