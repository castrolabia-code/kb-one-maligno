#include "TestHelpers.h"
#include "PluginProcessor.h"

namespace
{
    struct Harness
    {
        explicit Harness (double sampleRate, int maxBlock = 512)
            : sr (sampleRate), blockSize (maxBlock)
        {
            proc.setPlayConfigDetails (0, 2, sr, blockSize);
            proc.prepareToPlay (sr, blockSize);
        }

        // Renderiza numSamples en bloques del tamaño indicado (0 = aleatorio 1..4096).
        // Devuelve el canal izquierdo.
        std::vector<float> render (int numSamples, int block = 256, juce::MidiBuffer* firstMidi = nullptr)
        {
            std::vector<float> out;
            out.reserve ((size_t) numSamples);
            juce::AudioBuffer<float> buffer (2, 4096);
            juce::Random rng (42);
            bool first = true;

            while ((int) out.size() < numSamples)
            {
                int n = block > 0 ? block : 1 + rng.nextInt (4096);
                n = std::min (n, numSamples - (int) out.size());
                buffer.setSize (2, n, false, false, true);

                juce::MidiBuffer midi;
                if (first && firstMidi != nullptr)
                    midi = *firstMidi;
                first = false;

                proc.processBlock (buffer, midi);
                for (int i = 0; i < n; ++i)
                    out.push_back (buffer.getSample (0, i));
                lastFinite = lastFinite && kbone::test::allFinite (buffer.getReadPointer (0), n)
                                        && kbone::test::allFinite (buffer.getReadPointer (1), n);
            }
            return out;
        }

        void send (const juce::MidiMessage& m)
        {
            juce::MidiBuffer midi;
            midi.addEvent (m, 0);
            juce::AudioBuffer<float> buffer (2, 1);
            proc.processBlock (buffer, midi);
        }

        void setParam (const char* id, float plainValue)
        {
            auto* p = proc.getApvts().getParameter (id);
            p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
        }

        double sr;
        int blockSize;
        bool lastFinite = true;
        KbOneAudioProcessor proc;
    };
}

class ProcessorTests final : public juce::UnitTest
{
public:
    ProcessorTests() : juce::UnitTest ("Processor / voices", "Processor") {}

    void runTest() override
    {
        namespace pid = kbone::params::id;
        const auto ch = 1;

        beginTest ("Una nota suena a cualquier sample rate y tamaño de bloque, sin NaN ni clipping");
        for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        {
            for (int block : { 1, 7, 64, 4096, 0 /* variable */ })
            {
                Harness h (sr, 4096);
                h.send (juce::MidiMessage::noteOn (ch, 60, 1.0f));
                auto out = h.render ((int) (sr * 0.25), block);
                const float p = kbone::test::peak (out.data(), (int) out.size());
                expect (h.lastFinite, "NaN/Inf en la salida");
                expectGreaterThan (p, 0.02f, "la nota no suena");
                expectLessOrEqual (p, 1.0f, "salida por encima de 0 dBFS");
            }
        }

        beginTest ("MIDI con precisión de muestra");
        {
            Harness h (48000.0);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (ch, 60, 1.0f), 100);
            auto out = h.render (512, 512, &midi);
            bool silentBefore = true;
            for (int i = 0; i < 100; ++i)
                silentBefore = silentBefore && juce::exactlyEqual (out[(size_t) i], 0.0f);
            expect (silentBefore, "hay sonido antes del note on");
            expectGreaterThan (kbone::test::peak (out.data() + 100, 412), 0.0f);
        }

        beginTest ("Note off + release → silencio y voces liberadas");
        {
            Harness h (48000.0);
            h.setParam (pid::envVcaRelease, 100.0f);
            h.send (juce::MidiMessage::noteOn (ch, 64, 0.8f));
            h.render (4800);
            expectEquals (h.proc.getNumActiveVoices(), 1);
            h.send (juce::MidiMessage::noteOff (ch, 64));
            h.render (48000);
            auto tail = h.render (4800);
            expectEquals (h.proc.getNumActiveVoices(), 0);
            expectLessThan (kbone::test::peak (tail.data(), (int) tail.size()), 1.0e-4f);
        }

        beginTest ("Polifonía: 8 voces por defecto, robo de voces sin pasarse");
        {
            Harness h (48000.0);
            for (int n = 0; n < 12; ++n)
                h.send (juce::MidiMessage::noteOn (ch, 48 + n, 0.8f));
            auto out = h.render (9600);
            expectEquals (h.proc.getNumActiveVoices(), 8);
            expect (h.lastFinite);
            expectLessOrEqual (kbone::test::peak (out.data(), (int) out.size()), 1.0f);

            h.setParam (pid::polyVoices, 4.0f);
            h.render (4800);
            expectEquals (h.proc.getNumActiveVoices(), 4);
        }

        beginTest ("Sustain pedal (CC64) sostiene y libera");
        {
            Harness h (48000.0);
            h.setParam (pid::envVcaRelease, 50.0f);
            h.send (juce::MidiMessage::controllerEvent (ch, 64, 127));
            h.send (juce::MidiMessage::noteOn (ch, 60, 0.8f));
            h.render (2400);
            h.send (juce::MidiMessage::noteOff (ch, 60));
            h.render (48000);
            expectEquals (h.proc.getNumActiveVoices(), 1, "el pedal no sostuvo la nota");
            h.send (juce::MidiMessage::controllerEvent (ch, 64, 0));
            h.render (24000);
            expectEquals (h.proc.getNumActiveVoices(), 0, "nota colgada al soltar el pedal");
        }

        beginTest ("All Notes Off (CC123) y panic");
        {
            Harness h (48000.0);
            h.setParam (pid::envVcaRelease, 50.0f);
            for (int n = 0; n < 4; ++n) h.send (juce::MidiMessage::noteOn (ch, 60 + n, 0.8f));
            h.render (2400);
            h.send (juce::MidiMessage::allNotesOff (ch));
            h.render (24000);
            expectEquals (h.proc.getNumActiveVoices(), 0);

            h.setParam (pid::envVcaRelease, 10000.0f);
            for (int n = 0; n < 4; ++n) h.send (juce::MidiMessage::noteOn (ch, 60 + n, 0.8f));
            h.render (2400);
            h.proc.requestPanic();
            h.render (480); // 10 ms
            expectEquals (h.proc.getNumActiveVoices(), 0, "el panic no silenció en 10 ms");
            // Lo único que queda es la cola suave del filtro DC del master (< −40 dB, sin click).
            auto after = h.render (480);
            expectLessThan (kbone::test::peak (after.data(), (int) after.size()), 0.01f);
            h.render (9600);
            auto silent = h.render (480);
            expectLessThan (kbone::test::peak (silent.data(), (int) silent.size()), 1.0e-4f);
        }

        beginTest ("Pitch bend cambia la afinación");
        {
            auto countCrossings = [] (const std::vector<float>& v)
            {
                int c = 0;
                for (size_t i = 1; i < v.size(); ++i)
                    if (v[i - 1] <= 0.0f && v[i] > 0.0f) ++c;
                return c;
            };

            Harness h (48000.0);
            h.setParam (pid::flt1Cutoff, 400.0f); // deja casi solo la fundamental
            h.setParam (pid::flt1Reso, 0.0f);
            h.send (juce::MidiMessage::noteOn (ch, 57, 1.0f)); // 220 Hz
            h.render (9600);
            const int base = countCrossings (h.render (48000));
            h.send (juce::MidiMessage::pitchWheel (ch, 16383)); // +2 st
            h.render (4800);
            const int bent = countCrossings (h.render (48000));
            logMessage ("  cruces por cero: " + juce::String (base) + " → " + juce::String (bent));
            expectWithinAbsoluteError (base, 220, 3);
            expectWithinAbsoluteError (bent, 247, 3);
        }

        beginTest ("Volumen global al mínimo = silencio");
        {
            Harness h (48000.0);
            h.setParam (pid::masterVolume, kbone::params::masterVolumeMinDb);
            h.send (juce::MidiMessage::noteOn (ch, 60, 1.0f));
            h.render (24000); // rampa de volumen de 20 ms + cola del filtro DC
            auto out = h.render (4800);
            expectLessThan (kbone::test::peak (out.data(), (int) out.size()), 1.0e-6f);
        }
    }
};

static ProcessorTests processorTests;
