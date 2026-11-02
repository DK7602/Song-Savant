#include "StyleGuide.h"
#include "SSBinaryData.h"

namespace ss
{
    namespace
    {
        const juce::var& data()
        {
            static const juce::var v = juce::JSON::parse (juce::String::fromUTF8 (SSBinary::StyleGuide_json, SSBinary::StyleGuide_jsonSize));
            return v;
        }

        juce::StringArray list (const juce::var& v)
        {
            juce::StringArray out;
            if (auto* a = v.getArray())
                for (auto& s : *a) out.add (s.toString());
            return out;
        }

        void readGroove (const juce::var& g, Groove& out)
        {
            out = {};
            if (! g.isObject()) return;
            out.steps = juce::jlimit (16, 32, (int) g.getProperty ("steps", 16));
            out.swing = juce::jlimit (50, 75, (int) g.getProperty ("swing", 50));
            out.crash = (bool) g.getProperty ("crash", false);
            if (auto* o = g.getDynamicObject())
                for (auto& p : o->getProperties())
                {
                    const auto name = p.name.toString();
                    if (name == "fill")
                    {
                        if (auto* f = p.value.getDynamicObject())
                            for (auto& fp : f->getProperties()) out.fill[fp.name.toString()] = fp.value.toString();
                    }
                    else if (p.value.isString())
                        out.lanes[name] = p.value.toString();
                }
        }

        // what's in the entry replaces what was there
        void apply (const juce::var& e, StyleGuide& g)
        {
            if (! e.isObject()) return;
            auto text = [&e] (const char* key, juce::String& field) { if (e.hasProperty (key)) field = e[key].toString(); };
            auto texts = [&e] (const char* key, juce::StringArray& field) { if (e.hasProperty (key)) field = list (e[key]); };
            text ("summary", g.summary);
            text ("tempo", g.tempo);
            text ("feel", g.feel);
            text ("keys", g.keys);
            texts ("sounds", g.sounds);
            texts ("drums", g.drums);
            texts ("bass", g.bass);
            texts ("harmony", g.harmony);
            texts ("arrangement", g.arrangement);
            texts ("mix", g.mix);
            texts ("tips", g.tips);

            const auto midi = e["midi"];
            if (! midi.isObject()) return;
            if (auto* progs = midi["progressions"].getArray())
            {
                g.progressions.clear();
                for (auto& p : *progs) g.progressions.push_back (list (p));
            }
            if (midi.hasProperty ("groove")) readGroove (midi["groove"], g.groove);
            if (midi.hasProperty ("bass")) g.bassStyle = midi["bass"].toString();
            if (midi.hasProperty ("chords")) g.chordStyle = midi["chords"].toString();
            if (midi.hasProperty ("melody")) g.melodyStyle = midi["melody"].toString();
            if (auto* form = midi["form"].getArray())
            {
                g.form.clear();
                for (auto& f : *form)
                    g.form.push_back ({ f["name"].toString(), juce::jlimit (1, 64, (int) f["bars"]), f["play"].toString(), (int) f["progression"] });
            }
        }
    }

    StyleGuide styleGuide (Genre genre, const juce::String& styleId)
    {
        StyleGuide g;
        g.genre = genreName (genre);
        g.bassStyle = "rootkick";
        g.chordStyle = "pad";
        g.melodyStyle = "hook";

        // the genre's entry is kept under its "General" style id
        juce::String general;
        for (auto* s : stylesFor (genre)) if (s->isGeneral()) general = s->id;
        apply (data()["genres"][juce::Identifier (general.isNotEmpty() ? general : juce::String ("pop"))], g);

        const auto* style = styleById (styleId);
        if (style != nullptr && style->genre == genre && ! style->isGeneral())
        {
            apply (data()["styles"][juce::Identifier (style->id)], g);
            g.id = style->id;
            g.name = style->name;
        }
        else
        {
            g.id = general;
            g.name = genreName (genre);
        }
        if (g.progressions.empty()) g.progressions.push_back ({ "I", "V", "vi", "IV" });
        return g;
    }
}
