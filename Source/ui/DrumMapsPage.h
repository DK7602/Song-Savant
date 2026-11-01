// Song Savant - the Drum Maps page: drop a drum kit preset you saved in your DAW, Song Savant plays every key in the
// background, names each pad and saves a Cubase drum map for it.
#pragma once

#include "Widgets.h"
#include "core/KitMap.h"

class SongSavantProcessor;

namespace ss::ui
{
    // The pads that were found: key, note, name
    class PadList : public juce::Component
    {
    public:
        PadList()                                        { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
        void setMap (std::shared_ptr<const KitMap> m)   { map = std::move (m); repaint(); }
        int heightNeeded() const;
        void paint (juce::Graphics&) override;
        void mouseMove (const juce::MouseEvent& e) override  { setHover (rowAt (e.position.y)); }
        void mouseExit (const juce::MouseEvent&) override    { setHover (-1); }
        void mouseUp (const juce::MouseEvent&) override;

        std::function<void (int pad, juce::Rectangle<int> screenArea)> onPadClicked;   // to rename it

    private:
        int rowAt (float y) const;
        void setHover (int r)                            { if (r != hover) { hover = r; repaint(); } }
        std::shared_ptr<const KitMap> map;
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
        void renamePad (int pad, juce::Rectangle<int> screenArea);
        void changePad (int pad, const std::function<void (KitMap&, KitPad&)>&);

        SongSavantProcessor& proc;
        PillButton chooseBtn { "CHOOSE A PRESET...", PillButton::Style::outline },
                   showBtn { "SHOW THE DRUM MAP", PillButton::Style::gold },
                   againBtn { "MAP IT AGAIN", PillButton::Style::outline };
        PadList pads;
        juce::Viewport padsView;
        std::unique_ptr<juce::FileChooser> chooser;
        std::unique_ptr<juce::AlertWindow> nameWindow;

        juce::Rectangle<float> leftPanel, rightPanel, stepsArea, dropArea, statusArea, titleArea, resultTextArea, howArea;
        int seenGeneration = -1;
        juce::File savedFile;
        juce::String dropProblem;
        bool dropHover = false;
    };
}
