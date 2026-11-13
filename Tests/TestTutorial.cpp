// SSTests --tutorial DIR: walks through Song Savant the way a new user would (describe a song, the palette, the starter
// parts, ARRANGE, the piano roll, MASTERCLASS and COACH MY SONG, FAVS, LIBRARY, the manual) on the made-up computer the
// tests use, and saves each step as a picture of the real window with where the things to point at are (frames.json
// "marks"), plus the song's starter parts played by the sketch player (beat.wav). tools/tutorial/make_video.py turns
// them into the video tutorial.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FakeComputer.h"
#include "core/SketchPlayer.h"
#include "core/Coach.h"
#include "ui/WelcomePanel.h"
#include <iostream>

namespace
{
    void pumpFor (int ms)
    {
        const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) ms;
        while (juce::Time::getMillisecondCounter() < end)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (15);
    }

    bool shownIn (const juce::Component* c, const juce::Component* root)
    {
        for (auto* p = c; p != nullptr && p != root; p = p->getParentComponent())
            if (! p->isVisible()) return false;
        return true;
    }

    template <typename T>
    std::vector<T*> all (juce::Component& root)
    {
        std::vector<T*> out;
        std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
        {
            if (auto* t = dynamic_cast<T*> (&c); t != nullptr && shownIn (&c, &root)) out.push_back (t);
            for (auto* child : c.getChildren()) walk (*child);
        };
        walk (root);
        return out;
    }

    juce::Button* button (juce::Component& root, const juce::String& text, bool startsWith = false)
    {
        for (auto* b : all<juce::Button> (root))
        {
            const auto t = b->getButtonText().trim();
            if (startsWith ? t.startsWithIgnoreCase (text) : t.equalsIgnoreCase (text)) return b;
        }
        return nullptr;
    }

    // A menu as the app draws one, placed in the window so the picture has it (a real menu closes itself at once here)
    struct DrawnMenu : public juce::Component
    {
        struct Item { juce::String text; bool header = false, separator = false, enabled = true, ticked = false, sub = false, lit = false; };
        std::vector<Item> items;
        std::vector<int> heights;

        void place (juce::Component& parent, juce::Point<int> topLeft)
        {
            parent.addAndMakeVisible (this);
            auto& lf = getLookAndFeel();
            const int border = lf.getPopupMenuBorderSize();
            int w = 120, h = 0;
            heights.clear();
            for (const auto& it : items)
            {
                int iw = 0, ih = 0;
                lf.getIdealPopupMenuItemSize (it.text, it.separator, -1, iw, ih);
                if (it.sub) iw += 20;
                w = juce::jmax (w, iw + 16);
                heights.push_back (ih);
                h += ih;
            }
            setBounds (topLeft.x, topLeft.y, w + 2 * border, h + 2 * border);
            toFront (false);
        }

        void paint (juce::Graphics& g) override
        {
            auto& lf = getLookAndFeel();
            juce::DropShadow (juce::Colours::black.withAlpha (0.5f), 14, { 0, 4 }).drawForRectangle (g, getLocalBounds());
            lf.drawPopupMenuBackground (g, getWidth(), getHeight());
            const int border = lf.getPopupMenuBorderSize();
            int y = border;
            for (size_t i = 0; i < items.size(); ++i)
            {
                const auto& it = items[i];
                const juce::Rectangle<int> r (border, y, getWidth() - 2 * border, heights[i]);
                if (it.header) lf.drawPopupMenuSectionHeader (g, r, it.text);
                else lf.drawPopupMenuItem (g, r, it.separator, it.enabled, it.lit, it.ticked, it.sub, it.text, {}, nullptr, nullptr);
                y += heights[i];
            }
        }
    };

    struct Capture
    {
        SongSavantEditor& ed;
        juce::File dir;
        float scale = 1.5f;
        juce::DynamicObject::Ptr frames = new juce::DynamicObject();
        juce::DynamicObject::Ptr marks = new juce::DynamicObject();

        juce::Rectangle<float> area (juce::Component* c) const     { return ed.getLocalArea (c, c->getLocalBounds().toFloat()); }
        juce::Rectangle<float> areaIn (juce::Component& c, juce::Rectangle<float> r) const   { return ed.getLocalArea (&c, r); }

        void mark (const juce::String& key, juce::Rectangle<float> r)
        {
            if (r.isEmpty()) { std::cout << "  (nothing to mark for " << key << ")" << std::endl; return; }
            juce::Array<juce::var> a { r.getX(), r.getY(), r.getWidth(), r.getHeight() };
            marks->setProperty (key, a);
        }
        void mark (const juce::String& key, juce::Component* c)
        {
            if (c != nullptr && shownIn (c, &ed)) mark (key, area (c));
            else std::cout << "  (no " << key << " on screen)" << std::endl;
        }
        void markIn (const juce::String& key, juce::Component& c, juce::Rectangle<float> r)
        {
            if (r.isEmpty()) { std::cout << "  (nothing to mark for " << key << ")" << std::endl; return; }
            mark (key, areaIn (c, r));
        }
        void markUnion (const juce::String& key, std::initializer_list<juce::Rectangle<float>> rs)
        {
            juce::Rectangle<float> u;
            for (auto& r : rs) if (! r.isEmpty()) u = u.isEmpty() ? r : u.getUnion (r);
            mark (key, u);
        }
        void clearMarks()
        {
            marks = new juce::DynamicObject();
            ed.createComponentSnapshot (ed.getLocalBounds(), true, 0.25f);    // (painted once: the cards know where their tags are)
        }

        void snap (const juce::String& name)
        {
            pumpFor (150);
            auto img = ed.createComponentSnapshot (ed.getLocalBounds(), true, scale);
            const auto f = dir.getChildFile ("frames").getChildFile (name + ".png");
            f.getParentDirectory().createDirectory();
            f.deleteFile();
            {
                juce::FileOutputStream out (f);
                juce::PNGImageFormat png;
                png.writeImageToStream (img, out);
            }
            auto* o = new juce::DynamicObject();
            o->setProperty ("file", "frames/" + name + ".png");
            o->setProperty ("marks", juce::var (marks->clone().release()));
            frames->setProperty (name, juce::var (o));
            std::cout << "  " << name << std::endl;
        }
    };

    // the song's starter parts, played by the sketch player: the video's music bed
    bool renderBeat (const ss::PartsPlan& plan, const std::vector<ss::ArrangementPart>& parts, const juce::File& file)
    {
        const auto song = ss::sketchOfParts (plan, parts);
        const double sr = 44100.0;
        const int total = (int) std::round (song.lengthBeats * 60.0 / juce::jmax (40.0, song.bpm) * sr);
        if (total <= 0) return false;
        ss::SketchPlayer p;
        p.prepare (sr);
        p.setSong (std::make_shared<const ss::SketchSong> (song));
        juce::AudioBuffer<float> mix (2, total), block (2, 512);
        const ss::SketchPlayer::Host app;
        p.process (block, app);
        p.play();
        for (int at = 0; at < total; at += 512)
        {
            const int n = juce::jmin (512, total - at);
            block.setSize (2, n, false, false, true);
            block.clear();
            p.process (block, app);
            for (int c = 0; c < 2; ++c) mix.copyFrom (c, at, block, c, 0, n);
        }
        const float peak = mix.getMagnitude (0, total);
        if (peak > 0.0f) mix.applyGain (0.7f / peak);
        // a 16-bit stereo WAV, written by hand (the tests don't link JUCE's audio file formats)
        juce::MemoryOutputStream data;
        for (int i = 0; i < total; ++i)
            for (int c = 0; c < 2; ++c)
                data.writeShort ((short) juce::jlimit (-32767, 32767, juce::roundToInt (mix.getSample (c, i) * 32767.0f)));
        juce::MemoryOutputStream wav;
        const auto bytes = (int) data.getDataSize();
        wav.write ("RIFF", 4);
        wav.writeInt (36 + bytes);
        wav.write ("WAVEfmt ", 8);
        wav.writeInt (16);
        wav.writeShort (1);
        wav.writeShort (2);
        wav.writeInt ((int) sr);
        wav.writeInt ((int) sr * 4);
        wav.writeShort (4);
        wav.writeShort (16);
        wav.write ("data", 4);
        wav.writeInt (bytes);
        wav.write (data.getData(), data.getDataSize());
        file.deleteFile();
        return file.replaceWithData (wav.getData(), wav.getDataSize());
    }
}

int runTutorial (const juce::File& outDir, const sstest::FakeComputer& fc, const ss::ScanResult& scan, const juce::String& description)
{
    using namespace ss::ui;
    outDir.deleteRecursively();
    outDir.createDirectory();
    writeFlag (WelcomePanel::seenKey, true);
    setLightTheme (false);

    SongSavantProcessor proc;
    proc.locationProvider = [loc = fc.loc] { return loc; };
    std::atomic<bool> cancel { false };
    proc.catalog.setCatalog (ss::buildCatalog (fc.loc, cancel));
    proc.scan.setResult (scan, fc.loc);
    proc.page = 0;
    std::unique_ptr<juce::AudioProcessorEditor> base (proc.createEditor());
    auto* edp = dynamic_cast<SongSavantEditor*> (base.get());
    if (edp == nullptr) return 1;
    auto& ed = *edp;
    ed.setSize (1920, 1080);
    pumpFor (300);
    auto& v = ed.getView();
    auto& song = v.getSong();
    Capture cap { ed, outDir };
    juce::var info (new juce::DynamicObject());

    // what's on every page: the tabs, help, light / dark
    auto common = [&]
    {
        cap.clearMarks();
        juce::Rectangle<float> tabs;
        for (auto* t : { "SONG", "ARRANGE", "MASTERCLASS", "FAVS", "LIBRARY" })
            if (auto* b = button (v, t))
            {
                cap.mark (juce::String ("tab.") + juce::String (t).toLowerCase(), b);
                tabs = tabs.isEmpty() ? cap.area (b) : tabs.getUnion (cap.area (b));
            }
        cap.mark ("tabs", tabs);
        if (auto h = all<HelpButton> (v); ! h.empty()) cap.mark ("help", h[0]);
        if (auto t = all<ThemeButton> (v); ! t.empty()) cap.mark ("theme", t[0]);
    };
    int keysCard = -1, drumsCard = -1, bassCard = -1;
    auto songMarks = [&]
    {
        common();
        for (auto* k : { "form", "understood", "cards", "details", "why", "how", "palettes" }) cap.markIn (k, song, song.tutorialArea (k));
        for (auto* t : { "FIND MY SOUNDS", "NEW PALETTES", "PLAY ALL", "NEW IDEA", "COPY NAME", "SEND TO GROOVE GRABBER", "SAVE SONG",
                         "BLANK SONG", "KEY COMPASS", "PALETTE 1", "PALETTE 2", "PALETTE 3", "MAKE A DRUM MAP" })
            if (auto* b = button (song, t)) cap.mark (juce::String (t).toLowerCase().replaceCharacter (' ', '_'), b);
        if (auto eds = all<juce::TextEditor> (song); ! eds.empty()) cap.mark ("description", eds[0]);
        if (auto boxes = all<juce::ComboBox> (song); ! boxes.empty())
        {
            juce::Rectangle<float> u;
            for (auto* b : boxes) u = u.isEmpty() ? cap.area (b) : u.getUnion (cap.area (b));
            cap.mark ("boxes", u);
        }
        if (auto chips = all<DragChip> (song); ! chips.empty()) cap.mark ("allParts", chips[0]);
        const auto cards = all<PartCard> (song);
        for (size_t i = 0; i < cards.size(); ++i)
        {
            const auto n = juce::String ((int) i);
            cap.mark ("card" + n, cards[i]);
            for (auto* k : { "midi", "play", "edit", "heart", "lock", "prev", "next" })
                cap.markIn ("card" + n + "." + k, *cards[i], cards[i]->tutorialArea (k));
            for (auto [alias, index] : { std::pair { "chordsCard", keysCard }, std::pair { "drumsCard", drumsCard }, std::pair { "bassCard", bassCard } })
                if ((int) i == index)
                {
                    cap.mark (alias, cards[i]);
                    for (auto* k : { "midi", "play", "edit", "heart", "lock", "prev", "next" })
                        cap.markIn (juce::String (alias) + "." + k, *cards[i], cards[i]->tutorialArea (k));
                }
        }
    };

    // ---- 1. describe the song
    const juce::String words = description.isNotEmpty() ? description : juce::String ("modern west coast rap, 94 BPM in A minor");
    v.showPage (MainView::song);
    pumpFor (200);
    songMarks();
    cap.snap ("song-empty");
    {
        // typed a few words at a time
        juce::StringArray steps;
        const auto tokens = juce::StringArray::fromTokens (words, " ", "");
        for (int n = 1; n <= tokens.size(); ++n)
            if (n == tokens.size() || n % 2 == 0) steps.add (tokens.joinIntoString (" ", 0, n));
        int i = 0;
        for (auto& s : steps)
        {
            song.setDescription (s);
            songMarks();
            cap.snap ("type-" + juce::String (++i));
        }
    }
    song.find();
    pumpFor (600);
    juce::String keysRole, drumsRole, bassRole;
    if (auto* pal = proc.song.currentPalette())
        for (int i = 0; i < (int) pal->slots.size(); ++i)
        {
            const auto& slot = pal->slots[(size_t) i];
            const auto t = ss::partTypeFor (slot.part);
            if (t == ss::PartType::chords && keysRole.isEmpty()) { keysRole = slot.part.role; keysCard = i; }
            if (t == ss::PartType::drums && drumsRole.isEmpty()) { drumsRole = slot.part.role; drumsCard = i; }
            if (t == ss::PartType::bass && bassRole.isEmpty()) { bassRole = slot.part.role; bassCard = i; }
        }
    songMarks();
    cap.snap ("song-found");
    info.getDynamicObject()->setProperty ("understood", proc.song.brief.understood.joinIntoString (", "));

    // the palette: other choices on a card (its arrow), the second palette, a card's details
    {
        auto* pal = proc.song.currentPalette();
        juce::StringArray names;
        if (pal != nullptr)
            for (auto& slot : pal->slots) names.add (slot.part.role);
        info.getDynamicObject()->setProperty ("parts", names.joinIntoString (", "));
    }
    if (auto* pal = proc.song.currentPalette(); pal != nullptr && bassCard >= 0 && pal->slots[(size_t) bassCard].numOptions() > 1)
    {
        pal->slots[(size_t) bassCard].choice = 1;          // the bass card's next choice (its right arrow)
        song.selectRole (bassRole);
        pumpFor (300);
    }
    songMarks();
    cap.snap ("song-next");
    if (auto* pal = proc.song.currentPalette(); pal != nullptr && bassCard >= 0) pal->slots[(size_t) bassCard].choice = 0;
    if (auto* b = button (song, "PALETTE 2")) { b->triggerClick(); pumpFor (300); }
    songMarks();
    cap.snap ("song-palette2");
    if (auto* b = button (song, "PALETTE 1")) { b->triggerClick(); pumpFor (300); }
    // the keys card's details (WHY IT WORKS, OTHER CHOICES, HOW TO LOAD IT)
    song.selectRole (keysRole);
    pumpFor (300);
    songMarks();
    cap.snap ("song-details");
    song.selectRole (drumsRole);
    pumpFor (300);
    songMarks();
    cap.snap ("song-drums");
    // a heart: the sound kept in FAVS
    song.toggleFavorite (keysCard);
    pumpFor (200);
    song.selectRole (keysRole);
    pumpFor (200);
    songMarks();
    cap.snap ("song-heart");

    // ---- 2. the starter parts: PLAY ALL, a card's MIDI tag
    song.playAll();
    pumpFor (900);
    songMarks();
    cap.snap ("song-playing");
    song.playAll();          // (stop)
    pumpFor (200);
    songMarks();
    cap.snap ("song-stopped");

    // ---- 3. ARRANGE
    auto& arrange = v.getArrange();
    int verse = -1, chorus = -1, padPart = -1;
    auto arrangeMarks = [&]
    {
        common();
        for (auto* k : { "top", "tips", "transport" }) cap.markIn (k, arrange, arrange.tutorialArea (k));
        auto& grid = arrange.getGrid();
        cap.mark ("grid", &grid);
        const auto& a = proc.song.arrangement;
        for (int s = 0; s < (int) a.sections.size(); ++s)
        {
            cap.markIn ("header" + juce::String (s), grid, grid.tutorialArea ("header:" + juce::String (s)));
            for (int p = 0; p < 8; ++p)
            {
                const auto r = grid.tutorialArea ("cell:" + juce::String (s) + ":" + juce::String (p));
                if (! r.isEmpty()) cap.markIn ("cell" + juce::String (s) + "." + juce::String (p), grid, r);
            }
        }
        cap.markIn ("addSection", grid, grid.tutorialArea ("add"));
        cap.markIn ("introHeader", grid, grid.tutorialArea ("header:0"));
        if (verse >= 0) cap.markIn ("verseHeader", grid, grid.tutorialArea ("header:" + juce::String (verse)));
        if (verse >= 0 && padPart >= 0) cap.markIn ("theBox", grid, grid.tutorialArea ("cell:" + juce::String (verse) + ":" + juce::String (padPart)));
        if (auto chips = all<DragChip> (arrange); ! chips.empty()) cap.mark ("wholeSong", chips[0]);
        auto tb = all<TransportButton> (arrange);
        juce::Rectangle<float> u;
        for (auto* b : tb) u = u.isEmpty() ? cap.area (b) : u.getUnion (cap.area (b));
        if (! u.isEmpty()) cap.mark ("transportButtons", u);
        for (auto* t : { "UNDO", "REDO", "RESET MAP" }) if (auto* b = button (arrange, t)) cap.mark (juce::String (t).toLowerCase().replaceCharacter (' ', '_'), b);
    };
    v.showPage (MainView::arrange);
    pumpFor (400);
    arrangeMarks();
    cap.snap ("arrange");
    // click a box: a part comes into the first verse
    {
        const auto& a = proc.song.arrangement;
        for (int s = 0; s < (int) a.sections.size(); ++s)
        {
            if (verse < 0 && a.sections[(size_t) s].name == "Verse") verse = s;
            if (chorus < 0 && a.sections[(size_t) s].name == "Chorus") chorus = s;
        }
        const auto parts = ss::arrangementParts (*proc.song.currentPalette(), proc.catalog.catalog().get());
        for (int p = 0; p < (int) parts.size(); ++p)
            if (verse >= 0 && parts[(size_t) p].type != ss::PartType::drums && a.sections[(size_t) verse].modeFor (parts[(size_t) p].role).isEmpty())
            { padPart = p; break; }
        info.getDynamicObject()->setProperty ("verse", verse);
        info.getDynamicObject()->setProperty ("chorus", chorus);
        info.getDynamicObject()->setProperty ("boxPart", padPart);
        if (verse >= 0 && padPart >= 0 && arrange.getGrid().onCell)
        {
            arrange.getGrid().onCell (verse, padPart);
            pumpFor (300);
        }
    }
    arrangeMarks();
    cap.snap ("arrange-box");
    // a section's menu (drawn: a real one closes itself here)
    {
        const auto& a = proc.song.arrangement;
        const int s = 0;
        auto menu = std::make_unique<DrawnMenu>();
        menu->items.push_back ({ a.sections[0].name + "  (" + juce::String (a.sections[0].bars) + " bars)", true });
        menu->items.push_back ({ "Add a section before", false, false, true, false, true });
        menu->items.push_back ({ "Add a section after", false, false, true, false, true });
        menu->items.push_back ({ {}, false, true });
        menu->items.push_back ({ "Add bars", false, false, true, false, true, true });
        menu->items.push_back ({ "Remove bars", false, false, true, false, true });
        menu->items.push_back ({ "Length", false, false, true, false, true });
        menu->items.push_back ({ "Copy this section (right after it)" });
        menu->items.push_back ({ "Move left", false, false, false });
        menu->items.push_back ({ "Move right" });
        menu->items.push_back ({ {}, false, true });
        menu->items.push_back ({ "Delete this section" });
        auto& grid = arrange.getGrid();
        const auto head = grid.tutorialArea ("header:" + juce::String (s));
        const auto at = v.getLocalPoint (&grid, head.getBottomLeft().toInt()) + juce::Point<int> (24, 4);
        menu->place (v, at);
        arrangeMarks();
        cap.mark ("menu", menu.get());
        cap.snap ("arrange-menu");
        v.removeChildComponent (menu.get());
    }
    arrange.changeBars (0, 8);           // Add bars > 8 bars more: a longer intro
    pumpFor (300);
    arrangeMarks();
    cap.snap ("arrange-longer");
    arrange.togglePlay();
    pumpFor (1500);
    arrangeMarks();
    cap.snap ("arrange-playing");
    arrange.togglePlay();
    pumpFor (200);

    // ---- 4. the piano roll: the keys in the first chorus
    auto& roll = v.getPianoRoll();
    auto rollMarks = [&]
    {
        common();
        const auto g = roll.gridBounds().toFloat();
        cap.markIn ("rollGrid", roll, g);
        cap.markIn ("ruler", roll, g.withHeight (34.0f));
        cap.markIn ("velocity", roll, g.withTop (g.getBottom() - 58.0f));
        cap.markIn ("notes", roll, g.withTrimmedTop (34.0f).withTrimmedBottom (66.0f));
        for (auto* k : { "panel", "top", "title" }) cap.markIn (k, roll, roll.tutorialArea (k));
        for (auto* t : { "DONE", "DRAW", "SELECT", "ERASE", "STAMP CHORDS", "STRUM", "ARPEGGIATE", "QUANTIZE", "HUMANIZE",
                         "A NEW IDEA FOR THIS PART", "MAKE ONE DIFFERENT", "WITH THE BAND", "ALONE", "UNDO" })
            if (auto* b = button (roll, t)) cap.mark (juce::String (t).toLowerCase().replaceCharacter (' ', '_'), b);
        if (auto chips = all<DragChip> (roll); ! chips.empty()) cap.mark ("rollDrag", chips[0]);
        juce::Rectangle<float> partTabs;
        for (auto* t : { "DRUMS", "BASS", "KEYS", "PAD", "LEAD", "RHYTHM", "STRINGS", "MELODY" })
            if (auto* b = button (roll, t)) partTabs = partTabs.isEmpty() ? cap.area (b) : partTabs.getUnion (cap.area (b));
        if (! partTabs.isEmpty()) cap.mark ("partTabs", partTabs);
    };
    v.showPage (MainView::song);
    pumpFor (200);
    songMarks();
    cap.snap ("song-before-edit");
    v.showPianoRoll (keysRole, chorus);
    pumpFor (500);
    rollMarks();
    cap.snap ("roll");
    roll.newIdea();
    pumpFor (300);
    rollMarks();
    cap.snap ("roll-idea");
    if (roll.onDone) roll.onDone();
    pumpFor (300);

    // ---- 5. MASTERCLASS: the lesson, then COACH MY SONG
    auto& mc = v.getMasterclass();
    v.showPage (MainView::masterclass);
    mc.showCoach (false);
    pumpFor (400);
    auto mcMarks = [&]
    {
        common();
        for (auto* t : { "THE LESSON", "COACH MY SONG", "UNDO", "USE FOR MY SONG" }) if (auto* b = button (mc, t)) cap.mark (juce::String (t).toLowerCase().replaceCharacter (' ', '_'), b);
        if (auto boxes = all<juce::ComboBox> (mc); ! boxes.empty())
        {
            juce::Rectangle<float> u;
            for (auto* b : boxes) u = u.isEmpty() ? cap.area (b) : u.getUnion (cap.area (b));
            cap.mark ("styleBoxes", u);
        }
        auto& coach = mc.getCoach();
        if (mc.isCoaching())
        {
            for (auto* k : { "head", "working" }) cap.markIn ("coach." + juce::String (k), coach, coach.tutorialArea (k));
            for (int i = 0; i < 3; ++i)
                for (auto* k : { "tip", "fix", "show", "meant" })
                {
                    const auto r = coach.tutorialArea (juce::String (k) + ":" + juce::String (i));
                    if (! r.isEmpty()) cap.markIn ("coach." + juce::String (k) + juce::String (i), coach, r);
                }
        }
    };
    mcMarks();
    cap.snap ("lesson");
    mc.showCoach (true);
    pumpFor (400);
    int fixAt = -1;
    {
        auto& coach = mc.getCoach();
        for (int i = 0; i < coach.shownTips(); ++i)
            if (coach.getReport().tips[(size_t) i].fixLabel.isNotEmpty()) { fixAt = i; break; }
    }
    auto coachAliases = [&]
    {
        auto& coach = mc.getCoach();
        if (fixAt < 0) return;
        for (auto* k : { "tip", "fix", "show", "meant" })
        {
            const auto r = coach.tutorialArea (juce::String (k) + ":" + juce::String (fixAt));
            if (! r.isEmpty()) cap.markIn ("coach.the" + juce::String (k), coach, r);
        }
    };
    mcMarks();
    coachAliases();
    cap.snap ("coach");
    {
        auto& coach = mc.getCoach();
        juce::StringArray tips;
        for (int i = 0; i < (int) coach.getReport().tips.size(); ++i)
        {
            const auto& t = coach.getReport().tips[(size_t) i];
            tips.add (t.id + " | " + t.title + " | " + t.fixLabel);
        }
        info.getDynamicObject()->setProperty ("tips", tips.joinIntoString ("\n"));
        info.getDynamicObject()->setProperty ("fixed", fixAt >= 0 ? coach.getReport().tips[(size_t) fixAt].fixLabel : juce::String());
        info.getDynamicObject()->setProperty ("fixAt", fixAt);
        if (fixAt >= 0)
        {
            coach.fix (fixAt);
            pumpFor (400);
        }
    }
    mcMarks();
    cap.snap ("coach-fixed");

    // ---- 6. save, FAVS, LIBRARY, the manual, light
    v.showPage (MainView::song);
    pumpFor (200);
    info.getDynamicObject()->setProperty ("savedAs", v.saveSong());
    pumpFor (300);
    songMarks();
    cap.snap ("song-saved");
    v.showPage (MainView::favorites);
    pumpFor (400);
    common();
    {
        auto& fav = v.getFavorites();
        for (auto* k : { "songs", "sounds" }) cap.markIn (k, fav, fav.tutorialArea (k));
        for (auto* t : { "OPEN", "RENAME", "DELETE", "BACKUPS", "USE IT", "COPY NAME" }) if (auto* b = button (fav, t)) cap.mark (juce::String (t).toLowerCase().replaceCharacter (' ', '_'), b);
    }
    cap.snap ("favs");
    v.showPage (MainView::library);
    pumpFor (300);
    auto& lib = v.getLibrary();
    lib.setSearchText ("warm");
    lib.selectRow (0);
    pumpFor (400);
    common();
    if (auto eds = all<juce::TextEditor> (lib); ! eds.empty()) cap.mark ("search", eds[0]);
    if (auto lists = all<juce::ListBox> (lib); ! lists.empty()) cap.mark ("list", lists[0]);
    if (auto tables = all<juce::TableListBox> (lib); ! tables.empty()) cap.mark ("list", tables[0]);
    cap.snap ("library");
    v.showPage (MainView::song);
    pumpFor (200);
    songMarks();
    cap.snap ("song-final");
    v.showHelp();
    pumpFor (500);
    common();
    cap.snap ("manual");
    v.getHelp().setVisible (false);
    pumpFor (200);
    v.toggleTheme();
    pumpFor (400);
    songMarks();
    cap.snap ("song-light");
    v.toggleTheme();

    // the music bed: the song's starter parts
    const auto parts = ss::arrangementParts (*proc.song.currentPalette(), proc.catalog.catalog().get());
    const bool beat = renderBeat (proc.song.partsPlan(), parts, outDir.getChildFile ("beat.wav"));
    std::cout << (beat ? "  beat.wav" : "  (couldn't write the beat)") << std::endl;
    info.getDynamicObject()->setProperty ("version", SS_VERSION);
    info.getDynamicObject()->setProperty ("words", words);

    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty ("frames", juce::var (cap.frames.get()));
    root->setProperty ("scale", cap.scale);
    root->setProperty ("info", info);
    outDir.getChildFile ("frames.json").replaceWithText (juce::JSON::toString (juce::var (root.get())));
    std::cout << "  frames.json" << std::endl;
    return beat ? 0 : 1;
}
