// Song Savant - sounds you said aren't in your library (Analog Lab dropped a preset, say): never suggested again.
// NOT IN MY LIBRARY on the SONG page or the LIBRARY page hides one; SUGGEST IT AGAIN on the LIBRARY page brings it back.
// One line each (the sound's key: name|instrument|library) in <app data>/Hidden sounds.txt, read once per run.
#pragma once

#include "Catalog.h"

namespace ss::hidden
{
    bool isHidden (const Sound&);
    void hide (const Sound&);
    void show (const Sound&);                     // suggest it again
    int count();
    juce::File file();
    void useFileForTests (const juce::File&);     // (the tests: another file, read again)
}
