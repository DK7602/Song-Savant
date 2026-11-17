// Song Savant - the few things only the app (not the plug-in) can do with its window and its audio / MIDI settings.
#pragma once

namespace ss::standalone
{
    bool isStandalone();          // running as the app (false inside a DAW)
    void showAudioSettings();     // Options > Audio/MIDI Settings: the output, and which MIDI keyboards to listen to
}
