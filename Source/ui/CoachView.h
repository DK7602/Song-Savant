// Song Savant - COACH MY SONG (the MASTERCLASS page): what's working in your song and what to try, from what its style
// usually does (core/Coach). Each tip can SHOW ME (the section on ARRANGE, or the part in the piano roll), some can
// FIX IT (the page's UNDO takes a fix back), and I MEANT THAT puts a tip away for this song.
#pragma once

#include "Widgets.h"
#include "core/Coach.h"

class SongSavantProcessor;

namespace ss::ui
{
    class CoachView : public juce::Component
    {
    public:
        explicit CoachView (SongSavantProcessor&);

        void refresh();                              // reads the song again
        int layout (int width);                      // places everything; returns the height it needs (and takes that size)
        void paint (juce::Graphics&) override;

        const coach::Report& getReport() const       { return report; }
        int shownTips() const;
        // (the buttons; also the tests)
        bool fix (int tip);                          // FIX IT: true when the song changed
        void dismiss (int tip);                      // I MEANT THAT
        void showMe (int tip);
        void bringBack();                            // the tips you meant, back
        bool canUndo() const                         { return ! undos.empty(); }
        void undo();                                 // the last FIX IT, taken back (only while nothing else changed the song)

        std::function<void (int section)> onShowSection;                       // SHOW ME on ARRANGE
        std::function<void (const juce::String& role, int section)> onShowPart; // SHOW ME in the piano roll
        std::function<void()> onSongChanged;         // a fix or an undo changed the song: the other pages follow
        std::function<void()> onLayoutChanged;       // the tips changed: the page sizes this again
        std::function<void()> onUndoChanged;

    private:
        struct Row
        {
            coach::Tip tip;
            juce::Rectangle<float> area, textArea;
            std::unique_ptr<PillButton> show, fixBtn, meant;
        };
        void rebuildRows();

        SongSavantProcessor& proc;
        coach::Report report;
        std::vector<std::unique_ptr<Row>> rows;
        struct Step { coach::Snapshot before, after; };
        std::vector<Step> undos;
        juce::String note;                           // "Done: SET 94 BPM. UNDO takes it back."
        bool showAll = false;
        PillButton allBtn { "SHOW ALL TIPS", PillButton::Style::outline },
                   backBtn { "BRING BACK THE TIPS YOU MEANT", PillButton::Style::subtle };
        juce::Rectangle<float> headArea, emptyArea, workArea, tipsTitle;
        juce::String seen;                           // the song the rows were made for
    };
}
