#include "Envelope.h"

#include <algorithm>
#include <cmath>

namespace kbone::dsp
{
    namespace
    {
        constexpr double idleThreshold = 1.0e-4;  // −80 dB
    }

    void Envelope::setSampleRate (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        updateRates();
    }

    void Envelope::setSettings (const Settings& newSettings) noexcept
    {
        settings = newSettings;
        settings.sustainLevel = std::clamp (settings.sustainLevel, 0.0f, 1.0f);
        updateRates();
    }

    double Envelope::coefficientFor (float seconds) const noexcept
    {
        // Filtro de un polo: nivel += (objetivo - nivel) * coef.
        // Tras N = seconds*fs muestras queda (1-coef)^N = 0.01 del camino.
        const auto samples = std::max (1.0, static_cast<double> (seconds) * sampleRate);
        return 1.0 - std::exp (std::log (0.01) / samples);
    }

    void Envelope::updateRates() noexcept
    {
        const auto attackSamples = std::max (1.0, static_cast<double> (settings.attackSeconds) * sampleRate);
        attackStep  = 1.0 / attackSamples;
        decayCoef   = coefficientFor (settings.decaySeconds);
        releaseCoef = coefficientFor (settings.releaseSeconds);
    }

    void Envelope::noteOn() noexcept   { stage = Stage::attack; }

    void Envelope::noteOff() noexcept
    {
        if (stage != Stage::idle)
            stage = Stage::release;
    }

    void Envelope::reset() noexcept
    {
        stage = Stage::idle;
        level = 0.0;
    }

    float Envelope::process() noexcept
    {
        const double sustain = settings.sustainLevel;

        switch (stage)
        {
            case Stage::idle:
                break;

            case Stage::attack:
                level += attackStep;
                if (level >= 1.0 - 1.0e-9)
                {
                    level = 1.0;
                    stage = Stage::decay;
                }
                break;

            case Stage::decay:
                level += (sustain - level) * decayCoef;
                if (std::abs (level - sustain) < idleThreshold)
                {
                    level = sustain;
                    stage = Stage::sustain;
                }
                break;

            case Stage::sustain:
                // Sigue al sustain si la perilla se mueve (suavizado por el mismo coeficiente).
                level += (sustain - level) * decayCoef;
                break;

            case Stage::release:
                level -= level * releaseCoef;
                if (level < idleThreshold)
                {
                    level = 0.0;
                    stage = Stage::idle;
                }
                break;
        }

        return static_cast<float> (level);
    }
}
