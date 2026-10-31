// Song Savant - music knowledge shared with Groove Grabber: genres, styles, decades, and the artists and songs you
// can name ("like Morgan Wallen", "Elevation Worship", "80s pop"). The style and artist tables are copied from
// Groove Grabber by tools/sync_from_groove_grabber.py, so both apps understand the same words and use the same ids.
#pragma once

#include <juce_core/juce_core.h>
#include <optional>
#include <vector>

namespace ss
{
    // Same order and ids as Groove Grabber
    enum class Genre { trap, hiphop, rnb, pop, rock, country, worship, edm, lofi, cinematic, count };
    constexpr int numGenres = (int) Genre::count;
    juce::String genreName (Genre);                       // "Country", "Worship / Gospel"
    juce::String genreId (Genre);                         // "country", "boombap" (Groove Grabber's id for Hip-Hop)
    std::optional<Genre> genreFromId (const juce::String&);

    // Decades: 0 = 60s ... 5 = 2010s, 6 = today
    constexpr int numEras = 7;
    juce::String eraName (int era);                       // "80s", "Today"

    struct Style
    {
        Genre genre = Genre::pop;
        juce::String id, name;                            // "country.modern", "Modern Nashville"
        juce::StringArray keywords;                       // words that name it in a description
        std::vector<int> eras;
        double bpm = 100;
        bool isGeneral() const                            { return ! id.containsChar ('.'); }
    };
    const std::vector<Style>& allStyles();
    const Style* styleById (const juce::String&);
    std::vector<const Style*> stylesFor (Genre);         // "General" first

    struct ReferenceTake                                  // what an artist or song means inside one genre
    {
        juce::String styleId;
        int era = -1;
        juce::StringArray sound;                          // describing words: "warm", "punchy", "live"
    };
    struct Reference
    {
        juce::String name, by;                            // songs: by = the artist
        juce::StringArray aliases;                        // every way to name it, normalised
        juce::Array<bool> needsCue;                       // per alias: an ordinary word, needs "like ..." / "... type beat"
        std::vector<ReferenceTake> takes;                 // [0] = their main sound
        const ReferenceTake* takeFor (Genre) const;
        juce::String label() const                        { return by.isNotEmpty() ? name + " (" + by + ")" : name; }
    };
    const std::vector<Reference>& allReferences();

    // " lower case words " with letters, digits, & and # kept and everything else a space
    juce::String normaliseText (const juce::String&);

    // 0..11 = C..B major, 12..23 = C..B minor; -1 if none. "A minor", "Am", "F# maj", "in Bb", "key of E"
    int parseKey (const juce::String&);
    juce::String keyName (int key);                       // "A Minor", "no key"

    // What a description says about the song
    struct SongBrief
    {
        juce::String text;
        Genre genre = Genre::pop;
        bool genreSaid = false;                           // from the words (or picked), not a default
        juce::String styleId;                             // empty = the genre in general
        int era = -1;
        double bpm = 0;
        int key = -1;
        std::vector<int> references;                      // into allReferences()
        juce::StringArray sound;                          // describing words: warm, dark, airy, uplifting...
        juce::StringArray understood;                     // what it acted on, to show you
        const Style* style() const                        { return styleById (styleId); }
    };

    // pickedGenre / pickedStyle: chosen in the form; they win over the words
    SongBrief readBrief (const juce::String& text, std::optional<Genre> pickedGenre = {}, const juce::String& pickedStyle = {});

    juce::StringArray soundWords();                       // the describing words readBrief knows
}
