// Song Savant Scanner - the helper program that opens plug-ins for Song Savant, in its own process, so a plug-in that
// crashes or hangs can only take this little program down, never Song Savant or the DAW.
//
//   "Song Savant Scanner" <path to a .vst3> [answer file]
//       says what the plug-in is:
//       {"ok":true,"plugins":[{"name":"...","vendor":"...","instrument":true,"category":"Instrument|Synth","version":"..."}]}
//
//   "Song Savant Scanner" --map <preset.vstpreset> <plug-in .vst3> <answer file> [progress file]
//       maps a drum kit: loads the plug-in with the preset you saved in your DAW, plays every key from C0 to C6,
//       listens to each one and names it (see core/KitMap.h). The answer is a kit map as JSON.
//
// Answers go to the answer file when one is given (Song Savant reads them from there), and to the console.
#include <juce_audio_processors/juce_audio_processors.h>
#include "core/KitMap.h"
#include <iostream>

namespace
{
    int describePlugin (const juce::String& path, const juce::File& answerFile)
    {
        juce::OwnedArray<juce::PluginDescription> found;
       #if JUCE_PLUGINHOST_VST3
        juce::VST3PluginFormat vst3;
        if (path.endsWithIgnoreCase (".vst3")) vst3.findAllTypesForFile (found, path);
       #endif

        juce::Array<juce::var> list;
        for (auto* d : found)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("name", d->name);
            o->setProperty ("vendor", d->manufacturerName);
            o->setProperty ("instrument", d->isInstrument);
            o->setProperty ("category", d->category);
            o->setProperty ("version", d->version);
            list.add (juce::var (o));
        }
        auto* result = new juce::DynamicObject();
        result->setProperty ("ok", ! found.isEmpty());
        result->setProperty ("plugins", list);
        const auto answer = juce::JSON::toString (juce::var (result), true);
        if (answerFile != juce::File()) answerFile.replaceWithText (answer);
        std::cout << answer.toStdString() << std::endl;
        return found.isEmpty() ? 1 : 0;
    }

    int mapKit (const juce::File& preset, const juce::File& pluginFile, const juce::File& answerFile, const juce::File& progressFile)
    {
        ss::KitMap k;
        k.name = preset.getFileNameWithoutExtension();
        auto say = [&progressFile] (const juce::String& text)
        {
            if (progressFile != juce::File()) progressFile.replaceWithText (text);
        };
        auto finish = [&]
        {
            ss::nameKitPads (k);
            if (k.error.isEmpty() && k.pads.empty()) k.error = "No key made a sound";
            const auto answer = juce::JSON::toString (ss::kitMapToJson (k), true);
            if (answerFile != juce::File()) answerFile.replaceWithText (answer);
            std::cout << answer.toStdString() << std::endl;
            return k.ok() ? 0 : 1;
        };

       #if JUCE_PLUGINHOST_VST3
        juce::MemoryBlock data;
        if (! preset.loadFileAsData (data)) { k.error = "Couldn't read the preset file"; return finish(); }

        // open the plug-in and give it the preset (a plug-in file can hold several instruments: find the one it's for)
        say ("Opening " + pluginFile.getFileNameWithoutExtension() + "...");
        juce::VST3PluginFormat format;
        juce::OwnedArray<juce::PluginDescription> descriptions;
        format.findAllTypesForFile (descriptions, pluginFile.getFullPathName());
        const double sr = 44100.0;
        const int block = 512;
        std::unique_ptr<juce::AudioPluginInstance> plugin;
        for (auto* d : descriptions)
        {
            juce::String error;
            auto instance = format.createInstanceFromDescription (*d, sr, block, error);
            if (instance == nullptr) continue;
            if (juce::VST3PluginFormat::setStateFromVSTPresetFile (instance.get(), data))
            {
                plugin = std::move (instance);
                k.plugin = d->name;
                break;
            }
        }
        if (plugin == nullptr)
        {
            k.error = descriptions.isEmpty() ? "Couldn't open " + pluginFile.getFileNameWithoutExtension()
                                             : "This preset isn't for " + descriptions[0]->name;
            return finish();
        }

        plugin->enableAllBuses();
        plugin->setNonRealtime (true);
        plugin->prepareToPlay (sr, block);
        juce::VST3PluginFormat::setStateFromVSTPresetFile (plugin.get(), data);   // again, now that it's running

        const int outs = plugin->getTotalNumOutputChannels();
        const int channels = std::max (plugin->getTotalNumInputChannels(), outs);
        if (outs == 0) { k.error = k.plugin + " has no audio outputs"; return finish(); }

        juce::AudioBuffer<float> buffer (channels, block);
        juce::MidiBuffer midi;
        auto render = [&] (int numSamples, std::vector<float>* capture, const std::function<void (juce::MidiBuffer&, int, int)>& events)
        {
            for (int done = 0; done < numSamples; done += block)
            {
                const int n = std::min (block, numSamples - done);
                buffer.setSize (channels, n, false, false, true);
                buffer.clear();
                midi.clear();
                if (events) events (midi, done, n);
                plugin->processBlock (buffer, midi);
                if (capture != nullptr)
                    for (int i = 0; i < n; ++i)
                    {
                        float s = 0;
                        for (int c = 0; c < outs; ++c) s += buffer.getSample (c, i);
                        capture->push_back (s);
                    }
            }
        };

        // let it load its sounds (some plug-ins do that in the background)
        say ("Waiting for " + k.plugin + " to load its sounds...");
        for (int i = 0; i < 30; ++i)
        {
            render ((int) (sr * 0.1), nullptr, {});
            juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
        }

        // play every key, one at a time, and listen
        const int first = 24, last = 96;   // C0 to C6
        const int hold = (int) (sr * 0.25), listen = (int) (sr * 1.2), gap = (int) (sr * 0.4);
        std::vector<float> below;          // the key below, to spot one recording stretched over several keys
        for (int note = first; note <= last; ++note)
        {
            say ("Playing key " + ss::noteName (note) + " (" + juce::String (note - first + 1) + " of " + juce::String (last - first + 1) + ")");
            std::vector<float> hit;
            hit.reserve ((size_t) listen);
            render (listen, &hit, [note, hold] (juce::MidiBuffer& m, int start, int n)
            {
                if (start == 0) m.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 110), 0);
                if (hold >= start && hold < start + n) m.addEvent (juce::MidiMessage::noteOff (1, note), hold - start);
            });
            render (gap, nullptr, [] (juce::MidiBuffer& m, int start, int)
            {
                if (start == 0)
                {
                    m.addEvent (juce::MidiMessage::allNotesOff (1), 0);
                    m.addEvent (juce::MidiMessage::allSoundOff (1), 0);
                }
            });
            juce::MessageManager::getInstance()->runDispatchLoopUntil (1);

            const auto f = ss::analyseHit (hit.data(), (int) hit.size(), sr);
            const auto kind = ss::classifyHit (f);
            if (kind.isEmpty()) { below.clear(); continue; }
            ss::KitPad pad;
            pad.note = note;
            pad.kind = kind;
            pad.features = f;
            if (! below.empty()) pad.matchBelow = ss::pitchedCopyMatch (below, hit, sr);   // the key below, played higher?
            k.pads.push_back (pad);
            below = std::move (hit);
        }
        ss::groupPitchedZones (k);
        plugin->releaseResources();
        plugin.reset();
        say ("Done");
       #else
        juce::ignoreUnused (preset, pluginFile, say);
        k.error = "This helper can't open VST3 plug-ins";
       #endif
        return finish();
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;   // some plug-ins need a message thread to load
    juce::StringArray args;
    for (int i = 1; i < argc; ++i) args.add (juce::String::fromUTF8 (argv[i]));

    if (args[0] == "--map")
    {
        if (args.size() < 4)
        {
            std::cout << "{\"ok\":false,\"error\":\"usage: --map <preset> <plug-in> <answer file> [progress file]\"}" << std::endl;
            return 2;
        }
        auto file = [] (const juce::String& path) { return juce::File::getCurrentWorkingDirectory().getChildFile (path); };
        return mapKit (file (args[1]), file (args[2]), file (args[3]), args.size() > 4 ? file (args[4]) : juce::File());
    }
    if (args.isEmpty())
    {
        std::cout << "{\"ok\":false,\"error\":\"no plug-in given\"}" << std::endl;
        return 2;
    }
    return describePlugin (args[0], args.size() > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (args[1]) : juce::File());
}
