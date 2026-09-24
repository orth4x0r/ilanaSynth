# IlanaSynth — v0.9

**IlanaSynth, para un sonido más buto.**

Un sintetizador wavetable agresivo en formato VST3, hecho con JUCE.

---

## Qué es

IlanaSynth es una máquina completa de diseño de sonido: tres osciladores con
modos wavetable, cuerda física y sampler, dos filtros enroutables, cinco
envolventes con tensión, cuatro LFOs más un secuenciador de pasos y un MSEG,
una matriz de modulación de 8 ranuras, un rack de 10 efectos con 28 módulos,
un arpegiador de 8 modos y un resonador afinado — todo dentro de una interfaz
de hardware con 80 presets de fábrica.

---

## Características

### Osciladores (3)
- Cada oscilador tiene tres modos:
  - **Wavetable** — 16 tablas de fábrica (Basic, HardSync, Wavefold, FM Metal,
    Formant, Comb, PWM, DriveSaw, Sine, Triangle, Analog, Vowel, Glass,
    Fractal, Riser, Digital) más 4 ranuras de usuario. Cargá tu propio `.wav`
    con el botón LOAD o arrastrando el archivo sobre la pantalla de onda.
  - **String** — modelo físico Karplus–Strong con excite, decay, damp y
    sustain.
  - **Sample** — cargá cualquier `.wav` (hasta 120 s). Reproducción afinada o
    sin afinar, loop / one-shot, reversa, puntos de inicio y fin y fades.
    Cinco samples de fábrica incluidos (Metal Hit, Vocal Ah, Sub Tone,
    Vinyl Loop, Noise Rise).
- Unísono de hasta 8 voces por oscilador con detune y dispersión estéreo.
- Nivel, pan, semitonos y fine tuning por oscilador.
- Modos de acorde (Octava, Quinta, Power, Mayor, Menor, Sus4).
- Morfing de frames, modulable como cualquier control.
- El OSC 3 es el sub/tercer oscilador, con selector de octava extra y una
  capa de ruido.

### Filtros (2)
- Pasa bajos, pasa banda, pasa altos y notch — 12 dB o 24 dB por filtro.
- Cutoff, resonancia, drive, cantidad de envolvente, key tracking y FM de
  audio.
- Ruteo **serie** (F1 dentro de F2) o **paralelo** (los dos sumados), elegido
  con el switch de ruteo y sus diagramas de flujo.

### Envolventes y LFOs
- 5 envolventes con tensión: AMP, FILTER 1, FILTER 2, MOD y ENV 4.
  - Arrastrá los manejadores del gráfico para dar forma a A / D / S / R, y
    arrastrá la curva (o sus puntos medios) para doblar la **tensión** — por
    envolvente.
  - Sensibilidad a la velocidad en AMP y FILTER 1.
- 4 LFOs con formas Sine, Triangle, Saw up, Saw down, Square, S&H, Draw y
  Steps. Velocidad libre o divisiones sincronizadas al host, con retrigger
  por LFO.
- **Editores de Step LFO** (dos secuenciadores de 16 pasos) y un **MSEG de 4
  etapas** viven en la pestaña SEQ, que aparece sola cuando elegís una forma
  Steps o asignás el MSEG en la matriz.
- Los LFOs y las envolventes comparten una pestaña: arriba la fila de LFOs,
  abajo la de envolventes.

### Matriz de modulación
- 8 ranuras, 22 fuentes y 36 destinos, profundidad bipolar y medidores en vivo
  de cada fuente.
- Fuentes: LFO 1–4, envolventes MOD / FILTER 1 / FILTER 2 / AMP / ENV 4, MSEG,
  velocidad, key track, random, mod wheel, aftertouch, expresión, 4 macros y
  un sample & hold sincronizado.
- **Arrastrá un chip de fuente sobre cualquier perilla** para asignarla, o
  **clic derecho en cualquier perilla** para el menú rápido de modulación,
  reset y copiar/pegar.

### Rack de efectos (10 ranuras, 28 módulos)
Drive (tube / fuzz / clean), Bit Crusher, Amp, Chorus, Phaser, Comb, Flanger,
Dimension, Tremolo, Auto-Pan, Delay, Tape Delay, Delay Multi-Tap con grilla de
taps dibujable, Reverb (Room, Hall, Plate, Shimmer, Spring, Gated y **carga de
IR propio**), módulo Feedback, Smear, Freeze, Stutter granular (reversa +
pitch), Gate, Tape Stop, Tilt EQ, Utility, OTT, Limiter, Stereo Width, Pitch
Shifter, Ring Mod, Octaver y filtro Vowel.

- Cada ranura: bypass, blend y lectura de CPU.
- **Solo** de la ranura (solo señal húmeda) haciendo clic en su insignia S.
- El rack arranca vacío — clic derecho en cualquier fila para elegir un módulo.
- **Cadenas A/B** con copia de A a B, para comparar dos cadenas de efectos.

### Voz y global
- 16 voces de polifonía, modo MPE, voice spread y randomización de fase del
  unísono.
- Glide y rango de pitch bend, al lado de las macros en la barra inferior.
- Cross modulation: FM (OSC 2 → OSC 1) con feedback, ring mod, hard sync y
  drift analógico.
- Resonador: cuerpo afinado después de los filtros con amount, decay, offset y
  key tracking.
- Sobremuestreo 2x del motor de voces (pestaña SCOPE).
- Soft clipper con clip gain, volumen master y 4 macros con MIDI learn.

### Arpegiador
- 8 modos: Up, Down, UpDown, Random, DownUp, Converge, Walk y Chord.
- 1–4 octavas, velocidades sincronizadas al host, largo de gate — con
  visualización del patrón que avanza una barra por nota.

### Osciloscopio
- Osciloscopio / analizador de espectro, HOLD y PEAK hold, medidores estéreo
  con clic para aislar un canal y una pestaña SCOPE a pantalla completa.

### Interfaz
- Diseño skeuomórfico inspirado en hardware: cuerpo de cuero, frente de metal,
  perillas torneadas y pantallas de vidrio.
- 4 temas de color (botón SKIN).
- Tooltips al pasar el mouse, modulación con clic derecho, undo/redo con menú
  de historial y un tour de bienvenida (reabrilo con el botón `?`).
- Atajos de teclado: las teclas 1–9 cambian de pestaña.
- 80 presets de fábrica en 8 categorías (Bass, Lead, Pluck, Pad, Drone, FX y
  más) con búsqueda, favoritos y presets de usuario.

---

## Instalación

### Windows
1. Ejecutá `ilanaSynth-0.9-Windows-Setup.exe` (instala el VST3 y la versión
   standalone), o copiá la carpeta `ilanaSynth.vst3` a
   `C:\Program Files\Common Files\VST3\`.
2. Reescaneá la carpeta de plugins en tu DAW.
3. El standalone `ilanaSynth.exe` no necesita DAW.

### macOS
No hay binario de macOS precompilado en esta versión. El proyecto es CMake +
JUCE puro y compila en macOS con las herramientas de línea de comandos de
Xcode:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Copiá el `ilanaSynth.vst3` generado a `/Library/Audio/Plug-Ins/VST3/` (o
`~/Library/Audio/Plug-Ins/VST3/`) y reescaneá en tu DAW. La app standalone
queda en `build/ilanaSynth_artefacts/Release/Standalone/`.

---

## Cómo usarlo

- **Pasá el mouse por cualquier control** para ver su descripción y valor.
- **Clic derecho en una perilla** para asignar una fuente de modulación, 
  limpiar la modulación, volver al valor por defecto o copiar/pegar el valor.
- **Arrastrá un chip de fuente** de la barra inferior sobre una perilla para
  modularla.
- **Arrastrá un `.wav`** sobre la pantalla de onda de un oscilador para
  cargarlo como sample (cambia el oscilador a modo Sample) — o usá LOAD para
  llenar una ranura de wavetable de usuario.
- Los presets de usuario se guardan en `Documentos/ilanaSynth Presets`; el
  botón FOLDER del explorador de presets abre esa carpeta.
- El explorador de presets (clic en el nombre del preset) tiene chips por
  categoría, búsqueda, favoritos (clic izquierdo carga, clic derecho abre más
  opciones).

### Cosas a saber
- La pestaña SEQ está oculta hasta que la necesitás: aparece cuando una forma
  de LFO se pone en Steps o cuando asignás el MSEG como fuente de modulación.
- El rack de FX arranca vacío en INIT — clic derecho en cualquiera de las 10
  filas para agregar un efecto.
- Con las nuevas tablas de fábrica, las ranuras de wavetable de usuario
  quedaron en las posiciones 17–20 de la lista; parches muy viejos que
  apuntaban a una ranura de usuario pueden necesitar reseleccionarla.

### Pantallas de alta densidad en Windows
Si la interfaz se ve borrosa en una pantalla con escalado 125–200%:
- En Live: **Preferences → Look/Feel → activá HiDPI mode**.
- Clic derecho en la barra de título del plugin (o en su entrada del
  navegador de Live) y desactivá **Auto-Scale Plug-In Window**. Live escala
  las ventanas de los plugins y eso las vuelve borrosas; apagado, la interfaz
  se dibuja 1:1.
- Usá el botón **UI** del encabezado para elegir el zoom (100–200%) así la
  ventana queda cómoda y se sigue dibujando a resolución nativa.

---

## Compilar desde el código

Requisitos: CMake 3.22+, un compilador de C++20 y conexión a internet para
JUCE (se descarga solo).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release      # Windows: cmake -B build
cmake --build build --config Release
```

Targets: `ilanaSynth_VST3`, `ilanaSynth_Standalone` e `ilanaTableTest`
(suite de regresión offline).

---

*IlanaSynth v0.9 — Ilana Audio.*
