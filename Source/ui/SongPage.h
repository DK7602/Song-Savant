// Song Savant - the Song page: describe your song, and get palettes of sounds from your own library, one card per
// part. Switch a part to its alternatives, lock the ones you love, ask for new palettes, copy a name to load it.
#pragma once

#include "Widgets.h"
#include "core/SongSession.h"

class SongSavantProcessor;

namespace ss::ui
{
    class PartCard : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        std::function<void()> onSelect, onPrev, onNext, onLock, onDragPart;
        void show (const PaletteSlot*, const Sound*, bool selected, const juce::String& partInfo = {});
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override   { repaint(); }
        void mouseExit (const juce::MouseEvent&) override   { repaint(); }

    private:
        const PaletteSlot* slot = nullptr;
        const Sound* sound = nullptr;
        bool isSelected = false, pressedOnMidi = false, dragging = false;
        juce::String partInfo;                      // the starter part, when the card has one
        juce::Rectangle<float> prevArea, nextArea, lockArea, midiArea;
    };

    // The other sounds a part could use: click one to switch
    class ChoiceList : public juce::Component
    {
    public:
        std::function<void (int)> onChoose;
        void show (const PaletteSlot*, const Catalog*);
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;

    private:
        static constexpr int rowH = 26;
        const PaletteSlot* slot = nullptr;
        const Catalog* catalog = nullptr;
    };

    class SongPage : public juce::Component, private juce::Timer
    {
    public:
        explicit SongPage (SongSavantProcessor&);
        ~SongPage() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        void find();                                  // FIND MY SOUNDS (also the tests)
        void themeChanged();
        void setDescription (const juce::String&);    // tests
        juce::File writeStarterParts (int card = -1); // one card's part, or all of them (-1): the MIDI file to drag
        juce::File sendToGrooveGrabber();             // SEND TO GROOVE GRABBER (also the tests): the song file written
        // A kit you just saved in Groove Grabber: read into the library, and the one built for this song goes on the
        // drum card (checked every few seconds; also the tests). True when a kit went on the card.
        bool checkGrooveGrabberKits();
        void showForm();                              // shows the song's form again (after the Masterclass changed it)

    private:
        void timerCallback() override;
        void refresh();
        void readForm();
        void fillStyles (int genre);
        void showKeyCompassMenu();
        PaletteSlot* selectedSlot();
        void copySelected();
        void dragPart (int card);
        DrumNotes drumNotes() const;                  // the drum card's own drum map, if Song Savant made one

        SongSavantProcessor& proc;
        std::shared_ptr<const Catalog> catalog;
        int seenGeneration = -1;
        int kitTicks = 0;
        juce::File ggKits;                            // Groove Grabber's Kits folder
        juce::StringArray ggKitFiles;                 // what was in it at the last look
        bool lookedAtKits = false, kitsStale = false;
        juce::Time sentAt;                            // when this song was sent to Groove Grabber
        juce::String selectedRole, copiedNote, status;

        juce::TextEditor describeBox, bpmBox;
        juce::ComboBox genreBox, styleBox, keyBox;
        PillButton findBtn { "FIND MY SOUNDS", PillButton::Style::gold },
                   keyCompassBtn { "KEY COMPASS", PillButton::Style::outline },
                   newBtn { "NEW PALETTES", PillButton::Style::outline },
                   copyBtn { "COPY NAME", PillButton::Style::gold },
                   ideaBtn { "NEW IDEA", PillButton::Style::outline },
                   sendBtn { "SEND TO GROOVE GRABBER", PillButton::Style::outline };
        DragChip allParts { "ALL PARTS", DragChip::Kind::midi };
        juce::OwnedArray<PillButton> tabs;
        juce::OwnedArray<PartCard> cards;
        ChoiceList choices;

        juce::Rectangle<float> formPanel, cardsArea, detailsPanel, understoodLine, barHint, whyArea, howArea, choicesTitle;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongPage)
    };
}
