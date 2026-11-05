// Song Savant - the Masterclass page: learn a genre or style. It follows the genre and style on the Song page, or
// you pick another one to study. Shows what the style is, the sounds and instruments it uses (and the best ones in
// your own library), its drums (with the groove drawn out), bass, chords in your key, arrangement, mix and tips, and
// artists to listen to.
#pragma once

#include "Widgets.h"
#include "CoachView.h"
#include "core/StyleGuide.h"
#include "core/Catalog.h"

class SongSavantProcessor;

namespace ss::ui
{
    // The lesson itself: cards in one or two columns, inside a scrolling view
    class Lesson : public juce::Component
    {
    public:
        struct Card
        {
            juce::String title;
            juce::StringArray points;              // one bullet each
            bool groove = false;                   // draw the drum groove under the points
            bool fullWidth = false;
            juce::Rectangle<float> area;
        };

        void setContent (const StyleGuide&, std::vector<Card>, const juce::String& headline, const juce::String& subline);
        int layout (int width);                    // places the cards; returns the height needed
        void paint (juce::Graphics&) override;

        const std::vector<Card>& getCards() const  { return cards; }

    private:
        float cardHeight (const Card&, float width) const;
        void paintGroove (juce::Graphics&, juce::Rectangle<float>) const;
        StyleGuide guide;
        std::vector<Card> cards;
        juce::String headline, subline;
        juce::Rectangle<float> headArea;
    };

    class MasterclassPage : public juce::Component, private juce::Timer
    {
    public:
        explicit MasterclassPage (SongSavantProcessor&);
        ~MasterclassPage() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        void visibilityChanged() override;
        void themeChanged()                         { rebuild(); }

        void study (Genre, const juce::String& styleId);   // pick a style to learn (also the tests)
        void followSong();                                  // back to the Song page's genre and style
        bool isFollowingSong() const                        { return following; }
        const Lesson& getLesson() const                     { return lesson; }

        std::function<void()> onUseForSong;                 // the Song page should show the new genre and style

        // COACH MY SONG: tips for your own song, from what its style usually does (in place of the lesson)
        void showCoach (bool);
        bool isCoaching() const                             { return coaching; }
        CoachView& getCoach()                               { return coach; }
        std::function<void (int section)> onShowSection;                         // SHOW ME on ARRANGE
        std::function<void (const juce::String& role, int section)> onShowPart;   // SHOW ME in the piano roll
        std::function<void()> onSongChanged;                // a FIX IT (or its UNDO) changed the song

    private:
        void timerCallback() override;
        void rebuild();
        void fillStyles();
        Genre currentGenre() const;
        juce::String currentStyle() const;

        SongSavantProcessor& proc;
        juce::ComboBox genreBox, styleBox;
        PillButton followBtn { "BACK TO MY SONG", PillButton::Style::outline },
                   useBtn { "USE FOR MY SONG", PillButton::Style::gold };
        juce::Viewport view;
        Lesson lesson;
        CoachView coach;
        PillButton lessonTab { "THE LESSON", PillButton::Style::subtle },
                   coachTab { "COACH MY SONG", PillButton::Style::subtle },
                   undoBtn { "UNDO", PillButton::Style::outline };
        void layoutCoach();
        static juce::StringArray statusLines (bool coaching, bool following);   // longest first
    public:
        juce::Rectangle<float> getStatusArea() const        { return statusArea; }   // (the tests)
    private:
        bool coaching = false;
        bool following = true;
        Genre genre = Genre::pop;
        juce::String styleId;
        int seenCatalog = -1;
        juce::Rectangle<float> topPanel, titleArea, statusArea;
    };
}
