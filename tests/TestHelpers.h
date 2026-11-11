#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <cmath>
#include <vector>

namespace kbone::test
{
    inline bool allFinite (const float* data, int n)
    {
        for (int i = 0; i < n; ++i)
            if (! std::isfinite (data[i]))
                return false;
        return true;
    }

    inline float peak (const float* data, int n)
    {
        float p = 0.0f;
        for (int i = 0; i < n; ++i)
            p = std::max (p, std::abs (data[i]));
        return p;
    }

    inline double rmsOfSine (std::vector<float>& v, size_t from)
    {
        double sum = 0.0;
        for (size_t i = from; i < v.size(); ++i)
            sum += (double) v[i] * v[i];
        return std::sqrt (sum / (double) (v.size() - from));
    }

    // Espectro de potencia (ventana Blackman-Harris) de 2^order muestras.
    inline std::vector<double> powerSpectrum (const std::vector<float>& signal, int order)
    {
        const int n = 1 << order;
        std::vector<float> buffer ((size_t) n * 2, 0.0f);
        for (int i = 0; i < n; ++i)
        {
            const double w = 0.35875 - 0.48829 * std::cos (2.0 * juce::MathConstants<double>::pi * i / (n - 1))
                             + 0.14128 * std::cos (4.0 * juce::MathConstants<double>::pi * i / (n - 1))
                             - 0.01168 * std::cos (6.0 * juce::MathConstants<double>::pi * i / (n - 1));
            buffer[(size_t) i] = static_cast<float> (signal[(size_t) i] * w);
        }

        juce::dsp::FFT fft (order);
        fft.performFrequencyOnlyForwardTransform (buffer.data(), true);

        std::vector<double> power ((size_t) n / 2);
        for (size_t i = 0; i < power.size(); ++i)
            power[i] = (double) buffer[i] * buffer[i];
        return power;
    }
}
