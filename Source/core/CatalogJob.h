// Song Savant - builds the catalog on a background thread (it takes a second or two) so the window never waits.
#pragma once

#include "Catalog.h"
#include <memory>

namespace ss
{
    class CatalogJob : private juce::Thread
    {
    public:
        CatalogJob() : juce::Thread ("Song Savant catalog") {}
        ~CatalogJob() override       { cancelFlag = true; stopThread (10000); }

        void start (const SystemLocations& l)
        {
            if (isThreadRunning()) return;
            {
                const juce::ScopedLock sl (lock);
                loc = l;
            }
            cancelFlag = false;
            startThread (juce::Thread::Priority::normal);
        }

        bool isBuilding() const                              { return isThreadRunning(); }
        int generation() const                               { return gen.load(); }
        std::shared_ptr<const Catalog> catalog() const       { const juce::ScopedLock sl (lock); return latest; }

        void setCatalog (Catalog c)                          // tests and screenshots
        {
            const juce::ScopedLock sl (lock);
            latest = std::make_shared<const Catalog> (std::move (c));
            ++gen;
        }

    private:
        void run() override
        {
            SystemLocations l;
            {
                const juce::ScopedLock sl (lock);
                l = loc;
            }
            auto c = buildCatalog (l, cancelFlag);
            if (cancelFlag.load()) return;
            setCatalog (std::move (c));
        }

        mutable juce::CriticalSection lock;
        SystemLocations loc;
        std::shared_ptr<const Catalog> latest;
        std::atomic<bool> cancelFlag { false };
        std::atomic<int> gen { 0 };
    };
}
