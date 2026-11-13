# Paper Cut — Fork RK3326 / PortMaster (`jbcgames`)

Adaptación y optimización de [LLCOOLT/paper-cut](https://github.com/LLCOOLT/paper-cut) (basado en la descompilación del port nativo y su backend OpenGL ES 3.0 en [Aurora](https://github.com/jbcgames/aurora)) para consolas portátiles **RK3326** con GPU **Mali-G31 MP2** bajo **KMSDRM / PortMaster** (objetivo verificado: `dArkOS4Clone` / G350 / K36, versión europea `G8MP01`).

> **Aviso legal:** Ni este repositorio ni los archivos de *Release* contienen el juego original, imágenes de disco (`game.iso`), archivos extraídos del disco (`orig/`) ni contenido protegido por derechos de autor. Debes aportar tu propia copia legal europea (`G8MP01`) en `papercut/game.iso`.

---

## Progreso realizado y mejoras implementadas

### 1. Estabilidad gráfica en OpenGL ES 3.0 / Mali-G31 (`extern/aurora` + `paper-cut`)
- **Conversión 3D → 2D → 3D del barco (`gor_00` / `ship_pera`)**: corrección de cálculo de *bounding box* y cacheo de copias de framebuffer (`invalidate_copy_caches` en `GXFrameBuffer.cpp` + `command_processor.cpp`), evitando texturas corruptas al girar el barco antes del prólogo.
- **Texturas de personajes (`gor_00` / `christine_osoware`)**: corrección de decodificación TLUT/paletas en `command_processor.cpp`, `regs.cpp` y `gx.cpp`, eliminando paletas rotas en NPCs y escenas con Goombella/Crump.
- **Barras de estado y HUD**: corregida la geometría/renderizado de indicadores en batalla y mundo.
- **Flujo de inicio directo y sin menús extra**: eliminados los menús agregados sobre la pantalla de título (`title_menu`, `notes`, `whats_new`, pistas), pasando de `Press Start` (`SEQ_TITLE`) directamente a la selección de archivo (`SEQ_LOAD`) y evitando el sobrecoste visual y de CPU de la interfaz superpuesta.

### 2. Optimizaciones de rendimiento para RK3326 (Cortex-A35 + Mali-G31)
1. **Caché persistente de geometría nativa (`GL_EXT_buffer_storage`)**:
   - `g_nativeVertexCacheBuffer` y `g_nativeIndexCacheBuffer` pasan a mapeo persistente coherente con copia directa en hilo principal + sincronización segura en `begin_frame`, eliminando los bloqueos completos de GPU (`glBufferSubData` stalls de `145–248 ms`) durante promociones de geometría.
2. **Memoización de pipelines GX (`s_nativePipeMemo`)**:
   - Reducción del coste de búsqueda/resolución de pipelines en `command_processor.cpp` (`pipe_ref`: `1.5–1.8 ms` → `~0.1 ms`).
3. **`glDrawElementsBaseVertex` en Aurora GL (`gl/gl_core.hpp`, `gl/gl_loader.cpp`, `gl/pass.cpp`)**:
   - Resolución dinámica (`core` / `EXT` / `OES`) soportada por `libmali-bifrost-g31-rxp0-gbm.so`.
   - Aislamiento de `GxVaoLayout::vao` respecto a `fallbackVao` y alineación de vértices nativos al `nativeStride` (`align_up_any`), evitando reconfigurar `glVertexAttribPointer` en cada draw indexado.
4. **Compactación y deduplicación de Uniform Buffers + Caché XF (`gx/gx.hpp`, `gx/shader.cpp`, `gx/shader_info.cpp`)**:
   - Acotación dinámica de `postex_mtx` (`1` u `11` en lugar de `20`), `nrm_mtx` (`1` en draws con una matriz) y `texcoord_scale` solo cuando el shader muestrea UVs.
   - Caché de deduplicación por frame `s_uniformDedup` (1024 entradas con hash XXH3 + `memcmp` vía `uniform_data_at`) reutilizando rangos UBO repetidos.
   - Ampliación de `xfRegCache`/`xfRegValid` hasta `0x60` (escalares `0x3F..0x5F`, viewport `0x1A..0x1F` y proyección `0x20..0x26`, invalidando al escribir BP `0x00`).

### 3. Integración PortMaster, idioma y splash de carga (`port/portmaster/`)
- **Idioma español por defecto**: `language=4` configurado en `papercut/saves/settings.txt` (verificado en título, selector de archivos, diálogos y menú de batalla).
- **Splash de carga adaptado a resolución (`Logo*.jpg` + `sdl2imgshow`)**:
  - Selección automática según resolución/relación de aspecto usando logotipos propios `jbcgames ports` (sin arte protegido por derechos de autor).
  - Inicialización anticipada de `MALI_DIR` (`/tmp/papercut_mali` apuntando a `libmali-bifrost-g31-rxp0-gbm.so`) **antes** de lanzar `sdl2imgshow`, pasando `LD_LIBRARY_PATH="$MALI_DIR..."` para evitar fallos de carga EGL (`libEGL.so.1.1.0`) en sistemas `dArkOS4Clone`.
  - Preservación automática del volumen de EmulationStation (`ES_VOL`) mediante `amixer` y `oom_score_adj=-500`.

---

## Resultados medidos en hardware real (RK3326 @ Mali-G31 MP2)

| Métrica / Escena | Baseline previo | Tras optimizaciones |
|---|---:|---:|
| Worker GL en `gor_00` (`ren` / `work`) | `25.5–26.4 ms/f` | **`12.71–13.86 ms/f`** |
| Subida de Uniforms por frame (`gor_00`) | `476.3 KB/f` | **`131.1–133.7 KB/f`** (~72% menos) |
| CPU GX drain (`[DRAIN-TIMING-30f] gor_00`) | `19.2–20.2 ms` | **`16.0–17.3 ms`** |
| Combate (`hei_00`) | `22.6–25.0 ms` (`drain`) + stalls | **`13.97–18.01 ms`** (`ren`) / **`18.7–19.5 ms`** (`drain`) |
| Caché de binarios GLSL al arranque | — | `156` binarios cargados, `0 source compiles` en `1322 ms` |

---

## Instalación en PortMaster (desde el paquete Release)

1. Descarga `papercut-0.4.1-rk3326-portmaster.zip` desde la sección **Releases** de este repositorio.
2. Descomprime su contenido en la carpeta de puertos de tu consola (`/roms/ports/`), de modo que queden:
   - `/roms/ports/Paper Cut.sh`
   - `/roms/ports/papercut/PaperCut.sh`, `ttyd-eu`, `papercut.gptk`, `libs.aarch64/`, `Logo*.jpg`, `saves/settings.txt`
3. Coloca tu copia legal del disco europeo (**Paper Mario: The Thousand-Year Door PAL / `G8MP01`**) en:
   `/roms/ports/papercut/game.iso`
4. Inicia **Paper Cut** desde EmulationStation / PortMaster. *(El primer arranque compilará la caché de shaders en `papercut/cache/`.)*

---

## Compilación cruzada (AArch64 / RK3326 sin datos del disco)

Requiere sysroot AArch64 (`/opt/rk3326-sysroot`), `clang-21`, `cmake`, `ninja`, `python3` y el submódulo `extern/aurora` (rama `rk3326-gles`):

```bash
git clone --recursive https://github.com/jbcgames/paper-cut.git
cd paper-cut
cmake -S . -B build-aarch64 -G Ninja \
  --toolchain=cmake/toolchains/aarch64-linux-gnu.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DTTYD_VERSION=G8MP01 \
  -DTTYD_BACKEND=aurora \
  -DTTYD_AURORA_GLES=ON \
  -DTTYD_NO_DISC_ASSETS=ON
ninja -C build-aarch64 ttyd-eu
```
