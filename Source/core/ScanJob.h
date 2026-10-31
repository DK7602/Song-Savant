// Song Savant - runs the setup scan on a background thread so the app (or the DAW) never freezes.
// The screen polls progress() and result().
#pragma once

#include "SetupScan.h"
#include <memory>

namespace ss
{
    class ScanJob : private juce::Thread
    {
    public:
        ScanJob();
        ~ScanJob() override;

        void start (const SystemLocations&);
        void cancel();
        bool isScanning() const                          { return isThreadRunning(); }

        float progress() const                           { return progressValue.load(); }
        juce::String stage() const;

        std::shared_ptr<const ScanResult> result() const;
        SystemLocations locations() const;
        int generation() const                           { return gen.load(); }   // +1 each time a result arrives

        void setResult (ScanResult, const SystemLocations&);   // tests and screenshots

    private:
        void run() override;

        mutable juce::CriticalSection lock;
        SystemLocations loc;
        std::shared_ptr<const ScanResult> latest;
        juce::String stageText;
        std::atomic<float> progressValue { 0.0f };
        std::atomic<bool> cancelFlag { false };
        std::atomic<int> gen { 0 };
    };
}
