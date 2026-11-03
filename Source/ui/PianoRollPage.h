// Song Savant - the piano roll: edit a part's pattern in a section, or make your own from an empty grid.
//
// Pick the part and the section across the top; the keyboard is down the left (the song's key brighter, each bar's
// chord lit gold; for the drums, a row per drum), the notes in the middle, how hard each plays (velocity) underneath. Click to add a note, drag it
// to move it, drag its end to stretch it, right-click to delete; drag a box around notes to select them; Ctrl+C / V
// / D / Z / Y. The other parts show faintly (ghost notes). Hear it looped, alone or with the band, and drag it into
// the DAW. Every change is kept with the song straight away: ARRANGE and WHOLE SONG play your version.
// The panel on the right builds parts: a chord stamp (the bar's chord, the song's chords, more in the key), strum,
// arpeggiate, quantize, humanize, legato, move in key, chop, flip, and a new idea for the part in the song's style.
#pragma once

#include "Widgets.h"
#include "core/Arrangement.h"
#include "core/PatternEdit.h"

class SongSavantProcessor;

namespace ss::ui
{
    class PianoRollPage : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
    {
    public:
        explicit PianoRollPage (SongSavantProcessor&);
        ~PianoRollPage() override;

        // Opens a part (the card's role) in a section of the arrangement. False when there's nothing to edit there.
        bool open (const juce::String& role, int section);
        std::function<void()> onDone;                    // DONE (or Esc with nothing selected)

        void paint (juce::Graphics&) override;
        void paintOverChildren (juce::Graphics&) override;
        void resized() override;
        bool keyPressed (const juce::KeyPress&) override;

        // A MIDI file dropped in (a pattern from Groove Grabber, the DAW, Song Savant): it replaces the bars it covers,
        // from the bar it lands on (from bar 1 when `atBeat` < 0). One step to undo. False when it isn't a MIDI pattern.
        bool dropFile (const juce::File&, double atBeat = -1.0);
        bool isInterestedInFileDrag (const juce::StringArray&) override;
        void fileDragEnter (const juce::StringArray&, int, int) override   { dropHover = true; repaint(); }
        void fileDragExit (const juce::StringArray&) override              { dropHover = false; repaint(); }
        void filesDropped (const juce::StringArray&, int x, int y) override;
        bool isDrums() const;
        const juce::String& getStatus() const            { return status; }
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
        void stampAt (double beat, int pitch);           // the STAMP tool: the chord picked, at a click
        void shape (int tool, int choice);               // a building tool on the selected notes (or all of them)
        void newIdea();                                  // A NEW IDEA FOR THIS PART
        void setStamp (bool on);
        void pickStampChord (int index);                 // 0: the bar's own chord; then the chips, then MORE's
        int numStampChords() const                       { return (int) stampChords.size(); }
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
        void buildChordChips();
        void showMoreChords();
        void showShapeMenu (int tool);
        void layoutPanel (juce::Rectangle<float>);
        std::vector<int> voiced (const Chord&) const;
        const ArrangementPart* part() const;
        void preview (int note);                         // hear a note straight away
        juce::String patternName() const;
        void findGhosts();                               // the other parts in this section (worked out once, not at every repaint)
        std::vector<PatternNote> ghostNotes;
        std::vector<juce::Colour> ghostColours;
        DrumNotes kit;                                   // the drum card's own notes (its drum map, or Groove Grabber's pads)
        juce::String status;                             // what just happened (a dropped file), shown for a moment
        bool dropHover = false;

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

        // the building tools, on the right
        struct StampChord { juce::String name; std::optional<Chord> chord; };   // no chord: the bar's own
        std::vector<StampChord> stampChords;
        int stampIndex = 0, moreIndex = -1;              // moreIndex: a chord picked from MORE (its place in stampChords)
        enum class Voicing { triad, seventh, spread } voicing = Voicing::triad;
        juce::OwnedArray<PillButton> chordChips;
        PillButton stampBtn { "STAMP CHORDS", PillButton::Style::subtle },
                   moreBtn { "MORE", PillButton::Style::subtle },
                   triadBtn { "TRIAD", PillButton::Style::subtle },
                   seventhBtn { "7TH", PillButton::Style::subtle },
                   spreadBtn { "SPREAD", PillButton::Style::subtle },
                   strumBtn { "STRUM", PillButton::Style::outline },
                   arpBtn { "ARPEGGIATE", PillButton::Style::outline },
                   quantizeBtn { "QUANTIZE", PillButton::Style::outline },
                   humanizeBtn { "HUMANIZE", PillButton::Style::outline },
                   legatoBtn { "LEGATO", PillButton::Style::outline },
                   chopBtn { "CHOP", PillButton::Style::outline },
                   flipBtn { "FLIP", PillButton::Style::outline },
                   keyUpBtn { "IN KEY +", PillButton::Style::outline },
                   keyDownBtn { "IN KEY -", PillButton::Style::outline },
                   ideaBtn { "A NEW IDEA FOR THIS PART", PillButton::Style::outline };
        juce::Rectangle<float> panelArea, stampLabel, shapeLabel, ideaLabel;
        int ideas = 0;
        DragChip dragChip { "THIS PART", DragChip::Kind::midi };
        std::unique_ptr<Grid> grid;
        juce::Rectangle<float> topLine, titleArea, hintArea;
        enum class Tool { draw, select, erase, stamp } tool = Tool::draw;
        bool ghosts = true, alone = false;
    };
}
