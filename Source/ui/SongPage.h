// Song Savant - the Song page: describe your song, and get palettes of sounds from your own library, one card per
// part. Switch a part to its alternatives, lock the ones you love, ask for new palettes, copy a name to load it.
#pragma once

#include "Widgets.h"
#include "core/SongSession.h"

class SongSavantProcessor;

namespace ss::ui
{
    class PartCard : public juce::Component
    {
    public:
        std::function<void()> onSelect, onPrev, onNext, onLock;
        void show (const PaletteSlot*, const Sound*, bool selected);
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override   { repaint(); }
        void mouseExit (const juce::MouseEvent&) override   { repaint(); }

    private:
        const PaletteSlot* slot = nullptr;
        const Sound* sound = nullptr;
        bool isSelected = false;
        juce::Rectangle<float> prevArea, nextArea, lockArea;
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
        void setDescription (const juce::String&);    // tests

    private:
        void timerCallback() override;
        void refresh();
        void readForm();
        void fillStyles (int genre);
        void showKeyCompassMenu();
        PaletteSlot* selectedSlot();
        void copySelected();

        SongSavantProcessor& proc;
        std::shared_ptr<const Catalog> catalog;
        int seenGeneration = -1;
        juce::String selectedRole, copiedNote, status;

        juce::TextEditor describeBox, bpmBox;
        juce::ComboBox genreBox, styleBox, keyBox;
        PillButton findBtn { "FIND MY SOUNDS", PillButton::Style::gold },
                   keyCompassBtn { "KEY COMPASS", PillButton::Style::outline },
                   newBtn { "NEW PALETTES", PillButton::Style::outline },
                   copyBtn { "COPY NAME", PillButton::Style::gold };
        juce::OwnedArray<PillButton> tabs;
        juce::OwnedArray<PartCard> cards;
        ChoiceList choices;

        juce::Rectangle<float> formPanel, cardsArea, detailsPanel, understoodLine, barHint, whyArea, howArea, choicesTitle;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongPage)
    };
}
