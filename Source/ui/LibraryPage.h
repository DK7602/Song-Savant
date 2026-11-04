// Song Savant - Library: every sound you own in one place. Search, filter by kind of sound and instrument, see
// where a sound lives and how to load it, and copy its name to paste into Cubase's MediaBay.
#pragma once

#include "Widgets.h"
#include "core/Catalog.h"

class SongSavantProcessor;

namespace ss::ui
{
    // The "kinds of sound" list on the left: click to pick one or more, "All kinds" clears
    class KindList : public juce::Component
    {
    public:
        void setCounts (std::vector<std::pair<Kind, int>>);
        std::set<Kind> selected;
        std::function<void()> onChange;
        int preferredHeight() const   { return (int) (counts.size() + 1) * rowH; }
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent& e) override   { hover = e.y / rowH; repaint(); }
        void mouseExit (const juce::MouseEvent&) override     { hover = -1; repaint(); }

    private:
        static constexpr int rowH = 27;
        std::vector<std::pair<Kind, int>> counts;
        int hover = -1;
    };

    class LibraryPage : public juce::Component, private juce::Timer, private juce::ListBoxModel
    {
    public:
        explicit LibraryPage (SongSavantProcessor&);
        std::function<void (const juce::String& kitName, const juce::String& instrument)> onMakeDrumMap;
        ~LibraryPage() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        void refreshCatalog();            // pick up a newly built catalog
        void themeChanged();
        void selectRow (int row);         // tests
        void setSearchText (const juce::String&);   // tests

    private:
        void timerCallback() override;
        int getNumRows() override                       { return (int) rows.size(); }
        void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
        void selectedRowsChanged (int) override         { copiedNote.clear(); updateButtons(); repaint(); }
        void updateButtons();
        void toggleHidden();
    public:
        void toggleFavorite (int row);                  // a sound's heart (also the tests)
    private:
        void listBoxItemDoubleClicked (int, const juce::MouseEvent&) override   { copySelected(); }
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;   // (its heart: a favorite)
        static juce::Rectangle<float> heartIn (int w, int h);

        void applyFilter();
        void updateCounts();
        void copySelected();
        const Sound* selectedSound() const;
        void paintDetails (juce::Graphics&, juce::Rectangle<float>);

        SongSavantProcessor& proc;
        std::shared_ptr<const Catalog> catalog;
        int seenGeneration = -1;
        std::vector<int> rows;
        SoundFilter filter;

        KindList kinds;
        juce::Viewport kindsView;
        juce::ComboBox instrumentBox;
        PillButton ownedToggle { "ONLY SOUNDS I OWN", PillButton::Style::subtle },
                   yoursToggle { "ONLY MY OWN PRESETS", PillButton::Style::subtle },
                   refreshBtn { "REFRESH LIBRARY", PillButton::Style::outline },
                   copyBtn { "COPY NAME", PillButton::Style::gold },
                   hideBtn { "NOT IN MY LIBRARY", PillButton::Style::subtle },   // or SUGGEST IT AGAIN, for one you hid
                   favBtn { "FAVORITE", PillButton::Style::subtle },              // the heart: in FAVORITES
                   drumMapBtn { "MAKE A DRUM MAP", PillButton::Style::outline };  // a drum kit Cubase can't name
        juce::TextEditor searchBox;
        juce::ListBox list { "sounds", this };
        juce::String copiedNote;

        juce::Rectangle<float> leftPanel, centerPanel, rightPanel, kindsTitle, instrumentTitle, sourcesArea, countLine, detailsArea;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LibraryPage)
    };
}
