#include "SketchPlayer.h"
#include <algorithm>
#include <cmath>

namespace ss
{
    namespace
    {
        void addNotes (SketchSong& s, juce::MidiMessageSequence seq, int part)
        {
            seq.updateMatchedPairs();
            for (int i = 0; i < seq.getNumEvents(); ++i)
            {
                const auto* e = seq.getEventPointer (i);
                if (! e->message.isNoteOn()) continue;
                const double on = e->message.getTimeStamp();
                const double off = e->noteOffObject != nullptr ? e->noteOffObject->message.getTimeStamp() : on + ticksPerQuarter / 4.0;
                SketchNote n;
                n.start = on / ticksPerQuarter;
                n.length = juce::jmax (0.02, (off - on) / ticksPerQuarter);
                n.note = e->message.getNoteNumber();
                n.velocity = e->message.getVelocity();
                n.part = part;
                s.notes.push_back (n);
            }
        }

        void sortNotes (SketchSong& s)
        {
            std::stable_sort (s.notes.begin(), s.notes.end(), [] (const SketchNote& a, const SketchNote& b) { return a.start < b.start; });
        }

        constexpr double twoPi = juce::MathConstants<double>::twoPi;
    }

    SketchSong sketchOfParts (const PartsPlan& plan, const std::vector<ArrangementPart>& parts)
    {
        SketchSong s;
        s.bpm = plan.bpm;
        s.loop = true;
        s.lengthBeats = juce::jmax (1, plan.bars) * 4.0;
        for (auto& p : parts)
        {
            if (p.type == PartType::none) continue;
            s.parts.push_back ({ p.label, p.type, p.instrument, p.role });
            addNotes (s, makePart (plan, p.type, generalMidiDrums(), p.instrument), (int) s.parts.size() - 1);
        }
        sortNotes (s);
        return s;
    }

    SketchSong sketchOfSong (const PartsPlan& plan, const Arrangement& a, const std::vector<ArrangementPart>& parts)
    {
        SketchSong s;
        s.bpm = plan.bpm;
        s.loop = false;
        s.lengthBeats = juce::jmax (1, a.totalBars()) * 4.0;
        const auto song = arrangeParts (plan, a, parts, generalMidiDrums());
        for (size_t i = 0; i < parts.size() && i < song.size(); ++i)
        {
            s.parts.push_back ({ parts[i].label, parts[i].type, parts[i].instrument, parts[i].role });
            addNotes (s, song[i].notes, (int) s.parts.size() - 1);
        }
        sortNotes (s);
        return s;
    }

    //==============================================================================
    SketchPlayer::SketchPlayer() = default;
    SketchPlayer::~SketchPlayer() = default;

    void SketchPlayer::prepare (double sr)
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        for (auto& v : voices) v.active = false;
    }

    void SketchPlayer::setSong (std::shared_ptr<const SketchSong> s)
    {
        const juce::SpinLock::ScopedLockType sl (songLock);
        song = s.get();
        keep.push_front (std::move (s));
        while (keep.size() > 4) keep.pop_back();
    }

    void SketchPlayer::play (double fromBeat)
    {
        startAt = juce::jmax (0.0, fromBeat);
        position = juce::jmax (0.0, fromBeat);     // (until the next block says where it is)
        restart = true;
        armed = true;
    }

    void SketchPlayer::stop()
    {
        armed = false;
    }

    void SketchPlayer::release (Voice& v)
    {
        if (! v.active) return;
        if (v.drum) v.gate = juce::jmin (v.gate, (int) (sampleRate * 0.03));   // drums: a quick fade
        else v.releasing = true;
    }

    void SketchPlayer::releaseAll()
    {
        for (auto& v : voices) release (v);
    }

    void SketchPlayer::startVoice (const SketchNote& n, const SketchSong& s, double lengthSamples, int offset)
    {
        Voice* v = nullptr;
        for (auto& x : voices) if (! x.active) { v = &x; break; }
        if (v == nullptr)                         // all busy: take the quietest
        {
            v = &voices[0];
            for (auto& x : voices) if (x.env < v->env) v = &x;
        }
        const auto& part = s.parts[(size_t) juce::jlimit (0, (int) s.parts.size() - 1, n.part)];
        *v = Voice();
        v->active = true;
        v->drum = part.type == PartType::drums;
        v->type = part.type;
        v->instrument = part.instrument;
        v->note = n.note;
        v->part = n.part;
        v->vel = juce::jlimit (0.05f, 1.0f, (float) n.velocity / 127.0f);
        v->freq = juce::MidiMessage::getMidiNoteInHertz (n.note);
        v->gate = v->drum ? (int) (sampleRate * 2.5) : juce::jmax (1, (int) lengthSamples);
        v->seed = (juce::uint32) n.note * 2654435761u + (juce::uint32) offset * 40503u + 7u;
        v->t = -(double) offset / sampleRate;    // starts `offset` samples into the block
        // a closed or pedal hat cuts an open one off, like a real hi-hat
        if (v->drum && (n.note == 42 || n.note == 44))
            for (auto& x : voices)
                if (&x != v && x.active && x.drum && x.note == 46) x.gate = juce::jmin (x.gate, (int) (sampleRate * 0.02));
    }

    void SketchPlayer::schedule (const SketchSong& s, double from, double to, double samplesPerBeat, int blockLength)
    {
        if (s.notes.empty()) return;
        // the notes that start between a and b (song beats); beatAtSample0: the song beat at the block's first sample
        auto run = [&] (double a, double b, double beatAtSample0)
        {
            auto it = std::lower_bound (s.notes.begin(), s.notes.end(), a, [] (const SketchNote& n, double v) { return n.start < v; });
            for (; it != s.notes.end() && it->start < b; ++it)
            {
                if (! heard (it->part)) continue;
                const int offset = juce::jlimit (0, juce::jmax (0, blockLength - 1), (int) ((it->start - beatAtSample0) * samplesPerBeat));
                startVoice (*it, s, it->length * samplesPerBeat, offset);
            }
        };
        if (s.loop)
        {
            const double L = s.lengthBeats;
            double a = std::fmod (juce::jmax (0.0, from), L);
            const double total = to - juce::jmax (0.0, from);
            double done = 0.0;
            int guard = 0;
            while (total - done > 1e-9 && ++guard < 8)
            {
                const double seg = juce::jmin (L - a, total - done);
                run (a, a + seg, a - done - juce::jmax (0.0, -from));
                done += seg;
                a = 0.0;
            }
        }
        else if (from < s.lengthBeats && to > 0.0)
            run (juce::jmax (0.0, from), juce::jmin (to, s.lengthBeats), from);
    }

    void SketchPlayer::chase (const SketchSong& s, double at, double samplesPerBeat, int except)
    {
        if (s.notes.empty() || s.lengthBeats <= 0.0) return;
        if (s.loop) at = std::fmod (juce::jmax (0.0, at), s.lengthBeats);
        if (at <= 0.0 || at >= s.lengthBeats) return;
        // the notes that started a little earlier and are still held (a pad's chord, a long bass note): the rest of them
        auto it = std::lower_bound (s.notes.begin(), s.notes.end(), at - 32.0, [] (const SketchNote& n, double v) { return n.start < v; });
        for (; it != s.notes.end() && it->start < at; ++it)
        {
            if (it->part == except || ! heard (it->part)) continue;
            if (s.parts[(size_t) juce::jlimit (0, (int) s.parts.size() - 1, it->part)].type == PartType::drums) continue;
            const double left = it->start + it->length - at;
            if (left > 0.05) startVoice (*it, s, left * samplesPerBeat, 0);
        }
    }

    void SketchPlayer::process (juce::AudioBuffer<float>& buffer, const Host& host)
    {
        buffer.clear();
        const int n = buffer.getNumSamples();
        if (n <= 0) return;

        {
            // the song to play (if the message thread is swapping it right now, the one from the last block)
            const juce::SpinLock::ScopedTryLockType sl (songLock);
            if (sl.isLocked()) cachedSong = song;
        }
        const SketchSong* s = cachedSong;

        bool jumped = false;
        if (restart.exchange (false))           // PLAY, or a jump while it plays
        {
            ownRunning = true;
            ownPos = juce::jmax (0.0, startAt.load());
            expectedPos = -1.0;
            releaseAll();
            jumped = true;
        }

        // one part on its own: the others fade out (and when they come back in, they pick up what they hold)
        const int so = solo.load();
        const bool soloChanged = so != lastSolo;
        if (soloChanged && so >= 0)
            for (auto& v : voices)
                if (v.active && v.part != so) release (v);

        bool sounding = false, following = false;
        if (armed.load() && s != nullptr)
        {
            const double bpm = host.known && host.bpm > 0.0 ? host.bpm : s->bpm;
            const double samplesPerBeat = sampleRate * 60.0 / juce::jlimit (20.0, 400.0, bpm);
            following = host.known && host.playing;
            if (wasFollowing && ! following) ownRunning = false;   // the DAW stopped: so does the sketch (until PLAY or the DAW plays)
            double pos = -1.0;
            if (following)       pos = host.ppq;
            else if (ownRunning) pos = ownPos;
            if (following || ownRunning)
            {
                if (expectedPos >= 0.0 && std::abs (pos - expectedPos) > 0.05) { releaseAll(); jumped = true; }   // the DAW jumped
                if (expectedPos < 0.0) jumped = true;                 // starting: from the playhead, or the DAW started
                if (jumped) chase (*s, pos, samplesPerBeat, -1);
                else if (soloChanged && lastSolo >= 0) chase (*s, pos, samplesPerBeat, lastSolo);
                const double end = pos + (double) n / samplesPerBeat;
                schedule (*s, pos, end, samplesPerBeat, n);
                expectedPos = end;
                if (! following)
                {
                    ownPos = end;
                    if (! s->loop && ownPos >= s->lengthBeats)        // the whole song played: done
                    {
                        ownRunning = false;
                        armed = false;
                    }
                }
                position = s->loop ? std::fmod (juce::jmax (0.0, end), s->lengthBeats) : end;
                sounding = true;
            }
            else
                expectedPos = -1.0;
            wasFollowing = following;
        }
        else
        {
            if (expectedPos >= 0.0) releaseAll();
            expectedPos = -1.0;
            wasFollowing = false;
            ownRunning = false;
        }
        lastSolo = so;
        followingHost = following;
        if (! sounding) position = -1.0;
        if (wasSounding && ! sounding) releaseAll();         // stopped (or the DAW did): what's sounding fades out
        wasSounding = sounding;

        render (buffer, 0, n);
    }

    //==============================================================================
    float SketchPlayer::toneSample (Voice& v)
    {
        const double dt = 1.0 / sampleRate;
        if (v.t < 0.0) { v.t += dt; return 0.0f; }

        // the envelope: attack, decay to the sustain level, release after the note ends
        double a = 0.004, d = 0.3, s = 0.7, r = 0.12;
        const auto inst = v.instrument;
        const bool pianoLike = inst == Kind::piano || inst == Kind::keys || inst == Kind::other || inst == Kind::vocal;
        const bool plucked = inst == Kind::guitar || inst == Kind::pluck || inst == Kind::world || inst == Kind::sequence;
        switch (v.type)
        {
            case PartType::bass:   a = 0.004; d = 0.25; s = 0.65; r = 0.08; break;
            case PartType::pad:    a = 0.35;  d = 0.6;  s = 0.8;  r = 0.6;  break;
            case PartType::hook:   a = 0.01;  d = 0.2;  s = 0.7;  r = 0.15; break;
            case PartType::chords:
                if (pianoLike)                   { a = 0.003; d = 0.9;  s = 0.0;  r = 0.2;  }
                else if (plucked)                { a = 0.002; d = 0.45; s = 0.0;  r = 0.1;  }
                else if (inst == Kind::organ)    { a = 0.01;  d = 0.3;  s = 0.9;  r = 0.06; }
                else                             { a = 0.08;  d = 0.5;  s = 0.75; r = 0.3;  }
                break;
            case PartType::drums:
            case PartType::none:
                break;
        }
        if (! v.releasing)
        {
            v.env = v.t < a ? (float) (v.t / a) : (float) (s + (1.0 - s) * std::exp (-(v.t - a) / d));
            if (--v.gate <= 0) v.releasing = true;
        }
        else
        {
            v.env *= (float) std::exp (-dt / (r * 0.35));
            if (v.env < 1.0e-4f) { v.active = false; return 0.0f; }
        }
        v.t += dt;

        v.phase += v.freq * dt;            if (v.phase >= 1.0) v.phase -= 1.0;
        v.phase2 += v.freq * 1.004 * dt;   if (v.phase2 >= 1.0) v.phase2 -= 1.0;
        const double sine = std::sin (twoPi * v.phase);
        const double saw = 2.0 * v.phase - 1.0;
        auto lowpass = [this] (float& state, double x, double cutoff)
        {
            const double k = 1.0 - std::exp (-twoPi * juce::jlimit (20.0, 18000.0, cutoff) / sampleRate);
            state += (float) (k * (x - state));
            return (double) state;
        };

        double x = 0.0, gain = 0.3;
        switch (v.type)
        {
            case PartType::bass:
                x = lowpass (v.lp, 0.75 * sine + 0.3 * saw, 700.0);
                gain = 0.55;
                break;
            case PartType::pad:
                x = lowpass (v.lp, 0.5 * (saw + (2.0 * v.phase2 - 1.0)), 1400.0);
                gain = 0.14;
                break;
            case PartType::hook:
            {
                const double tri = 4.0 * std::abs (v.phase - 0.5) - 1.0;
                x = 0.8 * tri + 0.15 * (sine >= 0.0 ? 1.0 : -1.0);
                x = lowpass (v.lp, x, 3500.0);
                gain = 0.26;
                break;
            }
            case PartType::chords:
                if (pianoLike)
                    x = sine + 0.35 * std::sin (2.0 * twoPi * v.phase) * std::exp (-v.t / 0.35) + 0.12 * std::sin (3.0 * twoPi * v.phase);
                else if (plucked)
                    x = lowpass (v.lp, saw, 300.0 + 3200.0 * std::exp (-v.t / 0.12));
                else if (inst == Kind::organ)
                    x = sine + 0.5 * std::sin (2.0 * twoPi * v.phase) + 0.25 * std::sin (4.0 * twoPi * v.phase);
                else
                    x = lowpass (v.lp, 0.5 * (saw + (2.0 * v.phase2 - 1.0)), 2000.0);
                gain = 0.17;
                break;
            case PartType::drums:
            case PartType::none:
                break;
        }
        return (float) (x * gain * v.env * v.vel);
    }

    float SketchPlayer::drumSample (Voice& v)
    {
        const double dt = 1.0 / sampleRate;
        if (v.t < 0.0) { v.t += dt; return 0.0f; }
        const double t = v.t;
        v.t += dt;
        if (--v.gate <= 0) { v.active = false; return 0.0f; }

        v.seed ^= v.seed << 13; v.seed ^= v.seed >> 17; v.seed ^= v.seed << 5;
        const float noise = (float) (v.seed & 0xffff) / 32768.0f - 1.0f;
        v.hpOut = 0.72f * (v.hpOut + noise - v.hpIn);       // a bright (high-passed) noise
        v.hpIn = noise;
        const float bright = v.hpOut;
        v.env = 1.0f;

        auto tom = [&] (double base)
        {
            v.phase += base * (1.0 + 0.6 * std::exp (-t / 0.04)) * dt;
            return std::sin (twoPi * v.phase) * std::exp (-t / 0.3) * 0.7;
        };
        double x = 0.0;
        switch (v.note)
        {
            case 35: case 36:                               // kick
                v.phase += (48.0 + 120.0 * std::exp (-t / 0.035)) * dt;
                x = std::sin (twoPi * v.phase) * std::exp (-t / 0.38) * 0.95 + noise * std::exp (-t / 0.003) * 0.25;
                break;
            case 37:                                        // rim
                x = std::sin (twoPi * 1700.0 * t) * std::exp (-t / 0.02) * 0.5 + noise * std::exp (-t / 0.01) * 0.3;
                break;
            case 38: case 40:                               // snare
                v.phase += 185.0 * dt;
                x = bright * std::exp (-t / 0.13) * 0.65 + std::sin (twoPi * v.phase) * std::exp (-t / 0.06) * 0.45;
                break;
            case 39:                                        // clap: three quick bursts, then the tail
            {
                const double env = t < 0.026 ? (std::fmod (t, 0.009) < 0.004 ? 1.0 : 0.15) : std::exp (-(t - 0.026) / 0.12);
                x = bright * env * 0.6;
                break;
            }
            case 42: x = bright * std::exp (-t / 0.035) * 0.35; break;     // closed hat
            case 44: x = bright * std::exp (-t / 0.025) * 0.28; break;     // pedal hat
            case 46: x = bright * std::exp (-t / 0.28) * 0.3;   break;     // open hat
            case 41: x = tom (70.0);  break;
            case 43: x = tom (85.0);  break;
            case 45: x = tom (100.0); break;
            case 47: x = tom (120.0); break;
            case 48: x = tom (140.0); break;
            case 50: x = tom (165.0); break;
            case 49: case 52: case 55: case 57:              // crash
                x = bright * std::exp (-t / 1.1) * 0.26;
                break;
            case 51: case 53: case 59:                       // ride
                x = bright * std::exp (-t / 0.7) * 0.14 + std::sin (twoPi * 3200.0 * t) * std::exp (-t / 0.5) * 0.04;
                break;
            case 56:                                         // cowbell
            {
                const double sq = (std::sin (twoPi * 562.0 * t) > 0.0 ? 1.0 : -1.0) + (std::sin (twoPi * 845.0 * t) > 0.0 ? 1.0 : -1.0);
                v.lp += (float) (0.25 * (sq * 0.5 - v.lp));
                x = v.lp * std::exp (-t / 0.12) * 0.25;
                break;
            }
            default:                                         // shaker, tambourine and the rest
                x = bright * std::exp (-t / 0.06) * 0.25;
                break;
        }
        if (t > 2.4) v.active = false;
        return (float) (x * v.vel);
    }

    void SketchPlayer::render (juce::AudioBuffer<float>& buffer, int from, int to)
    {
        const float vol = juce::jlimit (0.0f, 1.0f, volume.load()) * 0.7f;
        auto* left = buffer.getWritePointer (0);
        for (auto& v : voices)
        {
            if (! v.active) continue;
            for (int i = from; i < to && v.active; ++i)
                left[i] += (v.drum ? drumSample (v) : toneSample (v)) * vol;
        }
        for (int i = from; i < to; ++i) left[i] = std::tanh (left[i]);    // never louder than full scale
        for (int c = 1; c < buffer.getNumChannels(); ++c) buffer.copyFrom (c, from, buffer, 0, from, to - from);
    }
}
