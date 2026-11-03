// Song Savant - editing a pattern in the piano roll: adding, moving, stretching and deleting notes, selecting them,
// copying, pasting and duplicating, how hard they play (velocity), with undo and redo. Nothing is drawn here (the
// piano roll draws it), so every action can be tested on its own.
#pragma once

#include "Patterns.h"

namespace ss
{
    struct EditNote
    {
        PatternNote n;
        bool selected = false;
    };

    // What Ctrl+C keeps: the notes, from the start of the bar where they began
    struct NoteClip
    {
        std::vector<PatternNote> notes;           // starts from `fromBar`'s first beat
        double fromBar = 0.0;                     // where they came from (in beats)
        bool empty() const                        { return notes.empty(); }
    };

    class PatternEditor
    {
    public:
        void load (const Pattern&, int visibleBars);           // a fresh start: undo forgets what came before
        Pattern pattern() const;                               // the notes in order
        const std::vector<EditNote>& notes() const             { return items; }
        int bars() const                                       { return pat.bars; }
        int visibleBars() const                                { return visible; }
        void setVisibleBars (int);                             // the section got longer: the pattern repeats into the new bars

        double snap = 0.25;                                    // in beats (a 16th); 0 = no snap
        double lastLength = 1.0;                               // a new note is as long as the last one you made or stretched
        int lastVelocity = 100;
        double snapNearest (double beats) const;
        double snapDown (double beats) const;

        int noteAt (double beat, int pitch) const;             // the note there (the last drawn on top), -1 if none
        bool nearEnd (int index, double beat, double tolerance) const;   // on the note's right end: stretching, not moving

        // ---- one step each (one undo)
        int addNote (double beat, int pitch);                  // at the snap before `beat`: it becomes the only selected note
        void erase (int index);
        void eraseSelected();
        void clear();                                          // an empty grid
        void replaceAll (std::vector<PatternNote>);            // start again from these (the suggestion, another section...)
        void transposeSelected (int semitones);                // up / down (the arrow keys)
        NoteClip copySelected() const;
        void paste (const NoteClip&, double atBar = -1.0);     // at the bar starting at `atBar` beats (-1: where it came from)
        void duplicateSelected();                              // a copy right after the selection

        // ---- the building tools: one step each, on the selected notes (or every note you can see when none is selected)
        // A chord at a click: its root at or just below the note clicked, the intervals stacked above (selected after)
        int stampChord (double beat, int pitch, int rootPitchClass, const std::vector<int>& intervals);
        void strum (double perNote, bool up);                  // notes that start together, one after another (beats apart)
        void arpeggiate (double step, int direction);          // chords broken up: 1 up, -1 down, 0 up and back down
        void quantize (double grid, double amount);            // starts towards the grid: 1 all the way, 0.5 halfway
        void humanize (double timing, int velocity, juce::uint32 seed);   // a little loose: up to `timing` beats, +-velocity
        void legato();                                         // every note (chord) lasts until the next one starts
        void moveInKey (int steps, int key);                   // up / down the song's scale (key 0..11 major, 12..23 minor)
        void chop (double piece);                              // long notes cut into pieces about this long
        void flipTime();                                       // back to front
        void flipPitch (int key);                              // upside down, kept in the key

        // ---- selecting (not an edit: nothing to undo)
        void select (int index, bool addToSelection);
        void selectNone();
        void selectAll();                                      // the notes you can see
        void selectBox (double beatA, double beatB, int pitchA, int pitchB, bool addToSelection);
        int numSelected() const;

        // ---- dragging: begin, update as the mouse moves (from where it started), end (one undo for the whole drag)
        void beginDrag();
        void dragMove (double beats, int semitones);           // every selected note
        void dragResize (double beats);                        // every selected note's end
        // that note's velocity; when it's selected, every selected note moves up or down by the same amount, from where it was
        void dragVelocity (int index, int velocity);
        bool endDrag();                                        // true when something changed

        bool canUndo() const                                   { return ! undos.empty(); }
        bool canRedo() const                                   { return ! redos.empty(); }
        void undo();
        void redo();
        int edits() const                                      { return editCount; }   // goes up with every change (and undo)

    private:
        void checkpoint();                                     // what to go back to with undo
        std::vector<size_t> targets() const;                   // what a tool works on
        void changed()                                         { ++editCount; }
        double selectionStart() const;
        Pattern pat;
        int visible = 8;
        std::vector<EditNote> items;
        std::vector<std::vector<EditNote>> undos, redos;
        std::vector<EditNote> dragFrom;
        bool dragging = false;
        int editCount = 0;
    };
}
