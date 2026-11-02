// Song Savant - the window's frame: background, logo, the page tabs, and the current page.
#pragma once

#include "SongPage.h"
#include "LibraryPage.h"
#include "SetupCheckPage.h"
#include "DrumMapsPage.h"
#include "MasterclassPage.h"
#include "ArrangePage.h"
#include "HelpPanel.h"

class SongSavantProcessor;

namespace ss::ui
{
    class MainView : public juce::Component, public juce::SettableTooltipClient, private juce::KeyListener
    {
    public:
        enum Page { song = 0, library = 1, setupCheck = 2, drumMaps = 3, masterclass = 4, arrange = 5 };

        explicit MainView (SongSavantProcessor&);
        ~MainView() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        void showPage (int);
        SongPage& getSong()              { return songPage; }
        LibraryPage& getLibrary()        { return libraryPage; }
        SetupCheckPage& getSetupCheck()  { return setupPage; }
        DrumMapsPage& getDrumMaps()      { return drumMapsPage; }
        MasterclassPage& getMasterclass() { return masterclassPage; }
        ArrangePage& getArrange()        { return arrangePage; }
        HelpPanel& getHelp()             { return help; }
        void showHelp();                 // the manual (the ? button, F1)
        void toggleTheme();              // light / dark (the sun / moon button, or a click on the logo)
        juce::Rectangle<float> getLogoArea() const   { return logoArea; }
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void parentHierarchyChanged() override;

    private:
        SongSavantProcessor& proc;
        SavantLookAndFeel lookAndFeel;
        juce::Image owl, wordmark;
        ThemeButton themeBtn;
        SetupButton setupBtn;
        HelpButton helpBtn;
        using juce::Component::keyPressed;
        bool keyPressed (const juce::KeyPress&, juce::Component*) override;
        juce::Component::SafePointer<juce::Component> listeningTo;
        void applyTheme();
        PillButton songTab { "SONG", PillButton::Style::subtle },
                   arrangeTab { "ARRANGE", PillButton::Style::subtle },
                   libraryTab { "LIBRARY", PillButton::Style::subtle },
                   masterclassTab { "MASTERCLASS", PillButton::Style::subtle },
                   drumMapsTab { "DRUM MAPS", PillButton::Style::subtle };
        SongPage songPage;
        LibraryPage libraryPage;
        SetupCheckPage setupPage;
        DrumMapsPage drumMapsPage;
        MasterclassPage masterclassPage;
        ArrangePage arrangePage;
        HelpPanel help;
        juce::TooltipWindow tooltips { this, 500 };
        juce::Rectangle<float> headerArea, subtitleArea, logoArea;   // logoArea: where the owl and the name are drawn
        int page = song;
    };
}
