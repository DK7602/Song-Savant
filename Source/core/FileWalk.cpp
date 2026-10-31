#include "FileWalk.h"
#include <vector>

namespace ss
{
    bool skipFolder (const juce::File& d)
    {
        const auto name = d.getFileName();
        if (name.startsWithChar ('.') || name.startsWithChar ('$') || looksPrivate (name)) return true;
        if (name.endsWithIgnoreCase (".vst3") || name.endsWithIgnoreCase (".component") || name.endsWithIgnoreCase (".vst")
            || name.endsWithIgnoreCase (".app") || name.endsWithIgnoreCase (".bundle") || name.endsWithIgnoreCase (".aaxplugin"))
            return true;
        for (auto* n : { "node_modules", "System Volume Information", "Windows", "WinSxS", "Temp", "Cache", "Caches", "GPUCache", "Code Cache",
                         "CrashDumps", "Crashpad", "Logs" })
            if (name.equalsIgnoreCase (n)) return true;
       #if JUCE_WINDOWS
        // Windows marks OneDrive folders as "reparse points" too, so links can't simply be skipped there;
        // skip the old compatibility junctions instead (they loop back on themselves). Depth limits do the rest.
        for (auto* n : { "Application Data", "My Documents", "My Music", "My Pictures", "My Videos", "Local Settings",
                         "NetHood", "PrintHood", "Recent", "SendTo", "Start Menu", "Templates", "Cookies",
                         "Default User", "All Users", "Documents and Settings" })
            if (name.equalsIgnoreCase (n)) return true;
        return false;
       #else
        return d.isSymbolicLink();
       #endif
    }

    WalkStats walkFiles (const juce::File& root, const WalkLimits& limits,
                         const std::function<bool (const juce::File&, int)>& onFile,
                         const std::atomic<bool>* cancel)
    {
        WalkStats st;
        if (! root.isDirectory()) return st;
        const auto start = juce::Time::getMillisecondCounterHiRes();

        std::vector<std::pair<juce::File, int>> stack { { root, 0 } };
        while (! stack.empty())
        {
            auto [dir, depth] = stack.back();
            stack.pop_back();
            ++st.folders;

            for (const auto& e : juce::RangedDirectoryIterator (dir, false, "*", juce::File::findFilesAndDirectories))
            {
                if (cancel != nullptr && cancel->load()) { st.truncated = true; return st; }
                const auto f = e.getFile();
                if (e.isDirectory())
                {
                    if (depth + 1 <= limits.maxDepth && ! skipFolder (f))
                        stack.push_back ({ f, depth + 1 });
                    else if (depth + 1 > limits.maxDepth)
                        st.truncated = true;
                    continue;
                }
                ++st.files;
                st.bytes += e.getFileSize();
                if (! onFile (f, depth)) { st.truncated = true; return st; }
                if (st.files >= limits.maxFiles
                    || (st.files % 256 == 0 && (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0 > limits.maxSeconds))
                {
                    st.truncated = true;
                    return st;
                }
            }
        }
        return st;
    }

    juce::String formatBytes (juce::int64 b)
    {
        if (b < 1024) return juce::String (b) + " B";
        if (b < 1024 * 1024) return juce::String ((double) b / 1024.0, 1) + " KB";
        if (b < (juce::int64) 1024 * 1024 * 1024) return juce::String ((double) b / (1024.0 * 1024.0), 1) + " MB";
        return juce::String ((double) b / (1024.0 * 1024.0 * 1024.0), 2) + " GB";
    }

    bool looksPrivate (const juce::String& name)
    {
        for (auto* w : { "secret", "password", "passwd", "credential", "token", "signing", "private", "licence", "license",
                         ".lic", ".jwt", ".pem", ".key", "keystore", "wallet" })
            if (name.containsIgnoreCase (w)) return true;
        return false;
    }
}
