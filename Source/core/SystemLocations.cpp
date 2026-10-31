#include "SystemLocations.h"
#include <algorithm>

namespace ss
{
    namespace
    {
        void addIfDir (juce::Array<juce::File>& list, const juce::File& f)
        {
            if (f != juce::File() && f.isDirectory() && ! list.contains (f))
                list.add (f);
        }

        struct WinDirs
        {
            juce::File home, documents, appData, localAppData, programData, programFiles, commonFiles, publicDocs;
        };

        SystemLocations fromWindowsDirs (const WinDirs& w)
        {
            SystemLocations s;
            s.home = w.home;

            addIfDir (s.vst3Folders, w.commonFiles.getChildFile ("VST3"));
            addIfDir (s.vst3Folders, w.localAppData.getChildFile ("Programs").getChildFile ("Common").getChildFile ("VST3"));

            for (auto* sub : { "VSTPlugins", "Steinberg/VSTPlugins", "Common Files/VST2", "Common Files/Steinberg/VST2", "VstPlugins" })
                addIfDir (s.vst2Folders, w.programFiles.getChildFile (sub));

            // Cubase installs (their own instruments, e.g. Groove Agent SE / Retrologue / Padshop, can live inside)
            auto steinbergPF = w.programFiles.getChildFile ("Steinberg");
            if (steinbergPF.isDirectory())
                for (const auto& e : juce::RangedDirectoryIterator (steinbergPF, false, "*", juce::File::findDirectories))
                    if (e.getFile().getFileName().startsWithIgnoreCase ("Cubase"))
                    {
                        s.cubaseInstalls.add (e.getFile());
                        addIfDir (s.vst3Folders, e.getFile().getChildFile ("VST3"));
                    }

            auto steinbergAD = w.appData.getChildFile ("Steinberg");
            if (steinbergAD.isDirectory())
                for (const auto& e : juce::RangedDirectoryIterator (steinbergAD, false, "*", juce::File::findDirectories))
                    if (e.getFile().getFileName().startsWithIgnoreCase ("Cubase"))
                        s.cubaseSettings.add (e.getFile());

            for (auto& base : { w.documents, w.appData, w.programData, w.commonFiles })
                addIfDir (s.presetRoots, base.getChildFile ("VST3 Presets"));

            for (auto& d : { w.documents, w.appData, w.localAppData, w.programData, w.programFiles, w.commonFiles, w.publicDocs })
                addIfDir (s.vendorRoots, d);

            s.mediaBayDb = w.localAppData.getChildFile ("Steinberg/Steinberg MediaBay Server/mediabay3.db");
            s.arturiaDb  = w.programData.getChildFile ("Arturia/Presets/db.db3");
            addIfDir (s.cymaticsRoots, w.programData.getChildFile ("Cymatics"));
            addIfDir (s.cymaticsRoots, w.appData.getChildFile ("Cymatics"));
            addIfDir (s.cymaticsRoots, w.documents.getChildFile ("Cymatics"));

            s.keyCompassSongs     = w.appData.getChildFile ("KeyCompass").getChildFile ("Songs");
            s.grooveGrabberKits   = w.documents.getChildFile ("Groove Grabber").getChildFile ("Kits");
            s.sampleSnaggerLibrary = w.documents.getChildFile ("Sample Snagger").getChildFile ("Samples");
            return s;
        }

        juce::File envDir (const char* name, const juce::File& fallback)
        {
            auto v = juce::SystemStats::getEnvironmentVariable (name, {});
            return v.isNotEmpty() ? juce::File (v) : fallback;
        }
    }

    SystemLocations SystemLocations::windowsLayoutUnder (const juce::File& root)
    {
        WinDirs w;
        w.home         = root.getChildFile ("Users/me");
        w.documents    = w.home.getChildFile ("Documents");
        w.appData      = w.home.getChildFile ("AppData/Roaming");
        w.localAppData = w.home.getChildFile ("AppData/Local");
        w.programData  = root.getChildFile ("ProgramData");
        w.programFiles = root.getChildFile ("Program Files");
        w.commonFiles  = w.programFiles.getChildFile ("Common Files");
        w.publicDocs   = root.getChildFile ("Users/Public/Documents");
        return fromWindowsDirs (w);
    }

    SystemLocations SystemLocations::forThisComputer()
    {
        using F = juce::File;
        const auto home = F::getSpecialLocation (F::userHomeDirectory);

       #if JUCE_WINDOWS
        WinDirs w;
        w.home         = home;
        w.documents    = F::getSpecialLocation (F::userDocumentsDirectory);
        w.appData      = F::getSpecialLocation (F::userApplicationDataDirectory);
        w.localAppData = envDir ("LOCALAPPDATA", home.getChildFile ("AppData/Local"));
        w.programData  = F::getSpecialLocation (F::commonApplicationDataDirectory);
        w.programFiles = envDir ("ProgramW6432", F::getSpecialLocation (F::globalApplicationsDirectory));
        w.commonFiles  = envDir ("CommonProgramW6432", w.programFiles.getChildFile ("Common Files"));
        w.publicDocs   = envDir ("PUBLIC", F ("C:\\Users\\Public")).getChildFile ("Documents");
        auto s = fromWindowsDirs (w);

        // makers' folders moved to the top of a drive (D:\Steinberg, E:\Arturia ...): only names are compared
        juce::Array<F> drives;
        F::findFileSystemRoots (drives);
        for (auto& d : drives)
            if (d.isOnHardDisk() && d.isDirectory())
                addIfDir (s.driveRoots, d);
        return s;
       #else
        SystemLocations s;
        s.home = home;
        const auto docs = F::getSpecialLocation (F::userDocumentsDirectory);
        const auto appData = F::getSpecialLocation (F::userApplicationDataDirectory);   // Mac: ~/Library
       #if JUCE_MAC
        addIfDir (s.vst3Folders, F ("/Library/Audio/Plug-Ins/VST3"));
        addIfDir (s.vst3Folders, home.getChildFile ("Library/Audio/Plug-Ins/VST3"));
        addIfDir (s.vst2Folders, F ("/Library/Audio/Plug-Ins/VST"));
        addIfDir (s.vst2Folders, home.getChildFile ("Library/Audio/Plug-Ins/VST"));
        addIfDir (s.auFolders, F ("/Library/Audio/Plug-Ins/Components"));
        addIfDir (s.auFolders, home.getChildFile ("Library/Audio/Plug-Ins/Components"));
        addIfDir (s.presetRoots, home.getChildFile ("Library/Audio/Presets"));
        addIfDir (s.presetRoots, F ("/Library/Audio/Presets"));
        addIfDir (s.presetRoots, F ("/Network/Library/Audio/Presets"));
        for (auto& d : { docs, home.getChildFile ("Music"), home.getChildFile ("Library"), home.getChildFile ("Library/Application Support"),
                         F ("/Library"), F ("/Library/Application Support"), F ("/Users/Shared") })
            addIfDir (s.vendorRoots, d);
        if (F apps ("/Applications"); apps.isDirectory())
            for (const auto& e : juce::RangedDirectoryIterator (apps, false, "Cubase*", F::findFilesAndDirectories))
                s.cubaseInstalls.add (e.getFile());
        if (auto prefs = home.getChildFile ("Library/Preferences"); prefs.isDirectory())
            for (const auto& e : juce::RangedDirectoryIterator (prefs, false, "Cubase*", F::findDirectories))
                s.cubaseSettings.add (e.getFile());
        s.keyCompassSongs = appData.getChildFile ("KeyCompass/Songs");
        s.mediaBayDb = home.getChildFile ("Library/Application Support/Steinberg/Steinberg MediaBay Server/mediabay3.db");
        s.arturiaDb  = F ("/Library/Arturia/Presets/db.db3");
        addIfDir (s.cymaticsRoots, F ("/Library/Application Support/Cymatics"));
        addIfDir (s.cymaticsRoots, home.getChildFile ("Library/Application Support/Cymatics"));
        addIfDir (s.cymaticsRoots, docs.getChildFile ("Cymatics"));
       #else
        addIfDir (s.vst3Folders, home.getChildFile (".vst3"));
        addIfDir (s.vst3Folders, F ("/usr/lib/vst3"));
        addIfDir (s.vst3Folders, F ("/usr/local/lib/vst3"));
        addIfDir (s.vst2Folders, home.getChildFile (".vst"));
        addIfDir (s.vst2Folders, F ("/usr/lib/vst"));
        addIfDir (s.presetRoots, home.getChildFile (".vst3/presets"));
        addIfDir (s.presetRoots, F ("/usr/share/vst3/presets"));
        addIfDir (s.presetRoots, F ("/usr/local/share/vst3/presets"));
        for (auto& d : { docs, home.getChildFile (".local/share"), home.getChildFile (".config"), F ("/usr/share"), F ("/opt") })
            addIfDir (s.vendorRoots, d);
        s.keyCompassSongs = appData.getChildFile ("KeyCompass/Songs");
       #endif
        juce::ignoreUnused (envDir);
        s.grooveGrabberKits    = docs.getChildFile ("Groove Grabber/Kits");
        s.sampleSnaggerLibrary = docs.getChildFile ("Sample Snagger/Samples");
        return s;
       #endif
    }

    juce::String SystemLocations::tidy (const juce::String& text) const
    {
        const auto h = home.getFullPathName();
        if (h.length() <= 3) return text;
        juce::StringArray forms { h, h.replaceCharacter ('\\', '/'), h.replaceCharacter ('/', '\\') };
        if (h.length() > 2 && h[1] == ':')                     // "C:\Users\name" -> also "\Users\name" and "/Users/name"
            for (auto f : juce::StringArray (forms))
                forms.add (f.substring (2));
        forms.removeDuplicates (true);
        std::sort (forms.begin(), forms.end(), [] (const juce::String& x, const juce::String& y) { return x.length() > y.length(); });
        auto out = text;
        for (auto& f : forms)
            if (f.length() > 3) out = out.replace (f, "~", true);
        return out;
    }

    juce::String SystemLocations::tidy (const juce::File& f) const   { return tidy (f.getFullPathName()); }
}
