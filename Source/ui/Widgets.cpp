#include "Widgets.h"

namespace ss::ui
{
    //==============================================================================
    PillButton::PillButton (const juce::String& buttonText, Style s) : juce::Button (buttonText), style (s)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void PillButton::paintButton (juce::Graphics& g, bool over, bool down)
    {
        const auto& p = pal();
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        const float radius = r.getHeight() * 0.5f;
        const bool on = getToggleState();
        const float alpha = isEnabled() ? 1.0f : 0.38f;
        juce::Path path;
        path.addRoundedRectangle (r, radius);

        if (style == Style::gold)
        {
            if (isEnabled())
                juce::DropShadow (p.gold.withAlpha (over ? 0.55f : 0.35f), over ? 18 : 12, {}).drawForPath (g, path);
            juce::ColourGradient grad (p.gold.brighter (over ? 0.15f : 0.0f), r.getX(), r.getY(), p.goldDeep, r.getX(), r.getBottom(), false);
            g.setGradientFill (grad);
            g.setOpacity (alpha);
            g.fillPath (path);
            g.setColour (juce::Colours::white.withAlpha (0.35f * alpha));
            g.strokePath (path, juce::PathStrokeType (1.0f));
            g.setColour (p.goldText.withAlpha (alpha));
        }
        else if (style == Style::outline)
        {
            g.setColour (p.panelHi.withAlpha ((over ? 0.95f : 0.6f) * alpha));
            g.fillPath (path);
            juce::ColourGradient edge (p.gold.withAlpha (alpha), r.getX(), r.getY(), p.purple.withAlpha (alpha), r.getRight(), r.getBottom(), false);
            g.setGradientFill (edge);
            g.strokePath (path, juce::PathStrokeType (1.4f));
            g.setColour (p.text.withAlpha (alpha));
        }
        else
        {
            const auto fill = on ? (getComponentID() == "bad" ? p.bad : p.good).withAlpha (0.22f)
                                 : p.panelHi.withAlpha (over ? 0.9f : 0.55f);
            g.setColour (fill.withMultipliedAlpha (alpha));
            g.fillPath (path);
            g.setColour ((on ? (getComponentID() == "bad" ? p.bad : p.good) : p.dim).withAlpha (alpha));
            g.strokePath (path, juce::PathStrokeType (1.0f));
            g.setColour ((on ? p.text : p.muted).withAlpha (alpha));
        }

        if (down) r = r.translated (0.0f, 1.0f);
        g.setFont (sans (juce::jmin (15.0f, r.getHeight() * 0.42f), true));
        g.drawText (getButtonText(), r, juce::Justification::centred);
    }

    //==============================================================================
    DragChip::DragChip (const juce::String& t, Kind k) : title (t), kind (k) {}

    void DragChip::setFile (const juce::File& f, const juce::String& l, const juce::String& why)
    {
        file = f;
        label = l;
        reason = why;
        setMouseCursor (hasFile() ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
        setTooltip (hasFile() ? "Drag this into Cubase: " + file.getFileName() : reason);
        repaint();
    }

    void DragChip::paint (juce::Graphics& g)
    {
        const auto& p = pal();
        auto r = getLocalBounds().toFloat().reduced (1.5f);
        const bool live = hasFile();
        const bool over = live && isMouseOver();

        juce::Path box;
        box.addRoundedRectangle (r, 12.0f);
        g.setColour (p.panelHi.withAlpha (over ? 0.95f : 0.65f));
        g.fillPath (box);
        g.setColour ((live ? (over ? p.gold : p.blue.withAlpha (0.7f)) : p.dim.withAlpha (0.5f)));
        const float dashes[] = { 5.0f, 4.0f };
        juce::Path dashed;
        juce::PathStrokeType (1.2f).createDashedStroke (dashed, box, dashes, 2);
        g.fillPath (dashed);

        // icon
        auto icon = r.removeFromLeft (r.getHeight()).reduced (12.0f);
        g.setGradientFill (juce::ColourGradient (live ? p.blue : p.dim, icon.getX(), icon.getY(), live ? p.purple : p.dim.darker(), icon.getRight(), icon.getBottom(), false));
        g.fillEllipse (icon);
        g.setColour (live ? p.text : p.muted);
        auto in = icon.reduced (icon.getWidth() * 0.27f);
        if (kind == Kind::midi)
        {
            // three little note bars
            const float w = in.getWidth() / 5.0f;
            for (int i = 0; i < 3; ++i)
                g.fillRoundedRectangle (in.getX() + (float) i * w * 2.0f, in.getY() + in.getHeight() * (0.15f * (float) i), w, in.getHeight() * (1.0f - 0.15f * (float) i), 1.5f);
        }
        else
        {
            // a sliders glyph
            for (int i = 0; i < 3; ++i)
            {
                const float x = in.getX() + in.getWidth() * (0.15f + 0.35f * (float) i);
                g.drawLine (x, in.getY(), x, in.getBottom(), 1.6f);
                const float y = in.getY() + in.getHeight() * (i == 1 ? 0.25f : 0.65f);
                g.fillEllipse (x - 3.0f, y - 3.0f, 6.0f, 6.0f);
            }
        }

        // text
        auto right = r.removeFromRight (78.0f);
        r.removeFromLeft (4.0f);
        auto top = r.removeFromTop (r.getHeight() * 0.5f);
        g.setColour (live ? p.text : p.muted);
        g.setFont (sans (16.0f, true));
        g.drawText (title, top.withTrimmedTop (4.0f), juce::Justification::bottomLeft, true);
        g.setColour (live ? p.muted : p.dim);
        g.setFont (sans (13.0f));
        g.drawFittedText (live ? label : reason, r.toNearestInt().withTrimmedBottom (2), juce::Justification::topLeft, 2, 0.85f);

        // drag handle
        if (live)
        {
            g.setColour (over ? p.gold : p.muted);
            g.setFont (sans (12.0f, true));
            g.drawText ("DRAG", right.removeFromBottom (right.getHeight() * 0.45f), juce::Justification::centredTop);
            for (int row = 0; row < 3; ++row)
                for (int col = 0; col < 2; ++col)
                    g.fillEllipse (right.getCentreX() - 5.0f + (float) col * 8.0f, right.getBottom() - 10.0f - (float) row * 7.0f, 3.5f, 3.5f);
        }
    }

    void DragChip::mouseDrag (const juce::MouseEvent& e)
    {
        if (dragging || ! hasFile() || e.getDistanceFromDragStart() < 6) return;
        dragging = true;
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this,
                                                                   [safe = juce::Component::SafePointer<DragChip> (this)]
                                                                   { if (safe != nullptr) safe->dragging = false; });
    }

    //==============================================================================
    ResultPicker::ResultPicker()
    {
        failed.setComponentID ("bad");
        for (auto* b : { &worked, &failed })
        {
            b->setClickingTogglesState (false);
            addAndMakeVisible (b);
        }
        worked.onClick = [this] { setValue (value == 1 ? 0 : 1); if (onChange) onChange (value); };
        failed.onClick = [this] { setValue (value == -1 ? 0 : -1); if (onChange) onChange (value); };
    }

    void ResultPicker::setValue (int v)   { value = v; refresh(); }

    void ResultPicker::refresh()
    {
        worked.setToggleState (value == 1, juce::dontSendNotification);
        failed.setToggleState (value == -1, juce::dontSendNotification);
    }

    void ResultPicker::resized()
    {
        auto r = getLocalBounds();
        worked.setBounds (r.removeFromLeft (r.getWidth() / 2 - 4));
        r.removeFromLeft (8);
        failed.setBounds (r);
    }

    //==============================================================================
    void SummaryList::setItems (std::vector<SummaryItem> i)   { items = std::move (i); repaint(); }

    // dot + gap (32) | title | gap (12) | detail, plus 20 px of padding
    float SummaryList::rowHeight (const SummaryItem& it, float width)
    {
        const float detailW = juce::jmax (80.0f, width - 32.0f - titleWidth - 12.0f - 20.0f);
        return juce::jmax (22.0f, paragraphHeight (it.title, titleWidth, sans (14.5f, true)),
                           paragraphHeight (it.detail, detailW, sans (14.0f))) + 12.0f;
    }

    int SummaryList::heightFor (int width) const
    {
        if (items.empty()) return 60;
        float h = 0;
        for (auto& it : items) h += rowHeight (it, (float) width);
        return (int) std::ceil (h);
    }

    void SummaryList::paint (juce::Graphics& g)
    {
        const auto& p = pal();
        auto r = getLocalBounds().toFloat();
        if (items.empty())
        {
            g.setColour (p.dim);
            g.setFont (sans (15.0f));
            g.drawFittedText (placeholder, r.toNearestInt(), juce::Justification::centred, 3);
            return;
        }
        for (size_t i = 0; i < items.size(); ++i)
        {
            const auto& it = items[i];
            auto row = r.removeFromTop (rowHeight (it, r.getWidth()));
            if (i % 2 == 0)
            {
                g.setColour (p.panelHi.withAlpha (0.35f));
                g.fillRoundedRectangle (row, 8.0f);
            }
            auto content = row.reduced (10.0f, 6.0f);
            auto dot = content.removeFromLeft (14.0f).withSizeKeepingCentre (10.0f, 10.0f).withY (content.getY() + 6.0f);
            const auto col = it.status > 0 ? p.good : it.status < 0 ? p.warn : p.dim;
            juce::Path d;
            d.addEllipse (dot);
            juce::DropShadow (col.withAlpha (0.6f), 8, {}).drawForPath (g, d);
            g.setColour (col);
            g.fillPath (d);

            content.removeFromLeft (8.0f);
            auto titleArea = content.removeFromLeft (titleWidth);
            drawParagraph (g, it.title, titleArea, sans (14.5f, true), p.text);
            content.removeFromLeft (12.0f);
            drawParagraph (g, it.detail, content, sans (14.0f), it.status == 0 ? p.dim : p.muted);
        }
    }

    //==============================================================================
    void ProgressStrip::set (float f, const juce::String& text, bool active)
    {
        if (std::abs (f - value) < 0.0005f && text == stage && active == running) return;
        value = f;
        stage = text;
        running = active;
        repaint();
    }

    void ProgressStrip::paint (juce::Graphics& g)
    {
        const auto& p = pal();
        auto r = getLocalBounds().toFloat();
        auto bar = r.removeFromTop (8.0f);
        g.setColour (p.panelHi.withAlpha (0.8f));
        g.fillRoundedRectangle (bar, 4.0f);
        if (value > 0.0f)
        {
            auto fill = bar.withWidth (juce::jmax (8.0f, bar.getWidth() * juce::jlimit (0.0f, 1.0f, value)));
            g.setGradientFill (juce::ColourGradient (p.blue, fill.getX(), 0, p.purple, fill.getRight(), 0, false));
            g.fillRoundedRectangle (fill, 4.0f);
            if (running)
            {
                juce::Path glowPath;
                glowPath.addRoundedRectangle (fill, 4.0f);
                juce::DropShadow (p.purple.withAlpha (0.5f), 10, {}).drawForPath (g, glowPath);
            }
        }
        g.setColour (p.muted);
        g.setFont (sans (13.5f));
        g.drawText (stage, r.withTrimmedTop (4.0f), juce::Justification::centredLeft, true);
    }
}
