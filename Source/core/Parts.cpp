#include "Parts.h"

namespace ss
{
    namespace
    {
        PartSpec part (const char* role, const char* hint, std::vector<Kind> kinds, const char* want, const char* avoid = "", bool optional = false)
        {
            PartSpec p;
            p.role = role;
            p.hint = hint;
            p.kinds = std::move (kinds);
            p.want = juce::StringArray::fromTokens (want, ",", "");
            p.avoid = juce::StringArray::fromTokens (avoid, ",", "");
            p.want.trim(); p.want.removeEmptyStrings();
            p.avoid.trim(); p.avoid.removeEmptyStrings();
            p.optional = optional;
            return p;
        }

        PartSpec* find (std::vector<PartSpec>& parts, const juce::String& role)
        {
            for (auto& p : parts) if (p.role == role) return &p;
            return nullptr;
        }

        void wantMore (std::vector<PartSpec>& parts, const juce::String& role, const char* words)
        {
            if (auto* p = find (parts, role))
                for (auto& w : juce::StringArray::fromTokens (words, ",", "")) p->want.insert (0, w.trim());
        }

        void avoidMore (std::vector<PartSpec>& parts, const juce::String& role, const char* words)
        {
            if (auto* p = find (parts, role))
                for (auto& w : juce::StringArray::fromTokens (words, ",", "")) p->avoid.addIfNotAlreadyThere (w.trim());
        }

        using K = Kind;

        std::vector<PartSpec> band (Genre g)
        {
            switch (g)
            {
                case Genre::country: return {
                    part ("Drums", "A live kit with a tight snare", { K::drums }, "acoustic,live,country,pop,rock,drumset,warm", "trap,808,electronic,edm,techno"),
                    part ("Bass", "Round electric bass", { K::bass }, "e. bass,electric,finger,pick,warm,country,pop", "synth bass,808,distorted,acid,reese"),
                    part ("Rhythm 1", "Acoustic guitar strums", { K::guitar }, "a. guitar,acoustic,strum,steel,country,bright", "distorted,nylon,synth"),
                    part ("Rhythm 2", "Piano or organ under the guitars", { K::piano, K::keys, K::organ }, "piano,a. piano,warm,b3,organ,vintage", "synth,distorted,digital"),
                    part ("Lead", "Twangy electric or steel guitar", { K::guitar }, "e. guitar,electric,clean,twang,slide,steel,country", "distorted,heavy,synth"),
                    part ("Atmosphere", "Warm strings or a soft pad", { K::strings, K::pad }, "warm,soft,ensemble,strings,lush", "dark,harsh,digital,bizarre"),
                    part ("Fiddle", "Fiddle or harmonica fills", { K::strings, K::woodwinds }, "violin,fiddle,solo,harmonica", "synth", true) };
                case Genre::worship: return {
                    part ("Drums", "A big live kit with room", { K::drums }, "acoustic,live,pop,big,room,drumset", "trap,808,harsh,industrial"),
                    part ("Bass", "Warm bass, electric or a soft sub", { K::bass }, "e. bass,warm,clean,sub,pop", "distorted,harsh,acid,reese"),
                    part ("Keys", "Grand or felt piano", { K::piano, K::keys }, "piano,a. piano,grand,warm,soft,felt,clean", "honky,distorted,toy"),
                    part ("Pad", "A wide, warm pad that holds the song up", { K::pad }, "warm,ambient,wide,evolving,soft,airy,lush,swell,atmospheric", "harsh,dirty,bizarre,distorted"),
                    part ("Rhythm", "Clean electric guitar with delay", { K::guitar }, "e. guitar,electric,clean,delay,ambient,chorus", "distorted,heavy"),
                    part ("Lead", "Ambient lead or swells", { K::guitar, K::lead, K::pluck }, "ambient,delay,bright,swell,soft,airy", "harsh,distorted,aggressive"),
                    part ("Strings", "Warm string ensemble", { K::strings }, "ensemble,warm,soft,cinematic,legato", "staccato,harsh", true) };
                case Genre::pop: return {
                    part ("Drums", "Punchy modern pop drums", { K::drums }, "pop,punchy,modern,crisp,electronic,drumset", "jazz,brush"),
                    part ("Bass", "Tight synth or electric bass", { K::bass }, "synth bass,pop,punchy,modern,sub,e. bass", "acoustic,upright"),
                    part ("Chords", "Piano or synth chords", { K::piano, K::synth, K::keys }, "piano,bright,pop,modern,chords,clean", "dark,bizarre"),
                    part ("Lead", "A bright, catchy lead", { K::lead, K::pluck }, "bright,pop,catchy,clean,modern", "dark,harsh"),
                    part ("Pad", "Wide, bright pad", { K::pad }, "wide,bright,airy,lush,pop", "dark,dirty"),
                    part ("Pluck", "Plucks or bells for sparkle", { K::pluck, K::sequence }, "pluck,bell,bright,sparkle", "dark", true),
                    part ("Guitar", "Clean pop guitar", { K::guitar }, "clean,electric,pop,muted", "distorted", true) };
                case Genre::rock: return {
                    part ("Drums", "A big live rock kit", { K::drums }, "rock,live,acoustic,room,punchy,drumset", "electronic,808,trap"),
                    part ("Bass", "Picked electric bass", { K::bass }, "e. bass,pick,rock,electric,driven", "synth,808,sub"),
                    part ("Rhythm 1", "Crunchy rhythm guitar", { K::guitar }, "distorted,crunch,rock,e. guitar,electric,power", "nylon,clean"),
                    part ("Rhythm 2", "Clean or acoustic guitar", { K::guitar }, "clean,electric,acoustic,a. guitar", "synth"),
                    part ("Lead", "Lead guitar for the solo", { K::guitar, K::lead }, "lead,solo,distorted,e. guitar", "synth"),
                    part ("Keys", "Organ or piano", { K::organ, K::piano }, "b3,organ,rock,piano,drawbar", "synth", true) };
                case Genre::hiphop: return {
                    part ("Drums", "Dusty, punchy boom-bap drums", { K::drums }, "hip hop,boom bap,punchy,vintage,dusty,breaks,urban", "rock,brush,edm"),
                    part ("Bass", "Deep, warm bass", { K::bass }, "e. bass,warm,deep,sub,finger", "distorted,acid"),
                    part ("Keys", "Rhodes or soulful piano", { K::keys, K::piano }, "e. piano,rhodes,jazzy,warm,vintage,soul", "bright,harsh"),
                    part ("Sample", "Soulful strings, horns or a loop", { K::strings, K::brass, K::pluck, K::lead }, "soul,vintage,warm,jazzy,stab", "edm"),
                    part ("Pad", "Warm background pad", { K::pad }, "warm,dark,soft,vintage", "bright,harsh") };
                case Genre::trap: return {
                    part ("Drums", "Hard trap drums with rolling hats", { K::drums }, "trap,hip hop,808,hard,modern,urban", "rock,jazz,brush,acoustic"),
                    part ("808", "A long 808 bass", { K::bass }, "808,sub,deep,hard,trap", "e. bass,acoustic,upright,pick"),
                    part ("Melody", "Dark bells, keys or a pluck", { K::pluck, K::keys, K::piano }, "dark,bell,piano,pluck,melodic", "bright,happy"),
                    part ("Lead", "A dark lead or vocal chop", { K::lead, K::vocal, K::synth }, "dark,ambient,modern", "happy,bright"),
                    part ("Pad", "Dark atmosphere", { K::pad }, "dark,atmospheric,ambient,evolving", "bright,happy"),
                    part ("Strings", "Dark strings or choir", { K::strings, K::vocal }, "dark,orchestral,cinematic,choir", "happy", true) };
                case Genre::rnb: return {
                    part ("Drums", "Smooth, punchy R&B drums", { K::drums }, "r&b,urban,smooth,punchy,modern", "rock,metal"),
                    part ("Bass", "Smooth synth or electric bass", { K::bass }, "synth bass,smooth,sub,warm,e. bass,finger", "distorted,acid"),
                    part ("Keys", "Rhodes or warm electric piano", { K::keys, K::piano }, "e. piano,rhodes,warm,smooth,soft", "harsh,bright"),
                    part ("Pad", "Lush, soft pad", { K::pad }, "warm,lush,soft,smooth,airy", "harsh,dirty"),
                    part ("Lead", "Smooth lead or pluck", { K::lead, K::pluck }, "smooth,soft,warm,mellow", "harsh,aggressive"),
                    part ("Guitar", "Clean, soft guitar", { K::guitar }, "clean,soft,electric,chorus", "distorted", true),
                    part ("Strings", "Soft strings", { K::strings }, "soft,warm,ensemble", "harsh", true) };
                case Genre::edm: return {
                    part ("Drums", "Four-on-the-floor dance drums", { K::drums }, "edm,house,electronic,punchy,dance,techno", "acoustic,brush,jazz"),
                    part ("Bass", "Driving synth bass", { K::bass }, "synth bass,electronic,hard,sub,dance", "acoustic,upright,e. bass"),
                    part ("Chords", "Big synth chords or stabs", { K::synth, K::keys }, "supersaw,bright,wide,stab,chords,big", "acoustic"),
                    part ("Lead", "A big, bright lead", { K::lead }, "bright,big,supersaw,edm,trance", "soft,acoustic"),
                    part ("Pad", "Wide, airy pad", { K::pad }, "wide,airy,bright,evolving", "dirty"),
                    part ("Arp", "Arpeggio or sequence", { K::sequence, K::pluck }, "arpeggio,sequence,pluck,electronic", "", true),
                    part ("FX", "Risers and impacts", { K::fx }, "riser,impact,sweep,hit", "", true) };
                case Genre::lofi: return {
                    part ("Drums", "Dusty, laid-back drums", { K::drums }, "lofi,dusty,vintage,soft,hip hop,jazzy", "edm,metal,hard"),
                    part ("Bass", "Soft, warm bass", { K::bass }, "e. bass,warm,soft,finger,upright", "distorted,hard"),
                    part ("Keys", "Warm, worn electric piano", { K::keys, K::piano }, "e. piano,rhodes,lofi,warm,vintage,tape,soft", "bright,harsh"),
                    part ("Pad", "Soft, tape-warm pad", { K::pad }, "warm,soft,tape,lofi,vintage", "harsh,bright"),
                    part ("Lead", "Mellow guitar or pluck", { K::guitar, K::pluck, K::lead }, "soft,mellow,jazzy,clean,warm", "distorted,harsh"),
                    part ("Texture", "Vinyl and tape textures", { K::fx }, "vinyl,noise,tape,lofi", "", true) };
                case Genre::cinematic: return {
                    part ("Strings", "Full string ensemble", { K::strings }, "ensemble,epic,cinematic,orchestral,legato", "solo"),
                    part ("Brass", "Big brass", { K::brass }, "epic,big,ensemble,cinematic,orchestral", "solo,jazz"),
                    part ("Pad", "Dark, evolving bed", { K::pad }, "dark,cinematic,evolving,atmospheric,huge", "happy"),
                    part ("Percussion", "Epic drums and percussion", { K::percussion, K::drums }, "epic,taiko,cinematic,big,orchestral", "pop,trap"),
                    part ("Piano", "Soft, emotional piano", { K::piano }, "soft,cinematic,emotional,a. piano,felt", "honky"),
                    part ("Choir", "Choir or voices", { K::vocal }, "choir,epic,ensemble,ambient", "", true),
                    part ("FX", "Hits, risers and drones", { K::fx }, "hit,riser,impact,drone,cinematic", "", true) };
                case Genre::count: break;
            }
            return {};
        }

        void applyStyle (std::vector<PartSpec>& parts, const juce::String& id)
        {
            if (id == "country.rap")
            {
                wantMore (parts, "Drums", "trap,808,hip hop,hard");
                avoidMore (parts, "Drums", "brush,jazz");
                if (auto* d = find (parts, "Drums")) d->avoid.removeString ("trap"), d->avoid.removeString ("808"), d->avoid.removeString ("electronic");
                if (auto* b = find (parts, "Bass")) { b->want = { "808", "sub", "deep", "hard" }; b->avoid = { "upright", "acoustic" }; }
            }
            else if (id == "country.americana")
            {
                wantMore (parts, "Drums", "brush,vintage,live,soft");
                wantMore (parts, "Lead", "mandolin,banjo,acoustic,a. guitar,dobro");
                wantMore (parts, "Bass", "upright,acoustic");
            }
            else if (id == "country.classic")
            {
                wantMore (parts, "Rhythm 2", "honky tonk,piano,vintage");
                wantMore (parts, "Lead", "steel,twang,vintage,tele");
            }
            else if (id == "worship.gospel")
            {
                if (auto* k = find (parts, "Keys")) { k->kinds = { Kind::organ, Kind::keys, Kind::piano }; k->want = { "b3", "organ", "gospel", "drawbar", "e. piano", "rhodes" }; }
                wantMore (parts, "Drums", "live,punchy,gospel,funk");
                wantMore (parts, "Bass", "synth bass,e. bass,punchy,funk");
            }
            else if (id == "worship.ambient")
            {
                if (auto* d = find (parts, "Drums")) { d->optional = true; d->want.insert (0, "soft"); }
                wantMore (parts, "Pad", "evolving,ambient,shimmer");
                if (auto* s = find (parts, "Strings")) s->optional = false;
            }
            else if (id == "pop.synth")
            {
                for (auto* role : { "Drums", "Bass", "Chords", "Lead", "Pad" }) wantMore (parts, role, "80s,analog,synthwave,vintage");
            }
            else if (id == "rock.metal")
            {
                wantMore (parts, "Rhythm 1", "heavy,metal,high gain,aggressive");
                wantMore (parts, "Drums", "metal,heavy,aggressive");
            }
            else if (id.startsWith ("edm.house") || id == "edm.disco")
            {
                wantMore (parts, "Chords", "house,piano,organ,stab");
                wantMore (parts, "Bass", "house,funky");
            }
        }

        void applyEra (std::vector<PartSpec>& parts, int era)
        {
            const char* words = era <= 1 ? "vintage,warm,analog,tape,70s,60s"
                              : era == 2 ? "80s,analog,vintage,retro"
                              : era == 3 ? "90s,vintage"
                              : "modern,crisp,clean";
            for (auto& p : parts)
                for (auto& w : juce::StringArray::fromTokens (words, ",", "")) p.want.addIfNotAlreadyThere (w);
        }
    }

    juce::StringArray oppositesOf (const juce::String& w)
    {
        static const std::vector<std::pair<const char*, const char*>> pairs {
            { "warm", "cold,harsh,thin" }, { "bright", "dark,dull" }, { "dark", "bright,happy" }, { "soft", "hard,aggressive,harsh" },
            { "gentle", "hard,aggressive,harsh" }, { "clean", "dirty,distorted" }, { "gritty", "clean,polished" }, { "dirty", "clean" },
            { "vintage", "modern,digital" }, { "retro", "modern" }, { "modern", "vintage" }, { "acoustic", "electronic,synth" },
            { "electronic", "acoustic" }, { "organic", "electronic,digital" }, { "airy", "dark,heavy" }, { "wide", "mono,narrow" },
            { "punchy", "soft" }, { "mellow", "hard,harsh,aggressive" }, { "smooth", "harsh,distorted" }, { "chill", "aggressive,hard" },
            { "uplifting", "dark,sad" }, { "happy", "dark,sad" }, { "sad", "happy" }, { "aggressive", "soft" }, { "heavy", "soft,thin" } };
        for (auto& [word, opp] : pairs)
            if (w == word) return juce::StringArray::fromTokens (opp, ",", "");
        return {};
    }

    std::vector<PartSpec> partsFor (const SongBrief& b)
    {
        auto parts = band (b.genre);
        applyStyle (parts, b.styleId);
        if (b.era >= 0) applyEra (parts, b.era);
        for (int i = b.sound.size(); --i >= 0;)    // your words go first, in the order you wrote them
            for (auto& p : parts)
            {
                const auto& w = b.sound[i];
                p.want.removeString (w);
                p.want.insert (0, w);
                for (auto& o : oppositesOf (w)) p.avoid.addIfNotAlreadyThere (o);
                p.avoid.removeString (w);   // what you asked for is never held against a sound
            }
        return parts;
    }
}
