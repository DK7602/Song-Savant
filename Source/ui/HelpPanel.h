// Song Savant - the manual, read inside the app or plug-in (Resources/Manual.md): contents on the left, the page on the
// right, a search box that highlights every match. Opens from the ? button in the header or F1. (Adapted from Groove
// Grabber's manual viewer.)
#pragma once

#include "Widgets.h"

namespace ss::ui
{
    //==============================================================================
    // A page of simple markdown: # and ## headings, paragraphs with **bold**, - and 1. lists (one level of nesting),
    // | tables |. Laid out once per width, then only painted.
    class DocView : public juce::Component
    {
    public:
        void setMarkdown (const juce::String&);
        int layoutFor (int width);                       // lays out (if the width changed), returns the height
        void invalidate()                                { laidOutWidth = -1; }

        struct Section { juce::String title; int y = 0; };
        const std::vector<Section>& getSections() const { return sections; }

        int setSearch (const juce::String&);             // highlights the blocks that contain it; returns how many
        int getMatchY (int index) const;                 // where match `index` starts, -1 if none
        int numBlocks() const                            { return (int) blocks.size(); }
        void paint (juce::Graphics&) override;

    private:
        struct Cell { juce::String text; juce::TextLayout layout; juce::Rectangle<float> box; };
        struct Block
        {
            enum Type { h1, h2, h3, para, bullet, number, table } type = para;
            juce::String text;
            int level = 0, ordinal = 0;
            std::vector<std::vector<Cell>> rows;          // a table's rows (row 0 = the header)
            std::vector<float> rowTops;
            juce::TextLayout layout;
            juce::Rectangle<float> box;
            bool match = false;
        };
        std::vector<Block> blocks;
        std::vector<Section> sections;
        std::vector<int> matches;
        int laidOutWidth = -1, laidOutHeight = 0;
    };

    //==============================================================================
    class HelpPanel : public juce::Component
    {
    public:
        HelpPanel();
        ~HelpPanel() override;
        void show();                                     // opens it (and lays it out in the current theme)
        void scrollToSection (int index);
        void themeChanged();
        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        bool keyPressed (const juce::KeyPress&) override;

        DocView& getManual()                             { return manual; }      // (for the tests)
        juce::TextEditor& getSearch()                    { return search; }

    private:
        class Contents;
        class Scroller;
        void layoutDoc();
        void nextMatch();
        void updateCurrentSection();

        DocView manual;
        std::unique_ptr<Scroller> viewport;
        std::unique_ptr<Contents> contents;
        juce::TextEditor search;
        PillButton close { "CLOSE", PillButton::Style::outline };
        juce::Rectangle<int> card, titleArea, matchArea;
        int matchCount = 0, matchIndex = -1;
    };
}
