#include "XYPad.h"
#include "Colours.h"
#include <cmath>

using namespace ShooterColours;

XYPad::XYPad (QuadShooterProcessor& p)
    : processor (p),
      px (p.apvts.getParameter (ParamIDs::xyX)),
      py (p.apvts.getParameter (ParamIDs::xyY))
{
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
    setTooltip ("Drag to blend the four slots. Corners = one slot, centre = all four equal. Double-click = centre.");
}

juce::Rectangle<float> XYPad::padArea() const
{
    return getLocalBounds().toFloat().reduced (16.0f).withTrimmedTop (22.0f);
}

void XYPad::paint (juce::Graphics& g)
{
    auto full = getLocalBounds().toFloat();
    g.setColour (card);
    g.fillRoundedRectangle (full, 10.0f);
    g.setColour (cardBorder);
    g.drawRoundedRectangle (full.reduced (0.5f), 10.0f, 1.0f);

    g.setColour (text);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("XY MIX", 16, 10, 120, 18, juce::Justification::centredLeft);

    const float x = px->convertFrom0to1 (px->getValue());
    const float y = py->convertFrom0to1 (py->getValue());
    const auto w = xyWeights (x, y);

    auto pad = padArea();
    g.setColour (inset);
    g.fillRoundedRectangle (pad, 8.0f);

    // Farbige Ecken
    const juce::Point<float> corners[numSlots] = { pad.getTopLeft(), pad.getTopRight(), pad.getBottomLeft(), pad.getBottomRight() };
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clipPath;
        clipPath.addRoundedRectangle (pad, 8.0f);
        g.reduceClipRegion (clipPath);
        for (int s = 0; s < numSlots; ++s)
        {
            const bool muted = processor.apvts.getRawParameterValue (ParamIDs::mute (s))->load() > 0.5f;
            const auto c = slot (s);
            const float alpha = (muted ? 0.06f : 0.18f) + (muted ? 0.0f : 0.32f * w[(size_t) s] * w[(size_t) s]);
            juce::ColourGradient grad (c.withAlpha (alpha), corners[s], c.withAlpha (0.0f),
                                       corners[s] + (pad.getCentre() - corners[s]) * 1.25f, true);
            g.setGradientFill (grad);
            g.fillRect (pad);
        }

        // Raster
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        for (int k = 1; k < 8; ++k)
        {
            const float gx = pad.getX() + pad.getWidth() * (float) k / 8.0f;
            const float gy = pad.getY() + pad.getHeight() * (float) k / 8.0f;
            g.drawVerticalLine ((int) gx, pad.getY(), pad.getBottom());
            g.drawHorizontalLine ((int) gy, pad.getX(), pad.getRight());
        }
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawVerticalLine ((int) pad.getCentreX(), pad.getY(), pad.getBottom());
        g.drawHorizontalLine ((int) pad.getCentreY(), pad.getX(), pad.getRight());
    }

    // Puck
    const juce::Point<float> puck (pad.getX() + x * pad.getWidth(), pad.getBottom() - y * pad.getHeight());

    // Linien zu den Ecken, Dicke = Anteil
    for (int s = 0; s < numSlots; ++s)
    {
        const float share = w[(size_t) s] * w[(size_t) s];
        if (share < 0.005f) continue;
        g.setColour (slot (s).withAlpha (0.2f + 0.6f * share));
        g.drawLine ({ puck, corners[s] }, 1.0f + 5.0f * share);
    }

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.fillEllipse (juce::Rectangle<float> (46.0f, 46.0f).withCentre (puck));
    g.setColour (text);
    g.fillEllipse (juce::Rectangle<float> (18.0f, 18.0f).withCentre (puck));
    g.setColour (bgBottom);
    g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (puck));

    // Ecken-Labels mit Prozent (Leistungsanteil)
    for (int s = 0; s < numSlots; ++s)
    {
        const bool right = s == 1 || s == 3, bottom = s >= 2;
        const bool muted = processor.apvts.getRawParameterValue (ParamIDs::mute (s))->load() > 0.5f;
        auto box = juce::Rectangle<float> (64.0f, 40.0f);
        box.setPosition (right ? pad.getRight() - 72.0f : pad.getX() + 8.0f,
                         bottom ? pad.getBottom() - 48.0f : pad.getY() + 8.0f);
        const auto just = right ? juce::Justification::right : juce::Justification::left;

        g.setColour (muted ? slot (s).withAlpha (0.35f) : slot (s));
        g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
        g.drawText (slotLetter (s), box.removeFromTop (22.0f), just);
        g.setColour (muted ? textDim.withAlpha (0.5f) : text);
        g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        g.drawText (muted ? juce::String ("MUTE") : juce::String (juce::roundToInt (w[(size_t) s] * w[(size_t) s] * 100.0f)) + " %",
                    box, just);
    }
}

void XYPad::setXY (float x, float y)
{
    px->setValueNotifyingHost (px->convertTo0to1 (juce::jlimit (0.0f, 1.0f, x)));
    py->setValueNotifyingHost (py->convertTo0to1 (juce::jlimit (0.0f, 1.0f, y)));
    repaint();
}

void XYPad::setFromMouse (juce::Point<float> pos)
{
    auto pad = padArea();
    setXY ((pos.x - pad.getX()) / pad.getWidth(), (pad.getBottom() - pos.y) / pad.getHeight());
}

void XYPad::mouseDown (const juce::MouseEvent& e)
{
    dragging = true;
    px->beginChangeGesture();
    py->beginChangeGesture();
    setFromMouse (e.position);
}

void XYPad::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging)
        setFromMouse (e.position);
}

void XYPad::mouseUp (const juce::MouseEvent&)
{
    if (! dragging) return;
    dragging = false;
    px->endChangeGesture();
    py->endChangeGesture();
}

void XYPad::mouseDoubleClick (const juce::MouseEvent&)
{
    px->beginChangeGesture();
    py->beginChangeGesture();
    setXY (0.5f, 0.5f);
    px->endChangeGesture();
    py->endChangeGesture();
}

void XYPad::refresh()
{
    const float x = px->getValue(), y = py->getValue();
    juce::uint32 mask = 0;
    for (int s = 0; s < numSlots; ++s)
        if (processor.apvts.getRawParameterValue (ParamIDs::mute (s))->load() > 0.5f)
            mask |= 1u << s;
    if (std::abs (x - lastX) > 0.0001f || std::abs (y - lastY) > 0.0001f || mask != lastMuteMask)
    {
        lastX = x; lastY = y; lastMuteMask = mask;
        repaint();
    }
}
