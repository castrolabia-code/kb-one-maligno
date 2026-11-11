#include "TestHelpers.h"
#include "dsp/filters/LadderFilter.h"

class FilterTests final : public juce::UnitTest
{
public:
    FilterTests() : juce::UnitTest ("Ladder filter", "Filter") {}

    static double gainAt (double freq, float cutoff, double sr)
    {
        kbone::dsp::LadderFilter f;
        f.setSampleRate (sr);
        f.setCutoff (cutoff);
        f.setResonance (0.0f);

        const int n = (int) sr;
        std::vector<float> out ((size_t) n);
        for (int i = 0; i < n; ++i)
            out[(size_t) i] = f.process (0.05f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * i / sr));

        return kbone::test::rmsOfSine (out, (size_t) n / 2) / (0.05 / std::sqrt (2.0));
    }

    void runTest() override
    {
        beginTest ("Respuesta pasa-bajos 24 dB/oct");
        for (double sr : { 44100.0, 96000.0 })
        {
            const double pass = juce::Decibels::gainToDecibels (gainAt (100.0, 1000.0f, sr));
            const double stop = juce::Decibels::gainToDecibels (gainAt (8000.0, 1000.0f, sr));
            logMessage ("  sr " + juce::String (sr) + ": 100 Hz = " + juce::String (pass, 2) + " dB, 8 kHz = " + juce::String (stop, 1) + " dB");
            expectWithinAbsoluteError (pass, 0.0, 1.0);
            expectLessThan (stop, -50.0);
        }

        beginTest ("Estabilidad con resonancia máxima y cutoff modulado a audio rate");
        for (double sr : { 44100.0, 48000.0, 192000.0 })
        {
            kbone::dsp::LadderFilter f;
            f.setSampleRate (sr);
            f.setResonance (1.0f);
            juce::Random rng (1234);

            const int n = (int) (sr * 2.0);
            bool finite = true;
            float maxAbs = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                // Cutoff barriendo 20 Hz – 20 kHz (log) con un LFO de 1 kHz.
                const double lfo = 0.5 + 0.5 * std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * i / sr);
                f.setCutoff ((float) (20.0 * std::pow (1000.0, lfo)));
                const float y = f.process (rng.nextFloat() * 2.0f - 1.0f);
                finite = finite && std::isfinite (y);
                maxAbs = std::max (maxAbs, std::abs (y));
            }
            expect (finite, "el filtro produjo NaN/Inf");
            expectLessThan (maxAbs, 4.0f, "el filtro se volvió inestable");
        }

        beginTest ("Silencio de entrada → silencio de salida tras reset");
        {
            kbone::dsp::LadderFilter f;
            f.setSampleRate (48000.0);
            f.setCutoff (5000.0f);
            f.setResonance (1.0f);
            for (int i = 0; i < 1000; ++i) f.process (1.0f);
            f.reset();
            expectEquals (f.process (0.0f), 0.0f);
        }
    }
};

static FilterTests filterTests;
