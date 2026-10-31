#include "Paths.h"

namespace ss::paths
{
    static juce::File testHome()
    {
       #if SS_TEST_RUNNER
        if (auto env = juce::SystemStats::getEnvironmentVariable ("SS_TEST_HOME", {}); env.isNotEmpty())
            return juce::File (env);
       #endif
        return {};
    }

    juce::File documentsDir()
    {
        if (auto t = testHome(); t != juce::File())
            return t.getChildFile ("Documents").getChildFile ("Song Savant");
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Song Savant");
    }

    juce::File appDataDir()
    {
        juce::File dir;
        if (auto t = testHome(); t != juce::File())
            dir = t.getChildFile ("AppData").getChildFile ("Song Savant");
        else
        {
            auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
           #if JUCE_MAC
            dir = base.getChildFile ("Application Support").getChildFile ("Song Savant");
           #else
            dir = base.getChildFile ("Song Savant");
           #endif
        }
        dir.createDirectory();
        return dir;
    }

    juce::File reportFile()   { return documentsDir().getChildFile ("Setup Report.txt"); }

    juce::File tempDir()
    {
        auto d = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Song Savant");
        d.createDirectory();
        return d;
    }
}
