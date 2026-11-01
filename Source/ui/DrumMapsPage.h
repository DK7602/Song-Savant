// Song Savant - the Drum Maps page: drop a drum kit preset you saved in your DAW, Song Savant plays every key in the
// background, names each pad and saves a Cubase drum map for it.
#pragma once

#include "Widgets.h"
#include "core/KitMap.h"

class SongSavantProcessor;

namespace ss::ui
{
    // Picking pads to rename: a click picks one, dragging across pads or shift-clicking picks a run of them.
    // The first pad clicked is remembered even after the menu closes, so a shift-click afterwards still works.
    struct RangePicker
    {
        int anchor = -1, first = -1, last = -1;
        void press (int row, bool shift)
        {
            if (row < 0) return;
            if (shift && anchor >= 0) { first = std::min (anchor, row); last = std::max (anchor, row); }
            else { anchor = first = last = row; }
        }
        void drag (int row)          { if (row >= 0 && anchor >= 0) { first = std::min (anchor, row); last = std::max (anchor, row); } }
        void menuClosed()            { first = last = -1; }          // the picked pads are no longer shown; the anchor stays
        void reset()                 { anchor = first = last = -1; } // a new or changed map
        bool picked (int row) const  { return row >= first && row <= last && first >= 0; }
    };

    // The pads that were found: key, note, name
    class PadList : public juce::Component
    {
    public:
        PadList()                                        { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
        void setMap (std::shared_ptr<const KitMap> m)   { if (m != map) picker.reset(); map = std::move (m); repaint(); }
        int heightNeeded() const;
        void paint (juce::Graphics&) override;
        void mouseMove (const juce::MouseEvent& e) override  { setHover (rowAt (e.position.y)); }
        void mouseExit (const juce::MouseEvent&) override    { setHover (-1); }
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

        // to rename them: the pads picked (one, or a run of them) and where the menu should open
        std::function<void (int first, int last, juce::Rectangle<int> screenArea)> onPadsClicked;
        void menuClosed()                                { picker.menuClosed(); repaint(); }
        void resetPicks()                                { picker.reset(); repaint(); }

    private:
        int rowAt (float y) const;
        int rowNear (float y) const;                     // the nearest row, even above or below the list (while dragging)
        void setHover (int r)                            { if (r != hover) { hover = r; repaint(); } }
        std::shared_ptr<const KitMap> map;
        RangePicker picker;
        int hover = -1;
        static constexpr float rowHeight = 30.0f;
    };

    class DrumMapsPage : public juce::Component,
                         public juce::FileDragAndDropTarget,
                         private juce::Timer
    {
    public:
        explicit DrumMapsPage (SongSavantProcessor&);
        ~DrumMapsPage() override;

        void paint (juce::Graphics&) override;
        void paintOverChildren (juce::Graphics&) override;
        void resized() override;

        void mapPreset (const juce::File&);   // also used by the tests
        void refresh();
        void themeChanged()                   { refresh(); }

        bool isInterestedInFileDrag (const juce::StringArray&) override;
        void fileDragEnter (const juce::StringArray&, int, int) override  { dropHover = true; repaint(); }
        void fileDragExit (const juce::StringArray&) override             { dropHover = false; repaint(); }
        void filesDropped (const juce::StringArray& files, int, int) override;

    private:
        void timerCallback() override;
        void choosePreset();
        void renamePads (int first, int last, juce::Rectangle<int> screenArea);
        void changeMap (const std::function<void (KitMap&)>&);
        juce::String loadSteps (const juce::String& mapName) const;   // step 4, for the DAW in use

        SongSavantProcessor& proc;
        PillButton chooseBtn { "CHOOSE A PRESET...", PillButton::Style::outline },
                   showBtn { "SHOW THE DRUM MAP", PillButton::Style::gold },
                   againBtn { "MAP IT AGAIN", PillButton::Style::outline };
        PadList pads;
        juce::Viewport padsView;
        std::unique_ptr<juce::FileChooser> chooser;
        std::unique_ptr<juce::AlertWindow> nameWindow;

        juce::Rectangle<float> leftPanel, rightPanel, stepsArea, dropArea, statusArea, step4Area, titleArea, resultTextArea, howArea;
        int seenGeneration = -1;
        juce::File savedFile, detailsFile;
        juce::String daw;
        juce::String dropProblem;
        bool dropHover = false;
    };
}
