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

juce::String SongSavantProcessor::dawName() const
{
    if (wrapperType != wrapperType_Standalone && wrapperType != wrapperType_Undefined)
    {
        const juce::PluginHostType host;
        if (host.isCubase())        return "Cubase";
        if (host.isNuendo())        return "Nuendo";
        if (host.isStudioOne())     return "Studio One";
        if (host.isReaper())        return "REAPER";
        if (host.isFruityLoops())   return "FL Studio";
        if (host.isAbletonLive())   return "Ableton Live";
        if (host.isBitwigStudio())  return "Bitwig Studio";
        if (host.isSonar())         return "Cakewalk";
        if (host.isLogic())         return "Logic Pro";
        return host.getHostDescription();
    }
    // the app on its own: the DAW installed on this computer
    if (locationProvider && ! locationProvider().cubaseInstalls.isEmpty()) return "Cubase";
    return {};
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

void SongSavantProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    // inside a DAW: its tempo, and whether it's playing and where
    ss::SketchPlayer::Host host;
    if (isInDaw())
        if (auto* ph = getPlayHead())
            if (const auto pos = ph->getPosition())
            {
                if (const auto bpm = pos->getBpm())
                {
                    host.known = true;
                    host.bpm = *bpm;
                    hostBpm = *bpm;
                }
                if (const auto ppq = pos->getPpqPosition()) host.ppq = *ppq;
                host.playing = pos->getIsPlaying() && pos->getPpqPosition().hasValue();
            }
    sketch.process (buffer, host);
}

void SongSavantProcessor::playSketch (SketchMode m)
{
    sketchMode = m;
    sketch.setSong (m == SketchMode::loop ? loopSketch : songSketch);
    sketch.play();
}

void SongSavantProcessor::updateSketch (SketchMode m, std::shared_ptr<const ss::SketchSong> s)
{
    (m == SketchMode::loop ? loopSketch : songSketch) = s;
    if (m == sketchMode) sketch.setSong (std::move (s));
}

juce::AudioProcessorEditor* SongSavantProcessor::createEditor()   { return new SongSavantEditor (*this); }

void SongSavantProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::XmlElement x ("SongSavant");
    x.setAttribute ("version", SS_VERSION);
    x.setAttribute ("page", page);
    x.addChildElement (song.toXml().release());
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
    page = juce::jlimit (0, 5, x->getIntAttribute ("page", 0));
    if (auto* s = x->getChildByName ("Song")) song.fromXml (*s);
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
