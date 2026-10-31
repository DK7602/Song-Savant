#include "PluginProfiles.h"
#include "VendorProbe.h"
#include <set>

namespace ss
{
    namespace
    {
        using K = Kind;
        const std::vector<PluginProfile>& profiles()
        {
            static const std::vector<PluginProfile> p {
                // drums
                { "ssdsampler", "SSD Sampler 5", { K::drums }, "acoustic,live,rock,pop,country,punchy,drumset,room", "Steven Slate's sampled acoustic drum kits" },
                { "stevenslatedrums", "Steven Slate Drums", { K::drums }, "acoustic,live,rock,pop,country,punchy,drumset,room", "Steven Slate's sampled acoustic drum kits" },
                { "mtpowerdrumkit", "MT-PowerDrumKit", { K::drums }, "acoustic,live,rock,pop,drumset", "A free acoustic rock and pop drum kit" },
                { "groovegrabber", "Groove Grabber", { K::drums }, "your own samples,kit,punchy,modern,hip hop,trap,pop,country,worship", "Builds drum kits for your song from your own samples" },
                { "addictivedrums", "Addictive Drums 2", { K::drums }, "acoustic,live,rock,pop,drumset", "Sampled acoustic drum kits" },
                { "superiordrummer", "Superior Drummer 3", { K::drums }, "acoustic,live,rock,pop,country,drumset,room", "Deep sampled acoustic drum kits" },
                { "ezdrummer", "EZdrummer", { K::drums }, "acoustic,live,rock,pop,country,drumset", "Sampled acoustic drum kits" },
                { "battery", "Battery", { K::drums }, "electronic,hip hop,trap,edm,punchy", "Native Instruments' drum sampler" },
                { "cymaticsdrumpod", "Cymatics DrumPod", { K::drums }, "hip hop,trap,modern,punchy,loop", "Cymatics drum loops and stems" },
                // keys and piano
                { "keyscape", "Keyscape", { K::piano, K::keys }, "piano,e. piano,rhodes,wurli,clav,warm,vintage", "Spectrasonics' sampled pianos and keyboards" },
                { "pianoteq", "Pianoteq", { K::piano, K::keys }, "piano,grand,acoustic,e. piano", "Modelled acoustic and electric pianos" },
                { "ezkeys", "EZkeys", { K::piano, K::keys }, "piano,grand,warm,pop,country", "Sampled pianos with built-in chords" },
                { "noire", "Noire", { K::piano }, "piano,felt,soft,cinematic,warm", "A felt-and-hammer grand piano" },
                { "thegiant", "The Giant", { K::piano }, "piano,grand,big,cinematic", "A huge upright grand piano" },
                { "keyzone", "Keyzone Classic", { K::piano }, "piano,grand", "A free sampled grand piano" },
                { "cymaticsmemory", "Cymatics Memory", { K::keys, K::piano }, "lofi,warm,vintage,tape,keys,piano", "Lo-fi keys" },
                { "cymaticsrwnd", "Cymatics RWND", { K::keys, K::pad }, "lofi,tape,vintage,warm", "Tape-worn lo-fi instrument" },
                { "cymaticsdaydream", "Cymatics Daydream", { K::keys, K::pad }, "lofi,dreamy,warm,soft,vintage", "Dreamy lo-fi keys and textures" },
                // bass
                { "trilian", "Trilian", { K::bass }, "e. bass,acoustic,upright,synth bass,warm", "Spectrasonics' bass instrument" },
                { "ezbass", "EZbass", { K::bass }, "e. bass,finger,pick,warm,pop,rock,country", "Sampled electric basses" },
                { "cymaticsquake", "Cymatics Quake", { K::bass, K::drums }, "808,sub,hard,trap,hip hop", "808s and bass" },
                { "cymaticsinvaderlite", "Cymatics Invader Lite", { K::bass, K::lead }, "synth bass,hard,modern,edm", "Synth basses" },
                // synths
                { "fm8", "FM8", { K::keys, K::pluck, K::bass, K::lead, K::pad, K::synth }, "fm,digital,80s,bright,bell,e. piano", "Native Instruments' FM synth: bells, electric pianos, basses" },
                { "dexed", "Dexed", { K::keys, K::pluck, K::bass, K::synth }, "fm,dx7,80s,bright,bell,e. piano", "A free DX7-style FM synth" },
                { "serum", "Serum", { K::synth, K::lead, K::bass, K::pad, K::pluck }, "modern,edm,bright,wide,digital", "Xfer's wavetable synth" },
                { "vital", "Vital", { K::synth, K::lead, K::bass, K::pad, K::pluck }, "modern,edm,bright,wide,digital", "A wavetable synth" },
                { "massive", "Massive", { K::bass, K::lead, K::synth, K::pad }, "edm,hard,digital,modern", "Native Instruments' wavetable synth" },
                { "omnisphere", "Omnisphere", { K::pad, K::synth, K::keys, K::lead, K::bass, K::pluck }, "ambient,cinematic,evolving,wide,lush,warm", "Spectrasonics' huge synth library" },
                { "pigments", "Pigments", { K::pad, K::synth, K::lead, K::bass, K::pluck }, "modern,evolving,wide,cinematic", "Arturia's wavetable and granular synth" },
                { "sylenth", "Sylenth1", { K::lead, K::synth, K::bass, K::pad }, "edm,trance,bright,analog", "A virtual analog synth" },
                { "diva", "Diva", { K::synth, K::lead, K::bass, K::pad }, "analog,vintage,warm,80s", "u-he's analog-modelled synth" },
                { "surgext", "Surge XT", { K::synth, K::lead, K::bass, K::pad, K::pluck }, "digital,modern,edm", "A free hybrid synth" },
                { "talunolx", "TAL-U-NO-LX", { K::pad, K::synth, K::bass }, "analog,80s,juno,warm,vintage", "A Juno-60-style analog synth" },
                { "cymaticsmonarch", "Cymatics Monarch", { K::synth, K::lead, K::pad }, "modern,wide,bright", "Cymatics synth" },
                { "cymaticsnightdrive", "Cymatics Night Drive", { K::synth, K::lead, K::pad }, "80s,synthwave,retro,analog", "Retro synthwave synth" },
                { "cymaticsparadigmlite", "Cymatics Paradigm Lite", { K::synth, K::pad, K::lead }, "modern,wide", "Cymatics synth" },
                { "cymaticsripple", "Cymatics Ripple", { K::synth, K::pluck, K::pad }, "modern,evolving", "Cymatics synth" },
                { "cymaticssketch", "Cymatics Sketch", { K::synth, K::keys, K::pad }, "soft,lofi,warm", "Cymatics synth" },
                { "cymaticsgameover", "Cymatics GAMEOVER", { K::lead, K::synth }, "chiptune,8 bit,retro,bright", "Chiptune and video-game sounds" },
                { "cymaticsorigin", "Cymatics Origin", { K::synth, K::lead, K::bass, K::pad }, "modern,edm", "Cymatics synth" },
                { "cymaticspluto", "Cymatics Pluto", { K::synth, K::lead, K::pad }, "modern", "Cymatics synth" },
                { "cymaticsgalaxy", "Cymatics Galaxy", { K::pad, K::synth }, "ambient,wide,evolving,space", "Spacey pads and textures" },
                { "cymaticshalo", "Cymatics Halo", { K::pad }, "ambient,airy,wide", "Airy pads" },
                { "cymaticsplinko", "Cymatics Plinko", { K::pluck, K::keys }, "pluck,bell,bright,mallet", "Plucks and bells" },
                { "cymaticsoceanpluck", "Cymatics Ocean Pluck", { K::pluck }, "pluck,bright,soft", "Plucks" },
                // orchestral and the rest
                { "labs", "Spitfire LABS", { K::strings, K::piano, K::pad, K::vocal }, "soft,cinematic,ambient,warm", "Free sampled instruments from Spitfire" },
                { "bbcsymphonyorchestra", "BBC Symphony Orchestra", { K::strings, K::brass, K::woodwinds, K::percussion }, "orchestral,cinematic,ensemble,legato", "A sampled symphony orchestra" },
                { "kontakt", "Kontakt", { K::other }, "", "Native Instruments' sampler: load one of its libraries" },
                { "voxaversasinger", "VoxaVersa Singer", { K::vocal }, "vocal,voice,singer,choir", "A singing voice" },
                { "sanctuarychoir", "Sanctuary Choir", { K::vocal }, "choir,ensemble,ambient,worship", "A choir" },
            };
            return p;
        }

        juce::String hintFor (Kind k)
        {
            if (k == Kind::drums) return "Pick a kit in the plug-in";
            if (k == Kind::vocal) return "Pick a voice in the plug-in";
            return "Pick a " + kindName (k).toLowerCase() + " sound in the plug-in";
        }
    }

    const PluginProfile* profileFor (const juce::String& pluginName)
    {
        const auto n = normaliseName (pluginName);
        auto matches = [&n] (const juce::String& m)
        {
            // short names must be the whole name ("Vital", "Vital 2"), so "Vitalizer" or "AudioLabs" don't count
            if (m.length() <= 5) return n == m || (n.startsWith (m) && n.substring (m.length()).containsOnly ("0123456789"));
            return n.contains (m);
        };
        const PluginProfile* best = nullptr;
        for (auto& p : profiles())
            if (matches (p.match) && (best == nullptr || juce::String (p.match).length() > juce::String (best->match).length()))
                best = &p;
        return best;
    }

    const PluginProfile* profileFor (const PluginInfo& plugin)
    {
        if (plugin.categoriesKnown && ! plugin.isInstrument) return nullptr;      // an effect that shares a name
        if (auto* p = profileFor (plugin.name)) return p;
        // "Cymatics/Plinko.vst3": the maker's name is only in the folder (or the plug-in's own info)
        if (plugin.vendor.isNotEmpty())
            if (auto* p = profileFor (plugin.vendor + plugin.name)) return p;
        return profileFor (plugin.file.getParentDirectory().getFileName() + plugin.name);
    }

    std::vector<Kind> kindsFromCategories (const juce::StringArray& cats)
    {
        std::vector<Kind> k;
        auto add = [&k] (Kind x) { if (std::find (k.begin(), k.end(), x) == k.end()) k.push_back (x); };
        for (auto& c : cats)
        {
            if (c.equalsIgnoreCase ("Drum")) add (Kind::drums);
            else if (c.equalsIgnoreCase ("Piano")) add (Kind::piano);
            else if (c.equalsIgnoreCase ("Synth")) add (Kind::synth);
            else if (c.equalsIgnoreCase ("Sampler")) add (Kind::other);
            else if (c.equalsIgnoreCase ("Organ")) add (Kind::organ);
            else if (c.equalsIgnoreCase ("Guitar")) add (Kind::guitar);
            else if (c.equalsIgnoreCase ("Strings")) add (Kind::strings);
            else if (c.equalsIgnoreCase ("Vocals") || c.equalsIgnoreCase ("Voice")) add (Kind::vocal);
        }
        return k;
    }

    std::vector<Sound> pluginLevelSounds (const std::vector<PluginInfo>& installed, const std::vector<Sound>& alreadyRead)
    {
        std::set<juce::String> covered;          // instruments whose presets are already in the catalog
        for (auto& s : alreadyRead) covered.insert (normaliseName (s.instrument));
        auto isCovered = [&covered] (const juce::String& n)    // "HALion" covers "HALion 7"
        {
            for (auto& c : covered)
                if (c == n || (juce::jmin (c.length(), n.length()) >= 5 && (n.startsWith (c) || c.startsWith (n)))) return true;
            return false;
        };

        std::vector<Sound> out;
        std::set<juce::String> done;
        for (auto& p : installed)
        {
            const auto* prof = profileFor (p);
            const auto key = normaliseName (prof != nullptr ? juce::String (prof->name) : p.name);
            if (done.count (key) > 0 || isCovered (normaliseName (p.name)) || isCovered (key)) continue;

            std::vector<Kind> kinds;
            juce::String words, note;
            if (prof != nullptr)
            {
                kinds = prof->kinds;
                words = prof->words;
                note = prof->note;
            }
            else if (p.isInstrument && ! p.categories.contains ("Tools", true))
            {
                kinds = kindsFromCategories (p.categories);
                note = "An instrument you have" + (p.vendor.isNotEmpty() ? " (" + p.vendor + ")" : juce::String());
            }
            kinds.erase (std::remove (kinds.begin(), kinds.end(), Kind::other), kinds.end());
            if (kinds.empty()) continue;
            done.insert (key);

            for (auto k : kinds)
            {
                Sound s;
                s.name = prof != nullptr ? juce::String (prof->name) : p.name;
                s.instrument = p.name;
                s.source = "Plug-in";
                s.kind = k;
                s.detail = hintFor (k);
                s.description = note;
                s.character = juce::StringArray::fromTokens (words, ",", "");
                s.character.trim();
                s.character.removeEmptyStrings();
                s.library = "Plug-in";
                s.location = p.file.getFullPathName();
                out.push_back (std::move (s));
            }
        }
        return out;
    }
}
