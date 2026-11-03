#include "HiddenSounds.h"
#include "Palette.h"
#include "Paths.h"
#include <mutex>

namespace ss::hidden
{
    namespace
    {
        struct Store
        {
            std::mutex lock;
            std::set<juce::String> keys;
            juce::File file;
            bool loaded = false;
        };

        Store& store()
        {
            static Store s;                       // (plain strings: nothing JUCE has to shut down)
            return s;
        }

        void loadIfNeeded (Store& s)
        {
            if (s.loaded) return;
            s.loaded = true;
            if (s.file == juce::File()) s.file = paths::appDataDir().getChildFile ("Hidden sounds.txt");
            s.keys.clear();
            juce::StringArray lines;
            s.file.readLines (lines);
            for (auto& l : lines)
                if (l.trim().isNotEmpty()) s.keys.insert (l.trim());
        }

        void save (Store& s)
        {
            juce::StringArray lines;
            for (auto& k : s.keys) lines.add (k);
            s.file.getParentDirectory().createDirectory();
            s.file.replaceWithText (lines.joinIntoString ("\n") + (lines.isEmpty() ? "" : "\n"));
        }
    }

    bool isHidden (const Sound& sound)
    {
        auto& s = store();
        std::lock_guard<std::mutex> l (s.lock);
        loadIfNeeded (s);
        return ! s.keys.empty() && s.keys.count (soundKey (sound)) > 0;
    }

    void hide (const Sound& sound)
    {
        auto& s = store();
        std::lock_guard<std::mutex> l (s.lock);
        loadIfNeeded (s);
        if (s.keys.insert (soundKey (sound)).second) save (s);
    }

    void show (const Sound& sound)
    {
        auto& s = store();
        std::lock_guard<std::mutex> l (s.lock);
        loadIfNeeded (s);
        if (s.keys.erase (soundKey (sound)) > 0) save (s);
    }

    int count()
    {
        auto& s = store();
        std::lock_guard<std::mutex> l (s.lock);
        loadIfNeeded (s);
        return (int) s.keys.size();
    }

    juce::File file()
    {
        auto& s = store();
        std::lock_guard<std::mutex> l (s.lock);
        loadIfNeeded (s);
        return s.file;
    }

    void useFileForTests (const juce::File& f)
    {
        auto& s = store();
        std::lock_guard<std::mutex> l (s.lock);
        s.file = f;
        s.loaded = false;
    }
}
