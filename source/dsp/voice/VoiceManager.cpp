#include "VoiceManager.h"

namespace kbone::dsp
{
    namespace
    {
        constexpr double smoothingSeconds = 0.02;      // 20 ms de rampa para perillas
        constexpr double bendSmoothingSeconds = 0.005; // el pitch bend responde más rápido

        // Provisorio (fase 1): la mod wheel abre el filtro hasta +3 octavas.
        // En la fase de modulación pasa a ser una fuente más de la matriz.
        constexpr float modWheelCutoffOctaves = 3.0f;
    }

    void VoiceManager::prepare (double sampleRate) noexcept
    {
        for (auto& v : voices)
            v.prepare (sampleRate);

        cutoff.reset (sampleRate, smoothingSeconds);
        level.reset (sampleRate, smoothingSeconds);
        reso.reset (sampleRate, smoothingSeconds);
        modWheel.reset (sampleRate, smoothingSeconds);
        bend.reset (sampleRate, bendSmoothingSeconds);

        cutoff.setCurrentAndTargetValue (current.cutoffHz);
        level.setCurrentAndTargetValue (current.engineVolume);
        reso.setCurrentAndTargetValue (current.resonance);
        for (auto& v : voices)
            v.setEnvelopeSettings (current.ampEnv);
        lastEnvSettings = current.ampEnv;

        sustainDown = false;
        resetControllers();
    }

    void VoiceManager::setParameters (const VoiceParameters& p) noexcept
    {
        current = p;
        current.voiceLimit = juce::jlimit (1, params::maxVoices, p.voiceLimit);

        cutoff.setTargetValue (juce::jlimit (params::cutoffMinHz, params::cutoffMaxHz, p.cutoffHz));
        level.setTargetValue (p.engineVolume);
        reso.setTargetValue (juce::jlimit (0.0f, 1.0f, p.resonance));

        const auto& e = p.ampEnv;
        const auto& l = lastEnvSettings;
        const bool envChanged = ! juce::exactlyEqual (e.attackSeconds, l.attackSeconds)
                             || ! juce::exactlyEqual (e.decaySeconds, l.decaySeconds)
                             || ! juce::exactlyEqual (e.sustainLevel, l.sustainLevel)
                             || ! juce::exactlyEqual (e.releaseSeconds, l.releaseSeconds);
        if (envChanged)
        {
            for (auto& v : voices)
                v.setEnvelopeSettings (e);
            lastEnvSettings = e;
        }

        // Si se bajó la cantidad de voces, las que quedan fuera se apagan con fade.
        for (int i = current.voiceLimit; i < params::maxVoices; ++i)
            if (voices[(size_t) i].isActive())
                voices[(size_t) i].kill();
    }

    void VoiceManager::handleMidi (const juce::MidiMessage& m) noexcept
    {
        if (m.isNoteOn())
            noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff())
            noteOff (m.getNoteNumber());
        else if (m.isPitchWheel())
            setPitchWheel (m.getPitchWheelValue());
        else if (m.isSustainPedalOn())
            setSustain (true);
        else if (m.isSustainPedalOff())
            setSustain (false);
        else if (m.isAllSoundOff())
            panic();
        else if (m.isAllNotesOff())
            allNotesOff();
        else if (m.isResetAllControllers())
            resetControllers();
        else if (m.isController() && m.getControllerNumber() == 1)
            setModWheel (static_cast<float> (m.getControllerValue()) / 127.0f);
    }

    SynthVoice* VoiceManager::findVoiceToUse() noexcept
    {
        const auto limit = static_cast<size_t> (current.voiceLimit);

        // 1) Voz libre.
        for (size_t i = 0; i < limit; ++i)
            if (! voices[i].isActive())
                return &voices[i];

        // 2) Voz en release más silenciosa.
        SynthVoice* best = nullptr;
        for (size_t i = 0; i < limit; ++i)
            if (voices[i].isReleasing() && ! voices[i].keyDown
                && (best == nullptr || voices[i].getLevel() < best->getLevel()))
                best = &voices[i];
        if (best != nullptr)
            return best;

        // 3) La más antigua (preferentemente sostenida por pedal y no por tecla).
        for (size_t i = 0; i < limit; ++i)
        {
            auto& v = voices[i];
            if (best == nullptr
                || (! v.keyDown && best->keyDown)
                || (v.keyDown == best->keyDown && v.getTimestamp() < best->getTimestamp()))
                best = &v;
        }
        return best;
    }

    void VoiceManager::noteOn (int note, float velocity) noexcept
    {
        if (velocity <= 0.0f)
        {
            noteOff (note);
            return;
        }

        ++noteCounter;

        // Misma nota ya sonando (por pedal o release): se reutiliza la voz.
        for (int i = 0; i < current.voiceLimit; ++i)
        {
            auto& v = voices[(size_t) i];
            if (v.isActive() && v.getNote() == note)
            {
                v.retrigger (velocity, noteCounter);
                return;
            }
        }

        if (auto* v = findVoiceToUse())
            v->startNote (note, velocity, noteCounter);
    }

    void VoiceManager::noteOff (int note) noexcept
    {
        for (auto& v : voices)
        {
            if (v.isActive() && v.keyDown && v.getNote() == note)
            {
                if (sustainDown)
                {
                    v.keyDown = false;
                    v.sustained = true;
                }
                else
                {
                    v.release();
                }
            }
        }
    }

    void VoiceManager::setSustain (bool down) noexcept
    {
        sustainDown = down;
        if (! down)
            for (auto& v : voices)
                if (v.isActive() && v.sustained && ! v.keyDown)
                    v.release();
    }

    void VoiceManager::setPitchWheel (int value14bit) noexcept
    {
        // 0..16383 con centro en 8192 → −1..+1
        const auto normalised = juce::jlimit (-1.0f, 1.0f, static_cast<float> (value14bit - 8192) / 8191.0f);
        bend.setTargetValue (normalised);
    }

    void VoiceManager::setModWheel (float value01) noexcept { modWheel.setTargetValue (juce::jlimit (0.0f, 1.0f, value01)); }

    void VoiceManager::allNotesOff() noexcept
    {
        sustainDown = false;
        for (auto& v : voices)
            if (v.isActive())
                v.release();
    }

    void VoiceManager::panic() noexcept
    {
        sustainDown = false;
        for (auto& v : voices)
            v.kill();
        resetControllers();
    }

    void VoiceManager::resetControllers() noexcept
    {
        bend.setCurrentAndTargetValue (0.0f);
        modWheel.setCurrentAndTargetValue (0.0f);
        if (sustainDown)
            setSustain (false);
    }

    void VoiceManager::render (float* left, float* right, int startSample, int numSamples) noexcept
    {
        while (numSamples > 0)
        {
            const int n = juce::jmin (numSamples, controlBlockSize);

            ControlBlock ctrl;
            const float range = current.pitchBendRange;

            const float cutStart = cutoff.getCurrentValue();
            const float mwStart  = modWheel.getCurrentValue();
            ctrl.bendSemitonesStart = bend.getCurrentValue() * range;
            ctrl.levelStart = level.getCurrentValue();

            const float cutEnd = cutoff.skip (n);
            const float mwEnd  = modWheel.skip (n);
            ctrl.bendSemitonesEnd = bend.skip (n) * range;
            ctrl.levelEnd = level.skip (n);

            ctrl.cutoffHzStart = cutStart * std::exp2 (mwStart * modWheelCutoffOctaves);
            ctrl.cutoffHzEnd   = cutEnd   * std::exp2 (mwEnd   * modWheelCutoffOctaves);
            ctrl.resonance = reso.skip (n);

            for (auto& v : voices)
                if (v.isActive())
                    v.render (left + startSample, right + startSample, n, ctrl);

            startSample += n;
            numSamples -= n;
        }
    }

    int VoiceManager::getNumActiveVoices() const noexcept
    {
        int count = 0;
        for (auto& v : voices)
            if (v.isActive())
                ++count;
        return count;
    }
}
