#include "GrooveGrabberLink.h"
#include "FileWalk.h"
#include "Paths.h"

#ifdef SS_VERSION
 #define SS_LINK_VERSION SS_VERSION
#else
 #define SS_LINK_VERSION ""
#endif

namespace ss
{
    juce::File grooveGrabberSongsFolder()   { return paths::documentsDir().getChildFile ("Songs"); }

    bool grooveGrabberInstalled()
    {
        // Documents/Groove Grabber, next to Documents/Song Savant (it's made the first time Groove Grabber runs)
        return paths::documentsDir().getSiblingFile ("Groove Grabber").isDirectory();
    }

    juce::String songNameFor (const SongSession& song)
    {
        if (song.form.fromSong.isNotEmpty()) return song.form.fromSong;
        // the first few words of the description, as a title: "modern country like morgan wallen, warm" -> "Modern Country Like Morgan Wallen"
        auto words = juce::StringArray::fromTokens (song.form.description.upToFirstOccurrenceOf (",", false, false)
                                                      .upToFirstOccurrenceOf (".", false, false), " ", "\"");
        words.removeEmptyStrings();
        while (words.size() > 6) words.remove (words.size() - 1);
        juce::String name;
        for (auto& w : words) name << (name.isEmpty() ? "" : " ") << w.substring (0, 1).toUpperCase() << w.substring (1);
        name = juce::File::createLegalFileName (name).trim();
        if (name.isEmpty())
        {
            const auto b = song.makeBrief();
            name = (b.style() != nullptr && ! b.style()->isGeneral() ? b.style()->name : genreName (b.genre)) + " Song";
            name = juce::File::createLegalFileName (name).trim();
        }
        return name.substring (0, 60).trim();
    }

    juce::var songForGrooveGrabber (const SongSession& song, const juce::String& drums)
    {
        const auto b = song.makeBrief();
        const auto plan = song.partsPlan();
        juce::DynamicObject::Ptr o = new juce::DynamicObject();
        o->setProperty ("app", "Song Savant");
        o->setProperty ("format", 1);
        o->setProperty ("version", SS_LINK_VERSION);
        o->setProperty ("name", songNameFor (song));
        o->setProperty ("description", song.form.description);
        o->setProperty ("genre", genreId (b.genre));
        o->setProperty ("style", b.style() != nullptr && ! b.style()->isGeneral() ? b.styleId : juce::String());
        o->setProperty ("era", b.era);
        // the key and tempo the starter parts are in (when no key was given, the one Song Savant picked for them)
        o->setProperty ("key", plan.key);
        o->setProperty ("keyName", keyName (plan.key));
        o->setProperty ("bpm", juce::roundToInt (plan.bpm));
        juce::Array<juce::var> sections;
        for (auto& s : song.arrangement.sections)
        {
            juce::DynamicObject::Ptr sec = new juce::DynamicObject();
            sec->setProperty ("name", s.name);
            sec->setProperty ("bars", s.bars);
            sections.add (juce::var (sec.get()));
        }
        o->setProperty ("sections", sections);
        o->setProperty ("drums", drums);
        o->setProperty ("sent", juce::Time::getCurrentTime().toISO8601 (true));
        return juce::var (o.get());
    }

    juce::File sendToGrooveGrabber (const SongSession& song, const juce::String& drums)
    {
        const auto folder = grooveGrabberSongsFolder();
        if (! folder.createDirectory()) return {};
        const auto v = songForGrooveGrabber (song, drums);
        const auto file = folder.getChildFile (v["name"].toString() + ".sssong");
        // written whole, then moved into place, so Groove Grabber never reads half a file
        juce::TemporaryFile tmp (file);
        if (! tmp.getFile().replaceWithText (juce::JSON::toString (v)) || ! tmp.overwriteTargetFileWithTemporary())
            return {};
        return file;
    }

    //==============================================================================
    namespace
    {
        // Groove Grabber's character sliders (0..1), as the words Song Savant's parts ask for
        juce::StringArray characterWords (const juce::var& target)
        {
            juce::StringArray out;
            auto* a = target.getArray();
            if (a == nullptr || a->size() < 7) return out;
            auto t = [a] (int i) { return (float) (double) a->getReference (i); };
            // punch, warmth, brightness, room, vintage, aggression, live feel
            if (t (0) > 0.62f) out.add ("Punchy");
            if (t (1) > 0.62f) out.add ("Warm");
            if (t (2) > 0.62f) out.add ("Bright");
            else if (t (2) < 0.36f) out.add ("Dark");
            if (t (3) > 0.62f) out.add ("Roomy");
            else if (t (3) < 0.33f) out.add ("Dry");
            if (t (4) > 0.62f) out.add ("Vintage");
            else if (t (4) < 0.33f) out.add ("Modern");
            if (t (5) > 0.62f) out.add ("Hard");
            else if (t (5) < 0.33f) out.add ("Soft");
            if (t (6) > 0.62f) { out.add ("Live"); out.add ("Acoustic"); }
            else if (t (6) < 0.36f) out.add ("Electronic");
            return out;
        }
    }

    std::vector<Sound> readGrooveGrabberKits (const juce::File& kitsFolder)
    {
        std::vector<Sound> out;
        if (! kitsFolder.isDirectory()) return out;
        std::set<juce::String> seen;
        walkFiles (kitsFolder, { 3, 5000, 20.0 }, [&] (const juce::File& f, int)
        {
            if (! f.hasFileExtension (".ggkit") || f.getSize() > 2 * 1024 * 1024) return true;
            const auto v = juce::JSON::parse (f);
            if (! v.isObject() || v["name"].toString().trim().isEmpty()) return true;
            auto* slots = v["slots"].getDynamicObject();
            if (slots == nullptr || slots->getProperties().isEmpty()) return true;

            Sound s;
            s.name = v["name"].toString().trim();
            s.instrument = "Groove Grabber";
            s.source = "Groove Grabber";
            s.kind = Kind::drums;
            s.detail = "Drum Kit";
            s.yours = true;
            s.location = f.getFullPathName();
            if (const auto g = genreFromId (v["genre"].toString()))
            {
                s.styles.add (genreName (*g));
                s.library = genreName (*g) + " kits";
            }
            else
                s.library = "My kits";
            if (auto* st = styleById (v["style"].toString()); st != nullptr && ! st->isGeneral())
                s.styles.add (st->name);
            if (const int era = (int) v["era"]; v.hasProperty ("era") && era >= 0 && era < numEras)
                s.styles.add (eraName (era));
            s.character = characterWords (v["target"]);
            const auto eight = slots->getProperty ("808");
            if (eight.isObject() && eight["path"].toString().isNotEmpty()) s.character.add ("808");
            s.description = v["blurb"].toString();
            if (const int key = (int) v["key"]; v.hasProperty ("key") && key >= 0 && key < 24)
                s.description << (s.description.isEmpty() ? "" : " ") << "Tuned to " << keyName (key) << ".";
            s.rating = (bool) v["favorite"] ? 4 : 0;
            // the same kit saved twice (in two genre folders): once is enough
            if (! seen.insert (s.name + "|" + s.library).second) return true;
            prepareForSearch (s);
            out.push_back (std::move (s));
            return true;
        });
        return out;
    }

    bool isGrooveGrabberKit (const Sound& s)   { return s.source == "Groove Grabber"; }

    DrumNotes grooveGrabberDrums()
    {
        DrumNotes d;
        d.source = "Groove Grabber's pads";
        d.notes = { { "kick", 36 }, { "rim", 37 }, { "snare", 38 }, { "clap", 39 }, { "hat", 42 }, { "pedalhat", 44 },
                    { "openhat", 46 }, { "tomlow", 43 }, { "tommid", 47 }, { "tomhigh", 50 }, { "crash", 49 }, { "ride", 51 },
                    { "808", 52 }, { "perc", 53 }, { "shaker", 54 } };
        return d;
    }
}
