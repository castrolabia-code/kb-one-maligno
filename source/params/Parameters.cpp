#include "Parameters.h"

namespace kbone::params
{
    namespace
    {
        using Apf    = juce::AudioParameterFloat;
        using Api    = juce::AudioParameterInt;
        using Attrs  = juce::AudioParameterFloatAttributes;
        using IAttrs = juce::AudioParameterIntAttributes;

        // Rango logarítmico (para frecuencias y tiempos): mapea 0..1 a min..max
        // de forma exponencial, así el centro de la perilla cae en la media geométrica.
        juce::NormalisableRange<float> logRange (float minV, float maxV)
        {
            return { minV, maxV,
                     [] (float start, float end, float n) { return start * std::pow (end / start, n); },
                     [] (float start, float end, float v) { return std::log (v / start) / std::log (end / start); },
                     [] (float start, float end, float v) { return juce::jlimit (start, end, v); } };
        }

        juce::String formatHz (float v, int)
        {
            return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz"
                                : juce::String (v, v < 100.0f ? 1 : 0) + " Hz";
        }

        juce::String formatMs (float v, int)
        {
            return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " s"
                                : juce::String (v, v < 10.0f ? 1 : 0) + " ms";
        }

        juce::String formatPercent (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }

        juce::String formatDb (float v, int)
        {
            return v <= masterVolumeMinDb + 0.01f ? juce::String ("-inf dB") : juce::String (v, 1) + " dB";
        }

        // Parseo tolerante de texto escrito por el usuario ("2.5k", "300 ms", "1.2 s").
        float parseScaled (const juce::String& text)
        {
            auto t = text.trim().toLowerCase();
            auto v = t.getFloatValue();
            if (t.endsWith ("khz") || t.endsWith ("k")) return v * 1000.0f;
            if (t.endsWith (" s") || (t.endsWith ("s") && ! t.endsWith ("ms"))) return v * 1000.0f;
            return v;
        }

        std::unique_ptr<Apf> timeParam (const char* pid, const char* name, float defMs)
        {
            return std::make_unique<Apf> (juce::ParameterID { pid, 1 }, name,
                                          logRange (envMinTimeMs, envMaxTimeMs), defMs,
                                          Attrs().withLabel ("ms")
                                                 .withStringFromValueFunction (formatMs)
                                                 .withValueFromStringFunction (parseScaled));
        }

        std::unique_ptr<Apf> percentParam (const char* pid, const char* name, float def)
        {
            return std::make_unique<Apf> (juce::ParameterID { pid, 1 }, name,
                                          juce::NormalisableRange<float> (0.0f, 1.0f), def,
                                          Attrs().withLabel ("%")
                                                 .withStringFromValueFunction (formatPercent)
                                                 .withValueFromStringFunction ([] (const juce::String& s)
                                                                               { return s.getFloatValue() / 100.0f; }));
        }
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        // ---------------- Master / global ----------------
        {
            auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("master", "Master", "|");

            group->addChild (std::make_unique<Apf> (juce::ParameterID { id::masterVolume, 1 }, "Global Volume",
                                                    juce::NormalisableRange<float> (masterVolumeMinDb, masterVolumeMaxDb, 0.0f, 2.0f),
                                                    -6.0f,
                                                    Attrs().withLabel ("dB")
                                                           .withStringFromValueFunction (formatDb)
                                                           .withValueFromStringFunction ([] (const juce::String& s)
                                                                                         { return s.trim().startsWithIgnoreCase ("-inf")
                                                                                                      ? masterVolumeMinDb : s.getFloatValue(); })));

            group->addChild (std::make_unique<Api> (juce::ParameterID { id::pitchBendRange, 1 }, "Pitch Bend Range",
                                                    1, 24, 2,
                                                    IAttrs().withLabel ("st")
                                                            .withStringFromValueFunction ([] (int v, int) { return "+/-" + juce::String (v) + " st"; })));

            group->addChild (std::make_unique<Api> (juce::ParameterID { id::polyVoices, 1 }, "Voices",
                                                    1, maxVoices, defaultVoices, IAttrs()));
            layout.add (std::move (group));
        }

        // ---------------- Engine 1 ----------------
        {
            auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("eng1", "Engine 1", "|");
            group->addChild (percentParam (id::eng1Volume, "Engine 1 Volume", 0.8f));
            layout.add (std::move (group));
        }

        // ---------------- Filter 1 ----------------
        {
            auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("flt1", "Filter 1", "|");
            group->addChild (std::make_unique<Apf> (juce::ParameterID { id::flt1Cutoff, 1 }, "Filter 1 Cutoff",
                                                    logRange (cutoffMinHz, cutoffMaxHz), 2500.0f,
                                                    Attrs().withLabel ("Hz")
                                                           .withStringFromValueFunction (formatHz)
                                                           .withValueFromStringFunction (parseScaled)));
            group->addChild (percentParam (id::flt1Reso, "Filter 1 Resonance", 0.25f));
            layout.add (std::move (group));
        }

        // ---------------- Env VCA ----------------
        {
            auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("env_vca", "Env VCA", "|");
            group->addChild (timeParam (id::envVcaAttack,  "Env VCA Attack",  5.0f));
            group->addChild (timeParam (id::envVcaDecay,   "Env VCA Decay",   400.0f));
            group->addChild (percentParam (id::envVcaSustain, "Env VCA Sustain", 0.7f));
            group->addChild (timeParam (id::envVcaRelease, "Env VCA Release", 300.0f));
            layout.add (std::move (group));
        }

        return layout;
    }

    ParameterCache::ParameterCache (juce::AudioProcessorValueTreeState& apvts)
    {
        auto get = [&apvts] (const char* pid)
        {
            auto* p = apvts.getRawParameterValue (pid);
            jassert (p != nullptr); // ID inexistente: revisar Parameters.cpp
            return p;
        };

        masterVolume   = get (id::masterVolume);
        pitchBendRange = get (id::pitchBendRange);
        polyVoices     = get (id::polyVoices);
        eng1Volume     = get (id::eng1Volume);
        flt1Cutoff     = get (id::flt1Cutoff);
        flt1Reso       = get (id::flt1Reso);
        envVcaAttack   = get (id::envVcaAttack);
        envVcaDecay    = get (id::envVcaDecay);
        envVcaSustain  = get (id::envVcaSustain);
        envVcaRelease  = get (id::envVcaRelease);
    }
}
