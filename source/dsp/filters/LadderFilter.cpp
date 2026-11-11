#include "LadderFilter.h"

#include <algorithm>
#include <iterator>
#include <cmath>
#include <numbers>

namespace kbone::dsp
{
    void LadderFilter::setSampleRate (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        reset();
    }

    void LadderFilter::reset() noexcept
    {
        std::fill (std::begin (s), std::end (s), 0.0f);
    }

    float LadderFilter::computeG (float cutoffHz) const noexcept
    {
        const auto fc = std::clamp (static_cast<double> (cutoffHz), 10.0, 0.45 * sampleRate);
        // Pre-warping bilineal: la frecuencia de corte digital coincide con la analógica.
        return static_cast<float> (std::tan (std::numbers::pi * fc / sampleRate));
    }

    void LadderFilter::setCutoff (float cutoffHz) noexcept { g = computeG (cutoffHz); }

    void LadderFilter::setG (float newG) noexcept { g = newG; }

    void LadderFilter::setResonance (float resonance) noexcept
    {
        // k = 4 es el umbral teórico de auto-oscilación del ladder lineal.
        // Se deja un pequeño margen (3.95) para que el máximo cante sin explotar.
        k = 3.95f * std::clamp (resonance, 0.0f, 1.0f);
    }

    float LadderFilter::process (float input) noexcept
    {
        // Cada etapa es un pasa-bajos TPT de 1 polo:  y = G*x + B*s
        // con G = g/(1+g) y B = 1/(1+g).
        const float G = g / (1.0f + g);
        const float B = 1.0f / (1.0f + g);

        // La salida de la 4ª etapa es lineal en la entrada u:
        //   y4 = G^4 * u + S,   S = G^3 B s0 + G^2 B s1 + G B s2 + B s3
        // y como u = x - k*y4, se despeja u sin retardo (zero-delay feedback).
        const float G2 = G * G;
        const float G4 = G2 * G2;
        const float S  = B * (G2 * G * s[0] + G2 * s[1] + G * s[2] + s[3]);

        // Compensación parcial de ganancia en la banda de paso al subir la
        // resonancia (el ladder pierde graves con k alto).
        const float x = input * (1.0f + 0.5f * k);

        float u = (x - k * S) / (1.0f + k * G4);
        u = std::tanh (u);  // saturación suave de la entrada del lazo

        float y = u;
        for (auto& state : s)
        {
            const float v = G * (y - state);   // y - state con escala TPT
            const float out = v + state;
            state = out + v;                   // actualización del integrador trapezoidal
            y = out;
        }

        return y;
    }
}
