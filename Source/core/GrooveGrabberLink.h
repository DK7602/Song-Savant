// Song Savant - working with Groove Grabber, both ways:
//   SEND TO GROOVE GRABBER writes the song (description, genre, style, decade, key, tempo, the arrangement's sections)
//   to Documents/Song Savant/Songs/<name>.sssong. Groove Grabber offers it on its BUILD page and in its KEY COMPASS menu,
//   and builds four kits for it from your own samples, tuned to the key.
//   The kits you save in Groove Grabber (Documents/Groove Grabber/Kits/<genre>/*.ggkit) are drum choices here, and the
//   starter drum parts play Groove Grabber's pads (General MIDI drums on 36..51, the 808 on 52, percussion on 53..55).
#pragma once

#include "SongSession.h"

namespace ss
{
    // ---- sending a song
    juce::File grooveGrabberSongsFolder();                 // Documents/Song Savant/Songs
    bool grooveGrabberInstalled();                         // its Documents folder is there
    juce::String songNameFor (const SongSession&);         // the Key Compass song's name, or a short name from the description
    juce::var songForGrooveGrabber (const SongSession&, const juce::String& drums);
    juce::File sendToGrooveGrabber (const SongSession&, const juce::String& drums);   // the file written (none if it failed)

    // ---- kits saved in Groove Grabber (readGrooveGrabberKits and isGrooveGrabberKit are in Catalog.h)
    DrumNotes grooveGrabberDrums();
}
