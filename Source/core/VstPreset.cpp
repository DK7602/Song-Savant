#include "VstPreset.h"
#include <algorithm>

namespace ss
{
    namespace
    {
        constexpr int headerSize = 48;

        juce::int64 readI64 (const juce::uint8* p)
        {
            juce::uint64 v = 0;
            for (int i = 7; i >= 0; --i) v = (v << 8) | p[i];
            return (juce::int64) v;
        }
        juce::int32 readI32 (const juce::uint8* p)
        {
            return (juce::int32) ((juce::uint32) p[0] | ((juce::uint32) p[1] << 8) | ((juce::uint32) p[2] << 16) | ((juce::uint32) p[3] << 24));
        }
        bool isHex (juce::uint8 c)
        {
            return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
        }
        juce::String fourCC (const juce::uint8* p)
        {
            juce::String s;
            for (int i = 0; i < 4; ++i) s << (p[i] >= 32 && p[i] < 127 ? juce::String::charToString ((juce::juce_wchar) p[i]) : juce::String ("?"));
            return s;
        }

        // Reads `size` bytes at `offset` from either a memory block or a stream
        struct Source
        {
            const juce::uint8* data = nullptr;
            size_t size = 0;
            juce::InputStream* stream = nullptr;

            juce::int64 total() const { return stream != nullptr ? stream->getTotalLength() : (juce::int64) size; }

            bool read (juce::int64 offset, void* dest, size_t n) const
            {
                if (offset < 0 || offset + (juce::int64) n > total()) return false;
                if (stream != nullptr)
                    return stream->setPosition (offset) && stream->read (dest, (int) n) == (int) n;
                std::memcpy (dest, data + offset, n);
                return true;
            }
        };

        VstPresetInfo parse (const Source& src)
        {
            VstPresetInfo info;
            juce::uint8 h[headerSize];
            if (! src.read (0, h, headerSize) || std::memcmp (h, "VST3", 4) != 0)
            {
                info.error = "not a VST3 preset";
                return info;
            }
            for (int i = 8; i < 40; ++i)
                if (! isHex (h[i])) { info.error = "bad class ID"; return info; }
            info.classId = juce::String ((const char*) h + 8, 32);

            const auto listOffset = readI64 (h + 40);
            juce::uint8 lh[8];
            if (listOffset < headerSize || ! src.read (listOffset, lh, 8) || std::memcmp (lh, "List", 4) != 0)
            {
                info.error = "no chunk list";
                return info;
            }
            const int count = readI32 (lh + 4);
            if (count < 0 || count > 64) { info.error = "bad chunk list"; return info; }
            info.valid = true;
            info.totalSize = listOffset + 8 + (juce::int64) count * 20;

            for (int i = 0; i < count; ++i)
            {
                juce::uint8 e[20];
                if (! src.read (listOffset + 8 + i * 20, e, 20)) { info.error = "short chunk list"; info.valid = false; break; }
                const auto id = fourCC (e);
                info.chunkIds.add (id);
                const auto chunkStart = readI64 (e + 4), chunkSize = readI64 (e + 12);
                if (chunkStart < 0 || chunkSize < 0 || chunkStart + chunkSize > src.total())
                {
                    info.error = "a chunk is outside the file";
                    info.valid = false;
                    break;
                }
                info.totalSize = std::max (info.totalSize, chunkStart + chunkSize);
                if (id == "Info")
                {
                    const auto off = readI64 (e + 4), sz = readI64 (e + 12);
                    if (sz > 0 && sz <= 1024 * 1024)
                    {
                        juce::MemoryBlock mb ((size_t) sz);
                        if (src.read (off, mb.getData(), (size_t) sz))
                        {
                            info.attributes = parseMetaInfo (juce::String::fromUTF8 ((const char*) mb.getData(), (int) sz));
                            info.hasInfo = true;
                        }
                    }
                }
            }
            return info;
        }
    }

    VstPresetInfo readVstPreset (const juce::File& f)
    {
        juce::FileInputStream in (f);
        if (! in.openedOk())
        {
            VstPresetInfo info;
            info.error = "couldn't open";
            return info;
        }
        Source src;
        src.stream = &in;
        return parse (src);
    }

    VstPresetInfo parseVstPreset (const juce::uint8* data, size_t size)
    {
        Source src;
        src.data = data;
        src.size = size;
        return parse (src);
    }

    juce::StringPairArray parseMetaInfo (const juce::String& xmlText)
    {
        juce::StringPairArray out;
        auto start = xmlText.indexOf ("<MetaInfo");
        if (start < 0) return out;
        auto xml = juce::XmlDocument::parse (xmlText.substring (start));
        if (xml == nullptr) return out;
        for (auto* a : xml->getChildWithTagNameIterator ("Attr"))
        {
            const auto id = a->getStringAttribute ("id");
            if (id.isNotEmpty())
                out.set (id, a->getStringAttribute ("value"));
        }
        return out;
    }

    juce::String makeMetaInfoXml (const juce::StringPairArray& attrs)
    {
        juce::XmlElement root ("MetaInfo");
        for (int i = 0; i < attrs.size(); ++i)
        {
            auto* a = root.createNewChildElement ("Attr");
            a->setAttribute ("id", attrs.getAllKeys()[i]);
            a->setAttribute ("value", attrs.getAllValues()[i]);
            a->setAttribute ("type", "string");
            a->setAttribute ("flags", "writeProtected");
        }
        return root.toString();
    }

    juce::MemoryBlock makeVstPreset (const juce::String& classId, const juce::MemoryBlock& comp, const juce::StringPairArray& attrs)
    {
        juce::MemoryOutputStream out;
        const auto info = makeMetaInfoXml (attrs).toStdString();

        out.write ("VST3", 4);
        out.writeInt (1);
        out.write (classId.paddedRight ('0', 32).substring (0, 32).toRawUTF8(), 32);
        const juce::int64 compOffset = headerSize;
        const juce::int64 infoOffset = compOffset + (juce::int64) comp.getSize();
        const juce::int64 listOffset = infoOffset + (juce::int64) info.size();
        out.writeInt64 (listOffset);
        out.write (comp.getData(), comp.getSize());
        out.write (info.data(), info.size());
        out.write ("List", 4);
        out.writeInt (2);
        out.write ("Comp", 4); out.writeInt64 (compOffset); out.writeInt64 ((juce::int64) comp.getSize());
        out.write ("Info", 4); out.writeInt64 (infoOffset); out.writeInt64 ((juce::int64) info.size());
        return out.getMemoryBlock();
    }

    const juce::StringArray& soundTagKeys()
    {
        static const juce::StringArray keys { "MusicalCategory", "MusicalInstrument", "MusicalStyle", "MusicalCharacter" };
        return keys;
    }
}
