// Song Savant - the Arrange page: the arrangement map. Sections across (intro, verse, chorus...), the palette's parts
// down; a box for each part in each section. Click a box to switch a part on or off (the drums step through light,
// groove, build and full); click a section's name to change its length, copy, move or remove it. Drag the whole
// song into the DAW: one track per part, a marker per section.
#pragma once

#include "Widgets.h"
#include "core/Arrangement.h"

class SongSavantProcessor;

namespace ss::ui
{
    class ArrangementGrid : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        void show (const Arrangement*, const std::vector<ArrangementPart>*, const PartsPlan*);
        int layout (int width, int height);     // returns the width it needs (more than given: it scrolls)
        void paint (juce::Graphics&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override   { hoverSection = hoverPart = -1; repaint(); }
        void mouseUp (const juce::MouseEvent&) override;

        std::function<void (int section, int part)> onCell;
        std::function<void (int section, juce::Rectangle<int> screenArea)> onSection;

        static constexpr float labelW = 190.0f, headerH = 70.0f;

    private:
        juce::Rectangle<float> cellArea (int section, int part) const;
        juce::Rectangle<float> headerArea (int section) const;
        void hitTestAt (juce::Point<float>, int& section, int& part, bool& header) const;

        const Arrangement* arr = nullptr;
        const std::vector<ArrangementPart>* parts = nullptr;
        const PartsPlan* plan = nullptr;
        std::vector<float> columnX, columnW;
        float rowH = 34.0f;
        int hoverSection = -1, hoverPart = -1;
        bool hoverHeader = false;
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
        const Arrangement& getArrangement() const;

    private:
        void timerCallback() override;
        void sectionMenu (int section, juce::Rectangle<int> area);
        void changed();

        SongSavantProcessor& proc;
        juce::ComboBox shapeBox;
        PillButton resetBtn { "START OVER", PillButton::Style::outline };
        DragChip wholeSong { "WHOLE SONG", DragChip::Kind::midi };
        juce::Viewport view;
        ArrangementGrid grid;

        std::vector<ArrangementPart> parts;
        PartsPlan plan;
        std::shared_ptr<const Catalog> catalog;
        juce::String seen;                                   // what the map was last drawn for
        juce::Rectangle<float> topPanel, titleArea, tipsArea, emptyArea;
        bool hasPalette = false;
    };
}
