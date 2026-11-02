#include "Siblings.h"
#include "FileWalk.h"
#include <algorithm>

namespace ss
{
    std::optional<KeyCompassSong> readKeyCompassSong (const juce::File& f)
    {
        auto xml = juce::XmlDocument::parse (f);
        if (xml == nullptr || ! xml->hasTagName ("KeyCompassSong") || xml->getBoolAttribute ("deleted", false))
            return std::nullopt;
        auto* map = xml->getChildByName ("SongMap");
        if (map == nullptr) return std::nullopt;

        KeyCompassSong s;
        s.file = f;
        s.name = xml->getStringAttribute ("name", map->getStringAttribute ("name", f.getFileNameWithoutExtension()));
        s.bpm = map->getDoubleAttribute ("bpm", 0.0);
        s.key = map->getIntAttribute ("keyOverride", -1);
        if (s.key < 0) s.key = map->getIntAttribute ("lastSetKey", -1);
        for (auto* sec : map->getChildWithTagNameIterator ("Section"))
        {
            s.sections.add (sec->getStringAttribute ("name"));
            s.chords.addTokens (sec->getStringAttribute ("chords"), " ", "");
            auto own = juce::StringArray::fromTokens (sec->getStringAttribute ("chords"), " ", "");
            own.removeEmptyStrings();
            s.sectionChords.push_back (own);
        }
        s.chords.removeEmptyStrings();
        return s;
    }

    SiblingApps checkSiblingApps (const SystemLocations& loc)
    {
        SiblingApps a;

        a.keyCompassFolder = loc.keyCompassSongs.isDirectory();
        if (a.keyCompassFolder)
        {
            auto files = loc.keyCompassSongs.findChildFiles (juce::File::findFiles, false, "*.kcsong");
            std::sort (files.begin(), files.end(), [] (const juce::File& x, const juce::File& y)
                       { return x.getLastModificationTime() > y.getLastModificationTime(); });
            for (auto& f : files)
            {
                if (a.songs.size() >= 20) break;
                if (auto s = readKeyCompassSong (f)) a.songs.push_back (*s);
            }
        }

        a.grooveGrabberFolder = loc.grooveGrabberKits.isDirectory();
        if (a.grooveGrabberFolder)
            walkFiles (loc.grooveGrabberKits, { 3, 5000, 20.0 }, [&] (const juce::File& f, int)
            {
                if (! f.hasFileExtension (".ggkit") || f.getSize() > 2 * 1024 * 1024) return true;
                const auto v = juce::JSON::parse (f);
                if (! v.isObject()) return true;
                ++a.grooveGrabberKits;
                const auto genre = v["genre"].toString();
                a.kitsByGenre[genre.isNotEmpty() ? genre : juce::String ("(no genre)")]++;
                if (a.kitSamples.size() < 6)
                {
                    juce::String line = v["name"].toString();
                    juce::StringArray bits;
                    if (genre.isNotEmpty()) bits.add (genre);
                    if ((int) v.getProperty ("key", -1) >= 0) bits.add (keyName ((int) v["key"]));
                    if ((double) v.getProperty ("bpm", 0.0) > 0) bits.add (juce::String (juce::roundToInt ((double) v["bpm"])) + " BPM");
                    a.kitSamples.add (line + (bits.isEmpty() ? juce::String() : " (" + bits.joinIntoString (", ") + ")"));
                }
                return true;
            });

        a.sampleSnaggerFolder = loc.sampleSnaggerLibrary.isDirectory();
        if (a.sampleSnaggerFolder)
            a.sampleSnaggerFiles = walkFiles (loc.sampleSnaggerLibrary, { 6, 50000, 20.0 }, [] (const juce::File&, int) { return true; }).files;

        return a;
    }
}
