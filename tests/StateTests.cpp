#include "TestHelpers.h"
#include "PluginProcessor.h"

class StateTests final : public juce::UnitTest
{
public:
    StateTests() : juce::UnitTest ("State save/load", "State") {}

    void runTest() override
    {
        beginTest ("Ida y vuelta idéntica de todos los parámetros");

        KbOneAudioProcessor a;
        juce::Random rng (777);
        for (auto* p : a.getParameters())
            p->setValueNotifyingHost (rng.nextFloat());
        a.setEditorScale (1.5f);

        juce::MemoryBlock data;
        a.getStateInformation (data);
        expectGreaterThan ((int) data.getSize(), 0);

        KbOneAudioProcessor b;
        b.setStateInformation (data.getData(), (int) data.getSize());

        const auto& pa = a.getParameters();
        const auto& pb = b.getParameters();
        expectEquals (pa.size(), pb.size());
        for (int i = 0; i < pa.size(); ++i)
            expectEquals (pb[i]->getValue(), pa[i]->getValue(),
                          "parámetro distinto: " + pa[i]->getName (64));
        expectEquals (b.getEditorScale(), 1.5f);

        beginTest ("El estado lleva número de versión");
        auto xml = juce::AudioProcessor::getXmlFromBinary (data.getData(), (int) data.getSize());
        expect (xml != nullptr);
        expectEquals (xml->getIntAttribute ("stateVersion"), kbone::params::stateVersion);

        beginTest ("Datos basura no rompen nada");
        const char junk[] = "esto no es un estado valido";
        b.setStateInformation (junk, (int) sizeof (junk));
        expectEquals (b.getEditorScale(), 1.5f);

        beginTest ("IDs estables: nunca deben cambiar");
        for (auto* id : { "master_volume", "pitch_bend_range", "poly_voices", "eng1_volume", "flt1_cutoff",
                          "flt1_reso", "env_vca_attack", "env_vca_decay", "env_vca_sustain", "env_vca_release" })
            expect (a.getApvts().getParameter (id) != nullptr, juce::String ("falta el ID ") + id);
    }
};

static StateTests stateTests;
