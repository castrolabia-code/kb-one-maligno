#include "KbOneLookAndFeel.h"
#include "Colours.h"

namespace kbone::ui
{
    KbOneLookAndFeel::KbOneLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId,       colours::labelDark);
        setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId,  colours::brass.withAlpha (0.5f));
        setColour (juce::Label::textColourId,               colours::labelDark);
        setColour (juce::TextButton::buttonColourId,        colours::graphite);
        setColour (juce::TextButton::buttonOnColourId,      colours::pointerRed);
        setColour (juce::TextButton::textColourOffId,       colours::brassLight);
        setColour (juce::TextButton::textColourOnId,        juce::Colours::white);
        setColour (juce::TooltipWindow::backgroundColourId, colours::carbon);
        setColour (juce::TooltipWindow::textColourId,       colours::brassLight);
    }

    void KbOneLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                             float pos, float startAngle, float endAngle, juce::Slider& slider)
    {
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);
        const bool hot = slider.isMouseOverOrDragging();

        // Sombra proyectada.
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre.translated (0.0f, radius * 0.08f)));

        // Arco de valor (latón) sobre pista oscura.
        const float arcRadius = radius * 0.92f;
        const float arcThickness = radius * 0.09f;
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
        g.setColour (colours::carbon.withAlpha (0.55f));
        g.strokePath (track, juce::PathStrokeType (arcThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
        g.setColour (hot ? colours::brassLight : colours::brass);
        g.strokePath (value, juce::PathStrokeType (arcThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Anillo de latón.
        const float ringRadius = radius * 0.76f;
        auto ring = juce::Rectangle<float> (ringRadius * 2.0f, ringRadius * 2.0f).withCentre (centre);
        g.setGradientFill (juce::ColourGradient (colours::brassLight, ring.getTopLeft(), colours::brass.darker (0.6f), ring.getBottomRight(), false));
        g.fillEllipse (ring);

        // Cuerpo negro con degradado radial.
        const float bodyRadius = ringRadius * 0.86f;
        auto body = juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre);
        g.setGradientFill (juce::ColourGradient (colours::graphite.brighter (0.35f), centre.translated (-bodyRadius * 0.35f, -bodyRadius * 0.45f),
                                                 colours::carbon, centre.translated (bodyRadius * 0.6f, bodyRadius * 0.8f), true));
        g.fillEllipse (body);

        // Indicador rojo.
        juce::Path pointer;
        const float pw = bodyRadius * 0.14f;
        pointer.addRoundedRectangle (-pw * 0.5f, -bodyRadius * 0.92f, pw, bodyRadius * 0.5f, pw * 0.5f);
        g.setColour (colours::pointerRed);
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
    }

    void KbOneLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& bg,
                                                 bool isMouseOver, bool isDown)
    {
        auto r = button.getLocalBounds().toFloat().reduced (1.5f);
        const float corner = r.getHeight() * 0.2f;

        auto base = isDown ? colours::pointerRed : bg;
        if (isMouseOver && ! isDown)
            base = base.brighter (0.15f);

        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.fillRoundedRectangle (r.translated (0.0f, 2.0f), corner);
        g.setGradientFill (juce::ColourGradient (base.brighter (0.2f), r.getTopLeft(), base.darker (0.3f), r.getBottomLeft(), false));
        g.fillRoundedRectangle (r, corner);
        g.setColour (colours::brass.withAlpha (0.8f));
        g.drawRoundedRectangle (r, corner, 1.2f);
    }
}
