# KB ONE Maligno — Bitácora de progreso

---

## Fase 1 — Esqueleto del proyecto (2026-11-11)

### Qué se hizo

| Área | Detalle |
|---|---|
| Build | CMake + **JUCE 8.0.15** por FetchContent (tag fijado). VST3 + Standalone. C++20. Runtime MSVC estático (el .vst3 no depende del redistribuible de Visual C++). |
| Opciones CMake | `KBONE_COPY_AFTER_BUILD` (OFF por defecto): copia el `.vst3` a `C:\Program Files\Common Files\VST3\`. `KBONE_BUILD_TESTS` (ON por defecto). |
| Parámetros | `source/params/Parameters.h/.cpp`: IDs estables en snake_case con `ParameterID{ id, 1 }`, agrupados (Master, Engine 1, Filter 1, Env VCA), nombres legibles para el host ("Filter 1 Cutoff") y unidades (Hz, ms, dB, %, st). `ParameterCache` resuelve los punteros una sola vez. |
| Voces | `VoiceManager`: 16 voces disponibles, **8 activas por defecto** (parámetro `poly_voices`, 1–16). Robo de voz: primero la voz en release más silenciosa, si no, la más antigua; fade de 3 ms (sin clicks). |
| Sonido | Por voz: **saw PolyBLEP** → **ladder 4 polos ZDF/TPT** (con tanh suave) → **Env VCA ADSR**. Controles interpolados cada 32 muestras (sin escalones). |
| MIDI | Precisión de muestra. Pitch bend (rango `pitch_bend_range` ±1–24 st, por defecto ±2), **mod wheel** (provisoria: abre el cutoff hasta +3 octavas), **sustain pedal** CC64, All Notes Off CC123, All Sound Off CC120, Reset Controllers CC121. |
| Master | **Global Volume** (−inf a +6 dB, suavizado), filtro DC (5 Hz) y limitador suave de seguridad (transparente por debajo de −0.9 dBFS). Protección NaN/Inf por voz. |
| Panic | Botón PANIC en la UI (sin locks: bandera atómica que lee el hilo de audio) + CC120. |
| Editor | Provisorio, lienzo **1376 × 768** escalado por transformación; **redimensionable 75 %–200 %** con proporción fija. La escala se guarda con el proyecto. Fondo amarillo, 4 perillas funcionales (**Global Volume, Cutoff, Resonance, Release**), doble clic = valor por defecto, rueda = paso fino, tooltip con el nombre del parámetro, valor con unidad debajo. Captura: `docs/capturas/fase1_editor.png`. |
| Estado | `getStateInformation` / `setStateInformation` con `stateVersion = 1` (preparado para migraciones) + escala del editor. |
| Tests | `tests/` con ejecutable propio (`juce::UnitTest`) registrado en **ctest** en 5 grupos: Oscillator, Filter, Envelope, Processor, State (93 comprobaciones). |

### Parámetros creados (IDs definitivos, no renombrar nunca)

| ID | Nombre en el host | Rango |
|---|---|---|
| `master_volume` | Global Volume | −60 dB (= −inf) … +6 dB, def. −6 dB |
| `pitch_bend_range` | Pitch Bend Range | 1 … 24 st, def. 2 |
| `poly_voices` | Voices | 1 … 16, def. 8 |
| `eng1_volume` | Engine 1 Volume | 0 … 100 %, def. 80 % |
| `flt1_cutoff` | Filter 1 Cutoff | 20 Hz … 20 kHz (log), def. 2.5 kHz |
| `flt1_reso` | Filter 1 Resonance | 0 … 100 %, def. 25 % |
| `env_vca_attack` | Env VCA Attack | 0.5 ms … 20 s (log), def. 5 ms |
| `env_vca_decay` | Env VCA Decay | 0.5 ms … 20 s (log), def. 400 ms |
| `env_vca_sustain` | Env VCA Sustain | 0 … 100 %, def. 70 % |
| `env_vca_release` | Env VCA Release | 0.5 ms … 20 s (log), def. 300 ms |

### Resultados de verificación (build Release en Linux, ver "Problemas conocidos")

- **ctest:** 5/5 grupos OK (93 comprobaciones).
- **pluginval v1.0.4, nivel 10:** `SUCCESS`, 0 errores. Pasó todas las pruebas: apertura en frío/caliente, editor (también abierto mientras procesa), audio en varias frecuencias de muestreo y tamaños de bloque, estado, automatización, seguridad de hilos y fuzz de parámetros.
- **Aliasing del saw PolyBLEP** (48 kHz, FFT con ventana Blackman-Harris):

  | Nota | Aliasing total | Aliasing < 8 kHz | Saw ingenuo (total) |
  |---|---|---|---|
  | A4 440 Hz | −35.5 dB | −63.7 dB | −19.6 dB |
  | 1244 Hz | −31.1 dB | −60.9 dB | −15.1 dB |
  | E7 2637 Hz | −28.5 dB | −58.1 dB | −11.9 dB |
  | C8 4186 Hz | −23.8 dB | −53.8 dB | −9.6 dB |

  El alias que queda se concentra cerca de Nyquist (poco audible). Con oversampling 2x bajaría a ≈ −48 dB total en C8: queda propuesto para la fase del motor Analog (ajuste de Calidad).

---

## Cómo compilar en Windows (comandos exactos)

### Requisitos (una sola vez)

1. **Visual Studio 2022** (Community sirve) con la carga de trabajo **"Desarrollo para el escritorio con C++"**. Ya trae CMake.
2. **Git para Windows**.
3. Conexión a internet en el primer build: CMake descarga JUCE 8.0.15 solo.

### Compilar

Abrí **"Developer PowerShell for VS 2022"** (menú Inicio) y ejecutá, un comando por vez:

```bash
git clone https://github.com/castrolabia-code/kb-one-maligno.git
```

```bash
cd kb-one-maligno
```

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
```

```bash
cmake --build build --config Release
```

Resultado:

- Plugin: `build\KBOne_artefacts\Release\VST3\KB ONE Maligno.vst3`
- Standalone: `build\KBOne_artefacts\Release\Standalone\KB ONE Maligno.exe`

### Correr los tests

```bash
ctest --test-dir build -C Release --output-on-failure
```

### Instalar el .vst3 (elegí una opción)

**Opción A — copia automática en cada build.** Abrí "Developer PowerShell for VS 2022" **como administrador** (clic derecho → Ejecutar como administrador), porque `C:\Program Files` está protegido:

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DKBONE_COPY_AFTER_BUILD=ON
```

```bash
cmake --build build --config Release
```

**Opción B — copia manual** (PowerShell como administrador):

```bash
Copy-Item -Recurse -Force "build\KBOne_artefacts\Release\VST3\KB ONE Maligno.vst3" "C:\Program Files\Common Files\VST3\"
```

### Validar con pluginval en Windows

1. Bajá `pluginval_Windows.zip` de https://github.com/Tracktion/pluginval/releases (v1.0.4 o posterior) y descomprimilo, por ejemplo en `C:\tools\pluginval\`.
2. Ejecutá:

```bash
C:\tools\pluginval\pluginval.exe --strictness-level 10 --validate "C:\Program Files\Common Files\VST3\KB ONE Maligno.vst3"
```

Tiene que terminar con `SUCCESS`.

---

## Cómo cargarlo en Reason (12.5+ o 14)

1. **Cerrá Reason** y copiá el `.vst3` a `C:\Program Files\Common Files\VST3\` (ver arriba).
2. Abrí Reason. Al arrancar escanea los plugins nuevos de esa carpeta.
3. En el **Browser** (panel izquierdo) entrá a **Instruments** (o **Plugins**) → **Castro Lab** → **KB ONE Maligno**. Arrastralo al rack o hacé doble clic.
4. Seleccioná la pista del instrumento en el secuenciador y tocá con el teclado MIDI.
5. Para abrir la interfaz, hacé clic en el botón de abrir del plugin en el rack.

**Si no aparece:**
- Verificá que la carpeta `KB ONE Maligno.vst3` quedó dentro de `C:\Program Files\Common Files\VST3\`.
- En Reason, en **Preferencias**, buscá la sección de **plugins** (Manage Plugins). Si KB ONE figura como desactivado o fallido, activalo y volvé a escanear.
- Reason 12 necesita la versión **12.5 o posterior** para cargar VST3.

**Qué probar en esta fase:**
- Acordes de hasta 8 notas. La 9ª roba una voz sin click.
- CUTOFF y RESONANCE mientras suena: sin ruido de escalones.
- Mod wheel: abre el filtro.
- Pitch bend: ±2 semitonos.
- Pedal de sustain.
- Botón PANIC: corta todo al instante.
- Automatizar cualquier parámetro desde el secuenciador de Reason.
- Agrandar/achicar la ventana desde la esquina: guardá el proyecto, reabrilo y verificá que recuerda el tamaño.

---

## Problemas conocidos / pendientes de la fase 1

- **No compilado todavía en Windows.** El build y las validaciones de esta fase se hicieron en Linux (el entorno de trabajo es Linux). El código es C++20/JUCE estándar y no usa nada específico de un sistema operativo, pero **el primer build con Visual Studio 2022 y la prueba en Reason los tiene que hacer el usuario**. Si aparece un error o un warning de MSVC, se corrige en la próxima sesión.
- **Validador oficial de Steinberg (`validator`)**: pluginval lo saltea si no se le indica su ruta. No se pudo compilar en esta sesión. Pendiente: correr pluginval con `--vst3validator <ruta>` en Windows.
- **Imágenes de referencia**: `docs/referencias/REFERENCIAS_1.jpg` y `REFERENCIAS_2.jpg` **todavía no están en el repositorio**. Hay que subirlas antes de la fase de interfaz.
- Editor provisorio: faltan Ctrl+arrastre (ajuste fino) y menú de clic derecho (CLAUDE.md §9.3). Llegan con el componente `Knob` definitivo en la fase de UI.
- La mod wheel → cutoff es una conexión fija provisoria. En la fase de modulación pasa a ser una fuente de la matriz.
- La medición de CPU (CLAUDE.md §11.7) se hace cuando estén los dos motores, los filtros y los FX.

## Decisiones tomadas

- **JUCE 8.0.15** (última 8.0.x estable al 2026-11-11).
- **Runtime MSVC estático** (`/MT`): evita que Reason falle al cargar si falta el redistribuible.
- El código del plugin está en una librería `INTERFACE` de CMake (`KBOneCore`) que comparten el plugin y los tests: los tests prueban exactamente el mismo procesador.
- La UI se dibuja sobre un lienzo fijo de 1376 × 768 y se escala con una transformación afín: el layout nunca depende del tamaño real de la ventana y queda nítido en 4K.
- Envolvente: attack lineal exacto; decay/release exponenciales donde "tiempo" = tiempo hasta el 99 % del recorrido (−40 dB).

## Siguiente paso sugerido

**Fase 2 — Motor Analog completo:** 3 osciladores con morph Saw/Pulse/Tri/Sine, PWM, sync, FM lineal, ring mod, ruido, Unison (voces, detune, stereo, blend), Engine 2 y Utility (sub + noise), con la arquitectura de motores compartida (`AnalogEngine`), más el ajuste de Calidad con oversampling.
