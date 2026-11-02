#include "Theme.h"
#include "SSBinaryData.h"
#include "core/Paths.h"
#include <cmath>

namespace ss::ui
{
    namespace
    {
        Palette lightPalette()
        {
            Palette p;
            p.bgTop    = juce::Colour (0xfffbf7ef);
            p.bgBottom = juce::Colour (0xffefe6d6);
            p.panel    = juce::Colour (0xd8fffdf8);
            p.panelHi  = juce::Colour (0xfffffcf5);
            p.gold     = juce::Colour (0xffb8861f);    // a deeper gold, readable on ivory
            p.goldDeep = juce::Colour (0xff8a6014);
            p.goldText = juce::Colour (0xff2a1e05);
            p.blue     = juce::Colour (0xff3d5afe);
            p.purple   = juce::Colour (0xff7c4dff);
            p.text     = juce::Colour (0xff1d1640);
            p.muted    = juce::Colour (0xff57527c);
            p.dim      = juce::Colour (0xff9a95b5);
            p.good     = juce::Colour (0xff1e9e5a);
            p.warn     = juce::Colour (0xffc77700);
            p.bad      = juce::Colour (0xffd93d5e);
            return p;
        }

        // The settings file is opened when it's needed and closed straight away. Never keep JUCE objects with timers,
        // fonts or images in a static: in a DAW those are let go only when the plug-in is unloaded at quit, which can
        // freeze the DAW on Windows (it did with Groove Grabber's fonts).
        std::unique_ptr<juce::PropertiesFile> openSettings()
        {
            juce::PropertiesFile::Options o;
            o.applicationName = "settings";
            o.filenameSuffix = ".xml";
            o.storageFormat = juce::PropertiesFile::storeAsXML;
            o.millisecondsBeforeSaving = 0;
            return std::make_unique<juce::PropertiesFile> (ss::paths::appDataDir().getChildFile ("settings.xml"), o);
        }

        bool& lightFlag()
        {
            static bool light = openSettings()->getBoolValue ("lightTheme", false);
            return light;
        }
    }

    const Palette& pal()
    {
        static const Palette dark;
        static const Palette light = lightPalette();
        return lightFlag() ? light : dark;
    }

    bool isLightTheme()   { return lightFlag(); }

    void setLightTheme (bool on)
    {
        lightFlag() = on;
        auto file = openSettings();
        file->setValue ("lightTheme", on);
        file->saveIfNeeded();
    }

    juce::Font sans (float h, bool bold)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }

    juce::Font serif (float h, bool bold)
    {
        static const juce::String name = []
        {
            const auto names = juce::Font::findAllTypefaceNames();
            for (auto* n : { "Georgia", "Cambria", "Palatino Linotype", "Book Antiqua", "DejaVu Serif", "Liberation Serif" })
                if (names.contains (n)) return juce::String (n);
            return juce::Font::getDefaultSerifFontName();
        }();
        return juce::Font (juce::FontOptions (name, h, bold ? juce::Font::bold : juce::Font::plain));
    }

    juce::Image owlImage()
    {
        return isLightTheme() ? juce::ImageCache::getFromMemory (SSBinary::owllight_png, SSBinary::owllight_pngSize)
                              : juce::ImageCache::getFromMemory (SSBinary::owl_png, SSBinary::owl_pngSize);
    }
    juce::Image wordmarkImage()
    {
        return isLightTheme() ? juce::ImageCache::getFromMemory (SSBinary::wordmarklight_png, SSBinary::wordmarklight_pngSize)
                              : juce::ImageCache::getFromMemory (SSBinary::wordmark_png, SSBinary::wordmark_pngSize);
    }

    void drawBackground (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const auto& p = pal();
        g.setGradientFill (juce::ColourGradient (p.bgTop, r.getX(), r.getY(), p.bgBottom, r.getRight(), r.getBottom(), false));
        g.fillRect (r);

        auto glow = [&] (juce::Point<float> c, float radius, juce::Colour col)
        {
            juce::ColourGradient grad (col, c, col.withAlpha (0.0f), c.translated (radius, 0), true);
            g.setGradientFill (grad);
            g.fillEllipse (juce::Rectangle<float> (radius * 2, radius * 2).withCentre (c));
        };
        glow ({ r.getX() + r.getWidth() * 0.08f, r.getY() + r.getHeight() * 0.05f }, r.getWidth() * 0.45f, p.blue.withAlpha (0.16f));
        glow ({ r.getRight() - r.getWidth() * 0.05f, r.getBottom() - r.getHeight() * 0.05f }, r.getWidth() * 0.5f, p.purple.withAlpha (0.15f));
        glow ({ r.getCentreX(), r.getY() + 40.0f }, r.getWidth() * 0.3f, p.gold.withAlpha (0.05f));
    }

    void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float radius)
    {
        const auto& p = pal();
        juce::Path path;
        path.addRoundedRectangle (r, radius);

        juce::DropShadow (juce::Colours::black.withAlpha (isLightTheme() ? 0.10f : 0.45f), 22, { 0, 6 }).drawForPath (g, path);
        g.setGradientFill (juce::ColourGradient (p.panelHi.withAlpha (0.82f), r.getX(), r.getY(), p.panel, r.getX(), r.getBottom(), false));
        g.fillPath (path);

        juce::ColourGradient edge (p.gold.withAlpha (0.65f), r.getX(), r.getY(), p.purple.withAlpha (0.55f), r.getRight(), r.getBottom(), false);
        edge.addColour (0.5, p.blue.withAlpha (0.35f));
        g.setGradientFill (edge);
        g.strokePath (path, juce::PathStrokeType (1.2f));
    }

    void drawSectionTitle (juce::Graphics& g, juce::Rectangle<float> r, int number, const juce::String& title)
    {
        const auto& p = pal();
        auto badge = r.removeFromLeft (r.getHeight()).reduced (2.0f);
        g.setGradientFill (juce::ColourGradient (p.gold, badge.getX(), badge.getY(), p.goldDeep, badge.getRight(), badge.getBottom(), false));
        g.fillEllipse (badge);
        g.setColour (p.goldText);
        g.setFont (sans (badge.getHeight() * 0.62f, true));
        g.drawText (juce::String (number), badge, juce::Justification::centred);

        r.removeFromLeft (12.0f);
        g.setColour (p.text);
        g.setFont (serif (r.getHeight() * 0.86f));
        g.drawText (title, r, juce::Justification::centredLeft);
    }

    static juce::AttributedString attributed (const juce::String& text, const juce::Font& f, juce::Colour c)
    {
        juce::AttributedString s;
        s.setText (text);
        s.setFont (f);
        s.setColour (c);
        s.setJustification (juce::Justification::topLeft);
        s.setWordWrap (juce::AttributedString::byWord);
        s.setLineSpacing (2.0f);
        return s;
    }

    float paragraphHeight (const juce::String& text, float width, const juce::Font& f)
    {
        juce::TextLayout tl;
        tl.createLayout (attributed (text, f, juce::Colours::white), width);
        return tl.getHeight();
    }

    float drawParagraph (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, const juce::Font& f, juce::Colour c)
    {
        juce::TextLayout tl;
        tl.createLayout (attributed (text, f, c), r.getWidth());
        tl.draw (g, r);
        return tl.getHeight();
    }

    //==============================================================================
    SavantLookAndFeel::SavantLookAndFeel()   { applyColours(); }

    void SavantLookAndFeel::applyColours()
    {
        const auto& p = pal();
        setColourScheme ({ p.bgTop, p.panelHi, p.panelHi, p.dim, p.text, p.purple, p.goldText, p.gold, p.text });
        setColour (juce::TextEditor::backgroundColourId, p.panelHi.withAlpha (0.8f));
        setColour (juce::TextEditor::textColourId, p.text);
        setColour (juce::TextEditor::highlightColourId, p.purple.withAlpha (0.5f));
        setColour (juce::CaretComponent::caretColourId, p.gold);
        setColour (juce::ComboBox::textColourId, p.text);
        setColour (juce::PopupMenu::backgroundColourId, p.panelHi);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, p.purple.withAlpha (0.6f));
        setColour (juce::PopupMenu::highlightedTextColourId, p.text);
        setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::TooltipWindow::backgroundColourId, p.panelHi);
        setColour (juce::TooltipWindow::textColourId, p.text);
        setColour (juce::TooltipWindow::outlineColourId, p.gold.withAlpha (0.6f));
    }

    void SavantLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
    {
        const auto& p = pal();
        auto r = juce::Rectangle<float> ((float) w, (float) h).reduced (1.0f);
        g.setColour (p.panelHi.withAlpha (box.isMouseOver (true) ? 1.0f : 0.8f));
        g.fillRoundedRectangle (r, 9.0f);
        g.setColour ((box.hasKeyboardFocus (true) ? p.gold : p.dim).withAlpha (0.9f));
        g.drawRoundedRectangle (r, 9.0f, 1.0f);
        juce::Path arrow;
        const float cx = r.getRight() - 16.0f, cy = r.getCentreY();
        arrow.addTriangle (cx - 5.0f, cy - 2.5f, cx + 5.0f, cy - 2.5f, cx, cy + 3.5f);
        g.setColour (p.gold);
        g.fillPath (arrow);
    }

    void SavantLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&)
    {
        g.setColour (pal().panelHi.withAlpha (0.85f));
        g.fillRoundedRectangle (juce::Rectangle<float> ((float) w, (float) h).reduced (1.0f), 10.0f);
    }

    void SavantLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& ed)
    {
        const auto& p = pal();
        g.setColour (ed.hasKeyboardFocus (true) ? p.gold : p.dim);
        g.drawRoundedRectangle (juce::Rectangle<float> ((float) w, (float) h).reduced (1.0f), 10.0f, ed.hasKeyboardFocus (true) ? 1.4f : 1.0f);
    }

    void SavantLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                                           int thumbStart, int thumbSize, bool over, bool down)
    {
        const auto& p = pal();
        auto track = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (2.0f);
        auto thumb = vertical ? juce::Rectangle<float> (track.getX(), (float) thumbStart, track.getWidth(), (float) thumbSize)
                              : juce::Rectangle<float> ((float) thumbStart, track.getY(), (float) thumbSize, track.getHeight());
        g.setColour ((down ? p.gold : over ? p.purple.brighter (0.2f) : p.purple).withAlpha (0.75f));
        g.fillRoundedRectangle (thumb.reduced (1.0f), juce::jmin (thumb.getWidth(), thumb.getHeight()) * 0.5f);
    }

    juce::Colour kindColour (Kind k)
    {
        const auto& p = pal();
        switch (k)
        {
            case Kind::piano: case Kind::keys: case Kind::organ:        return p.gold;
            case Kind::bass:                                           return juce::Colour (0xff4fc3f7);
            case Kind::pad: case Kind::strings:                         return p.purple.brighter (0.3f);
            case Kind::lead: case Kind::synth: case Kind::sequence:     return p.blue.brighter (0.3f);
            case Kind::drums: case Kind::percussion:                    return juce::Colour (0xffff8a65);
            case Kind::guitar: case Kind::pluck:                        return juce::Colour (0xff81c784);
            case Kind::brass: case Kind::woodwinds:                     return juce::Colour (0xffffd54f);
            case Kind::vocal:                                          return juce::Colour (0xfff48fb1);
            case Kind::world: case Kind::fx: case Kind::other:          break;
        }
        return p.muted;
    }

    void drawKindIcon (juce::Graphics& g, Kind k, juce::Rectangle<float> r, juce::Colour c)
    {
        r = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()), juce::jmin (r.getWidth(), r.getHeight()));
        const float w = r.getWidth(), x = r.getX(), y = r.getY();
        g.setColour (c);
        const juce::PathStrokeType stroke (juce::jmax (1.5f, w * 0.045f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        juce::Path p;
        switch (k)
        {
            case Kind::piano: case Kind::keys: case Kind::organ:
            {
                auto body = r.reduced (w * 0.08f, w * 0.22f);
                p.addRoundedRectangle (body, w * 0.04f);
                g.strokePath (p, stroke);
                const float kw = body.getWidth() / 7.0f;
                for (int i = 1; i < 7; ++i)
                    g.drawLine (body.getX() + kw * (float) i, body.getY(), body.getX() + kw * (float) i, body.getBottom(), stroke.getStrokeThickness() * 0.7f);
                for (int i : { 0, 1, 3, 4, 5 })
                    g.fillRect (juce::Rectangle<float> (body.getX() + kw * ((float) i + 0.65f), body.getY(), kw * 0.7f, body.getHeight() * 0.58f));
                break;
            }
            case Kind::guitar: case Kind::bass: case Kind::strings:
            {
                p.addEllipse (x + w * 0.12f, y + w * 0.48f, w * 0.38f, w * 0.38f);
                p.startNewSubPath (x + w * 0.42f, y + w * 0.58f);
                p.lineTo (x + w * 0.86f, y + w * 0.14f);
                g.strokePath (p, stroke);
                g.fillEllipse (x + w * 0.25f, y + w * 0.61f, w * 0.12f, w * 0.12f);
                break;
            }
            case Kind::drums: case Kind::percussion:
            {
                p.addEllipse (x + w * 0.15f, y + w * 0.42f, w * 0.7f, w * 0.22f);
                p.startNewSubPath (x + w * 0.15f, y + w * 0.53f);
                p.lineTo (x + w * 0.15f, y + w * 0.78f);
                p.quadraticTo (x + w * 0.5f, y + w * 0.95f, x + w * 0.85f, y + w * 0.78f);
                p.lineTo (x + w * 0.85f, y + w * 0.53f);
                p.startNewSubPath (x + w * 0.3f, y + w * 0.12f); p.lineTo (x + w * 0.48f, y + w * 0.44f);
                p.startNewSubPath (x + w * 0.72f, y + w * 0.1f); p.lineTo (x + w * 0.56f, y + w * 0.44f);
                g.strokePath (p, stroke);
                break;
            }
            case Kind::vocal:
            {
                p.addRoundedRectangle (x + w * 0.36f, y + w * 0.1f, w * 0.28f, w * 0.46f, w * 0.14f);
                p.startNewSubPath (x + w * 0.24f, y + w * 0.42f);
                p.quadraticTo (x + w * 0.5f, y + w * 0.82f, x + w * 0.76f, y + w * 0.42f);
                p.startNewSubPath (x + w * 0.5f, y + w * 0.66f); p.lineTo (x + w * 0.5f, y + w * 0.88f);
                g.strokePath (p, stroke);
                break;
            }
            case Kind::brass: case Kind::woodwinds:
            {
                p.startNewSubPath (x + w * 0.1f, y + w * 0.5f); p.lineTo (x + w * 0.55f, y + w * 0.5f);
                p.lineTo (x + w * 0.88f, y + w * 0.22f); p.lineTo (x + w * 0.88f, y + w * 0.78f); p.lineTo (x + w * 0.55f, y + w * 0.5f);
                g.strokePath (p, stroke);
                break;
            }
            case Kind::pluck: case Kind::fx:
            {
                static const float stars[3][3] = { { 0.5f, 0.42f, 0.32f }, { 0.25f, 0.75f, 0.14f }, { 0.78f, 0.74f, 0.12f } };
                for (auto& st : stars)
                {
                    juce::Path star;
                    star.addStar ({ x + w * st[0], y + w * st[1] }, 4, w * st[2] * 0.3f, w * st[2]);
                    g.fillPath (star);
                }
                break;
            }
            case Kind::pad: case Kind::synth: case Kind::lead: case Kind::sequence: case Kind::world: case Kind::other:   // a wave
            {
                const bool smooth = k == Kind::pad || k == Kind::world || k == Kind::other;
                p.startNewSubPath (x + w * 0.08f, y + w * 0.5f);
                for (int i = 1; i <= 40; ++i)
                {
                    const float t = (float) i / 40.0f;
                    const float phase = t * juce::MathConstants<float>::twoPi * 2.0f;
                    const float v = smooth ? std::sin (phase) : (k == Kind::sequence ? (std::sin (phase * 2.0f) > 0 ? 1.0f : -1.0f)
                                                                                     : 2.0f * (t * 4.0f - std::floor (t * 4.0f)) - 1.0f);
                    p.lineTo (x + w * (0.08f + 0.84f * t), y + w * (0.5f - 0.24f * v));
                }
                g.strokePath (p, stroke);
                break;
            }
        }
    }
}
