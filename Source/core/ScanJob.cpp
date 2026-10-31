#include "ScanJob.h"

namespace ss
{
    ScanJob::ScanJob() : juce::Thread ("Song Savant setup scan") {}

    ScanJob::~ScanJob()
    {
        cancelFlag = true;
        stopThread (10000);
    }

    void ScanJob::start (const SystemLocations& l)
    {
        if (isThreadRunning()) return;
        {
            const juce::ScopedLock sl (lock);
            loc = l;
            stageText = "Starting...";
        }
        cancelFlag = false;
        progressValue = 0.0f;
        startThread (juce::Thread::Priority::low);
    }

    void ScanJob::cancel()   { cancelFlag = true; }

    juce::String ScanJob::stage() const
    {
        const juce::ScopedLock sl (lock);
        return stageText;
    }

    std::shared_ptr<const ScanResult> ScanJob::result() const
    {
        const juce::ScopedLock sl (lock);
        return latest;
    }

    SystemLocations ScanJob::locations() const
    {
        const juce::ScopedLock sl (lock);
        return loc;
    }

    void ScanJob::setResult (ScanResult r, const SystemLocations& l)
    {
        const juce::ScopedLock sl (lock);
        loc = l;
        latest = std::make_shared<const ScanResult> (std::move (r));
        ++gen;
    }

    void ScanJob::run()
    {
        SystemLocations l;
        {
            const juce::ScopedLock sl (lock);
            l = loc;
        }
        auto r = runSetupScan (l, cancelFlag, [this] (float f, const juce::String& s)
        {
            progressValue = f;
            const juce::ScopedLock sl (lock);
            stageText = s;
        });
        {
            const juce::ScopedLock sl (lock);
            stageText = r.cancelled ? "Stopped." : "Done in " + juce::String (juce::roundToInt (r.seconds)) + " seconds.";
            latest = std::make_shared<const ScanResult> (std::move (r));
        }
        progressValue = 1.0f;
        ++gen;
    }
}
