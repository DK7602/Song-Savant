// Song Savant - the piano roll: edit a part's pattern in a section, or make your own from an empty grid.
//
// Pick the part and the section across the top; the keyboard is down the left (the song's key brighter, each bar's
// chord lit gold), the notes in the middle, how hard each plays (velocity) underneath. Click to add a note, drag it
// to move it, drag its end to stretch it, right-click to delete; drag a box around notes to select them; Ctrl+C / V
// / D / Z / Y. The other parts show faintly (ghost notes). Hear it looped, alone or with the band, and drag it into
// the DAW. Every change is kept with the song straight away: ARRANGE and WHOLE SONG play your version.
#pragma once

#include "Widgets.h"
#include "core/Arrangement.h"
#include "core/PatternEdit.h"

class SongSavantProcessor;

namespace ss::ui
{
    class PianoRollPage : public juce::Component, private juce::Timer
    {
    public:
        explicit PianoRollPage (SongSavantProcessor&);
        ~PianoRollPage() override;

        // Opens a part (the card's role) in a section of the arrangement. False when there's nothing to edit there.
        bool open (const juce::String& role, int section);
        std::function<void()> onDone;                    // DONE (or Esc with nothing selected)

        void paint (juce::Graphics&) override;
        void resized() override;
        bool keyPressed (const juce::KeyPress&) override;
        void visibilityChanged() override;
        void themeChanged();

        // for the tests
        PatternEditor& getEditor()                       { return editor; }
        const juce::String& getRole() const              { return role; }
        int getSection() const                           { return section; }
        void edited();                                   // after a change: the song keeps it
        void startFrom (int choice);                     // 1 the suggestion, 2 an empty grid, 100+ another pattern, 200+ another part
        void makeDifferent (int section);
        void setLength (int bars);
        void togglePlay();
        juce::File writeMidi();                          // what the drag chip drags
        juce::Rectangle<int> gridBounds() const;

    private:
        class Grid;
        friend class Grid;
        void timerCallback() override;
        void refreshData();                              // the palette's parts, the plan, the arrangement
        void load();                                     // the editor gets the part in the section
        void buildTabs();
        void updateButtons();
        void updateSketch();
        void showSnapMenu();
        void showStartMenu();
        void showDifferentMenu();
        const ArrangementPart* part() const;
        void preview (int note);                         // hear a note straight away
        juce::String patternName() const;
        void findGhosts();                               // the other parts in this section (worked out once, not at every repaint)
        std::vector<PatternNote> ghostNotes;
        std::vector<juce::Colour> ghostColours;

        SongSavantProcessor& proc;
        std::vector<ArrangementPart> parts;
        PartsPlan plan;
        std::shared_ptr<const Catalog> catalog;
        juce::String role;
        int section = -1;
        PatternEditor editor;
        int seenEdits = -1;

        juce::OwnedArray<PillButton> partTabs, sectionTabs;
        PillButton lengthBtn { "8 BARS", PillButton::Style::outline },
                   differentBtn { "MAKE ONE DIFFERENT", PillButton::Style::outline },
                   startBtn { "START FROM", PillButton::Style::outline },
                   drawBtn { "DRAW", PillButton::Style::subtle },
                   selectBtn { "SELECT", PillButton::Style::subtle },
                   eraseBtn { "ERASE", PillButton::Style::subtle },
                   snapBtn { "SNAP: 1/16", PillButton::Style::outline },
                   zoomOutBtn { "-", PillButton::Style::outline },
                   zoomInBtn { "+", PillButton::Style::outline },
                   ghostsBtn { "GHOSTS", PillButton::Style::subtle },
                   aloneBtn { "ALONE", PillButton::Style::subtle },
                   bandBtn { "WITH THE BAND", PillButton::Style::subtle },
                   undoBtn { "UNDO", PillButton::Style::outline },
                   redoBtn { "REDO", PillButton::Style::outline },
                   doneBtn { "DONE", PillButton::Style::gold };
        TransportButton playBtn { TransportButton::Icon::play };
        DragChip dragChip { "THIS PART", DragChip::Kind::midi };
        std::unique_ptr<Grid> grid;
        juce::Rectangle<float> topLine, titleArea, hintArea;
        enum class Tool { draw, select, erase } tool = Tool::draw;
        bool ghosts = true, alone = false;
    };
}
