// Song Savant - COACH MY SONG (on the MASTERCLASS page): reads your song (its sections and what plays in each, its
// chords, your own notes, its tempo and key) against what its genre and style usually do (StyleGuide), and says what's
// working and what to try. Tips only, no score. Some tips can FIX IT, and the coach's UNDO takes a fix back.
// It can't hear anything: it reads the notes, chords and sections, not a recording or a mix, and it knows the usual
// ways, not what's good. Only songs you made your own are coached (isYours).
#pragma once

#include "SongSession.h"

namespace ss::coach
{
    enum class Area { shape, energy, tempoKey, chords, melody, bass, drums };
    juce::String areaName (Area);                     // "Song shape", "Energy", "Tempo and key"...

    enum class FixKind
    {
        none,
        setBpm,            // value: the tempo
        setBars,           // section, value: its length
        addSection,        // name; value: where (0 .. the number of sections); every chorus with "beforeChoruses"
        liftChoruses,      // every chorus: all parts on, the drums full
        lightenStart,      // the intro without drums and bass, the first verse's drums at "groove"
        liftLastChorus,    // section: all parts on, the drums full
        chordBack,         // section, bar: Song Savant's chord there again
        chorusChords,      // section: the chorus's own chords back to Song Savant's
        foldMelody,        // section, role: notes far from the middle come an octave closer
        notesInKey,        // section, role: notes outside the key move to the nearest note in it
        bassRoots,         // section, role: each bar's first note on the chord's root
        styleSnare,        // section, role: the snare and clap where the style puts them
        swingHats          // section, role, value: the hats' off-beat 16ths swung to value %
    };

    struct Fix
    {
        FixKind kind = FixKind::none;
        int section = -1;
        int bar = -1;
        int value = 0;
        juce::String name, role;
        bool beforeChoruses = false;
    };

    struct Tip
    {
        juce::String id;             // the same while the song is: "I MEANT THAT" remembers it (SongSession::coachDismissed)
        Area area = Area::shape;
        int priority = 0;            // the most useful first
        juce::String title, why, tryThis;
        int section = -1;            // SHOW ME: the section on ARRANGE...
        juce::String role;           // ... or this part there, in the piano roll
        juce::String fixLabel;       // FIX IT's words ("SET 140 BPM"); empty: nothing to fix with one click
        Fix fix;
    };

    struct Working
    {
        Area area = Area::shape;
        juce::String text;           // "The chorus arrives at 0:32, early, the way hip-hop likes it"
    };

    struct Report
    {
        bool yours = false;                  // something of yours to coach
        bool empty = false;                  // yours, but nothing plays yet (a blank song just started)
        juce::String headline;               // "Hip-Hop: West Coast / G-Funk  |  94 BPM in A minor  |  9 sections, 3:12"
        std::vector<Working> working;
        std::vector<Tip> tips;               // the most useful first; the ones you meant are left out
        int dismissed = 0;                   // how many you said you meant
    };

    // Songs you made your own: a blank song, your own notes or chords, or a map that isn't the one Song Savant made
    bool isYours (const SongSession&, const Catalog*);
    Report coachSong (const SongSession&, const Catalog*);

    // FIX IT: true when the song changed
    bool applyFix (SongSession&, const Catalog*, const Fix&);

    // What a fix can change, kept so the coach's UNDO puts it back
    struct Snapshot
    {
        SongForm form;
        double briefBpm = 0;
        Arrangement arrangement;
        PatternBook patterns;
    };
    Snapshot snapshot (const SongSession&);
    void restore (SongSession&, const Snapshot&);
    bool sameSong (const Snapshot&, const Snapshot&);   // nothing a fix touches is different

    // (shared with the tests) the style's usual tempos, from "130-160 BPM, felt in half time (like 65-80)"
    std::vector<std::pair<double, double>> tempoRanges (const juce::String&);
    juce::String timeText (double seconds);           // "2:05"
}
