#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "core/Paths.h"

SongSavantProcessor::SongSavantProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    locationProvider = [] { return ss::SystemLocations::forThisComputer(); };
}

void SongSavantProcessor::startScan()
{
    auto loc = locationProvider();
    loc.extraFolders = extraFolders;
    scan.start (loc);
}

void SongSavantProcessor::startCatalog()
{
    catalog.start (locationProvider());
}

juce::String SongSavantProcessor::reportText() const
{
    auto r = scan.result();
    return r != nullptr ? ss::buildReport (*r, scan.locations(), dragTests) : juce::String();
}

juce::File SongSavantProcessor::saveReport()
{
    const auto text = reportText();
    if (text.isEmpty()) return {};
    auto f = ss::paths::reportFile();
    f.getParentDirectory().createDirectory();
    return f.replaceWithText (text) ? f : juce::File();
}

bool SongSavantProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void SongSavantProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
}

juce::AudioProcessorEditor* SongSavantProcessor::createEditor()   { return new SongSavantEditor (*this); }

void SongSavantProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::XmlElement x ("SongSavant");
    x.setAttribute ("version", SS_VERSION);
    x.setAttribute ("page", page);
    auto* d = x.createNewChildElement ("DragTests");
    d->setAttribute ("preset", dragTests.userPreset);
    d->setAttribute ("factory", dragTests.factoryPreset);
    d->setAttribute ("midi", dragTests.midiClip);
    for (auto& line : dragTests.fromDaw)
        d->createNewChildElement ("FromDaw")->setAttribute ("what", line);
    for (auto& f : extraFolders)
        x.createNewChildElement ("ExtraFolder")->setAttribute ("path", f.getFullPathName());
    copyXmlToBinary (x, dest);
}

void SongSavantProcessor::setStateInformation (const void* data, int size)
{
    auto x = getXmlFromBinary (data, size);
    if (x == nullptr || ! x->hasTagName ("SongSavant")) return;
    page = juce::jlimit (0, 1, x->getIntAttribute ("page", 0));
    if (auto* d = x->getChildByName ("DragTests"))
    {
        dragTests.userPreset    = juce::jlimit (-1, 1, d->getIntAttribute ("preset"));
        dragTests.factoryPreset = juce::jlimit (-1, 1, d->getIntAttribute ("factory"));
        dragTests.midiClip      = juce::jlimit (-1, 1, d->getIntAttribute ("midi"));
        dragTests.fromDaw.clear();
        for (auto* e : d->getChildWithTagNameIterator ("FromDaw"))
            dragTests.fromDaw.add (e->getStringAttribute ("what"));
    }
    extraFolders.clear();
    for (auto* e : x->getChildWithTagNameIterator ("ExtraFolder"))
        if (juce::File f (e->getStringAttribute ("path")); f.isDirectory())
            extraFolders.addIfNotAlreadyThere (f);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()   { return new SongSavantProcessor(); }
