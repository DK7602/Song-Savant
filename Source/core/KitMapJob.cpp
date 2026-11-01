#include "KitMapJob.h"
#include "PluginTypes.h"
#include "VendorProbe.h"
#include "VstPreset.h"

namespace ss
{
    PresetPlugin pluginForPreset (const juce::File& preset, const std::vector<PluginInfo>& installed)
    {
        PresetPlugin r;
        const auto info = readVstPreset (preset);
        if (! info.valid)
        {
            r.problem = "That isn't a VST3 preset file (" + preset.getFileName() + ")";
            return r;
        }

        // 1. a plug-in whose own info (moduleinfo.json) lists the preset's class ID
        if (info.classId.length() >= 32)
            for (auto& p : installed)
            {
                if (p.format != "VST3" || ! p.file.isDirectory()) continue;
                for (auto* rel : { "Contents/Resources/moduleinfo.json", "Contents/moduleinfo.json" })
                {
                    const auto mi = p.file.getChildFile (rel);
                    if (mi.existsAsFile() && mi.getSize() < 4 * 1024 * 1024 && mi.loadFileAsString().containsIgnoreCase (info.classId))
                    {
                        r.name = p.name;
                        r.file = p.file;
                        return r;
                    }
                }
            }

        // 2. by name: the one the preset gives, then the folders it's saved in
        juce::StringArray names;
        names.add (info.attributes["PlugInName"]);
        for (auto dir = preset.getParentDirectory(); names.size() < 5 && dir.getParentDirectory() != dir; dir = dir.getParentDirectory())
        {
            if (dir.getFileName().equalsIgnoreCase ("VST3 Presets")) break;
            names.add (dir.getFileName());
        }
        names.removeEmptyStrings();
        for (auto& n : names) if (n.equalsIgnoreCase ("HALion Sonic SE")) n = "HALion Sonic";   // its presets load in HALion Sonic

        for (int pass = 0; pass < 2; ++pass)
            for (auto& wanted : names)
            {
                const auto w = normaliseName (wanted);
                if (w.isEmpty()) continue;
                for (auto& p : installed)
                {
                    if (p.format != "VST3") continue;
                    const auto have = normaliseName (p.name);
                    const bool match = pass == 0 ? have == w
                                                 : juce::jmin (have.length(), w.length()) >= 5 && (have.startsWith (w) || w.startsWith (have));
                    if (match)
                    {
                        r.name = p.name;
                        r.file = p.file;
                        return r;
                    }
                }
            }

        r.problem = "Couldn't find the plug-in this preset belongs to" + (names.isEmpty() ? juce::String() : " (" + names[0] + ")");
        return r;
    }

    //==============================================================================
    void KitMapJob::start (const juce::File& preset, const SystemLocations& l)
    {
        if (isThreadRunning()) return;
        {
            const juce::ScopedLock sl (lock);
            presetFile = preset;
            loc = l;
            statusText = "Starting...";
        }
        cancelFlag = false;
        startThread (juce::Thread::Priority::normal);
    }

    juce::String KitMapJob::status() const                      { const juce::ScopedLock sl (lock); return statusText; }
    juce::File KitMapJob::preset() const                         { const juce::ScopedLock sl (lock); return presetFile; }
    std::shared_ptr<const KitMap> KitMapJob::result() const      { const juce::ScopedLock sl (lock); return latest; }
    void KitMapJob::setStatus (const juce::String& s)            { const juce::ScopedLock sl (lock); statusText = s; }

    void KitMapJob::setResult (const juce::File& preset, KitMap k)
    {
        const juce::ScopedLock sl (lock);
        presetFile = preset;
        latest = std::make_shared<const KitMap> (std::move (k));
        ++gen;
    }

    void KitMapJob::run()
    {
        juce::File preset;
        SystemLocations l;
        {
            const juce::ScopedLock sl (lock);
            preset = presetFile;
            l = loc;
        }
        KitMap k;
        k.name = preset.getFileNameWithoutExtension();
        auto publish = [&]
        {
            if (cancelFlag.load()) return;
            setStatus (k.ok() ? "Done" : k.error);
            setResult (preset, k);
        };

        setStatus ("Finding the plug-in this preset is for...");
        const auto found = pluginForPreset (preset, installedPlugins (l, &cancelFlag));
        if (cancelFlag.load()) return;
        if (found.problem.isNotEmpty()) { k.error = found.problem; publish(); return; }
        k.plugin = found.name;

        const auto scanner = scannerProgram();
        if (! scanner.existsAsFile())
        {
            k.error = "Song Savant Scanner is missing. Reinstall Song Savant to get it back.";
            publish();
            return;
        }

        const juce::TemporaryFile answer (".json"), progress (".txt");
        juce::ChildProcess p;
        setStatus ("Opening " + found.name + " in the background...");
        if (! p.start (juce::StringArray { scanner.getFullPathName(), "--map", preset.getFullPathName(), found.file.getFullPathName(),
                                           answer.getFile().getFullPathName(), progress.getFile().getFullPathName() }, 0))
        {
            k.error = "Couldn't start Song Savant Scanner";
            publish();
            return;
        }
        const auto started = juce::Time::getMillisecondCounter();
        while (p.isRunning())
        {
            if (cancelFlag.load() || threadShouldExit()) { p.kill(); return; }
            if ((int) (juce::Time::getMillisecondCounter() - started) > timeoutSeconds * 1000)
            {
                p.kill();
                k.error = found.name + " took too long, so Song Savant gave up on it.";
                publish();
                return;
            }
            if (const auto text = progress.getFile().loadFileAsString().trim(); text.isNotEmpty()) setStatus (text);
            p.waitForProcessToFinish (100);
        }

        const auto json = juce::JSON::parse (answer.getFile());
        if (! json.isObject())
        {
            k.error = found.name + " stopped while it was being played. Song Savant is fine; some plug-ins don't like being "
                                   "opened by another program.";
            publish();
            return;
        }
        auto mapped = kitMapFromJson (json);
        if (mapped.plugin.isEmpty()) mapped.plugin = found.name;
        if (mapped.name.isEmpty()) mapped.name = k.name;
        k = mapped;
        publish();
    }
}
