#include "PluginEditor.h"
#include "ui/lookandfeel/Colours.h"

namespace colours = kbone::ui::colours;
namespace pid = kbone::params::id;

// --- LabelledKnob ------------------------------------------------------------
KbOneAudioProcessorEditor::LabelledKnob::LabelledKnob (juce::AudioProcessorValueTreeState& apvts,
                                                       const char* paramId, const juce::String& title)
    : attachment (apvts, paramId, slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 110, 24);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (220);
    slider.setColour (juce::Slider::textBoxTextColourId, colours::labelDark);
    slider.setColour (juce::Slider::textBoxOutlineColourId, colours::labelDark.withAlpha (0.25f));

    // Doble clic = valor por defecto del parámetro.
    if (auto* p = apvts.getParameter (paramId))
    {
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        slider.setTooltip (p->getName (64));
    }

    label.setText (title, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (19.0f, juce::Font::bold));
}

// --- Content (lienzo de 1376 x 768) -------------------------------------------
KbOneAudioProcessorEditor::Content::Content (KbOneAudioProcessor& p)
    : processor (p),
      volume    (p.getApvts(), pid::masterVolume,  "GLOBAL VOLUME"),
      cutoff    (p.getApvts(), pid::flt1Cutoff,    "CUTOFF"),
      resonance (p.getApvts(), pid::flt1Reso,      "RESONANCE"),
      release   (p.getApvts(), pid::envVcaRelease, "RELEASE")
{
    for (auto* k : { &volume, &cutoff, &resonance, &release })
    {
        addAndMakeVisible (k->slider);
        addAndMakeVisible (k->label);
    }

    panicButton.setTooltip ("Panic: silences all voices and resets controllers");
    panicButton.onClick = [this] { processor.requestPanic(); };
    addAndMakeVisible (panicButton);
}

void KbOneAudioProcessorEditor::Content::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();

    // Panel amarillo industrial.
    g.setGradientFill (juce::ColourGradient (colours::panelYellow, 0.0f, 0.0f,
                                             colours::panelYellowDk, 0.0f, area.getHeight(), false));
    g.fillAll();

    // Barra superior carbón.
    auto top = area.withHeight (72.0f);
    g.setColour (colours::carbon);
    g.fillRect (top);

    g.setColour (colours::brassLight);
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    g.drawText ("CASTRO LAB", top.reduced (28.0f, 0.0f), juce::Justification::centredLeft);

    g.setColour (colours::ledRed);
    g.setFont (juce::FontOptions (34.0f, juce::Font::bold));
    g.drawText ("KB ONE MALIGNO", top, juce::Justification::centred);

    g.setColour (colours::brass);
    g.setFont (juce::FontOptions (14.0f));
    g.drawText ("v" KBONE_VERSION_STRING "  -  PHASE 1 PREVIEW", top.reduced (28.0f, 0.0f), juce::Justification::centredRight);

    // Bloque grafito detrás de las perillas.
    auto block = juce::Rectangle<float> (220.0f, 230.0f, 936.0f, 340.0f);
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillRoundedRectangle (block.translated (0.0f, 6.0f), 18.0f);
    g.setColour (colours::graphite);
    g.fillRoundedRectangle (block, 18.0f);
    g.setColour (colours::brass.withAlpha (0.6f));
    g.drawRoundedRectangle (block.reduced (6.0f), 14.0f, 1.5f);

    // Plaquitas amarillas detrás de cada perilla (para que las etiquetas se lean).
    g.setColour (colours::panelYellow);
    for (int i = 0; i < 4; ++i)
        g.fillRoundedRectangle (juce::Rectangle<float> (252.0f + (float) i * 222.0f, 262.0f, 206.0f, 276.0f), 12.0f);

    g.setColour (colours::labelDark.withAlpha (0.7f));
    g.setFont (juce::FontOptions (15.0f));
    g.drawText ("Provisional editor - final interface arrives in the UI phase",
                area.withTrimmedTop (area.getHeight() - 48.0f), juce::Justification::centred);
}

void KbOneAudioProcessorEditor::Content::resized()
{
    int x = 252;
    for (auto* k : { &volume, &cutoff, &resonance, &release })
    {
        k->label.setBounds (x, 272, 206, 28);
        k->slider.setBounds (x + 13, 304, 180, 214);
        x += 222;
    }

    panicButton.setBounds (1376 - 28 - 120, 600, 120, 44);
}

// --- Editor -------------------------------------------------------------------
KbOneAudioProcessorEditor::KbOneAudioProcessorEditor (KbOneAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), content (p)
{
    // Se lee antes de setResizeLimits(): ese llamado ya redimensiona la ventana.
    const float savedScale = processor.getEditorScale();

    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (content);
    content.setBounds (0, 0, baseWidth, baseHeight);

    setResizable (true, true);
    setResizeLimits (baseWidth * 3 / 4, baseHeight * 3 / 4, baseWidth * 2, baseHeight * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (static_cast<double> (baseWidth) / static_cast<double> (baseHeight));

    setSize (juce::roundToInt (baseWidth * savedScale), juce::roundToInt (baseHeight * savedScale));
    restoringSize = false;
}

KbOneAudioProcessorEditor::~KbOneAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void KbOneAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::carbon);
}

void KbOneAudioProcessorEditor::resized()
{
    const float scale = static_cast<float> (getWidth()) / static_cast<float> (baseWidth);
    content.setTransform (juce::AffineTransform::scale (scale));
    if (! restoringSize)
        processor.setEditorScale (scale);
}
