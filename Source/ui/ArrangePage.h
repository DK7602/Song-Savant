// Song Savant - the Arrange page: the arrangement map. Sections across (intro, verse, chorus...), the palette's parts
// down; a box for each part in each section. Click a box to switch a part on or off (the drums step through light,
// groove, build and full); click a section's name to change its length, copy, move or remove it. Hear it with the
// transport (to the start, rewind, play / stop, pause, fast forward, to the end) or click the bar ruler to move the
// playhead. Drag the whole song into the DAW: one track per part, a marker per section.
#pragma once

#include "Widgets.h"
#include "core/Arrangement.h"

class SongSavantProcessor;

namespace ss::ui
{
    class ArrangementGrid : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        void show (const Arrangement*, const std::vector<ArrangementPart>*, const PartsPlan*, const PatternBook* = nullptr);
        void setPlayhead (double beats, bool playing);   // where the playhead is (-1: none), and whether it's moving
        double beatsAt (float x) const;          // the bar line nearest x on the ruler, in beats
        float xAt (double beats) const;          // where a point in the song is across the map (-1: nowhere)
        int layout (int width, int height);     // returns the width it needs (more than given: it scrolls)
        void paint (juce::Graphics&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override   { hoverSection = hoverPart = -1; hoverHeader = hoverRuler = false; repaint(); }
        void mouseDown (const juce::MouseEvent&) override;
        static bool editable (const ArrangementPart&);
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

        std::function<void (int section, int part)> onCell;
        std::function<void (int section, juce::Rectangle<int> screenArea)> onSection;
        std::function<void (double beats)> onSeek;                 // the ruler was clicked (or dragged)
        std::function<void (int section, int part)> onEditCell;    // a box double-clicked: the piano roll
        std::function<void (int section, int part)> onDragCell;    // a box dragged out: that part in that section, as MIDI
        std::function<void (int part)> onDragPart;                 // a part's name dragged out: the whole part

        // the bar ruler along the top (click to move the playhead), then the sections' names, then a row per part
        static constexpr float labelW = 190.0f, rulerH = 20.0f, headerH = 90.0f;

    private:
        juce::Rectangle<float> cellArea (int section, int part) const;
        juce::Rectangle<float> headerArea (int section) const;
        void hitTestAt (juce::Point<float>, int& section, int& part, bool& header) const;

        const Arrangement* arr = nullptr;
        const std::vector<ArrangementPart>* parts = nullptr;
        const PartsPlan* plan = nullptr;
        const PatternBook* book = nullptr;
        std::vector<float> columnX, columnW;
        int pressSection = -1, pressPart = -1;
        bool pressLabel = false, draggedOut = false;
        float rowH = 34.0f;
        double playhead = -1.0;
        bool playheadMoving = false, onRuler = false;
        int hoverSection = -1, hoverPart = -1;
        bool hoverHeader = false, hoverRuler = false;
    };

    class ArrangePage : public juce::Component, private juce::Timer
    {
    public:
        explicit ArrangePage (SongSavantProcessor&);
        ~ArrangePage() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        void visibilityChanged() override                   { if (isVisible()) refresh(); }
        void themeChanged()                                 { refresh(); }

        void refresh();                                      // the map for the song's current palette (also the tests)
        juce::File writeWholeSong();                         // the MIDI file to drag (also the tests)
        void togglePlay();                                   // PLAY / STOP (also the tests)
        std::function<void (const juce::String& role, int section)> onEditPattern;   // a box double-clicked
        juce::File writeBit (int section, int part);         // one box (section >= 0) or a whole part, as MIDI (also the tests)
        juce::String positionText() const;                   // "Bar 17 of 72  |  Verse  |  0:34 / 2:02" (also the tests)
        const Arrangement& getArrangement() const;

    private:
        void timerCallback() override;
        void showTransport();
        void sectionMenu (int section, juce::Rectangle<int> area);
        void changed();

        SongSavantProcessor& proc;
        juce::ComboBox shapeBox;
        PillButton resetBtn { "RESET MAP", PillButton::Style::outline };
        using Icon = TransportButton::Icon;
        TransportButton toStartBtn { Icon::toStart }, rewindBtn { Icon::rewind }, playBtn { Icon::play },
                        pauseBtn { Icon::pause }, forwardBtn { Icon::forward }, toEndBtn { Icon::toEnd };
        DragChip wholeSong { "WHOLE SONG", DragChip::Kind::midi };
        juce::Viewport view;
        ArrangementGrid grid;

        std::vector<ArrangementPart> parts;
        PartsPlan plan;
        std::shared_ptr<const Catalog> catalog;
        juce::String seen;                                   // what the map was last drawn for
        juce::Rectangle<float> topPanel, titleArea, tipsArea, emptyArea, transportInfo, transportHint;
        juce::String shownPosition;
        bool hasPalette = false, wasPlaying = false;
    };
}
