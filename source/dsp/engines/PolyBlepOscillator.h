#pragma once

#include <cmath>

namespace kbone::dsp
{
    // -------------------------------------------------------------------------
    // Corrección PolyBLEP (band-limited step polinómico de 2º orden).
    // t  = fase actual normalizada [0, 1)
    // dt = incremento de fase por muestra (frecuencia / fs)
    // Devuelve el residuo que hay que RESTAR a una onda con un salto de +2 → 0
    // (como el diente de sierra ingenuo) en la muestra anterior y posterior al
    // salto. Atenúa fuertemente los armónicos que se reflejarían por encima de
    // Nyquist (aliasing).
    // -------------------------------------------------------------------------
    inline double polyBlep (double t, double dt) noexcept
    {
        if (t < dt)            // justo después del salto
        {
            t /= dt;
            return t + t - t * t - 1.0;
        }
        if (t > 1.0 - dt)      // justo antes del salto
        {
            t = (t - 1.0) / dt;
            return t * t + t + t + 1.0;
        }
        return 0.0;
    }

    // -------------------------------------------------------------------------
    // Diente de sierra con anti-aliasing PolyBLEP. Fase en doble precisión para
    // que la afinación no derive en notas largas. Será la base del AnalogEngine.
    // -------------------------------------------------------------------------
    class PolyBlepSaw
    {
    public:
        void setSampleRate (double newSampleRate) noexcept { sampleRate = newSampleRate; }

        void setFrequency (double hz) noexcept
        {
            // Límite por debajo de Nyquist: PolyBLEP requiere dt < 0.5.
            dt = std::fmin (std::fabs (hz) / sampleRate, 0.45);
        }

        void resetPhase (double newPhase = 0.0) noexcept { phase = newPhase; }

        float process() noexcept
        {
            double value = 2.0 * phase - 1.0;
            value -= polyBlep (phase, dt);

            phase += dt;
            if (phase >= 1.0)
                phase -= 1.0;

            return static_cast<float> (value);
        }

        double getPhaseIncrement() const noexcept { return dt; }

    private:
        double sampleRate = 44100.0;
        double phase = 0.0;
        double dt = 0.0;
    };
}
