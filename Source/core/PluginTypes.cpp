#include "PluginTypes.h"
#include "Paths.h"

namespace ss
{
    namespace
    {
        juce::File cacheFile()   { return paths::appDataDir().getChildFile ("plugin-types.json"); }

        juce::var loadCache()
        {
            auto v = juce::JSON::parse (cacheFile());
            return v.isObject() ? v : juce::var (new juce::DynamicObject());
        }

        juce::Identifier keyFor (const juce::File& f)   { return juce::Identifier (f.getFullPathName()); }

        // When the plug-in was last installed or updated: a bundle's own folder keeps its date when the plug-in
        // inside is replaced, so look at the files in Contents/<platform>/ too
        juce::int64 stampFor (const juce::File& f)
        {
            auto t = f.getLastModificationTime().toMilliseconds();
            if (f.isDirectory())
                for (const auto& d : juce::RangedDirectoryIterator (f.getChildFile ("Contents"), false, "*", juce::File::findDirectories))
                    for (const auto& e : juce::RangedDirectoryIterator (d.getFile(), false, "*", juce::File::findFilesAndDirectories))
                        t = juce::jmax (t, e.getModificationTime().toMilliseconds());
            return t / 1000;
        }

        bool needsCheck (const PluginInfo& p)   { return ! p.categoriesKnown && p.format == "VST3"; }
    }

    juce::File scannerProgram()
    {
       #if JUCE_WINDOWS
        const juce::String exe = "Song Savant Scanner.exe";
       #else
        const juce::String exe = "Song Savant Scanner";
       #endif
       #if SS_TEST_RUNNER
        if (auto env = juce::SystemStats::getEnvironmentVariable ("SS_SCANNER", {}); env.isNotEmpty()) return juce::File (env);
       #endif
        juce::Array<juce::File> places;
        places.add (juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory().getChildFile (exe));
       #if JUCE_WINDOWS
        // the installer notes where it put Song Savant (the plug-in runs inside the DAW, far from the app's folder)
        if (auto dir = juce::WindowsRegistry::getValue ("HKEY_LOCAL_MACHINE\\Software\\Grabber Audio\\Song Savant\\InstallDir"); dir.isNotEmpty())
            places.add (juce::File (dir).getChildFile (exe));
        const auto pf = juce::SystemStats::getEnvironmentVariable ("ProgramW6432", "C:\\Program Files");
        places.add (juce::File (pf).getChildFile ("Song Savant").getChildFile (exe));
       #elif JUCE_MAC
        places.add (juce::File ("/Applications/Song Savant.app/Contents/MacOS").getChildFile (exe));
       #endif
        for (auto& f : places)
            if (f.existsAsFile()) return f;
        return {};
    }

    CheckedPlugin checkPlugin (const juce::File& plugin, const juce::File& scanner, const std::atomic<bool>& cancel, int timeoutMs)
    {
        CheckedPlugin r;
        if (! scanner.existsAsFile()) return r;
        const juce::TemporaryFile answer (".json");
        juce::ChildProcess p;
        // the answer comes back in a file, not through a pipe, so a chatty plug-in can't block it
        if (! p.start (juce::StringArray { scanner.getFullPathName(), plugin.getFullPathName(), answer.getFile().getFullPathName() }, 0))
            return r;
        const auto start = juce::Time::getMillisecondCounter();
        while (p.isRunning())
        {
            if (cancel.load() || (int) (juce::Time::getMillisecondCounter() - start) > timeoutMs)
            {
                p.kill();                               // hung, or waiting on a licence window: give up on this one
                return r;
            }
            p.waitForProcessToFinish (100);
        }
        const auto json = juce::JSON::parse (answer.getFile());
        if (! (bool) json["ok"]) return r;
        if (auto* list = json["plugins"].getArray())
            for (auto& d : *list)
            {
                r.ok = true;
                const bool inst = (bool) d["instrument"];
                if (r.category.isEmpty() || (inst && ! r.instrument)) r.category = d["category"].toString();
                r.instrument = r.instrument || inst;
                if (r.vendor.isEmpty()) r.vendor = d["vendor"].toString();
            }
        return r;
    }

    std::vector<PluginInfo> installedPlugins (const SystemLocations& loc, const std::atomic<bool>* cancel)
    {
        auto plugins = scanInstalledPlugins (loc, cancel);
        for (auto& d : loc.cubaseSettings)
            for (const auto& e : juce::RangedDirectoryIterator (d, false, "VstPlugInfo*.xml", juce::File::findFiles))
            {
                applyCubasePluginList (plugins, readCubasePluginList (e.getFile()));
                break;
            }
        applyKnownTypes (plugins);
        return plugins;
    }

    void applyKnownTypes (std::vector<PluginInfo>& plugins)
    {
        const auto cache = loadCache();
        for (auto& p : plugins)
        {
            if (! needsCheck (p)) continue;
            const auto e = cache[keyFor (p.file)];
            if (! e.isObject() || (juce::int64) e["stamp"] != stampFor (p.file) || ! (bool) e["ok"]) continue;
            p.categories = juce::StringArray::fromTokens (e["category"].toString(), "|", "");
            p.categories.trim();
            p.categories.removeEmptyStrings();
            p.isInstrument = (bool) e["instrument"];
            p.categoriesKnown = true;
            p.categorySource = "checked";
            if (p.vendor.isEmpty()) p.vendor = e["vendor"].toString();
        }
    }

    std::vector<PluginInfo> pluginsToCheck (const std::vector<PluginInfo>& plugins)
    {
        const auto cache = loadCache();
        std::vector<PluginInfo> out;
        for (auto& p : plugins)
        {
            if (! needsCheck (p)) continue;
            const auto e = cache[keyFor (p.file)];
            if (e.isObject() && (juce::int64) e["stamp"] == stampFor (p.file)) continue;   // checked before (even if it failed)
            out.push_back (p);
        }
        return out;
    }

    int checkPlugins (const std::vector<PluginInfo>& plugins, const std::atomic<bool>& cancel, int maxSeconds,
                      std::function<void (int, int)> progress)
    {
        const auto scanner = scannerProgram();
        if (! scanner.existsAsFile() || plugins.empty()) return 0;
        const auto start = juce::Time::getMillisecondCounterHiRes();
        int checked = 0;
        for (auto& p : plugins)
        {
            if (cancel.load() || (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0 > maxSeconds) break;
            if (progress) progress (checked, (int) plugins.size());

            const auto r = checkPlugin (p.file, scanner, cancel);
            if (cancel.load()) break;                 // stopped part-way: don't remember a half check
            auto* o = new juce::DynamicObject();
            o->setProperty ("stamp", stampFor (p.file));
            o->setProperty ("ok", r.ok);
            o->setProperty ("instrument", r.instrument);
            o->setProperty ("category", r.category);
            o->setProperty ("vendor", r.vendor);

            // read the file again each time: the plug-in and the app may both be checking
            auto cache = loadCache();
            cache.getDynamicObject()->setProperty (keyFor (p.file), juce::var (o));
            cacheFile().getParentDirectory().createDirectory();
            cacheFile().replaceWithText (juce::JSON::toString (cache));
            ++checked;
        }
        if (progress) progress (checked, (int) plugins.size());
        return checked;
    }
}
