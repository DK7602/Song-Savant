#include "VendorProbe.h"
#include <algorithm>

namespace ss
{
    juce::String normaliseName (const juce::String& s)
    {
        return s.toLowerCase().retainCharacters ("abcdefghijklmnopqrstuvwxyz0123456789");
    }

    namespace
    {
        bool sameMaker (const juce::String& folderName, const juce::String& vendor)
        {
            const auto a = normaliseName (folderName), b = normaliseName (vendor);
            if (a.isEmpty() || b.isEmpty()) return false;
            if (a == b) return true;
            return std::min (a.length(), b.length()) >= 5 && (a.startsWith (b) || b.startsWith (a));
        }

        bool isDatabaseName (const juce::File& f)
        {
            const auto ext = f.getFileExtension().toLowerCase();
            return ext == ".db" || ext == ".db3" || ext == ".sqlite" || ext == ".sqlite3" || ext == ".sdb"
                || f.getFileName().containsIgnoreCase ("mediabay");
        }
    }

    juce::StringArray vendorsToLookFor (const std::vector<PluginInfo>& plugins)
    {
        juce::StringArray v { "Steinberg", "Arturia", "Cymatics", "Native Instruments", "Steven Slate Drums", "Slate Digital",
                              "Plugin Boutique", "Scaler", "Manda Audio", "MT Power Drum Kit", "Vital", "Spectrasonics",
                              "Toontrack", "Xfer Records", "u-he", "Output", "Spitfire Audio", "IK Multimedia", "Korg",
                              "Roland", "Reveal Sound", "Image-Line", "BandLab", "Cakewalk" };
        for (auto& p : plugins)
            if (p.vendor.isNotEmpty()) v.add (p.vendor);

        // one entry per maker
        juce::StringArray out, keys;
        for (auto& name : v)
        {
            const auto k = normaliseName (name);
            bool dup = k.isEmpty();
            for (auto& existing : keys)
                if (sameMaker (existing, k)) { dup = true; break; }
            if (! dup) { out.add (name); keys.add (k); }
        }
        return out;
    }

    juce::Array<juce::File> findVendorFolders (const SystemLocations& loc, const juce::String& vendor)
    {
        juce::Array<juce::File> out;
        auto consider = [&] (const juce::File& root, bool exact)
        {
            for (const auto& e : juce::RangedDirectoryIterator (root, false, "*", juce::File::findDirectories))
            {
                const auto name = e.getFile().getFileName();
                if (looksPrivate (name) || out.contains (e.getFile())) continue;
                if (exact ? normaliseName (name) == normaliseName (vendor) : sameMaker (name, vendor))
                    out.add (e.getFile());
            }
        };
        for (auto& root : loc.vendorRoots) consider (root, false);
        for (auto& root : loc.driveRoots)  consider (root, true);    // D:\Steinberg yes, D:\Steinberg Backup no
        return out;
    }

    VendorFolder summariseFolder (const juce::String& vendor, const juce::File& folder, FoundFiles& found,
                                  const std::atomic<bool>* cancel, const WalkLimits& limits)
    {
        VendorFolder vf;
        vf.vendor = vendor;
        vf.folder = folder;

        std::map<juce::String, int> extCount, subCount;
        std::map<juce::String, juce::StringArray> samples;

        vf.stats = walkFiles (folder, limits, [&] (const juce::File& f, int)
        {
            if (looksPrivate (f.getFileName())) { extCount["(private, not listed)"]++; return true; }
            auto ext = f.getFileExtension().toLowerCase();
            if (ext.isEmpty() || ext.length() > 16) ext = "(none)";
            extCount[ext]++;
            auto rel = f.getRelativePathFrom (folder);
            auto& s = samples[ext];
            if (s.size() < 3) s.add (rel);
            const auto sep = rel.indexOfAnyOf ("/\\");
            subCount[sep > 0 ? rel.substring (0, sep) : juce::String ("(files at the top)")]++;

            if (ext == ".vstpreset") found.addUnique (found.vstPresets, f);
            else if (ext == ".vstsound") found.addUnique (found.vstSounds, f);
            else if (isDatabaseName (f)) found.addUnique (found.databases, f);
            return true;
        }, cancel);

        vf.extensions.assign (extCount.begin(), extCount.end());
        std::sort (vf.extensions.begin(), vf.extensions.end(), [] (auto& a, auto& b) { return a.second > b.second; });
        for (size_t i = 0; i < vf.extensions.size() && i < 14; ++i)
            vf.samples[vf.extensions[i].first] = samples[vf.extensions[i].first];

        for (auto& [name, n] : subCount)
            vf.subfolders.push_back ({ name, n });
        std::sort (vf.subfolders.begin(), vf.subfolders.end(), [] (auto& a, auto& b) { return a.second > b.second; });
        return vf;
    }
}
