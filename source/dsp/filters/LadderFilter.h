#pragma once

namespace kbone::dsp
{
    // -------------------------------------------------------------------------
    // Ladder 4 polos (24 dB/oct) en topología ZDF/TPT (Zavalishin).
    //
    // Fase 1: versión "simple" — lazo de realimentación resuelto de forma exacta
    // (sin retardo de una muestra, estable con modulación rápida) y una
    // saturación tanh suave en la entrada del lazo. La versión "Mini" con tanh
    // por etapa y oversampling llega en la fase de filtros.
    //
    // Uso: setSampleRate() en prepare, setCutoff()/setResonance() a tasa de
    // control (o por muestra si hace falta), process() por muestra.
    // -------------------------------------------------------------------------
    class LadderFilter
    {
    public:
        void setSampleRate (double newSampleRate) noexcept;
        void reset() noexcept;

        // cutoffHz se limita internamente a [10 Hz, 0.45 fs].
        void setCutoff (float cutoffHz) noexcept;

        // resonance 0..1 (1 = al borde de la auto-oscilación).
        void setResonance (float resonance) noexcept;

        // Coeficiente g = tan(pi fc / fs) precalculado: permite interpolar g
        // linealmente dentro de un bloque de control sin recalcular tan().
        void setG (float newG) noexcept;
        float computeG (float cutoffHz) const noexcept;

        float process (float input) noexcept;

    private:
        double sampleRate = 44100.0;
        float g = 0.1f;        // tan(pi fc / fs)
        float k = 0.0f;        // realimentación 0..4
        float s[4] {};         // estados de los integradores TPT
    };
}
