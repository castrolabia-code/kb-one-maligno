#pragma once

namespace kbone::dsp
{
    // -------------------------------------------------------------------------
    // Envolvente ADSR (fase 1). Se usa como Env VCA.
    //
    // - Attack: rampa lineal, llega a 1.0 exactamente en "attack" segundos.
    // - Decay / Release: exponenciales (carácter analógico). El tiempo indicado
    //   es lo que tarda en recorrer el 99 % del camino (−40 dB).
    // - Al terminar el release (nivel < −80 dB) pasa a Idle y la voz se libera.
    //
    // Delay, Hold, curvas por segmento, loop y one-shot llegan en la fase de
    // modulación; la interfaz pública está pensada para crecer.
    // -------------------------------------------------------------------------
    class Envelope
    {
    public:
        enum class Stage { idle, attack, decay, sustain, release };

        struct Settings
        {
            float attackSeconds  = 0.005f;
            float decaySeconds   = 0.4f;
            float sustainLevel   = 0.7f;
            float releaseSeconds = 0.3f;
        };

        void setSampleRate (double newSampleRate) noexcept;
        void setSettings (const Settings& newSettings) noexcept;

        // Arranca el ataque desde el nivel actual (sin saltos si la voz sonaba).
        void noteOn() noexcept;
        void noteOff() noexcept;
        void reset() noexcept;

        float process() noexcept;

        float getLevel() const noexcept { return static_cast<float> (level); }
        Stage getStage() const noexcept { return stage; }
        bool isActive() const noexcept  { return stage != Stage::idle; }

    private:
        void updateRates() noexcept;
        double coefficientFor (float seconds) const noexcept;

        double sampleRate = 44100.0;
        Settings settings;
        Stage stage = Stage::idle;
        // Estado en double: a 192 kHz con tiempos largos la acumulación en
        // float pierde precisión y los tiempos dejan de ser exactos.
        double level = 0.0;
        double attackStep = 0.0;
        double decayCoef = 0.0;
        double releaseCoef = 0.0;
    };
}
