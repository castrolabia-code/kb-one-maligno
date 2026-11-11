#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <iterator>
#include <numbers>

namespace
{
    // Limitador suave de seguridad: transparente por debajo de 0.9 (≈ −0.9 dBFS)
    // y por encima comprime con tanh hacia 1.0. Derivada continua: no agrega
    // armónicos bruscos. Solo actúa ante picos destructivos.
    inline float safetyLimit (float x) noexcept
    {
        constexpr float threshold = 0.9f;
        const float a = std::abs (x);
        if (a <= threshold)
            return x;
        const float over = (a - threshold) / (1.0f - threshold);
        return std::copysign (threshold + (1.0f - threshold) * std::tanh (over), x);
    }

    float dbToGain (float db) noexcept
    {
        // Margen de 0.01 dB: la conversión normalizada → dB no devuelve exactamente el mínimo.
        return db <= kbone::params::masterVolumeMinDb + 0.01f ? 0.0f : juce::Decibels::decibelsToGain (db);
    }
}

KbOneAudioProcessor::KbOneAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "KBONE", kbone::params::createLayout()),
      paramCache (apvts)
{
}

kbone::dsp::VoiceParameters KbOneAudioProcessor::readVoiceParameters() const noexcept
{
    kbone::dsp::VoiceParameters p;
    p.voiceLimit     = juce::roundToInt (paramCache.polyVoices->load());
    p.pitchBendRange = paramCache.pitchBendRange->load();
    p.cutoffHz       = paramCache.flt1Cutoff->load();
    p.resonance      = paramCache.flt1Reso->load();
    p.engineVolume   = paramCache.eng1Volume->load();
    p.ampEnv.attackSeconds  = paramCache.envVcaAttack->load()  * 0.001f;
    p.ampEnv.decaySeconds   = paramCache.envVcaDecay->load()   * 0.001f;
    p.ampEnv.sustainLevel   = paramCache.envVcaSustain->load();
    p.ampEnv.releaseSeconds = paramCache.envVcaRelease->load() * 0.001f;
    return p;
}

void KbOneAudioProcessor::prepareToPlay (double sampleRate, int)
{
    voiceManager.setParameters (readVoiceParameters());
    voiceManager.prepare (sampleRate);

    masterGain.reset (sampleRate, 0.02);
    masterGain.setCurrentAndTargetValue (dbToGain (paramCache.masterVolume->load()));

    // Filtro DC de 1 polo con corte ≈ 5 Hz, independiente de la frecuencia de muestreo.
    dcCoef = static_cast<float> (std::exp (-2.0 * std::numbers::pi * 5.0 / sampleRate));
    std::fill (std::begin (dcX1), std::end (dcX1), 0.0f);
    std::fill (std::begin (dcY1), std::end (dcY1), 0.0f);

    panicRequested.store (false);
}

void KbOneAudioProcessor::releaseResources() {}

bool KbOneAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Instrumento: sin entradas, salida estéreo (o mono, por compatibilidad).
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled())
        return false;

    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void KbOneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    buffer.clear();

    if (numSamples == 0 || numChannels == 0)
        return;

    if (panicRequested.exchange (false))
        voiceManager.panic();

    voiceManager.setParameters (readVoiceParameters());
    masterGain.setTargetValue (dbToGain (paramCache.masterVolume->load()));

    float* left  = buffer.getWritePointer (0);
    float* right = numChannels > 1 ? buffer.getWritePointer (1) : left;
    const bool isMono = numChannels == 1;

    // En mono se renderiza dos veces sobre el mismo canal: se compensa abajo.
    // Se sintetiza por tramos entre eventos MIDI (precisión de muestra).
    int position = 0;
    for (const auto metadata : midi)
    {
        const int eventPos = juce::jlimit (0, numSamples, metadata.samplePosition);
        if (eventPos > position)
        {
            voiceManager.render (left, right, position, eventPos - position);
            position = eventPos;
        }
        voiceManager.handleMidi (metadata.getMessage());
    }
    if (position < numSamples)
        voiceManager.render (left, right, position, numSamples - position);

    // --- Master ---
    const float monoComp = isMono ? 0.5f : 1.0f;
    const int masterChannels = juce::jmin (numChannels, 2);

    for (int i = 0; i < numSamples; ++i)
    {
        const float g = masterGain.getNextValue() * monoComp;
        for (int ch = 0; ch < masterChannels; ++ch)
        {
            float* data = buffer.getWritePointer (ch);
            const float x = data[i] * g;

            // Filtro DC: y[n] = x[n] - x[n-1] + R * y[n-1]
            const float y = x - dcX1[ch] + dcCoef * dcY1[ch];
            dcX1[ch] = x;
            dcY1[ch] = std::isfinite (y) ? y : 0.0f;

            data[i] = safetyLimit (dcY1[ch]);
        }
    }

    // Canales extra (si el host pidiera más de 2): copia del izquierdo.
    for (int ch = 2; ch < numChannels; ++ch)
        buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);

    activeVoices.store (voiceManager.getNumActiveVoices());
}

juce::AudioProcessorEditor* KbOneAudioProcessor::createEditor()
{
    return new KbOneAudioProcessorEditor (*this);
}

// --- Estado -----------------------------------------------------------------
// Formato: <KBONE stateVersion="1" editorScale="1.0"> + parámetros del APVTS.
void KbOneAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("stateVersion", kbone::params::stateVersion, nullptr);
    state.setProperty ("pluginVersion", KBONE_VERSION_STRING, nullptr);
    state.setProperty ("editorScale", editorScale.load(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void KbOneAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    const int version = state.getProperty ("stateVersion", 1);
    juce::ignoreUnused (version); // acá irán las migraciones cuando stateVersion > 1

    if (state.hasProperty ("editorScale"))
        setEditorScale (static_cast<float> (state.getProperty ("editorScale")));

    apvts.replaceState (state);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KbOneAudioProcessor();
}
