#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// =============================================================================
// Definición central de parámetros de KB ONE Maligno.
//
// REGLAS (ver CLAUDE.md §2):
//  - Los IDs son snake_case y NUNCA se renombran ni se reutilizan: cambiar un ID
//    rompe presets y automatizaciones guardadas en los proyectos del host.
//  - Cada parámetro nace con una versión (ParameterID{ id, version }). Un
//    parámetro agregado en una versión posterior del plugin usa la versión de
//    plugin en la que apareció (2, 3, ...), nunca se modifica la de los viejos.
//  - Para agregar un parámetro: declarar su ID acá, agregarlo en
//    Parameters.cpp (createLayout) y, si el DSP lo usa, leerlo en
//    ParameterCache.
// =============================================================================
namespace kbone::params
{
    // Versión del formato de estado guardado (getStateInformation). Subir
    // cuando cambie la estructura y agregar la migración en el procesador.
    inline constexpr int stateVersion = 1;

    // --- IDs estables --------------------------------------------------------
    namespace id
    {
        // Master / global
        inline constexpr const char* masterVolume    = "master_volume";
        inline constexpr const char* pitchBendRange  = "pitch_bend_range";
        inline constexpr const char* polyVoices      = "poly_voices";

        // Engine 1 (fase 1: un solo oscilador saw PolyBLEP)
        inline constexpr const char* eng1Volume      = "eng1_volume";

        // Filter 1 (fase 1: ladder simple)
        inline constexpr const char* flt1Cutoff      = "flt1_cutoff";
        inline constexpr const char* flt1Reso        = "flt1_reso";

        // Env VCA
        inline constexpr const char* envVcaAttack    = "env_vca_attack";
        inline constexpr const char* envVcaDecay     = "env_vca_decay";
        inline constexpr const char* envVcaSustain   = "env_vca_sustain";
        inline constexpr const char* envVcaRelease   = "env_vca_release";
    }

    // --- Rangos compartidos (para DSP, UI y tests) ---------------------------
    inline constexpr float masterVolumeMinDb = -60.0f;   // el mínimo se trata como -inf (silencio)
    inline constexpr float masterVolumeMaxDb = 6.0f;
    inline constexpr float cutoffMinHz       = 20.0f;
    inline constexpr float cutoffMaxHz       = 20000.0f;
    inline constexpr int   maxVoices         = 16;
    inline constexpr int   defaultVoices     = 8;
    inline constexpr float envMinTimeMs      = 0.5f;
    inline constexpr float envMaxTimeMs      = 20000.0f;

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // -------------------------------------------------------------------------
    // Punteros a los valores atómicos de cada parámetro. Se resuelven una sola
    // vez al crear el procesador; el hilo de audio solo hace load().
    // -------------------------------------------------------------------------
    struct ParameterCache
    {
        explicit ParameterCache (juce::AudioProcessorValueTreeState& apvts);

        std::atomic<float>* masterVolume   = nullptr;
        std::atomic<float>* pitchBendRange = nullptr;
        std::atomic<float>* polyVoices     = nullptr;
        std::atomic<float>* eng1Volume     = nullptr;
        std::atomic<float>* flt1Cutoff     = nullptr;
        std::atomic<float>* flt1Reso       = nullptr;
        std::atomic<float>* envVcaAttack   = nullptr;
        std::atomic<float>* envVcaDecay    = nullptr;
        std::atomic<float>* envVcaSustain  = nullptr;
        std::atomic<float>* envVcaRelease  = nullptr;
    };
}
