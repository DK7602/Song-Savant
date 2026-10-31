// Song Savant - the owner's other apps: Key Compass songs (key, tempo, sections, chords), Groove Grabber kits,
// and Sample Snagger's library. Song Savant reads their files the same way Groove Grabber reads Key Compass.
#pragma once

#include "SystemLocations.h"
#include <map>
#include <optional>
#include <vector>

namespace ss
{
    // 0..11 = C..B major, 12..23 = C..B minor (the same numbering as Key Compass and Groove Grabber)
    juce::String keyName (int key);

    struct KeyCompassSong
    {
        juce::String name;
        int key = -1;
        double bpm = 0;
        juce::StringArray sections;
        juce::StringArray chords;        // every chord, in order, across the sections
        juce::File file;
    };

    std::optional<KeyCompassSong> readKeyCompassSong (const juce::File&);

    struct SiblingApps
    {
        bool keyCompassFolder = false;
        std::vector<KeyCompassSong> songs;            // newest first, up to 20

        bool grooveGrabberFolder = false;
        int grooveGrabberKits = 0;
        std::map<juce::String, int> kitsByGenre;
        juce::StringArray kitSamples;                 // "90s Westside Classic (hiphop, A Minor, 92 BPM)"

        bool sampleSnaggerFolder = false;
        int sampleSnaggerFiles = 0;
    };

    SiblingApps checkSiblingApps (const SystemLocations&);
}
