# KB ONE Maligno — by Castro Lab
## Especificación maestra del sintetizador VST3 (leer SIEMPRE antes de trabajar)

Sos un ingeniero senior de DSP y desarrollo de instrumentos virtuales, experto en C++, JUCE, síntesis sustractiva, wavetable y FM, y diseño de interfaces de plugins de nivel comercial (referencia de calidad: Arturia Pigments, Serum, Vital, Diva). Este archivo es la fuente de verdad del proyecto. Si una instrucción de una fase contradice este archivo, preguntá antes de seguir.

Las imágenes de referencia visual están en `docs/referencias/REFERENCIAS_1.jpg` y `docs/referencias/REFERENCIAS_2.jpg`. Miralas antes de cualquier trabajo de interfaz.

---

## 1. Identidad del producto

| Campo | Valor |
|---|---|
| Nombre del plugin | KB ONE Maligno |
| Fabricante | Castro Lab |
| Código de fabricante (4 caracteres) | `CsLb` |
| Código de plugin (4 caracteres) | `KbOn` |
| Formatos | VST3 + Standalone (solo Windows 10/11 x64) |
| Tipo | Instrumento (synth), entrada MIDI, salida estéreo |
| Host principal de prueba | Reason 12.5+ y Reason 14 (Windows). Reason 12 necesita la versión 12.5 o posterior para cargar VST3 |
| Versión inicial | 1.0.0 |

## 2. Stack técnico (obligatorio)

- **C++20**, **JUCE 8** (última 8.0.x estable) traído con `FetchContent` y versión fijada por tag. Nada de Projucer: todo por **CMake**.
- Compilador: **Visual Studio 2022 (MSVC)**, generador `"Visual Studio 17 2022"`, arquitectura x64.
- Parámetros con `juce::AudioProcessorValueTreeState` (APVTS). Cada parámetro tiene ID estable en `snake_case` (ej. `eng1_cutoff`) y **nunca se renombra** una vez creado (rompe presets y automatización). Usar `ParameterID{ "id", 1 }` con versión.
- Sin dependencias externas pesadas. Permitido: JUCE, `juce_dsp`, y código propio. Nada con licencia GPL incompatible con uso comercial.
- Tests: `ctest` con un ejecutable de pruebas propio (ver sección 12).
- Validación: **pluginval** en nivel de exigencia 10 debe pasar sin errores.
- Al terminar cada build, copiar el `.vst3` a `C:\Program Files\Common Files\VST3\` (paso post-build opcional activado con opción CMake `KBONE_COPY_AFTER_BUILD`).

### Estructura de carpetas

```
KB-ONE/
├─ CMakeLists.txt
├─ CLAUDE.md
├─ docs/referencias/        (imágenes de referencia)
├─ docs/PROGRESO.md         (bitácora: qué se hizo en cada fase, qué falta, decisiones)
├─ source/
│  ├─ PluginProcessor.*  PluginEditor.*
│  ├─ params/              (definición central de parámetros: Parameters.h/.cpp)
│  ├─ dsp/
│  │  ├─ engines/          (AnalogEngine, WavetableEngine, FmEngine, UtilityEngine)
│  │  ├─ filters/          (Ladder, SVF, MS20, Formant, Comb)
│  │  ├─ modulation/       (Envelope, Lfo, ModMatrix, Macros)
│  │  ├─ fx/               (cada efecto en su archivo + FxChain)
│  │  ├─ sequencer/        (Arpeggiator, StepSequencer)
│  │  └─ voice/            (SynthVoice, VoiceManager, Glide)
│  ├─ presets/             (PresetManager, FactoryPresets)
│  └─ ui/
│     ├─ lookandfeel/      (KbOneLookAndFeel, Colours, Fonts)
│     ├─ components/       (Knob, Slider, Button, Led, Jack, Display, EnvelopeEditor, Scope, Spectrum...)
│     └─ panels/           (TopBar, EnginePanel, FilterPanel, EnvPanel, LfoPanel, FxPanel, SeqPanel, BrowserPanel...)
├─ resources/              (fuentes, wavetables de fábrica generadas, presets .kbone)
├─ tools/                  (renderizador offline, generador de presets, scripts)
└─ tests/
```

## 3. Arquitectura de sonido (flujo de señal por voz)

```
[ENGINE 1] ─┐
[ENGINE 2] ─┼─► mezclador de voz ─► FILTER 1 ─┬─(serie/paralelo, slider continuo)─► FILTER 2 ─► VCA (Env VCA) ─► pan
[UTILITY ] ─┘                                 └───────────────────────────────────────────────┘
                                                        │
          suma de voces ─► FX BUS A ─┐
                         ─► FX BUS B ─┼─► SEND BUS (reverb/delay) ─► Master (volumen global, limitador suave de seguridad) ─► salida
```

- **Polifonía:** 1 a 16 voces (por defecto 8). Modos: Poly, Mono, Legato. Robo de voces por la más antigua/silenciosa con fade de 2–5 ms (nunca clicks).
- **Glide/portamento:** tiempo 0–2 s, modo siempre / solo legato, curva exponencial.
- **Pitch bend** configurable ±1 a ±24 semitonos. Mod wheel, aftertouch (canal) y velocidad disponibles como fuentes de modulación.
- **Unison** por motor (ver motores), con detune, spread estéreo y blend.
- **Drift analógico** global (micro-variaciones aleatorias lentas de afinación y cutoff por voz), 0–100 %.

## 4. Motores de sonido

Hay **dos slots de motor (ENGINE 1 y ENGINE 2)**, cada uno con un selector de tipo (desplegable como en la referencia): **Analog / Wavetable / FM / Off**. Cada slot tiene: Tune (±24 st), Fine (±100 cents), Volume, Pan, envío a Filter 1/Filter 2 (balance), y su sección de Unison (Voices 1–8, Detune, Stereo, Mode: clásico/acorde/supersaw).

### 4.1 Analog
- 3 osciladores por motor: formas Saw, Square/Pulse (PWM), Triangle, Sine; morph continuo entre formas.
- Anti-aliasing obligatorio: **PolyBLEP/PolyBLAMP** como mínimo (preferible minBLEP).
- Hard sync (osc 2 y 3 a osc 1), FM lineal osc1→osc2, ring mod.
- Ruido (white, pink, red) con color.
- Fase libre o reset por nota, ajustable.

### 4.2 Wavetable
- Tablas de 2048 muestras por frame, hasta 256 frames. Compatibles con el formato WAV de wavetables estilo Serum (frames concatenados).
- **Mipmaps band-limited** (una versión por octava generada con FFT) para cero aliasing audible. Interpolación entre frames y cúbica (o mejor) entre muestras.
- **Mínimo 32 wavetables de fábrica generadas por código** (aditivas, formantes, PWM, vocales sintéticas, digitales, metálicas, armónicas). Nada copiado de otros productos.
- Parámetros: Position (morph), Warp con modos (Bend +/-, Sync, PWM, Mirror, Quantize, FM desde osc interno, Asym), Warp Amount.
- Importar WAV propio (botón en el display).
- Display: forma de onda actual animada en el visor del motor (osciloscopio con brillo neón como en la referencia).

### 4.3 FM
- 4 operadores, **8 algoritmos** (incluyendo los clásicos tipo DX: pila 4→3→2→1, 2 pares, 3 portadoras+1 moduladora, todas en paralelo, etc.) con diagrama del algoritmo dibujado en el display.
- Por operador: Ratio (0.5–32, con modo fijo en Hz), Fine, Level, Velocity sens, forma (sine + 3 variantes), y envolvente ADSR propia simplificada.
- Feedback en el operador superior.
- **Oversampling 2x interno** en el motor FM para reducir aliasing.

### 4.4 Utility (siempre presente)
- Sub-oscilador (sine/square, -1 o -2 octavas), Noise (tipos), y nivel de cada uno.

## 5. Filtros

Dos filtros (FILTER 1 y FILTER 2) con encendido/apagado y routing **serie ↔ paralelo continuo**. Cada filtro: Type (desplegable), Cutoff (20 Hz–20 kHz, escala logarítmica), Reso, Drive, KBD tracking (−100 % a +200 %), Env amount (desde Env 2 por defecto), Volume, Pan.

Tipos (todos topología **ZDF/TPT**, estables con modulación rápida y resonancia alta):
- **Mini** — ladder 4 polos 24 dB/oct con saturación tanh por etapa (carácter Moog).
- **SVF Multimode** — LP/BP/HP/Notch 12 dB, morph continuo LP→BP→HP.
- **MS-20** — Sallen-Key 12 dB con saturación agresiva (carácter Korg).
- **Formant** — vocales A/E/I/O/U con morph continuo, Freq Shift y Blend (como en la referencia).
- **Comb** — feedback/feedforward, para sonidos metálicos y Karplus.
- **Acid 303** — 4 polos tipo diodo, con Accent que sube resonancia/cutoff, pensado para el secuenciador.

La saturación de los filtros corre con **oversampling** (2x en Eco, 4x en High quality).
Display: curva de respuesta del filtro en tiempo real (como en la referencia).

## 6. Modulación

- **3 envolventes**: **Env VCA** (amplitud, fija al VCA), **Env 2**, **Env 3**. Cada una: Delay, Attack, Hold, Decay, Sustain, Release, con **curva ajustable por segmento** (Att Curve, etc.), modo loop y modo one-shot. Editor gráfico arrastrable por puntos (como en la referencia), sincronizado con las perillas.
- **3 LFO**: formas Sine, Triangle, Saw up/down, Square, S&H, Smooth random, y forma personalizada dibujable (opcional en v1). Rate libre (0.01–50 Hz) o sincronizado al tempo del host (1/64 a 8 compases, con tresillos y puntillos), Phase, Fade-in, Retrigger/Free/One-shot, unipolar/bipolar.
- **4 Macros** (Timbre, Time, Brightness, FX — como en la referencia 2), renombrables por preset.
- **Fuentes extra:** Velocity, Aftertouch, Mod Wheel, Pitch Bend, Keytrack, Random por nota, Voice index, y la línea de modulación del secuenciador.
- **Matriz de modulación** de 24 slots: Fuente → Destino, Amount (bipolar), Vía (fuente secundaria que escala), curva. Casi todo parámetro continuo debe ser destino (cutoffs, resos, position, warp, niveles FM, tunes, pans, parámetros de FX, rates de LFO, etc.).
- **Asignación estilo Pigments:** arrastrar una fuente (desde su etiqueta o desde un **jack** del panel) y soltarla sobre una perilla crea la ruta. La perilla muestra un **anillo de modulación** de color con la profundidad y una animación del valor modulado en tiempo real.
- La modulación se calcula **por voz** (salvo LFOs en modo global, opcional) y en tasa de control con interpolación (cada 16–32 muestras), nunca a escalones audibles.

## 7. Efectos (sector FX)

- **FX BUS A** y **FX BUS B**: 3 slots cada uno, routing A→B en serie o en paralelo. **SEND BUS**: 2 slots con niveles de envío desde A y B.
- Cada slot: selector de efecto, bypass, Dry/Wet, y sus perillas. Orden reordenable arrastrando.
- Efectos disponibles (todos propios, de calidad comercial):
  - **Distortion**: Soft clip, Hard clip, Tube, Fold (wavefolder), Rectify, con oversampling 4x y tono pre/post.
  - **Bitcrusher**: bits, sample-rate reduction, jitter.
  - **Chorus** (estéreo, 1–4 voces), **Flanger**, **Phaser** (4–12 etapas).
  - **Delay**: estéreo / ping-pong, sync al tempo, feedback con filtro LP/HP en el loop, modulación, ducking.
  - **Reverb**: algorítmica tipo **FDN de 8 líneas** (o mejor) con Size, Decay, Pre-delay, Damping, Width, modulación. **No usar `juce::Reverb`** (calidad insuficiente).
  - **EQ** 3 bandas paramétrica + low/high cut.
  - **Compressor** (con sidechain interno no requerido), RMS/peak, makeup.
  - **Multi-filter** (LP/HP/BP sincronizable), **Stereo Widener** (mid/side), **Overdrive** del master (la perilla OVERDRIVE de la referencia).
- **Master**: volumen global (−inf a +6 dB), medidor de salida y un limitador suave de seguridad (desactivable) para evitar picos destructivos.

## 8. Arpegiador y secuenciador de pasos

Selector de modo en el panel: **OFF / ARP / SEQ**.

### 8.1 Arpegiador
- Modos: Up, Down, Up-Down, Down-Up, Random, Order (orden tocado), Chord, Converge/Diverge.
- Octavas 1–4, Rate sincronizado (1/1 a 1/64, tresillos/puntillos), Gate (5–200 %), Swing (0–75 %), Latch/Hold, Repeat por nota, Velocity: tocada o fija.
- Sincronía con la transporte del host (ppq position) — debe quedar alineado a compás al apretar Play en Reason.

### 8.2 Step Sequencer estilo 303 (como la referencia)
- 16 pasos (longitud 1–32), por paso: **Pitch** (nota + octava), **Gate** (on/off/tie), **Accent**, **Slide**, y un valor de **Mod lane** (fuente de modulación).
- **Accent** sube volumen y cutoff/reso (cantidad ajustable). **Slide** hace glide legato al paso siguiente (estilo 303, tiempo ajustable).
- **8 patrones por preset** (Pattern Selector 1–8), con cambio de patrón cuantizado al final del compás. Funciones: Clear, Copy/Paste, Insert/Delete paso, Randomize (con cantidad), Transpose (por teclado: la nota tocada transpone el patrón), Time Mode (1/8, 1/16, 1/32, tresillos).
- Botonera tipo 303 de la referencia: teclas de nota C–C, Pitch Mode, Time Mode, Accent, Slide, Start/Stop, Write/Next, Back, Tap, con LEDs rojos que siguen el paso actual.
- Modo grabación por paso desde el teclado MIDI.
- Sincronizado al host; también puede correr libre en el standalone con tempo propio.

## 9. Interfaz gráfica (estética = prioridad alta)

### 9.1 Dirección de arte
- Inspirada en las dos referencias: **panel amarillo industrial (#F2C230 aprox.) con bloques negro carbón/grafito (#16171A – #24262B)**, perillas negras con **anillo dorado/latón** e **indicador rojo**, LEDs rojos, displays oscuros con trazos **neón** (cian, magenta, naranja, rojo), logo del **diablito neón rojo "KB ONE Maligno"** arriba al centro con resplandor (glow).
- Aspecto **"render" fotorrealista dibujado 100 % por código vectorial** (sin bitmaps de perillas): cada perilla con sombra proyectada, degradado radial en el cuerpo, bisel con reflejo especular, marcas de escala alrededor, y anillo de valor. Botones con volumen y estado iluminado. Jacks 3D con anillo metálico. Tornillos y manijas laterales metálicas como en la referencia 1. Textura sutil de ruido sobre el amarillo (generada por código, cacheada en una imagen al cambiar de tamaño).
- **No copiar los textos de las imágenes literalmente**: son referencias generadas y tienen errores (SERGET, BITABM, TOGGE, OVERVRIVE, etc.). Usar etiquetas correctas y coherentes en inglés, en mayúsculas, con tipografía sans geométrica embebida (por ej. Inter, Montserrat o Rajdhani — licencia libre OFL, incluida en `resources/`).
- **Los jacks no son decorativos vacíos**: son las fuentes de modulación arrastrables (ENV 2, ENV 3, LFO 1–3, MACRO 1–4, VEL, MW, AT, SEQ MOD, RANDOM). Arrastrar desde un jack hacia una perilla crea la modulación, con un cable animado mientras se arrastra.

### 9.2 Distribución (basada en la referencia 1)
- **Barra superior**: menú/logo Castro Lab, navegador de presets (◄ nombre ►, clic abre el browser), medidor de CPU, logo central, **Global Volume**, botones: browser, matriz de modulación, ayuda, ajustes.
- **Fila 1**: ENGINE 1 (izquierda) | display central de **analizador de espectro** | ENGINE 2 (derecha). Cada motor con su display (osciloscopio/wavetable/algoritmo FM) y su sección Unison.
- **Fila 2**: Env VCA | Env 2 | Env 3 (editores gráficos) | botonera central | FILTER 1 | FILTER 2 (con curvas).
- **Fila 3**: panel de **Arp/Step Sequencer** estilo 303 + **Macros**.
- **Barra inferior**: LFOs, pestañas para cambiar la vista central entre **FX**, **MOD MATRIX** y **SEQ/ARP** (las vistas que no entran se muestran por pestañas, como Pigments, sin amontonar).
- Tamaño base **1376 × 768** (relación de las referencias), **redimensionable** de 75 % a 200 % manteniendo proporción, con el escalado guardado en ajustes. Nítido en pantallas 4K (todo vectorial).

### 9.3 Comportamiento de controles (todo funcional, nada falso)
- **Cada perilla, slider, botón y selector visible debe estar conectado a un parámetro real o a una acción real.** Prohibido dejar controles decorativos sin función.
- Arrastre vertical; **Ctrl+arrastre** = ajuste fino; **doble clic** = valor por defecto; rueda del mouse = paso fino; **clic derecho** = menú (resetear, MIDI Learn, quitar modulaciones, copiar/pegar valor).
- Tooltip/lectura flotante con el valor y su unidad (Hz, ms, dB, %, st) al pasar o arrastrar.
- Todos los parámetros automatizables desde el host con nombres legibles (ej. "Filter 1 Cutoff").
- Animaciones (scopes, espectro, LEDs del secuenciador, anillos de modulación) a 30–60 fps con `juce::VBlankAttachment` o timer, leyendo datos del audio por **FIFO lock-free** — la UI nunca bloquea al audio.
- CPU de la interfaz razonable: cachear fondos estáticos en imágenes y redibujar solo lo que cambia.

## 10. Presets

- **100 presets de fábrica**, embebidos en el binario (BinaryData), formato XML propio `.kbone` (estado APVTS + datos de secuenciador + nombre de macros + metadatos: nombre, autor "Castro Lab", categoría, tags).
- Distribución: Bass 15, Lead 15, Pad 15, Pluck 10, Keys 8, Arp/Sequence 12 (que usen el arp o el 303), FX/Atmósfera 8, Bells/FM 7, Brass/Strings sintéticos 5, Drums/Percusión 5.
- Cada preset debe sonar profesional y distinto: usar los 3 motores, filtros, modulación, macros asignadas con sentido (Timbre/Time/Brightness/FX), y efectos. Nombres creativos en inglés, sin nombres de marcas.
- Los presets se generan con una herramienta de diseño en `tools/` (C++ o script) que escribe los `.kbone`, para poder regenerarlos y ajustarlos.
- **Browser**: lista filtrable por categoría y tags, búsqueda por texto, favoritos, anterior/siguiente, Init patch, guardar preset de usuario en `%USERPROFILE%\Documents\Castro Lab\KB ONE\Presets\`. Cambiar de preset no debe producir clicks ni notas colgadas.
- El estado completo se guarda y recupera con el proyecto del host (`getStateInformation`/`setStateInformation`) con número de versión para migraciones futuras.

## 11. Reglas de calidad de audio (no negociables)

1. **Hilo de audio en tiempo real**: en `processBlock` no se reserva memoria, no se usan locks, no hay I/O de archivos, no hay `std::string` dinámicos ni logs. Todo se prepara en `prepareToPlay`.
2. `juce::ScopedNoDenormals` en el proceso; protección contra NaN/Inf (si aparece, resetear la voz y no propagar).
3. **Suavizado de todos los parámetros** que afectan el audio (`SmoothedValue` o equivalente) — cero zipper noise y cero clicks al mover perillas o cambiar presets.
4. Independiente de la frecuencia de muestreo (44.1 a 192 kHz) y del tamaño de bloque (1 a 4096, incluyendo tamaños variables). Eventos MIDI procesados con precisión de muestra (sample-accurate).
5. Anti-aliasing: PolyBLEP/minBLEP en analógico, mipmaps en wavetable, oversampling en FM, saturaciones y distorsión.
6. Ajuste de **Calidad** en Settings: Eco / High / Ultra (cambia oversampling y tasa de modulación). Por defecto High.
7. Rendimiento objetivo: un pad de 8 voces con unison 4 en ambos motores, 2 filtros y FX completos debe quedar por debajo de ~15 % de un núcleo moderno a 48 kHz / buffer 256 en modo High. Medir y reportar.
8. Nivel de salida: los presets de fábrica normalizados para picos en torno a −6 dBFS y sonoridad pareja entre sí.
9. Sin DC offset en la salida (filtro DC en el master).
10. Voces sin notas colgadas: All Notes Off / panic, sustain pedal (CC64) correcto.

## 12. Pruebas y verificación

- `tests/`: pruebas unitarias para osciladores (sin NaN, amplitud correcta, aliasing por debajo de un umbral en un barrido medido con FFT), filtros (estabilidad con resonancia máxima y cutoff modulado al audio rate), envolventes (tiempos correctos), arpegiador/secuenciador (timing exacto en muestras a distintos tempos), guardado/carga de estado (ida y vuelta idéntica).
- `tools/render_presets`: renderizador offline que carga cada preset de fábrica, toca una frase MIDI de prueba y escribe un WAV, y reporta: silencio, clipping, DC, NaN, pico y RMS. Ningún preset puede fallar.
- **pluginval** nivel 10 sin errores antes de cerrar cada fase que toque el procesador.
- Después de cada fase, actualizar `docs/PROGRESO.md` con: qué se hizo, cómo probarlo en Reason, problemas conocidos y siguiente paso.

## 13. Forma de trabajo

- Trabajá **por fases** (las define el usuario en cada prompt). Al final de cada fase el proyecto **debe compilar sin warnings importantes y funcionar** en Reason.
- Antes de escribir código en una fase grande, presentá un plan corto de archivos y clases; después ejecutalo sin esperar confirmación salvo que haya una decisión de diseño realmente dudosa.
- Código limpio, comentado en español en las partes de DSP no obvias, clases pequeñas, sin código duplicado entre motores (compartir utilidades).
- Usá git: un commit por bloque de trabajo con mensaje claro.
- Si algo de esta especificación es técnicamente inviable o muy costoso, decilo y proponé la mejor alternativa, en vez de entregar una versión falsa o a medias.
