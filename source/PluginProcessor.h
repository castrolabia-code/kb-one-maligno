#pragma once

#include "dsp/voice/VoiceManager.h"
#include "params/Parameters.h"

#include <juce_audio_processors/juce_audio_processors.h>

class KbOneAudioProcessor final : public juce::AudioProcessor
{
public:
    KbOneAudioProcessor();
    ~KbOneAudioProcessor() override = default;

    // --- AudioProcessor ------------------------------------------------------
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "KB ONE Maligno"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override    { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Init"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- API propia ----------------------------------------------------------
    juce::AudioProcessorValueTreeState& getApvts() noexcept { return apvts; }

    // Pide un panic desde cualquier hilo (botón de la UI). Se ejecuta en el
    // próximo bloque de audio, sin locks.
    void requestPanic() noexcept { panicRequested.store (true); }

    // Escala del editor (0.75 – 2.0). Se guarda en el estado del proyecto.
    float getEditorScale() const noexcept { return editorScale.load(); }
    void setEditorScale (float s) noexcept { editorScale.store (juce::jlimit (0.75f, 2.0f, s)); }

    int getNumActiveVoices() const noexcept { return activeVoices.load(); }

private:
    kbone::dsp::VoiceParameters readVoiceParameters() const noexcept;

    juce::AudioProcessorValueTreeState apvts;
    kbone::params::ParameterCache paramCache;

    kbone::dsp::VoiceManager voiceManager;

    // Master: volumen suavizado, filtro DC y limitador suave de seguridad.
    juce::SmoothedValue<float> masterGain { 0.5f };
    float dcX1[2] {}, dcY1[2] {};
    float dcCoef = 0.999f;

    std::atomic<bool> panicRequested { false };
    std::atomic<float> editorScale { 1.0f };
    std::atomic<int> activeVoices { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KbOneAudioProcessor)
};
