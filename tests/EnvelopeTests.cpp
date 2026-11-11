#include "TestHelpers.h"
#include "dsp/modulation/Envelope.h"

class EnvelopeTests final : public juce::UnitTest
{
public:
    EnvelopeTests() : juce::UnitTest ("ADSR", "Envelope") {}

    void runTest() override
    {
        using kbone::dsp::Envelope;

        for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
        {
            beginTest ("Tiempos correctos a " + juce::String (sr) + " Hz");

            Envelope env;
            env.setSampleRate (sr);
            Envelope::Settings s;
            s.attackSeconds = 0.1f;
            s.decaySeconds = 0.2f;
            s.sustainLevel = 0.5f;
            s.releaseSeconds = 0.3f;
            env.setSettings (s);

            env.noteOn();
            int samples = 0;
            while (env.getStage() == Envelope::Stage::attack && samples < (int) sr * 10)
            {
                env.process();
                ++samples;
            }
            expectWithinAbsoluteError (samples, (int) std::lround (0.1 * sr), 1);
            expectEquals (env.getLevel(), 1.0f);

            // Decay: 99 % del camino 1 → 0.5 en 200 ms.
            int decaySamples = 0;
            while (env.getLevel() - 0.5f > 0.005f && decaySamples < (int) sr * 10)
            {
                env.process();
                ++decaySamples;
            }
            expectWithinAbsoluteError ((double) decaySamples / sr, 0.2, 0.005);

            for (int i = 0; i < (int) sr; ++i) env.process();
            expect (env.getStage() == Envelope::Stage::sustain);
            expectWithinAbsoluteError (env.getLevel(), 0.5f, 1.0e-3f);

            // Release: llega al 1 % del nivel de partida en 300 ms y después se apaga.
            env.noteOff();
            int releaseSamples = 0;
            while (env.getLevel() > 0.005f && releaseSamples < (int) sr * 10)
            {
                env.process();
                ++releaseSamples;
            }
            expectWithinAbsoluteError ((double) releaseSamples / sr, 0.3, 0.005);

            int toIdle = 0;
            while (env.isActive() && toIdle < (int) sr * 10) { env.process(); ++toIdle; }
            expect (! env.isActive(), "la envolvente no termina");
        }

        beginTest ("Re-disparo sin salto: el ataque arranca desde el nivel actual");
        {
            Envelope env;
            env.setSampleRate (48000.0);
            env.setSettings ({ 0.05f, 0.1f, 0.8f, 0.5f });
            env.noteOn();
            for (int i = 0; i < 48000; ++i) env.process();
            env.noteOff();
            for (int i = 0; i < 2000; ++i) env.process();
            const float before = env.getLevel();
            env.noteOn();
            const float after = env.process();
            expectLessThan (std::abs (after - before), 0.01f);
        }
    }
};

static EnvelopeTests envelopeTests;
