#include "TestHelpers.h"
#include "dsp/engines/PolyBlepOscillator.h"

namespace
{
    // Mide la energía de aliasing: todo lo que NO cae cerca de un armónico
    // real (k * f0 < Nyquist) se considera alias. Devuelve dB relativos al total.
    // maxHz limita la medición a una banda (los alias de PolyBLEP se concentran
    // cerca de Nyquist, donde el oído es poco sensible).
    double aliasingDb (const std::vector<float>& signal, double f0, double sampleRate, int order, double maxHz = 1.0e9)
    {
        const auto power = kbone::test::powerSpectrum (signal, order);
        const double binHz = sampleRate / (double) (1 << order);
        const int guard = 6; // ancho del lóbulo principal de Blackman-Harris (±4 bins) + margen

        std::vector<bool> harmonic (power.size(), false);
        for (double h = f0; h < sampleRate * 0.5; h += f0)
        {
            const int centre = (int) std::lround (h / binHz);
            for (int b = centre - guard; b <= centre + guard; ++b)
                if (b >= 0 && b < (int) harmonic.size())
                    harmonic[(size_t) b] = true;
        }

        double total = 0.0, alias = 0.0;
        for (size_t b = (size_t) guard; b < power.size(); ++b)   // se ignora DC
        {
            total += power[b];
            if (! harmonic[b] && (double) b * binHz < maxHz)
                alias += power[b];
        }
        return 10.0 * std::log10 (alias / total + 1.0e-30);
    }

    std::vector<float> renderSaw (double f0, double sampleRate, int numSamples, bool naive)
    {
        std::vector<float> out ((size_t) numSamples);
        if (naive)
        {
            double phase = 0.0;
            for (auto& s : out)
            {
                s = (float) (2.0 * phase - 1.0);
                phase += f0 / sampleRate;
                if (phase >= 1.0) phase -= 1.0;
            }
        }
        else
        {
            kbone::dsp::PolyBlepSaw saw;
            saw.setSampleRate (sampleRate);
            saw.setFrequency (f0);
            for (auto& s : out)
                s = saw.process();
        }
        return out;
    }
}

class OscillatorTests final : public juce::UnitTest
{
public:
    OscillatorTests() : juce::UnitTest ("PolyBLEP saw", "Oscillator") {}

    void runTest() override
    {
        constexpr int order = 15;
        constexpr int n = 1 << order;

        beginTest ("Sin NaN/Inf, amplitud correcta y sin DC");
        for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
        {
            for (double f0 : { 20.0, 110.0, 1000.0, 8000.0, 19000.0 })
            {
                auto s = renderSaw (f0, sr, n, false);
                expect (kbone::test::allFinite (s.data(), n));
                const float p = kbone::test::peak (s.data(), n);
                expectLessOrEqual (p, 1.1f, "pico demasiado alto");
                // PolyBLEP suaviza el salto: en notas muy agudas el pico baja un poco.
                if (f0 < sr * 0.05)
                    expectGreaterThan (p, 0.85f, "amplitud demasiado baja");

                // DC medido sobre un número entero de períodos.
                const int periods = (int) std::floor ((double) n * f0 / sr);
                const int len = (int) std::lround (periods * sr / f0);
                if (periods >= 1)
                {
                    double mean = 0.0;
                    for (int i = 0; i < len; ++i) mean += s[(size_t) i];
                    expectLessThan (std::abs (mean / len), 0.01, "DC en el oscilador");
                }
            }
        }

        beginTest ("Aliasing medido con FFT en un barrido de frecuencias");
        {
            const double sr = 48000.0;
            for (double f0 : { 440.0, 1244.5, 2637.0, 4186.0 })
            {
                const auto blepSig  = renderSaw (f0, sr, n, false);
                const auto naiveSig = renderSaw (f0, sr, n, true);
                const double blep      = aliasingDb (blepSig,  f0, sr, order);
                const double naive     = aliasingDb (naiveSig, f0, sr, order);
                const double blepLow   = aliasingDb (blepSig,  f0, sr, order, 8000.0);
                const double naiveLow  = aliasingDb (naiveSig, f0, sr, order, 8000.0);
                logMessage ("  f0 = " + juce::String (f0, 1) + " Hz  PolyBLEP: total " + juce::String (blep, 1)
                            + " dB, <8 kHz " + juce::String (blepLow, 1) + " dB  |  saw ingenuo: total "
                            + juce::String (naive, 1) + " dB, <8 kHz " + juce::String (naiveLow, 1) + " dB");

                // Umbrales (fase 1, PolyBLEP de 2º orden, medido hasta C8):
                //  - total (incluye alias junto a Nyquist): < −22 dB
                //  - banda < 8 kHz (la más audible):        < −50 dB
                //  - mejora sobre el saw ingenuo:           > 10 dB
                expectLessThan (blep, -22.0, "aliasing total por encima del umbral");
                expectLessThan (blepLow, -50.0, "aliasing audible (< 8 kHz) por encima del umbral");
                expectLessThan (blep, naive - 10.0, "PolyBLEP no mejora lo suficiente al saw ingenuo");
            }
        }

        beginTest ("Afinación: período correcto");
        {
            const double sr = 48000.0, f0 = 440.0;
            auto s = renderSaw (f0, sr, 48000, false);
            // La rampa cruza el cero hacia arriba una vez por período.
            int crossings = 0;
            for (size_t i = 1; i < s.size(); ++i)
                if (s[i - 1] < 0.0f && s[i] >= 0.0f)
                    ++crossings;
            expectWithinAbsoluteError (crossings, 440, 1);
        }
    }
};

static OscillatorTests oscillatorTests;
