#pragma once

#include "PluginProcessor.h"
#include "ui/lookandfeel/KbOneLookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>

// -----------------------------------------------------------------------------
// Editor PROVISORIO de fase 1: fondo amarillo, 4 perillas y botón PANIC.
// Todo se diseña sobre un lienzo fijo de 1376 x 768 que se escala con una
// transformación afín (75 % – 200 %, proporción fija): la UI definitiva usará
// el mismo esquema, así el layout nunca depende del tamaño real de ventana.
// -----------------------------------------------------------------------------
class KbOneAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    static constexpr int baseWidth  = 1376;
    static constexpr int baseHeight = 768;

    explicit KbOneAudioProcessorEditor (KbOneAudioProcessor&);
    ~KbOneAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // Perilla + etiqueta conectadas a un parámetro del APVTS.
    struct LabelledKnob
    {
        LabelledKnob (juce::AudioProcessorValueTreeState& apvts, const char* paramId, const juce::String& title);

        juce::Slider slider;
        juce::Label label;
        juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    };

    class Content final : public juce::Component
    {
    public:
        explicit Content (KbOneAudioProcessor&);
        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        KbOneAudioProcessor& processor;
        LabelledKnob volume, cutoff, resonance, release;
        juce::TextButton panicButton { "PANIC" };
    };

    KbOneAudioProcessor& processor;
    kbone::ui::KbOneLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };
    Content content;
    bool restoringSize = true; // durante el constructor no se pisa la escala guardada

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KbOneAudioProcessorEditor)
};
