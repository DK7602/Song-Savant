#include "KitMap.h"
#include "Paths.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <cmath>
#include <map>

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
        if (f.highRatio > 0.5f)
            return f.decay < 0.15f ? "Closed Hat" : f.decay < 0.7f ? "Open Hat" : "Cymbal";
        if (f.lowRatio > 0.45f && f.pitchHz < 90.0f)
            return f.decay > 0.6f ? "808" : "Kick";
        if (f.bursts >= 3 && f.zcr > 1500.0f) return "Clap";
        if (f.zcr > 1500.0f) return "Snare";
        if (f.pitchHz >= 60.0f && f.pitchHz < 400.0f && f.decay > 0.12f) return "Tom";   // tonal, low to mid, rings a while
        return "Perc";
    }

    void nameKitPads (KitMap& k)
    {
        std::sort (k.pads.begin(), k.pads.end(), [] (const KitPad& a, const KitPad& b) { return a.note < b.note; });

        std::map<juce::String, int> counts;
        for (auto& p : k.pads) counts[p.kind]++;
        int most = 0;
        for (auto& [kind, n] : counts) most = std::max (most, n);
        k.oneSound = k.pads.size() >= 18 && most * 4 >= (int) k.pads.size() * 3;

        std::vector<KitPad*> toms;
        for (auto& p : k.pads) if (p.kind == "Tom") toms.push_back (&p);
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
            if (p.kind == "Tom") continue;
            const int n = ++seen[p.kind];
            p.name = n == 1 ? p.kind : p.kind + " " + juce::String (n);
        }
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
