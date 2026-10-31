#include "PluginScanner.h"
#include "FileWalk.h"
#include <algorithm>

namespace ss
{
    juce::var parseLenientJson (const juce::String& text)
    {
        auto v = juce::JSON::parse (text);
        if (! v.isVoid()) return v;

        // strip comments and trailing commas (outside strings), then try again
        juce::String out;
        out.preallocateBytes ((size_t) text.getNumBytesAsUTF8() + 8);
        bool inString = false, escaped = false;
        auto p = text.getCharPointer();
        while (! p.isEmpty())
        {
            const auto c = *p;
            if (inString)
            {
                out << juce::String::charToString (c);
                if (escaped) escaped = false;
                else if (c == '\\') escaped = true;
                else if (c == '"') inString = false;
                ++p;
                continue;
            }
            if (c == '"') { inString = true; out << "\""; ++p; continue; }
            if (c == '/' && *(p + 1) == '/') { while (! p.isEmpty() && *p != '\n') ++p; continue; }
            if (c == '/' && *(p + 1) == '*')
            {
                p += 2;
                while (! p.isEmpty() && ! (*p == '*' && *(p + 1) == '/')) ++p;
                if (! p.isEmpty()) p += 2;
                continue;
            }
            if (c == ',')
            {
                auto q = p + 1;
                while (! q.isEmpty() && juce::CharacterFunctions::isWhitespace (*q)) ++q;
                if (*q == '}' || *q == ']') { ++p; continue; }
            }
            out << juce::String::charToString (c);
            ++p;
        }
        return juce::JSON::parse (out);
    }

    bool readModuleInfo (const juce::File& f, PluginInfo& info)
    {
        if (! f.existsAsFile() || f.getSize() > 4 * 1024 * 1024) return false;
        const auto v = parseLenientJson (f.loadFileAsString());
        if (! v.isObject()) return false;

        if (auto n = v["Name"].toString(); n.isNotEmpty()) info.name = n;
        if (auto ver = v["Version"].toString(); ver.isNotEmpty()) info.version = ver;
        info.vendor = v["Factory Info"]["Vendor"].toString();

        if (auto* classes = v["Classes"].getArray())
            for (auto& c : *classes)
            {
                if (! c["Category"].toString().equalsIgnoreCase ("Audio Module Class")) continue;
                if (info.vendor.isEmpty()) info.vendor = c["Vendor"].toString();
                if (auto* subs = c["Sub Categories"].getArray())
                    for (auto& s : *subs)
                        info.categories.addIfNotAlreadyThere (s.toString());
                info.categoriesKnown = true;
            }
        info.isInstrument = info.categories.contains ("Instrument");
        return true;
    }

    namespace
    {
        void addVst3 (std::vector<PluginInfo>& out, const juce::File& f, const juce::File& root)
        {
            PluginInfo p;
            p.format = "VST3";
            p.file = f;
            p.name = f.getFileNameWithoutExtension();
            p.bundle = f.isDirectory();
            if (p.bundle)
                readModuleInfo (f.getChildFile ("Contents").getChildFile ("moduleinfo.json"), p);
            if (p.vendor.isEmpty() && f.getParentDirectory() != root)
                p.vendor = f.getParentDirectory().getFileName();   // makers often use a sub-folder
            out.push_back (p);
        }

        void collect (const juce::File& dir, const juce::File& root, int depth, std::vector<PluginInfo>& out,
                      const juce::String& ext, const juce::String& format, const std::atomic<bool>* cancel)
        {
            if (depth > 4 || (cancel != nullptr && cancel->load())) return;
            for (const auto& e : juce::RangedDirectoryIterator (dir, false, "*", juce::File::findFilesAndDirectories))
            {
                const auto f = e.getFile();
                if (f.getFileName().endsWithIgnoreCase (ext))
                {
                    if (format == "VST3") addVst3 (out, f, root);
                    else
                    {
                        PluginInfo p;
                        p.format = format;
                        p.file = f;
                        p.name = f.getFileNameWithoutExtension();
                        p.bundle = f.isDirectory();
                        if (f.getParentDirectory() != root) p.vendor = f.getParentDirectory().getFileName();
                        out.push_back (p);
                    }
                }
                else if (e.isDirectory() && ! skipFolder (f))
                    collect (f, root, depth + 1, out, ext, format, cancel);
            }
        }
    }

    std::vector<PluginInfo> scanInstalledPlugins (const SystemLocations& loc, const std::atomic<bool>* cancel)
    {
        std::vector<PluginInfo> out;
        for (auto& d : loc.vst3Folders) collect (d, d, 0, out, ".vst3", "VST3", cancel);
       #if JUCE_MAC
        const juce::String vst2Ext = ".vst";
       #elif JUCE_WINDOWS
        const juce::String vst2Ext = ".dll";
       #else
        const juce::String vst2Ext = ".so";
       #endif
        for (auto& d : loc.vst2Folders) collect (d, d, 0, out, vst2Ext, "VST2", cancel);
        for (auto& d : loc.auFolders)   collect (d, d, 0, out, ".component", "AU", cancel);

        // the same plug-in can sit in two folders: keep the first of each format + name
        std::vector<PluginInfo> unique;
        for (auto& p : out)
        {
            bool dup = false;
            for (auto& u : unique)
                if (u.format == p.format && u.name.equalsIgnoreCase (p.name)) { dup = true; break; }
            if (! dup) unique.push_back (p);
        }
        std::sort (unique.begin(), unique.end(), [] (const PluginInfo& a, const PluginInfo& b)
        {
            if (a.format != b.format) return a.format > b.format;   // VST3 first
            return a.name.compareIgnoreCase (b.name) < 0;
        });
        return unique;
    }
}
