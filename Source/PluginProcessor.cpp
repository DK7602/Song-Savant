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
    return r != nullptr ? ss::buildReport (*r, scan.locations()) : juce::String();
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
    sketch.setSong (sketchFor (m));
    applySolo();
    sketch.play (m == SketchMode::song ? juce::jlimit (0.0, songLength(), songCursor) : 0.0);
}

void SongSavantProcessor::updateSketch (SketchMode m, std::shared_ptr<const ss::SketchSong> s)
{
    sketchFor (m) = s;
    if (m == sketchMode)
    {
        sketch.setSong (std::move (s));
        applySolo();
    }
}

namespace
{
    int partIndex (const ss::SketchSong* s, const juce::String& role)
    {
        if (s != nullptr)
            for (size_t i = 0; i < s->parts.size(); ++i)
                if (s->parts[i].role == role) return (int) i;
        return -1;
    }
}

bool SongSavantProcessor::sketchHasPart (const juce::String& role) const
{
    return role.isNotEmpty() && partIndex (loopSketch.get(), role) >= 0;
}

bool SongSavantProcessor::setSketchSolo (const juce::String& role)
{
    if (role.isNotEmpty() && ! sketchHasPart (role)) return false;
    soloRole = role;
    applySolo();
    return true;
}

void SongSavantProcessor::applySolo()
{
    // only the Song page plays one part on its own; the whole song always plays every part
    int i = -1;
    if (sketchMode == SketchMode::loop && soloRole.isNotEmpty())
    {
        i = partIndex (loopSketch.get(), soloRole);
        if (i < 0) soloRole.clear();             // that card has no part now (a new palette): every part
    }
    if (sketchMode == SketchMode::section && sectionSolo.isNotEmpty())
        i = partIndex (sectionSketch.get(), sectionSolo);
    sketch.setSolo (i);
}

void SongSavantProcessor::songPlay()
{
    if (songCursor >= songLength() - 1.0e-6) songCursor = 0.0;      // at the end: from the top
    playSketch (SketchMode::song);
}

void SongSavantProcessor::songPause()
{
    if (! isSketchPlaying (SketchMode::song)) return;
    songCursor = juce::jlimit (0.0, songLength(), songPosition());
    sketch.stop();
}

void SongSavantProcessor::songMoveTo (double beats)
{
    songCursor = juce::jlimit (0.0, songLength(), beats);
    if (isSketchPlaying (SketchMode::song) && ! sketch.isFollowingHost())
    {
        if (songCursor >= songLength()) sketch.stop();             // to the end while it plays: the song's over
        else sketch.play (songCursor);
    }
}

void SongSavantProcessor::songStep (int direction)
{
    const double bar = songPosition() / 4.0;
    double to = std::floor (bar + 1.0e-6);
    if (direction > 0) to += 1.0;
    else if (bar - to < 0.25) to -= 1.0;           // just past a bar line (or on it): the bar before
    songMoveTo (juce::jmax (0.0, to) * 4.0);
}

double SongSavantProcessor::songPosition() const
{
    if (isSketchPlaying (SketchMode::song))
        if (const double p = sketch.getPosition(); p >= 0.0) return juce::jmin (p, songLength());
    return juce::jlimit (0.0, songLength(), songCursor);
}

juce::AudioProcessorEditor* SongSavantProcessor::createEditor()   { return new SongSavantEditor (*this); }

void SongSavantProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::XmlElement x ("SongSavant");
    x.setAttribute ("version", SS_VERSION);
    x.setAttribute ("page", page);
    x.addChildElement (song.toXml().release());
    for (auto& f : extraFolders)
        x.createNewChildElement ("ExtraFolder")->setAttribute ("path", f.getFullPathName());
    copyXmlToBinary (x, dest);
}

void SongSavantProcessor::setStateInformation (const void* data, int size)
{
    auto x = getXmlFromBinary (data, size);
    if (x == nullptr || ! x->hasTagName ("SongSavant")) return;
    page = juce::jlimit (0, 6, x->getIntAttribute ("page", 0));
    if (auto* s = x->getChildByName ("Song")) song.fromXml (*s);
    // (projects saved before 1.0.2 also hold the drag test's answers, "DragTests": no longer used)
    extraFolders.clear();
    for (auto* e : x->getChildWithTagNameIterator ("ExtraFolder"))
        if (juce::File f (e->getStringAttribute ("path")); f.isDirectory())
            extraFolders.addIfNotAlreadyThere (f);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()   { return new SongSavantProcessor(); }
