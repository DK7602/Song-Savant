// Song Savant Scanner - asks one plug-in what it is, the way a DAW does, and prints the answer as one line of JSON.
// Song Savant runs it in its own process for each plug-in that doesn't say what it is on disk, so a plug-in that
// crashes or hangs can only take this little program down, never Song Savant or the DAW.
//
//   "Song Savant Scanner" <path to a .vst3> [answer file]
//   -> {"ok":true,"plugins":[{"name":"...","vendor":"...","instrument":true,"category":"Instrument|Synth","version":"..."}]}
// The answer goes to the answer file when one is given (Song Savant reads it from there), and to the console.
#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;   // some plug-ins need a message thread to load
    if (argc < 2)
    {
        std::cout << "{\"ok\":false,\"error\":\"no plug-in given\"}" << std::endl;
        return 2;
    }
    const juce::String path = juce::String::fromUTF8 (argv[1]);

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
    if (argc >= 3) juce::File (juce::String::fromUTF8 (argv[2])).replaceWithText (answer);
    std::cout << answer.toStdString() << std::endl;
    return found.isEmpty() ? 1 : 0;
}
