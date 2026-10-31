#include "VstSoundProbe.h"
#include "VstPreset.h"
#include <cstring>

namespace ss
{
    namespace
    {
        constexpr juce::int64 wholeLimit = 64ll * 1024 * 1024;
        constexpr juce::int64 edgeBytes = 8ll * 1024 * 1024;
        constexpr int maxSamples = 8;

        template <typename Fn>
        void forEachMatch (const juce::uint8* d, size_t n, const void* pat, size_t pn, Fn&& fn)
        {
            if (pn == 0 || n < pn) return;
            const auto first = static_cast<const juce::uint8*> (pat)[0];
            size_t i = 0;
            while (i + pn <= n)
            {
                auto* hit = static_cast<const juce::uint8*> (std::memchr (d + i, first, n - pn + 1 - i));
                if (hit == nullptr) return;
                const auto pos = (size_t) (hit - d);
                if (std::memcmp (hit, pat, pn) == 0) fn (pos);
                i = pos + 1;
            }
        }

        bool printable (juce::uint8 c) { return c >= 32 && c < 127; }

        void addSample (juce::StringArray& list, const juce::String& s)
        {
            if (s.isNotEmpty() && list.size() < maxSamples && ! list.contains (s)) list.add (s);
        }

        void noteTags (VstSoundProbeResult& r, const juce::StringPairArray& a)
        {
            if (auto p = a["PlugInName"]; p.isNotEmpty()) r.plugins[p]++;
            if (auto c = a["MusicalCategory"]; c.isNotEmpty()) r.categories[c]++;
            juce::StringArray parts;
            for (auto& k : soundTagKeys())
                if (a[k].isNotEmpty()) parts.add (a[k]);
            if (! parts.isEmpty()) addSample (r.sampleTags, parts.joinIntoString (" / "));
        }

        void scanRegion (VstSoundProbeResult& r, const juce::uint8* d, size_t n, juce::int64 base)
        {
            // ".vstpreset" names, ASCII
            forEachMatch (d, n, ".vstpreset", 10, [&] (size_t pos)
            {
                ++r.presetNames;
                size_t s = pos;
                while (s > 0 && pos - s < 160 && printable (d[s - 1]) && d[s - 1] != '/' && d[s - 1] != '\\') --s;
                addSample (r.sampleNames, juce::String ((const char*) d + s, (size_t) (pos - s)));
            });
            // ".vstpreset" names, UTF-16LE
            static const char u16[] = { '.',0,'v',0,'s',0,'t',0,'p',0,'r',0,'e',0,'s',0,'e',0,'t',0 };
            forEachMatch (d, n, u16, sizeof (u16), [&] (size_t pos)
            {
                ++r.presetNames;
                size_t s = pos;
                while (s >= 2 && pos - s < 320 && d[s - 1] == 0 && printable (d[s - 2]) && d[s - 2] != '/' && d[s - 2] != '\\') s -= 2;
                juce::String name;
                for (size_t i = s; i < pos; i += 2) name << juce::String::charToString ((juce::juce_wchar) d[i]);
                addSample (r.sampleNames, name);
            });
            // loose MetaInfo XML
            forEachMatch (d, n, "<MetaInfo", 9, [&] (size_t pos)
            {
                ++r.metaInfoBlocks;
                const size_t span = std::min<size_t> (n - pos, 32 * 1024);
                size_t end = 0;
                forEachMatch (d + pos, span, "</MetaInfo>", 11, [&] (size_t e) { if (end == 0) end = e + 11; });
                if (end > 0)
                    noteTags (r, parseMetaInfo (juce::String::fromUTF8 ((const char*) d + pos, (int) end)));
            });
            // embedded VST3 presets
            forEachMatch (d, n, "VST3", 4, [&] (size_t pos)
            {
                if (pos + 48 > n) return;
                for (size_t i = pos + 8; i < pos + 40; ++i)
                {
                    const auto c = d[i];
                    if (! ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) return;
                }
                ++r.embeddedPresets;
                auto info = parseVstPreset (d + pos, n - pos);
                if (info.valid && info.hasInfo)
                {
                    ++r.embeddedWithTags;
                    noteTags (r, info.attributes);
                    if (r.example.offset < 0 && info.totalSize > 48 && info.totalSize < 32ll * 1024 * 1024)
                    {
                        r.example.offset = base + (juce::int64) pos;
                        r.example.size = info.totalSize;
                        r.example.plugin = info.attributes["PlugInName"];
                        juce::StringArray bits;
                        for (auto& k : soundTagKeys())
                            if (info.attributes[k].isNotEmpty()) bits.add (info.attributes[k].upToFirstOccurrenceOf ("|", false, false));
                        bits.removeDuplicates (true);
                        r.example.label = bits.joinIntoString (" ");
                    }
                }
            });
        }
    }

    VstSoundProbeResult probeVstSound (const juce::File& f, juce::int64& budget)
    {
        VstSoundProbeResult r;
        r.file = f;
        r.size = f.getSize();

        juce::FileInputStream in (f);
        if (! in.openedOk()) return r;

        juce::uint8 head[32] = {};
        const int got = in.read (head, 32);
        for (int i = 0; i < got; ++i)
        {
            r.headerHex << juce::String::toHexString ((int) head[i]).paddedLeft ('0', 2) << (i + 1 < got ? " " : "");
            r.headerText << (printable (head[i]) ? juce::String::charToString ((juce::juce_wchar) head[i]) : juce::String ("."));
        }

        // readable text in the first 256 KB: names, paths and keys in the container's index, if it has one
        {
            juce::MemoryBlock start ((size_t) std::min<juce::int64> (r.size, 256 * 1024));
            if (in.setPosition (0) && in.read (start.getData(), (int) start.getSize()) > 0)
            {
                auto* d = static_cast<const juce::uint8*> (start.getData());
                size_t runStart = 0;
                for (size_t i = 0; i <= start.getSize() && r.textAtStart.size() < 12; ++i)
                {
                    if (i < start.getSize() && printable (d[i])) continue;
                    if (i - runStart >= 10)
                        r.textAtStart.addIfNotAlreadyThere (juce::String ((const char*) d + runStart, std::min<size_t> (i - runStart, 120)));
                    runStart = i + 1;
                }
            }
        }

        auto readAt = [&] (juce::int64 offset, juce::int64 count)
        {
            count = std::min (count, budget);
            if (count <= 0) return;
            juce::MemoryBlock mb ((size_t) count);
            if (! in.setPosition (offset)) return;
            const auto n = in.read (mb.getData(), (int) count);
            if (n <= 0) return;
            budget -= n;
            r.bytesScanned += n;
            scanRegion (r, static_cast<const juce::uint8*> (mb.getData()), (size_t) n, offset);
        };

        if (r.size <= wholeLimit)
        {
            readAt (0, r.size);
            r.scannedWhole = r.bytesScanned == r.size;
        }
        else
        {
            readAt (0, edgeBytes);
            readAt (r.size - edgeBytes, edgeBytes);
        }
        return r;
    }

    bool extractEmbeddedPreset (const juce::File& container, juce::int64 offset, juce::int64 size, const juce::File& dest)
    {
        juce::FileInputStream in (container);
        if (! in.openedOk() || offset < 0 || size <= 0 || ! in.setPosition (offset)) return false;
        juce::MemoryBlock mb ((size_t) size);
        if (in.read (mb.getData(), (int) size) != (int) size) return false;
        if (! parseVstPreset (static_cast<const juce::uint8*> (mb.getData()), mb.getSize()).valid) return false;
        dest.getParentDirectory().createDirectory();
        return dest.replaceWithData (mb.getData(), mb.getSize());
    }
}
