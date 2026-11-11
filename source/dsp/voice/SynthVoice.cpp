#include "SynthVoice.h"

#include <cmath>

namespace kbone::dsp
{
    namespace
    {
        constexpr double stealFadeSeconds = 0.003;   // 3 ms: dentro del rango 2–5 ms de la especificación
        constexpr float  voiceHeadroom    = 0.25f;   // 8 voces sumadas no saturan el master

        float velocityToGain (float velocity) noexcept
        {
            // Curva suave: velocidad baja no queda muda, alta llega a 1.
            return 0.25f + 0.75f * velocity * velocity;
        }
    }

    void SynthVoice::prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        osc.setSampleRate (sampleRate);
        filter.setSampleRate (sampleRate);
        ampEnv.setSampleRate (sampleRate);
        fadeStep = static_cast<float> (1.0 / (stealFadeSeconds * sampleRate));
        hardReset();
    }

    void SynthVoice::hardReset() noexcept
    {
        active = false;
        note = -1;
        keyDown = false;
        sustained = false;
        fadingOut = false;
        fadeGain = 1.0f;
        hasPending = false;
        ampEnv.reset();
        filter.reset();
        osc.resetPhase();
    }

    void SynthVoice::startNote (int midiNote, float velocity, std::uint64_t timestamp) noexcept
    {
        startedAt = timestamp;
        keyDown = true;
        sustained = false;

        if (active && ampEnv.getLevel() * fadeGain > 1.0e-4f)
        {
            // Robo: se baja la voz anterior en 3 ms y recién ahí arranca la nueva.
            note = midiNote;
            pendingNote = midiNote;
            pendingVelocity = velocity;
            hasPending = true;
            fadingOut = true;
            return;
        }

        pendingNote = midiNote;
        pendingVelocity = velocity;
        beginPendingNote();
    }

    void SynthVoice::beginPendingNote() noexcept
    {
        hasPending = false;
        fadingOut = false;
        fadeGain = 1.0f;
        note = pendingNote;
        velocityGain = velocityToGain (pendingVelocity);
        ampEnv.reset();
        filter.reset();
        osc.resetPhase();
        ampEnv.noteOn();
        active = true;
    }

    void SynthVoice::retrigger (float velocity, std::uint64_t timestamp) noexcept
    {
        startedAt = timestamp;
        keyDown = true;
        sustained = false;

        if (hasPending) { pendingVelocity = velocity; return; }

        if (fadingOut)
        {
            // La voz se estaba apagando (panic o reducción de voces): se espera
            // a que termine el fade y se arranca de nuevo, sin saltos de nivel.
            pendingNote = note;
            pendingVelocity = velocity;
            hasPending = true;
            return;
        }

        velocityGain = velocityToGain (velocity);
        fadingOut = false;
        fadeGain = 1.0f;
        ampEnv.noteOn();
    }

    void SynthVoice::release() noexcept
    {
        keyDown = false;
        sustained = false;

        if (hasPending)
        {
            // La nota nueva todavía no arrancó: se arranca y se suelta enseguida,
            // así respeta el release configurado.
            beginPendingNote();
        }
        ampEnv.noteOff();
    }

    void SynthVoice::kill() noexcept
    {
        keyDown = false;
        sustained = false;
        hasPending = false;
        if (active)
            fadingOut = true;
    }

    void SynthVoice::render (float* left, float* right, int numSamples, const ControlBlock& ctrl) noexcept
    {
        if (! active || numSamples <= 0)
            return;

        filter.setResonance (ctrl.resonance);

        const float gStart = filter.computeG (ctrl.cutoffHzStart);
        const float gEnd   = filter.computeG (ctrl.cutoffHzEnd);
        const float invN   = 1.0f / static_cast<float> (numSamples);

        const auto noteHz = [this] (float bend)
        {
            return 440.0 * std::exp2 ((static_cast<double> (note) - 69.0 + bend) / 12.0);
        };
        const double hzStart = noteHz (ctrl.bendSemitonesStart);
        const double hzEnd   = noteHz (ctrl.bendSemitonesEnd);

        for (int i = 0; i < numSamples; ++i)
        {
            const float t = static_cast<float> (i) * invN;

            // Interpolación lineal de los controles dentro del tramo.
            osc.setFrequency (hzStart + (hzEnd - hzStart) * t);
            filter.setG (gStart + (gEnd - gStart) * t);
            const float level = ctrl.levelStart + (ctrl.levelEnd - ctrl.levelStart) * t;

            float sample = filter.process (osc.process());
            sample *= ampEnv.process() * velocityGain * level * voiceHeadroom;

            if (fadingOut)
            {
                fadeGain -= fadeStep;
                if (fadeGain <= 0.0f)
                {
                    fadeGain = 0.0f;
                    if (hasPending)
                    {
                        beginPendingNote();
                    }
                    else
                    {
                        hardReset();
                        return;
                    }
                }
            }
            sample *= fadeGain;

            // Protección: si aparece NaN/Inf se resetea la voz y no se propaga.
            if (! std::isfinite (sample))
            {
                hardReset();
                return;
            }

            left[i]  += sample;
            right[i] += sample;

            if (! ampEnv.isActive())
            {
                hardReset();
                return;
            }
        }
    }
}
