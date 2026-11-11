#pragma once

#include "SynthVoice.h"
#include "params/Parameters.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>

namespace kbone::dsp
{
    // Valores de parámetros leídos al inicio de cada bloque de audio.
    struct VoiceParameters
    {
        int   voiceLimit       = params::defaultVoices;
        float pitchBendRange   = 2.0f;   // semitonos
        float cutoffHz         = 2500.0f;
        float resonance        = 0.25f;
        float engineVolume     = 0.8f;
        Envelope::Settings ampEnv;
    };

    // -------------------------------------------------------------------------
    // Asignación de voces polifónica + controladores MIDI.
    //  - Robo de voz: primero la voz en release más silenciosa, si no hay, la
    //    más antigua. El robo hace fade de 3 ms (sin clicks).
    //  - Sustain (CC64), mod wheel (CC1), pitch bend, All Notes Off (CC123),
    //    All Sound Off (CC120), Reset All Controllers (CC121) y panic.
    //  - MIDI con precisión de muestra: el procesador corta el bloque en cada
    //    evento; acá además se subdivide en tramos de control de 32 muestras.
    // -------------------------------------------------------------------------
    class VoiceManager
    {
    public:
        static constexpr int controlBlockSize = 32;

        void prepare (double sampleRate) noexcept;
        void setParameters (const VoiceParameters& p) noexcept;

        void handleMidi (const juce::MidiMessage& m) noexcept;

        void noteOn (int note, float velocity) noexcept;
        void noteOff (int note) noexcept;
        void setSustain (bool down) noexcept;
        void setPitchWheel (int value14bit) noexcept;
        void setModWheel (float value01) noexcept;
        void allNotesOff() noexcept;   // release normal de todo
        void panic() noexcept;         // silencio inmediato (fade 3 ms) y reset de controladores

        // Suma las voces en left/right desde startSample (no limpia los buffers).
        void render (float* left, float* right, int startSample, int numSamples) noexcept;

        int getNumActiveVoices() const noexcept;
        float getModWheel() const noexcept { return modWheel.getTargetValue(); }

    private:
        SynthVoice* findVoiceToUse() noexcept;
        void resetControllers() noexcept;

        std::array<SynthVoice, params::maxVoices> voices;
        VoiceParameters current;
        Envelope::Settings lastEnvSettings;
        bool sustainDown = false;
        std::uint64_t noteCounter = 0;

        // Suavizado de controles continuos (anti zipper).
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> cutoff { 2500.0f };
        juce::SmoothedValue<float> bend, modWheel, level { 0.8f }, reso { 0.25f };
    };
}
