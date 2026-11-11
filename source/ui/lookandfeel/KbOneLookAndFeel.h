#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace kbone::ui
{
    // LookAndFeel provisorio (fase 1): perilla negra con anillo de latón e
    // indicador rojo, todo vectorial. La versión "render" completa (sombras,
    // bisel especular, marcas de escala) llega en la fase de interfaz.
    class KbOneLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        KbOneLookAndFeel();

        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPosProportional, float rotaryStartAngle,
                               float rotaryEndAngle, juce::Slider&) override;

        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                                   bool isMouseOverButton, bool isButtonDown) override;
    };
}
