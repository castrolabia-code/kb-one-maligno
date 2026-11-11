#pragma once

#include "dsp/engines/PolyBlepOscillator.h"
#include "dsp/filters/LadderFilter.h"
#include "dsp/modulation/Envelope.h"

#include <cstdint>

namespace kbone::dsp
{
    // Valores de control de un tramo (≤ 32 muestras). "start" y "end" permiten
    // interpolar linealmente dentro del tramo: sin escalones audibles.
    struct ControlBlock
    {
        float bendSemitonesStart = 0.0f, bendSemitonesEnd = 0.0f;
        float cutoffHzStart = 2500.0f,   cutoffHzEnd = 2500.0f;
        float resonance = 0.0f;
        float levelStart = 0.8f,         levelEnd = 0.8f;
    };

    // -------------------------------------------------------------------------
    // Una voz de fase 1: saw PolyBLEP → ladder → VCA (Env VCA).
    // -------------------------------------------------------------------------
    class SynthVoice
    {
    public:
        void prepare (double sampleRate) noexcept;
        void setEnvelopeSettings (const Envelope::Settings& s) noexcept { ampEnv.setSettings (s); }

        // Arranca una nota. Si la voz estaba sonando, primero hace un fade
        // corto (robo de voz sin click) y después arranca la nota nueva.
        void startNote (int midiNote, float velocity, std::uint64_t timestamp) noexcept;

        // Retrigger de la misma nota (misma tecla tocada de nuevo): el ataque
        // arranca desde el nivel actual, sin cortar.
        void retrigger (float velocity, std::uint64_t timestamp) noexcept;

        void release() noexcept;            // note off → release de la envolvente
        void kill() noexcept;               // panic: fade rápido a silencio
        void hardReset() noexcept;          // corte inmediato (solo si no suena o hay NaN)

        // Suma la voz en los buffers (no los pisa).
        void render (float* left, float* right, int numSamples, const ControlBlock& ctrl) noexcept;

        bool isActive() const noexcept        { return active; }
        bool isReleasing() const noexcept     { return active && (ampEnv.getStage() == Envelope::Stage::release || fadingOut); }
        int  getNote() const noexcept         { return note; }
        float getLevel() const noexcept       { return ampEnv.getLevel() * fadeGain; }
        std::uint64_t getTimestamp() const noexcept { return startedAt; }

        // Estado de teclado/pedal, lo maneja el VoiceManager.
        bool keyDown = false;
        bool sustained = false;

    private:
        void beginPendingNote() noexcept;

        double sampleRate = 44100.0;
        PolyBlepSaw osc;
        LadderFilter filter;
        Envelope ampEnv;

        bool active = false;
        int note = -1;
        float velocityGain = 1.0f;
        std::uint64_t startedAt = 0;

        // Fade de robo de voz / panic (2–5 ms, lineal).
        bool fadingOut = false;
        float fadeGain = 1.0f;
        float fadeStep = 0.0f;
        bool hasPending = false;
        int pendingNote = -1;
        float pendingVelocity = 0.0f;
    };
}
