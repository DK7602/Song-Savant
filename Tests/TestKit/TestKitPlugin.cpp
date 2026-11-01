// SS Test Kit - a tiny drum kit plug-in, built only for the tests. It stays silent until a preset switches it on
// (so a test can tell the preset really got loaded), then plays made-up drums on the General MIDI keys:
//   B0 808, C1 kick, D1 snare, D#1 clap, F#1 closed hat, A1 low tom, A#1 open hat, C2 high tom, C#2 crash,
//   and C3 to B4: one tom recording stretched over two octaves, higher on each key (like an old sampler's zone).
#include <juce_audio_processors/juce_audio_processors.h>

namespace
{
    struct Voice
    {
        int note = -1;
        double t = 0, phase = 0;
        juce::Random rng;
        juce::IIRFilter filter1, filter2;
        bool active() const   { return note >= 0; }
    };

    class TestKit : public juce::AudioProcessor
    {
    public:
        TestKit() : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)) {}

        const juce::String getName() const override              { return "SS Test Kit"; }
        bool acceptsMidi() const override                        { return true; }
        bool producesMidi() const override                       { return false; }
        double getTailLengthSeconds() const override             { return 2.0; }
        int getNumPrograms() override                            { return 1; }
        int getCurrentProgram() override                         { return 0; }
        void setCurrentProgram (int) override                    {}
        const juce::String getProgramName (int) override         { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        bool hasEditor() const override                          { return false; }
        juce::AudioProcessorEditor* createEditor() override      { return nullptr; }

        void getStateInformation (juce::MemoryBlock& dest) override
        {
            const juce::String s = on ? "SSTESTKIT on" : "SSTESTKIT off";
            dest.replaceAll (s.toRawUTF8(), s.getNumBytesAsUTF8());
        }
        void setStateInformation (const void* data, int size) override
        {
            on = juce::String::fromUTF8 (static_cast<const char*> (data), size).trim() == "SSTESTKIT on";
        }

        void prepareToPlay (double rate, int) override
        {
            sr = rate;
            for (auto& v : voices) v.note = -1;
            // the "recording": a tom with a stick click
            recording.assign ((size_t) sr, 0.0f);
            juce::Random rng (7);
            for (size_t i = 0; i < recording.size(); ++i)
            {
                const double t = (double) i / sr;
                recording[i] = (float) (std::sin (juce::MathConstants<double>::twoPi * 120.0 * t) * std::exp (-t / 0.12)
                                        + (rng.nextFloat() * 2.0f - 1.0f) * 0.5 * std::exp (-t / 0.006));
            }
        }
        void releaseResources() override                         {}

        void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
        {
            buffer.clear();
            int pos = 0;
            for (const auto m : midi)
            {
                render (buffer, pos, m.samplePosition - pos);
                pos = m.samplePosition;
                const auto msg = m.getMessage();
                if (msg.isNoteOn() && on) start (msg.getNoteNumber());
                if (msg.isAllSoundOff()) for (auto& v : voices) v.note = -1;
            }
            render (buffer, pos, buffer.getNumSamples() - pos);
        }

    private:
        void start (int note)
        {
            auto& v = voices[(size_t) (next++ % voices.size())];
            v.note = note;
            v.t = 0;
            v.phase = 0;
            v.rng.setSeed (note);
            v.filter1.reset();
            v.filter2.reset();
            const bool hat = note == 42 || note == 46 || note == 49;
            const auto c = hat ? juce::IIRCoefficients::makeHighPass (sr, 7000.0)
                         : note == 39 ? juce::IIRCoefficients::makeBandPass (sr, 1800.0, 1.0)
                                      : juce::IIRCoefficients::makeLowPass (sr, 5000.0);
            v.filter1.setCoefficients (c);
            v.filter2.setCoefficients (c);
        }

        float sample (Voice& v)
        {
            const double t = v.t, twoPi = juce::MathConstants<double>::twoPi;
            float noise = v.rng.nextFloat() * 2.0f - 1.0f;
            noise = v.filter1.processSingleSampleRaw (noise);
            if (v.note != 39) noise = v.filter2.processSingleSampleRaw (noise);
            if (v.note >= 60 && v.note <= 83)
            {
                const double pos = t * sr * std::pow (2.0, (v.note - 60) / 12.0);   // the recording, sped up
                const auto j = (size_t) pos;
                if (j + 1 >= recording.size()) { v.note = -1; return 0.0f; }
                const double frac = pos - (double) j;
                return (float) (recording[j] + (recording[j + 1] - recording[j]) * frac);
            }
            switch (v.note)
            {
                case 35: return (float) (std::sin (twoPi * 45.0 * t) * std::exp (-t / 0.35));
                case 36: v.phase += twoPi * (50.0 + 100.0 * std::exp (-t / 0.03)) / sr; return (float) (std::sin (v.phase) * std::exp (-t / 0.06));
                case 38: return (float) (noise * std::exp (-t / 0.05) + 0.6 * std::sin (twoPi * 190.0 * t) * std::exp (-t / 0.04));
                case 39:
                {
                    double a = 0;
                    for (double b : { 0.0, 0.011, 0.022, 0.033 }) if (t >= b) a = std::max (a, std::exp (-(t - b) / 0.002));
                    if (t >= 0.033) a = std::max (a, 0.5 * std::exp (-(t - 0.033) / 0.05));
                    return (float) (noise * 3.0 * a);
                }
                case 42: return (float) (noise * std::exp (-t / 0.015));
                case 46: return (float) (noise * std::exp (-t / 0.12));
                case 49: return (float) (noise * std::exp (-t / 0.5));
                case 45: return (float) (std::sin (twoPi * 110.0 * t) * std::exp (-t / 0.1));
                case 48: return (float) (std::sin (twoPi * 180.0 * t) * std::exp (-t / 0.1));
                default: v.note = -1; return 0.0f;
            }
        }

        void render (juce::AudioBuffer<float>& buffer, int from, int count)
        {
            for (auto& v : voices)
                for (int i = 0; i < count && v.active(); ++i)
                {
                    const float s = 0.5f * sample (v);
                    for (int c = 0; c < buffer.getNumChannels(); ++c) buffer.addSample (c, from + i, s);
                    v.t += 1.0 / sr;
                    if (v.t > 3.0) v.note = -1;
                }
        }

        bool on = false;
        double sr = 44100.0;
        std::vector<float> recording;
        std::array<Voice, 8> voices;
        size_t next = 0;
    };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()   { return new TestKit(); }
