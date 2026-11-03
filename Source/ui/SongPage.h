// Song Savant - the Song page: describe your song, and get palettes of sounds from your own library, one card per
// part. Switch a part to its alternatives, lock the ones you love, ask for new palettes, copy a name to load it.
#pragma once

#include "Widgets.h"
#include "core/SongSession.h"

class SongSavantProcessor;

namespace ss::ui
{
    class PartCard : public juce::Component, public juce::SettableTooltipClient, public juce::FileDragAndDropTarget
    {
    public:
        std::function<void()> onSelect, onPrev, onNext, onLock, onDragPart, onPlayPart, onEdit;
        std::function<void (const juce::File&)> onDropFile;   // a MIDI pattern dropped on the card: the piano roll
        bool isInterestedInFileDrag (const juce::StringArray&) override;
        void fileDragEnter (const juce::StringArray&, int, int) override   { dropHover = true; repaint(); }
        void fileDragExit (const juce::StringArray&) override              { dropHover = false; repaint(); }
        void filesDropped (const juce::StringArray&, int, int) override;
        void show (const PaletteSlot*, const Sound*, bool selected, const juce::String& partInfo = {});
        // heard: this card's part is playing (a little level meter); alone: it's playing on its own (its button: stop)
        void setPlaying (bool heard, bool alone);
        // EDIT (the piano roll) for the parts it can edit; `mine`: the part is your own (YOUR PART on the card)
        void setEditing (bool canEdit, bool mine);
        bool isHeard() const                        { return heard; }
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override   { repaint(); }

    private:
        const PaletteSlot* slot = nullptr;
        const Sound* sound = nullptr;
        bool isSelected = false, pressedOnMidi = false, dragging = false, heard = false, alone = false, canEdit = false, mine = false, dropHover = false;
        juce::String partInfo;                      // the starter part, when the card has one
        juce::Rectangle<float> prevArea, nextArea, lockArea, midiArea, playArea, editArea;
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
        // Hearing the starter parts with the sketch sounds (also the tests): PLAY ALL / STOP, and a card's play button
        // (that part on its own; while it plays, clicking another card plays that card's part instead)
        void playAll();
        void playPart (int card);
        // A kit you just saved in Groove Grabber: read into the library, and the one built for this song goes on the
        // drum card (checked every few seconds; also the tests). True when a kit went on the card.
        bool checkGrooveGrabberKits();
        void showForm();                              // shows the song's form again (after the Masterclass changed it)
        std::function<void (const juce::String& role)> onEditPart;   // a card's EDIT: the piano roll
        std::function<void()> onBlankSong;           // BLANK SONG: ARRANGE, with an empty map to fill yourself
        void hideSelected();                         // NOT IN MY LIBRARY (also the tests): never suggested again
        std::function<void (const juce::String& role, const juce::File&)> onDropPart;   // a MIDI pattern dropped on a card
        void visibilityChanged() override;

    private:
        void timerCallback() override;
        void showPlaying();
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
                   playBtn { "PLAY ALL", PillButton::Style::outline },
                   sendBtn { "SEND TO GROOVE GRABBER", PillButton::Style::outline },
                   blankBtn { "BLANK SONG", PillButton::Style::outline },
                   notThereBtn { "NOT IN MY LIBRARY?", PillButton::Style::subtle };
        DragChip allParts { "ALL PARTS", DragChip::Kind::midi };
        juce::OwnedArray<PillButton> tabs;
        juce::OwnedArray<PartCard> cards;
        ChoiceList choices;

        juce::Rectangle<float> formPanel, cardsArea, detailsPanel, understoodLine, barHint, whyArea, howArea, choicesTitle;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongPage)
    };
}
