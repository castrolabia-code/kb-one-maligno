cmake_minimum_required(VERSION 3.22)

project(KBOne VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Runtime estático de MSVC: el .vst3 no depende de que el usuario tenga instalado
# el redistribuible de Visual C++ (más robusto al cargarlo en Reason).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ----------------------------------------------------------------------------
# Opciones
# ----------------------------------------------------------------------------
option(KBONE_COPY_AFTER_BUILD "Copiar el .vst3 a C:/Program Files/Common Files/VST3 al terminar el build (requiere permisos de administrador)" OFF)
option(KBONE_BUILD_TESTS "Compilar el ejecutable de pruebas (ctest)" ON)

if(NOT WIN32)
    message(STATUS "KB ONE: el objetivo oficial es Windows x64. En otros sistemas se compila solo para pruebas/CI.")
endif()

# ----------------------------------------------------------------------------
# JUCE (versión fijada por tag: no cambiar sin probar)
# ----------------------------------------------------------------------------
include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(JUCE)

# ----------------------------------------------------------------------------
# Código compartido (plugin + tests). Es una librería INTERFACE: cada destino
# compila sus propias copias de los módulos de JUCE.
# ----------------------------------------------------------------------------
add_library(KBOneCore INTERFACE)

target_sources(KBOneCore INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/source/PluginProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/PluginEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/params/Parameters.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/dsp/filters/LadderFilter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/dsp/modulation/Envelope.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/dsp/voice/SynthVoice.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/dsp/voice/VoiceManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/ui/lookandfeel/KbOneLookAndFeel.cpp
)

target_include_directories(KBOneCore INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/source)

target_compile_definitions(KBOneCore INTERFACE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    KBONE_VERSION_STRING="${PROJECT_VERSION}"
)

target_link_libraries(KBOneCore INTERFACE
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# ----------------------------------------------------------------------------
# Plugin
# ----------------------------------------------------------------------------
set(KBONE_PLUGIN_COPY_ARGS)
if(WIN32)
    set(KBONE_PLUGIN_COPY_ARGS VST3_COPY_DIR "C:/Program Files/Common Files/VST3")
endif()

juce_add_plugin(KBOne
    PRODUCT_NAME            "KB ONE Maligno"
    COMPANY_NAME            "Castro Lab"
    BUNDLE_ID               com.castrolab.kbone
    PLUGIN_MANUFACTURER_CODE CsLb
    PLUGIN_CODE             KbOn
    FORMATS                 VST3 Standalone
    IS_SYNTH                TRUE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES         Instrument Synth
    COPY_PLUGIN_AFTER_BUILD ${KBONE_COPY_AFTER_BUILD}
    ${KBONE_PLUGIN_COPY_ARGS}
)

target_link_libraries(KBOne PRIVATE KBOneCore)

# ----------------------------------------------------------------------------
# Tests
# ----------------------------------------------------------------------------
if(KBONE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
