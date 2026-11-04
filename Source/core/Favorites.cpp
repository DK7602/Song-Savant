#include "Favorites.h"
#include "Palette.h"
#include "Paths.h"
#include "SongSession.h"
#include <mutex>

namespace ss
{
    namespace favorites
    {
        namespace
        {
            struct Store
            {
                std::mutex lock;
                std::vector<FavSound> list;
                juce::Time fileTime;
                juce::uint32 lastLook = 0;
                bool loaded = false;
            };

            Store& store()
            {
                static Store s;                       // (plain strings: nothing JUCE has to shut down)
                return s;
            }

            void sortList (std::vector<FavSound>& l)
            {
                std::stable_sort (l.begin(), l.end(), [] (const FavSound& a, const FavSound& b)
                {
                    if (a.kind != b.kind) return (int) a.kind < (int) b.kind;
                    return a.name.compareNatural (b.name) < 0;
                });
            }

            // read the file again when it changed (the app and the plug-in can both be open); at most twice a second
            void loadIfNeeded (Store& s, bool force = false)
            {
                const auto now = juce::Time::getMillisecondCounter();
                if (s.loaded && ! force && now - s.lastLook < 500) return;
                s.lastLook = now;
                const auto f = file();
                const auto t = f.getLastModificationTime();
                if (s.loaded && ! force && t == s.fileTime) return;
                s.loaded = true;
                s.fileTime = t;
                s.list.clear();
                if (auto x = juce::XmlDocument::parse (f))
                    for (auto* e : x->getChildWithTagNameIterator ("Sound"))
                    {
                        FavSound fs;
                        fs.key = e->getStringAttribute ("key");
                        if (fs.key.isEmpty()) continue;
                        fs.name = e->getStringAttribute ("name");
                        fs.instrument = e->getStringAttribute ("instrument");
                        fs.library = e->getStringAttribute ("library");
                        fs.source = e->getStringAttribute ("source");
                        fs.kind = kindFromName (e->getStringAttribute ("kind")).value_or (Kind::other);
                        fs.added = juce::Time (e->getStringAttribute ("added").getLargeIntValue());
                        s.list.push_back (fs);
                    }
                sortList (s.list);
            }

            void save (Store& s)
            {
                juce::XmlElement x ("FavoriteSounds");
                for (auto& fs : s.list)
                {
                    auto* e = x.createNewChildElement ("Sound");
                    e->setAttribute ("key", fs.key);
                    e->setAttribute ("name", fs.name);
                    e->setAttribute ("instrument", fs.instrument);
                    e->setAttribute ("library", fs.library);
                    e->setAttribute ("source", fs.source);
                    e->setAttribute ("kind", kindName (fs.kind));
                    e->setAttribute ("added", juce::String (fs.added.toMilliseconds()));
                }
                const auto f = file();
                f.getParentDirectory().createDirectory();
                x.writeTo (f);
                s.fileTime = f.getLastModificationTime();
            }
        }

        juce::File file()   { return paths::documentsDir().getChildFile ("Favorite sounds.xml"); }

        std::vector<FavSound> sounds()
        {
            auto& s = store();
            std::lock_guard<std::mutex> l (s.lock);
            loadIfNeeded (s);
            return s.list;
        }

        bool isFavorite (const juce::String& key)
        {
            auto& s = store();
            std::lock_guard<std::mutex> l (s.lock);
            loadIfNeeded (s);
            for (auto& fs : s.list) if (fs.key == key) return true;
            return false;
        }

        bool isFavorite (const Sound& sound)   { return isFavorite (soundKey (sound)); }

        void add (const Sound& sound)
        {
            auto& s = store();
            std::lock_guard<std::mutex> l (s.lock);
            loadIfNeeded (s, true);
            const auto key = soundKey (sound);
            for (auto& fs : s.list) if (fs.key == key) return;
            FavSound fs;
            fs.key = key;
            fs.name = sound.name;
            fs.instrument = sound.instrument;
            fs.library = sound.library;
            fs.source = sound.source;
            fs.kind = sound.kind;
            fs.added = juce::Time::getCurrentTime();
            s.list.push_back (fs);
            sortList (s.list);
            save (s);
        }

        void remove (const juce::String& key)
        {
            auto& s = store();
            std::lock_guard<std::mutex> l (s.lock);
            loadIfNeeded (s, true);
            const auto before = s.list.size();
            s.list.erase (std::remove_if (s.list.begin(), s.list.end(), [&key] (const FavSound& fs) { return fs.key == key; }), s.list.end());
            if (s.list.size() != before) save (s);
        }

        bool toggle (const Sound& sound)
        {
            if (isFavorite (sound)) { remove (soundKey (sound)); return false; }
            add (sound);
            return true;
        }

        void reloadForTests()
        {
            auto& s = store();
            std::lock_guard<std::mutex> l (s.lock);
            loadIfNeeded (s, true);
        }
    }

    //==============================================================================
    namespace mysongs
    {
        namespace
        {
            const juce::String extension (".songsavant");
            constexpr int keptBackups = 200;

            juce::String fileNameFor (const juce::String& name)
            {
                auto n = juce::File::createLegalFileName (name.trim()).substring (0, 80).trim();
                return n.isNotEmpty() ? n : juce::String ("My song");
            }

            std::optional<SavedSong> readInfo (const juce::File& f)
            {
                auto x = juce::XmlDocument::parse (f);
                if (x == nullptr || ! x->hasTagName ("SongSavantSong") || x->getChildByName ("Song") == nullptr) return {};
                SavedSong s;
                s.file = f;
                s.name = x->getStringAttribute ("name", f.getFileNameWithoutExtension());
                s.style = x->getStringAttribute ("style");
                s.description = x->getStringAttribute ("description");
                s.key = x->getIntAttribute ("key", -1);
                s.bpm = x->getDoubleAttribute ("bpm", 0.0);
                s.sections = x->getIntAttribute ("sections", 0);
                s.bars = x->getIntAttribute ("bars", 0);
                s.blank = x->getBoolAttribute ("blank", false);
                s.saved = juce::Time (x->getStringAttribute ("saved").getLargeIntValue());
                if (s.saved.toMilliseconds() == 0) s.saved = f.getLastModificationTime();
                return s;
            }

            // a copy before the song is saved over, renamed or deleted
            void backUp (const juce::File& f, const juce::String& why)
            {
                auto x = juce::XmlDocument::parse (f);
                if (x == nullptr) return;
                x->setAttribute ("backupWhy", why);
                x->setAttribute ("backupTime", juce::String (juce::Time::getCurrentTime().toMilliseconds()));
                const auto dir = backupFolder();
                dir.createDirectory();
                const auto stamp = juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S");
                x->writeTo (dir.getNonexistentChildFile (fileNameFor (x->getStringAttribute ("name")) + " - " + stamp, extension, false));
                // only the newest are kept
                auto all = dir.findChildFiles (juce::File::findFiles, false, "*" + extension);
                std::sort (all.begin(), all.end(), [] (const juce::File& a, const juce::File& b)
                {
                    return a.getLastModificationTime() > b.getLastModificationTime();
                });
                for (int i = keptBackups; i < all.size(); ++i) all.getReference (i).deleteFile();
            }
        }

        juce::File folder()         { return paths::documentsDir().getChildFile ("My Songs"); }
        juce::File backupFolder()   { return folder().getChildFile ("Backups"); }

        std::vector<SavedSong> list()
        {
            std::vector<SavedSong> out;
            for (auto& f : folder().findChildFiles (juce::File::findFiles, false, "*" + extension))
                if (auto s = readInfo (f)) out.push_back (*s);
            std::sort (out.begin(), out.end(), [] (const SavedSong& a, const SavedSong& b) { return a.saved > b.saved; });
            return out;
        }

        std::optional<SavedSong> find (const juce::String& name)
        {
            for (auto& s : list()) if (s.name.equalsIgnoreCase (name.trim())) return s;
            return {};
        }

        juce::String freeName (const juce::String& wanted)
        {
            const auto base = wanted.trim().isNotEmpty() ? wanted.trim() : juce::String ("My song");
            const auto songs = list();
            auto taken = [&songs] (const juce::String& n)
            {
                for (auto& s : songs) if (s.name.equalsIgnoreCase (n)) return true;
                return false;
            };
            if (! taken (base)) return base;
            for (int i = 2;; ++i)
                if (! taken (base + " " + juce::String (i))) return base + " " + juce::String (i);
        }

        juce::String suggestedName (const SongSession& song)
        {
            if (song.form.fromSong.isNotEmpty()) return song.form.fromSong;
            // the description's first words, up to the first comma: "Dark moody trap"
            auto words = juce::StringArray::fromTokens (song.form.description.upToFirstOccurrenceOf (",", false, false), " ", "\"");
            words.removeEmptyStrings();
            while (words.size() > 5) words.remove (words.size() - 1);
            auto name = words.joinIntoString (" ").trim();
            if (name.isEmpty()) return song.arrangement.shape == SongShape::blank ? "My blank song" : "My song";
            return name.substring (0, 1).toUpperCase() + name.substring (1);
        }

        juce::File save (const SongSession& song, const juce::String& wantedName)
        {
            const auto name = wantedName.trim().isNotEmpty() ? wantedName.trim().substring (0, 80) : suggestedName (song);
            juce::XmlElement x ("SongSavantSong");
            x.setAttribute ("version", SS_VERSION);
            x.setAttribute ("name", name);
            x.setAttribute ("saved", juce::String (juce::Time::getCurrentTime().toMilliseconds()));
            const auto brief = song.makeBrief();
            const auto* style = brief.style();
            x.setAttribute ("style", style != nullptr && ! style->isGeneral() ? genreName (brief.genre) + " / " + style->name : genreName (brief.genre));
            x.setAttribute ("description", song.form.description);
            const auto plan = song.partsPlan();
            x.setAttribute ("key", plan.key);
            x.setAttribute ("bpm", plan.bpm);
            x.setAttribute ("sections", (int) song.arrangement.sections.size());
            x.setAttribute ("bars", song.arrangement.totalBars());
            x.setAttribute ("blank", song.arrangement.shape == SongShape::blank);
            x.addChildElement (song.toXml().release());

            folder().createDirectory();
            // the same name: saved over (a copy of the old one goes to Backups first)
            juce::File target;
            if (auto existing = find (name))
            {
                backUp (existing->file, "before saving over");
                target = existing->file;
            }
            else
                target = folder().getNonexistentChildFile (fileNameFor (name), extension, false);
            return x.writeTo (target) ? target : juce::File();
        }

        std::unique_ptr<juce::XmlElement> load (const juce::File& f)
        {
            auto x = juce::XmlDocument::parse (f);
            if (x == nullptr) return nullptr;
            if (auto* song = x->getChildByName ("Song")) return std::make_unique<juce::XmlElement> (*song);
            return nullptr;
        }

        juce::File rename (const juce::File& f, const juce::String& newName)
        {
            const auto name = newName.trim().substring (0, 80);
            auto x = juce::XmlDocument::parse (f);
            if (x == nullptr || name.isEmpty()) return {};
            if (auto other = find (name); other.has_value() && other->file != f) return {};   // taken by another song
            // (the song inside remembers the name it was saved as: the new one)
            if (auto* song = x->getChildByName ("Song"); song != nullptr && song->hasAttribute ("savedAs")) song->setAttribute ("savedAs", name);
            x->setAttribute ("name", name);
            auto target = f.getSiblingFile (fileNameFor (name) + extension);
            if (target != f && target.exists()) target = folder().getNonexistentChildFile (fileNameFor (name), extension, false);
            if (! x->writeTo (target)) return {};
            if (target != f) f.deleteFile();
            return target;
        }

        bool remove (const juce::File& f)
        {
            if (! f.existsAsFile()) return false;
            backUp (f, "deleted");
            return f.deleteFile();
        }

        std::vector<Backup> backups()
        {
            std::vector<Backup> out;
            for (auto& f : backupFolder().findChildFiles (juce::File::findFiles, false, "*" + extension))
                if (auto x = juce::XmlDocument::parse (f))
                {
                    Backup b;
                    b.file = f;
                    b.name = x->getStringAttribute ("name");
                    b.why = x->getStringAttribute ("backupWhy");
                    b.when = juce::Time (x->getStringAttribute ("backupTime").getLargeIntValue());
                    if (b.when.toMilliseconds() == 0) b.when = f.getLastModificationTime();
                    out.push_back (b);
                }
            std::sort (out.begin(), out.end(), [] (const Backup& a, const Backup& b) { return a.when > b.when; });
            return out;
        }

        juce::File restore (const Backup& b)
        {
            auto x = juce::XmlDocument::parse (b.file);
            if (x == nullptr || x->getChildByName ("Song") == nullptr) return {};
            const auto name = freeName (b.name + " (restored)");
            x->removeAttribute ("backupWhy");
            x->removeAttribute ("backupTime");
            x->setAttribute ("name", name);
            x->setAttribute ("saved", juce::String (juce::Time::getCurrentTime().toMilliseconds()));
            folder().createDirectory();
            const auto target = folder().getNonexistentChildFile (fileNameFor (name), extension, false);
            return x->writeTo (target) ? target : juce::File();
        }
    }
}
