// Song Savant - builds the catalog on a background thread (it takes a second or two) so the window never waits.
// Then it finds out what any unlabelled plug-ins are (with the Song Savant Scanner, one plug-in at a time) and, if
// that taught it anything, reads the libraries again so those instruments can be suggested too.
#pragma once

#include "Catalog.h"
#include "PluginProfiles.h"
#include "PluginTypes.h"
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

        bool isBuilding() const                              { return isThreadRunning() && ! checking.load(); }
        bool isCheckingPlugins() const                       { return isThreadRunning() && checking.load(); }
        std::pair<int, int> pluginCheckProgress() const      { return { checkDone.load(), checkTotal.load() }; }
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

            // plug-ins that don't say what they are (and that Song Savant doesn't know by name): ask them
            std::vector<PluginInfo> unknown;
            for (auto& p : pluginsToCheck (installedPlugins (l, &cancelFlag)))
                if (profileFor (p) == nullptr) unknown.push_back (p);
            if (unknown.empty() || ! scannerProgram().existsAsFile() || cancelFlag.load()) return;

            checkDone = 0;
            checkTotal = (int) unknown.size();
            checking = true;
            const int checked = checkPlugins (unknown, cancelFlag, 300, [this] (int done, int total)
            {
                checkDone = done;
                checkTotal = total;
            });
            checking = false;
            if (checked == 0 || cancelFlag.load()) return;

            auto again = buildCatalog (l, cancelFlag);
            if (! cancelFlag.load()) setCatalog (std::move (again));
        }

        mutable juce::CriticalSection lock;
        SystemLocations loc;
        std::shared_ptr<const Catalog> latest;
        std::atomic<bool> cancelFlag { false };
        std::atomic<int> gen { 0 };
        std::atomic<bool> checking { false };
        std::atomic<int> checkDone { 0 }, checkTotal { 0 };
    };
}
