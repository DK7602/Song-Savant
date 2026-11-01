#include "KitMap.h"
#include "Paths.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <cmath>
#include <map>

#ifdef SS_VERSION
 #define SS_VERSION_TEXT SS_VERSION
#else
 #define SS_VERSION_TEXT ""
#endif

namespace ss
{
    namespace
    {
        // energy of the signal after a filter
        double filteredEnergy (const float* x, int n, juce::IIRCoefficients c, int passes = 1)
        {
            std::vector<float> y (x, x + n);
            for (int p = 0; p < passes; ++p)
            {
                juce::IIRFilter f;
                f.setCoefficients (c);
                f.processSamples (y.data(), n);
            }
            double e = 0;
            for (auto v : y) e += (double) v * v;
            return e;
        }

        int zeroCrossings (const float* x, int n)
        {
            int count = 0;
            for (int i = 1; i < n; ++i)
                if ((x[i - 1] < 0.0f) != (x[i] < 0.0f)) ++count;
            return count;
        }
    }

    HitFeatures analyseHit (const float* x, int n, double sr)
    {
        HitFeatures f;
        if (x == nullptr || n <= 0 || sr <= 0) return f;
        for (int i = 0; i < n; ++i) f.peak = std::max (f.peak, std::abs (x[i]));
        if (f.silent()) return f;

        // loudness every 5 ms: when does it die away (30 dB below its loudest)?
        const int frame = std::max (1, (int) (sr * 0.005));
        std::vector<float> env;
        for (int i = 0; i < n; i += frame)
        {
            double e = 0;
            const int m = std::min (frame, n - i);
            for (int j = 0; j < m; ++j) e += (double) x[i + j] * x[i + j];
            env.push_back ((float) std::sqrt (e / m));
        }
        const auto top = std::max_element (env.begin(), env.end());
        const float envPeak = *top;
        size_t end = env.size();
        for (auto it = top; it != env.end(); ++it)
            if (*it < envPeak * 0.0316f) { end = (size_t) std::distance (env.begin(), it); break; }
        f.decay = (float) ((double) end * frame / sr);

        // how low and how bright, over the body of the sound (at least 50 ms, at most half a second)
        const int body = juce::jlimit (std::min (n, (int) (sr * 0.05)), std::min (n, (int) (sr * 0.5)), (int) ((double) end * frame));
        double total = 0;
        for (int i = 0; i < body; ++i) total += (double) x[i] * x[i];
        if (total > 0)
        {
            f.lowRatio = (float) (filteredEnergy (x, body, juce::IIRCoefficients::makeLowPass (sr, 150.0), 2) / total);
            f.highRatio = (float) (filteredEnergy (x, body, juce::IIRCoefficients::makeHighPass (sr, 5000.0), 2) / total);
            f.bodyRatio = (float) (filteredEnergy (x, body, juce::IIRCoefficients::makeLowPass (sr, 400.0), 2) / total);
        }

        // how noisy at the start
        const int start = std::min (n, std::max (8, (int) (sr * 0.1)));
        f.zcr = (float) (zeroCrossings (x, start) / ((double) start / sr));

        // rough pitch of the low part, after the click of the attack
        {
            const int from = std::min (n, (int) (sr * 0.03));
            const int len = std::min (n - from, (int) (sr * juce::jlimit (0.04, 0.15, (double) f.decay)));
            if (len > 16)
            {
                std::vector<float> low (x + from, x + from + len);
                for (int p = 0; p < 2; ++p)
                {
                    juce::IIRFilter lp;
                    lp.setCoefficients (juce::IIRCoefficients::makeLowPass (sr, 400.0));
                    lp.processSamples (low.data(), len);
                }
                f.pitchHz = (float) (zeroCrossings (low.data(), len) / 2.0 / ((double) len / sr));
            }
        }

        // how clearly pitched: how well the sound matches itself one period later (a tom, a kick or a cowbell does;
        // the noise of a snare or a hat doesn't). Searched from 40 Hz to 1 kHz, over 60 ms after the attack.
        {
            const int from = std::min (n, (int) (sr * 0.005));
            const int len = std::min (n - from, (int) (sr * 0.06));
            const int minLag = std::max (2, (int) (sr / 1000.0));
            const int maxLag = std::min (len / 2, (int) (sr / 40.0));
            if (len > 64 && maxLag > minLag)
            {
                const float* w = x + from;
                std::vector<float> r ((size_t) (maxLag + 1), 0.0f);
                float best = 0;
                for (int lag = minLag; lag <= maxLag; ++lag)
                {
                    double xy = 0, xx = 0, yy = 0;
                    for (int i = 0; i + lag < len; ++i)
                    {
                        xy += (double) w[i] * w[i + lag];
                        xx += (double) w[i] * w[i];
                        yy += (double) w[i + lag] * w[i + lag];
                    }
                    r[(size_t) lag] = xx > 0 && yy > 0 ? (float) (xy / std::sqrt (xx * yy)) : 0.0f;
                    best = std::max (best, r[(size_t) lag]);
                }
                f.tonal = std::max (0.0f, best);
                // the period is the first strong match (not twice or three times it)
                for (int lag = minLag + 1; lag < maxLag; ++lag)
                    if (r[(size_t) lag] >= best * 0.9f && r[(size_t) lag] >= r[(size_t) lag - 1] && r[(size_t) lag] >= r[(size_t) lag + 1])
                    {
                        f.toneHz = (float) (sr / lag);
                        break;
                    }
            }
        }

        // separate bursts in the first 60 ms (a clap is several quick hits)
        {
            const int ms = std::max (1, (int) (sr * 0.001));
            const int span = std::min (n, (int) (sr * 0.06));
            std::vector<float> e1;
            for (int i = 0; i + ms <= span; i += ms)
            {
                double e = 0;
                for (int j = 0; j < ms; ++j) e += (double) x[i + j] * x[i + j];
                e1.push_back ((float) std::sqrt (e / ms));
            }
            const float mx = e1.empty() ? 0.0f : *std::max_element (e1.begin(), e1.end());
            int bursts = 0;
            bool armed = true;
            for (auto v : e1)
            {
                if (armed && v > mx * 0.4f) { ++bursts; armed = false; }
                else if (! armed && v < mx * 0.15f) armed = true;
            }
            f.bursts = std::max (1, bursts);
        }
        return f;
    }

    juce::String classifyHit (const HitFeatures& f)
    {
        if (f.silent()) return {};
        // noisy: lots of zero crossings, more than its pitch explains (a high tom or a cowbell crosses zero often too)
        const bool pitched = f.tonal > 0.6f;
        const bool noisy = f.zcr > 1500.0f && (! pitched || f.zcr > 6.0f * f.toneHz);
        if (f.highRatio > 0.5f && (noisy || ! pitched))
            return f.decay < 0.15f ? "Closed Hat" : f.decay < 0.7f ? "Open Hat" : "Cymbal";
        if (f.lowRatio > 0.45f && f.pitchHz < 90.0f)
            return f.decay > 0.6f ? "808" : "Kick";
        if (noisy)
        {
            if (f.bursts >= 3) return "Clap";
            // a tom with a noisy, breathy hit: not very noisy or bright, low, and it rings on (a snare dies away fast)
            const float low = f.toneHz > 0 ? f.toneHz : f.pitchHz;
            if (f.zcr < 2200.0f && f.decay >= 0.2f && f.highRatio < 0.1f && low >= 60.0f && low < 400.0f) return "Tom";
            return "Snare";
        }
        const float hz = pitched && f.toneHz > 0 ? f.toneHz : f.pitchHz;
        if (hz >= 60.0f && hz < 400.0f && f.decay > 0.12f && f.bodyRatio > 0.35f) return "Tom";   // pitched, low to mid body, rings a while
        return "Perc";
    }

    const juce::StringArray& padKinds()
    {
        static const juce::StringArray kinds { "Kick", "808", "Snare", "Rim", "Clap", "Snap", "Closed Hat", "Open Hat", "Pedal Hat", "Cymbal",
                                               "Crash", "Ride", "Tom", "Perc", "Shaker", "Tambourine", "Cowbell", "Conga", "Bongo", "Vocal", "FX" };
        return kinds;
    }

    float pitchedCopyMatch (const std::vector<float>& lower, const std::vector<float>& upper, double sr)
    {
        const double r = std::pow (2.0, 1.0 / 12.0);
        const int len = (int) std::min ({ (double) lower.size(), sr * 0.08, (double) upper.size() * r - 2.0 });
        if (len < 64) return 0.0f;

        // the upper key slowed down by a semitone: upper (t / r) lines up with lower (t) when it's the same recording
        std::vector<float> slowed ((size_t) len);
        for (int i = 0; i < len; ++i)
        {
            const double pos = i / r;
            const int j = (int) pos;
            const double frac = pos - j;
            const float a = upper[(size_t) j], b = (size_t) j + 1 < upper.size() ? upper[(size_t) j + 1] : a;
            slowed[(size_t) i] = (float) (a + (b - a) * frac);
        }

        // best match within +-2 ms (some plug-ins start a little late)
        const int maxLag = (int) (sr * 0.002);
        float best = 0;
        for (int lag = -maxLag; lag <= maxLag; ++lag)
        {
            double ab = 0, aa = 0, bb = 0;
            for (int i = std::max (0, -lag); i < len && i + lag < len; ++i)
            {
                const double a = lower[(size_t) i], b = slowed[(size_t) (i + lag)];
                ab += a * b;
                aa += a * a;
                bb += b * b;
            }
            if (aa > 0 && bb > 0) best = std::max (best, (float) (ab / std::sqrt (aa * bb)));
        }
        return best;
    }

    void groupPitchedZones (KitMap& k)
    {
        std::sort (k.pads.begin(), k.pads.end(), [] (const KitPad& a, const KitPad& b) { return a.note < b.note; });
        size_t start = 0;
        while (start < k.pads.size())
        {
            size_t end = start + 1;          // the zone: each key is the key below, played higher
            while (end < k.pads.size() && k.pads[end].note == k.pads[end - 1].note + 1 && k.pads[end].matchBelow >= 0.75f) ++end;
            const size_t size = end - start;
            if (size >= 3)
            {
                // what the lowest third of the zone sounds like (the recording closest to how it was made)
                std::map<juce::String, int> votes;
                const size_t low = std::max<size_t> (2, (size + 2) / 3);
                for (size_t i = start; i < start + low; ++i) votes[k.pads[i].kind]++;
                juce::String kind = k.pads[start].kind;
                int most = 0;
                for (size_t i = start; i < start + low; ++i)
                    if (votes[k.pads[i].kind] > most) { most = votes[k.pads[i].kind]; kind = k.pads[i].kind; }
                for (size_t i = start; i < end; ++i)
                    if (! k.pads[i].custom) k.pads[i].kind = kind;
            }
            start = end;
        }
    }

    void nameKitPads (KitMap& k)
    {
        std::sort (k.pads.begin(), k.pads.end(), [] (const KitPad& a, const KitPad& b) { return a.note < b.note; });

        // one sound across the keyboard: one recording stretched over most of the keys (or, without that information,
        // most keys sounding the same kind)
        int most = 0;
        bool heardZones = false;
        for (auto& p : k.pads) heardZones = heardZones || p.matchBelow > 0.0f;
        if (heardZones)
        {
            int run = 0;
            for (size_t i = 0; i < k.pads.size(); ++i)
            {
                run = i > 0 && k.pads[i].note == k.pads[i - 1].note + 1 && k.pads[i].matchBelow >= 0.75f ? run + 1 : 1;
                most = std::max (most, run);
            }
        }
        else
        {
            std::map<juce::String, int> counts;
            for (auto& p : k.pads) counts[p.kind]++;
            for (auto& [kind, n] : counts) most = std::max (most, n);
        }
        k.oneSound = k.pads.size() >= 18 && most * 4 >= (int) k.pads.size() * 3;

        std::vector<KitPad*> toms;
        for (auto& p : k.pads) if (p.kind == "Tom" && ! p.custom) toms.push_back (&p);
        const char* three[] = { "Low Tom", "Mid Tom", "High Tom" };
        const char* two[] = { "Low Tom", "High Tom" };
        for (size_t i = 0; i < toms.size(); ++i)
            toms[i]->name = toms.size() == 1 ? juce::String ("Tom")
                          : toms.size() == 2 ? juce::String (two[i])
                          : toms.size() == 3 ? juce::String (three[i])
                          : "Tom " + juce::String ((int) i + 1);

        std::map<juce::String, int> seen;
        for (auto& p : k.pads)
        {
            if (p.kind == "Tom" || p.custom) continue;
            const int n = ++seen[p.kind];
            p.name = n == 1 ? p.kind : p.kind + " " + juce::String (n);
        }
    }

    void setPadKind (KitMap& k, int first, int last, const juce::String& kind)
    {
        if (first > last) std::swap (first, last);
        for (int i = std::max (0, first); i <= last && i < (int) k.pads.size(); ++i)
        {
            k.pads[(size_t) i].kind = kind;
            k.pads[(size_t) i].custom = false;
        }
        nameKitPads (k);
    }

    void setPadName (KitMap& k, int index, const juce::String& name)
    {
        if (index < 0 || index >= (int) k.pads.size() || name.trim().isEmpty()) return;
        k.pads[(size_t) index].name = name.trim();
        k.pads[(size_t) index].custom = true;
        nameKitPads (k);
    }

    void removePads (KitMap& k, int first, int last)
    {
        if (first > last) std::swap (first, last);
        first = std::max (0, first);
        last = std::min (last, (int) k.pads.size() - 1);
        if (first <= last) k.pads.erase (k.pads.begin() + first, k.pads.begin() + last + 1);
        nameKitPads (k);
    }

    juce::String describeHits (const KitMap& k)
    {
        juce::String s;
        s << "Song Savant " << SS_VERSION_TEXT << " - what it heard on each key\n"
          << "Kit: " << k.name << "  |  " << k.plugin << "\n\n"
          << "key   note  name             heard as    same as key below  pitch-clear  tone Hz  low Hz  noise/s  decay s  low  body  high  bursts  level\n";
        for (auto& p : k.pads)
        {
            const auto& f = p.features;
            auto col = [] (const juce::String& t, int w) { return t.paddedRight (' ', w); };
            s << col (juce::String (p.note), 6) << col (noteName (p.note), 6) << col (p.name, 17) << col (p.kind, 12)
              << col (juce::String (p.matchBelow, 2), 19) << col (juce::String (f.tonal, 2), 13) << col (juce::String (juce::roundToInt (f.toneHz)), 9)
              << col (juce::String (juce::roundToInt (f.pitchHz)), 8) << col (juce::String (juce::roundToInt (f.zcr)), 9)
              << col (juce::String (f.decay, 2), 9) << col (juce::String (f.lowRatio, 2), 5) << col (juce::String (f.bodyRatio, 2), 6)
              << col (juce::String (f.highRatio, 2), 6) << col (juce::String (f.bursts), 8) << juce::String (f.peak, 3) << "\n";
        }
        return s;
    }

    juce::var kitMapToJson (const KitMap& k)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("ok", k.ok());
        o->setProperty ("name", k.name);
        o->setProperty ("plugin", k.plugin);
        o->setProperty ("oneSound", k.oneSound);
        o->setProperty ("error", k.error);
        juce::Array<juce::var> pads;
        for (auto& p : k.pads)
        {
            auto* d = new juce::DynamicObject();
            d->setProperty ("note", p.note);
            d->setProperty ("kind", p.kind);
            d->setProperty ("name", p.name);
            d->setProperty ("peak", p.features.peak);
            d->setProperty ("low", p.features.lowRatio);
            d->setProperty ("high", p.features.highRatio);
            d->setProperty ("zcr", p.features.zcr);
            d->setProperty ("decay", p.features.decay);
            d->setProperty ("pitch", p.features.pitchHz);
            d->setProperty ("bursts", p.features.bursts);
            d->setProperty ("tonal", p.features.tonal);
            d->setProperty ("body", p.features.bodyRatio);
            d->setProperty ("tone", p.features.toneHz);
            d->setProperty ("custom", p.custom);
            d->setProperty ("match", p.matchBelow);
            pads.add (juce::var (d));
        }
        o->setProperty ("pads", pads);
        return juce::var (o);
    }

    KitMap kitMapFromJson (const juce::var& v)
    {
        KitMap k;
        k.name = v["name"].toString();
        k.plugin = v["plugin"].toString();
        k.oneSound = (bool) v["oneSound"];
        k.error = v["error"].toString();
        if (auto* pads = v["pads"].getArray())
            for (auto& d : *pads)
            {
                KitPad p;
                p.note = juce::jlimit (0, 127, (int) d["note"]);
                p.kind = d["kind"].toString();
                p.name = d["name"].toString();
                p.features.peak = (float) d["peak"];
                p.features.lowRatio = (float) d["low"];
                p.features.highRatio = (float) d["high"];
                p.features.zcr = (float) d["zcr"];
                p.features.decay = (float) d["decay"];
                p.features.pitchHz = (float) d["pitch"];
                p.features.bursts = (int) d["bursts"];
                p.features.tonal = (float) d["tonal"];
                p.features.bodyRatio = (float) d["body"];
                p.features.toneHz = (float) d["tone"];
                p.custom = (bool) d["custom"];
                p.matchBelow = (float) d["match"];
                k.pads.push_back (p);
            }
        if (! v.isObject() && k.error.isEmpty()) k.error = "No answer from Song Savant Scanner";
        return k;
    }

    juce::String noteName (int note)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (names[((note % 12) + 12) % 12]) + juce::String (note / 12 - 2);
    }

    juce::String makeCubaseDrumMap (const KitMap& k)
    {
        // Cubase's drum map layout: a name, quantize settings, 128 sounds (one per key), the display order, the output
        auto stringEl = [] (juce::XmlElement& parent, const juce::String& name, const juce::String& value, bool wide)
        {
            auto* e = parent.createNewChildElement ("string");
            e->setAttribute ("name", name);
            e->setAttribute ("value", value);
            if (wide) e->setAttribute ("wide", "true");
        };
        auto intEl = [] (juce::XmlElement& parent, const juce::String& name, int value)
        {
            auto* e = parent.createNewChildElement ("int");
            e->setAttribute ("name", name);
            e->setAttribute ("value", value);
        };

        juce::XmlElement root ("DrumMap");
        stringEl (root, "Name", k.name.isNotEmpty() ? k.name : juce::String ("Song Savant kit"), true);

        auto* quantize = root.createNewChildElement ("list");
        quantize->setAttribute ("name", "Quantize");
        quantize->setAttribute ("type", "list");
        {
            auto* q = quantize->createNewChildElement ("item");
            intEl (*q, "Grid", 4);
            intEl (*q, "Type", 0);
            auto* swing = q->createNewChildElement ("float");
            swing->setAttribute ("name", "Swing");
            swing->setAttribute ("value", "0");
            intEl (*q, "Legato", 50);
        }

        std::map<int, juce::String> names;
        for (auto& p : k.pads) names[p.note] = p.name;

        auto* map = root.createNewChildElement ("list");
        map->setAttribute ("name", "Map");
        map->setAttribute ("type", "list");
        for (int note = 0; note < 128; ++note)
        {
            auto* item = map->createNewChildElement ("item");
            intEl (*item, "INote", note);
            intEl (*item, "ONote", note);
            intEl (*item, "Channel", 0);
            auto* len = item->createNewChildElement ("float");
            len->setAttribute ("name", "Length");
            len->setAttribute ("value", "200");
            intEl (*item, "Mute", 0);
            intEl (*item, "DisplayNote", note);
            intEl (*item, "HeadSymbol", 0);
            intEl (*item, "Voice", 0);
            intEl (*item, "PortIndex", 0);
            stringEl (*item, "Name", names.count (note) > 0 ? names[note] : juce::String(), true);
            intEl (*item, "QuantizeIndex", 0);
        }

        // the kit's sounds first, then the empty keys
        auto* order = root.createNewChildElement ("list");
        order->setAttribute ("name", "Order");
        order->setAttribute ("type", "int");
        for (auto& [note, name] : names) order->createNewChildElement ("item")->setAttribute ("value", note);
        for (int note = 0; note < 128; ++note)
            if (names.count (note) == 0) order->createNewChildElement ("item")->setAttribute ("value", note);

        auto* outs = root.createNewChildElement ("list");
        outs->setAttribute ("name", "OutputDevices");
        outs->setAttribute ("type", "list");
        {
            auto* o = outs->createNewChildElement ("item");
            stringEl (*o, "DeviceName", "Default Device", false);
            stringEl (*o, "PortName", "Default Port", false);
        }
        intEl (root, "Flags", 0);
        return root.toString();
    }

    juce::File drumMapsFolder()   { return paths::documentsDir().getChildFile ("Drum Maps"); }

    juce::File saveHitDetails (const KitMap& k)
    {
        const auto folder = drumMapsFolder();
        if (! folder.createDirectory()) return {};
        auto name = juce::File::createLegalFileName (k.name.isNotEmpty() ? k.name : juce::String ("Song Savant kit")).trim();
        if (name.isEmpty()) name = "Song Savant kit";
        const auto f = folder.getChildFile (name + " - what Song Savant heard.txt");
        return f.replaceWithText (describeHits (k)) ? f : juce::File();
    }

    juce::File saveCubaseDrumMap (const KitMap& k)
    {
        const auto folder = drumMapsFolder();
        if (! folder.createDirectory()) return {};
        auto name = juce::File::createLegalFileName (k.name.isNotEmpty() ? k.name : juce::String ("Song Savant kit")).trim();
        if (name.isEmpty()) name = "Song Savant kit";
        const auto f = folder.getChildFile (name + ".drm");
        return f.replaceWithText (makeCubaseDrumMap (k), false, false, "\n") ? f : juce::File();
    }
}
