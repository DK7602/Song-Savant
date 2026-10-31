#include "Theme.h"
#include "SSBinaryData.h"

namespace ss::ui
{
    const Palette& pal()
    {
        static const Palette p;
        return p;
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

    juce::Image owlImage()      { return juce::ImageCache::getFromMemory (SSBinary::owl_png, SSBinary::owl_pngSize); }
    juce::Image wordmarkImage() { return juce::ImageCache::getFromMemory (SSBinary::wordmark_png, SSBinary::wordmark_pngSize); }

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

        juce::DropShadow (juce::Colours::black.withAlpha (0.45f), 22, { 0, 6 }).drawForPath (g, path);
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
}
