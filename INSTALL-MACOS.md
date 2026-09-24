# Building ilanaSynth on macOS — instructions for a friend

There is no pre-built macOS binary, so you build it yourself. It takes about
ten minutes and only needs two commands once the tools are installed.

---

## English

### 1. Install the tools (once)

Open **Terminal** (Applications → Utilities → Terminal) and run:

```bash
xcode-select --install
```

That installs Apple's compilers (accept the dialog that pops up).

Now install CMake. The easy way is Homebrew:

```bash
brew install cmake
```

If you don't have Homebrew, install it first from https://brew.sh or download
the CMake `.pkg` installer from https://cmake.org/download/.

You will also need an internet connection the first time you build: CMake
downloads JUCE automatically.

### 2. Build

Unzip `ilanaSynth-0.9-source.zip` somewhere (for example your Desktop), then:

```bash
cd ~/Desktop/ilanaSynth-0.9-source
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j8
```

The first build takes a few minutes (JUCE is downloaded and compiled).

**Apple Silicon vs Intel:** the build is native for your Mac. If your DAW
runs under Rosetta (older versions), build an Intel or universal version
instead:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="x86_64"
# or universal:  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
```

### 3. Install the plugin

When the build finishes you will have:

- `build/ilanaSynth_artefacts/Release/VST3/ilanaSynth.vst3`
- `build/ilanaSynth_artefacts/Release/Standalone/ilanaSynth.app` (standalone
  app — you can just double-click this one)

Copy the VST3 into your plugin folder:

```bash
mkdir -p ~/Library/Audio/Plug-Ins/VST3
cp -R build/ilanaSynth_artefacts/Release/VST3/ilanaSynth.vst3 ~/Library/Audio/Plug-Ins/VST3/
```

(Use `/Library/Audio/Plug-Ins/VST3/` with `sudo` instead if you want it for
all users.)

Rescan plugins in your DAW (in Ableton Live: Preferences → Plug-Ins → Rescan).
The plugin is called **ilanaSynth** by **Ilana Audio**.

### 4. First launch — Gatekeeper

The build is not signed, so macOS may block the standalone app the first time.
If so:

- Right-click the app → **Open** → **Open** again, or
- `xattr -dr com.apple.quarantine ~/Desktop/ilanaSynth-0.9-source/build/ilanaSynth_artefacts/Release/Standalone/ilanaSynth.app`

Plugins loaded inside a DAW are normally not quarantined this way.

### 5. Handy to know

- User presets live in `~/Documents/ilanaSynth Presets`.
- The preset browser is opened by clicking the preset name in the header;
  right-click a preset row for favourites / delete.
- Hover any control for a tooltip; right-click knobs for the modulation menu;
  drag the coloured source chips onto knobs to assign modulation.
- Number keys 1–9 switch tabs; the `?` button reopens the welcome tour.

### Troubleshooting

| Problem | Fix |
|---|---|
| `cmake: command not found` | `brew install cmake` (or install the CMake .pkg) |
| `xcode-select: error` | Run `xcode-select --install` and accept the dialog |
| CMake fails downloading JUCE | Check your internet connection / proxy, then delete the `build` folder and retry |
| Plugin doesn't appear in the DAW | Make sure it's in `~/Library/Audio/Plug-Ins/VST3`, then rescan; restart the DAW |
| `mach-o, but wrong architecture` | Your DAW runs under Rosetta — rebuild with `-DCMAKE_OSX_ARCHITECTURES="x86_64"` |
| No sound in the standalone | Choose an output device in Options → Audio/MIDI Settings |

---

## Español

No hay un binario de macOS precompilado, así que lo compilás vos. Tarda unos
diez minutos y una vez instaladas las herramientas son solo dos comandos.

### 1. Instalar las herramientas (una sola vez)

Abrí **Terminal** (Aplicaciones → Utilidades → Terminal) y ejecutá:

```bash
xcode-select --install
```

Eso instala los compiladores de Apple (aceptá el diálogo que aparece).

Ahora instalá CMake. Lo más fácil es con Homebrew:

```bash
brew install cmake
```

Si no tenés Homebrew, instalalo desde https://brew.sh o bajá el instalador
`.pkg` de CMake desde https://cmake.org/download/.

También vas a necesitar internet la primera vez: CMake descarga JUCE solo.

### 2. Compilar

Descomprimí `ilanaSynth-0.9-source.zip` en algún lugar (por ejemplo el
Escritorio) y después:

```bash
cd ~/Desktop/ilanaSynth-0.9-source
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j8
```

La primera compilación tarda unos minutos (se descarga y compila JUCE).

**Apple Silicon vs Intel:** la compilación es nativa para tu Mac. Si tu DAW
corre bajo Rosetta (versiones viejas), compilá una versión Intel o universal:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="x86_64"
# o universal:  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
```

### 3. Instalar el plugin

Cuando termina la compilación vas a tener:

- `build/ilanaSynth_artefacts/Release/VST3/ilanaSynth.vst3`
- `build/ilanaSynth_artefacts/Release/Standalone/ilanaSynth.app` (app
  standalone — esta la podés abrir con doble clic)

Copiá el VST3 a tu carpeta de plugins:

```bash
mkdir -p ~/Library/Audio/Plug-Ins/VST3
cp -R build/ilanaSynth_artefacts/Release/VST3/ilanaSynth.vst3 ~/Library/Audio/Plug-Ins/VST3/
```

(Usá `/Library/Audio/Plug-Ins/VST3/` con `sudo` si lo querés para todos los
usuarios.)

Reescaneá los plugins en tu DAW (en Ableton Live: Preferences → Plug-Ins →
Rescan). El plugin se llama **ilanaSynth** de **Ilana Audio**.

### 4. Primera apertura — Gatekeeper

La compilación no está firmada, así que macOS puede bloquear la app
standalone la primera vez. Si pasa:

- Clic derecho en la app → **Abrir** → **Abrir** otra vez, o
- `xattr -dr com.apple.quarantine ~/Desktop/ilanaSynth-0.9-source/build/ilanaSynth_artefacts/Release/Standalone/ilanaSynth.app`

Los plugins cargados dentro de un DAW normalmente no quedan en cuarentena.

### 5. Cosas útiles

- Los presets de usuario se guardan en `~/Documents/ilanaSynth Presets`.
- El explorador de presets se abre haciendo clic en el nombre del preset en la
  barra superior; clic derecho en una fila para favoritos / borrar.
- Pasá el mouse por cualquier control para ver la descripción; clic derecho en
  las perillas para el menú de modulación; arrastrá los chips de fuentes sobre
  las perillas para asignar modulación.
- Las teclas 1–9 cambian de pestaña; el botón `?` reabre el tour de
  bienvenida.

### Problemas comunes

| Problema | Solución |
|---|---|
| `cmake: command not found` | `brew install cmake` (o instalá el .pkg de CMake) |
| Error de `xcode-select` | Ejecutá `xcode-select --install` y aceptá el diálogo |
| CMake falla bajando JUCE | Revisá la conexión / proxy, borrá la carpeta `build` y reintentá |
| El plugin no aparece en el DAW | Verificá que esté en `~/Library/Audio/Plug-Ins/VST3`, reescaneá y reiniciá el DAW |
| `mach-o, but wrong architecture` | Tu DAW corre bajo Rosetta — recompilá con `-DCMAKE_OSX_ARCHITECTURES="x86_64"` |
| Sin sonido en el standalone | Elegí el dispositivo de salida en Options → Audio/MIDI Settings |

---

*IlanaSynth v0.9 — Ilana Audio.  IlanaSynth, para un sonido más buto.*
