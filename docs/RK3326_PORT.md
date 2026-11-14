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

## Estado de desarrollo tras `v0.4.1-rk3326` (WIP local: Batch 1 y Batch 2 verificados; Batch 3 pendiente)

> **Nota sobre ramas y árbol de trabajo:** Los cambios detallados a continuación corresponden a los lotes de optimización **Batch 1 y Batch 2** actualmente compilados y verificados en hardware real RK3326, pendientes de completar **Batch 3**, retirar instrumentación temporal de perfilado y realizar las pruebas finales de cierre antes de publicar un nuevo commit de código y Release.

### 1. Lo realizado hasta ahora (WIP local Batch 1 + Batch 2)

#### A. Framerate mixto con lógica bloqueada a 60.00 Hz (`paper-cut`)
- **Activación por defecto (`port/settings/features/toggles.cpp`, `port/src/aurora/battery_aurora.c`, `port/src/aurora/vi_aurora.c`)**:
  - `battery_saver` pasa a valor por defecto `2` (`"Always"`), con override por variables `TTYD_MIXED_FPS` / `TTYD_NO_MIXED_FPS`.
  - `portBatteryFramerate()` mantiene `"60"` Hz de simulación lógica (`marioStMain()`) en todo momento y `portBatteryResolution()` conserva resolución nativa (`640×480` sin bajar a 0.5x).
  - En mundo/exploración (`inField`): `portBatterySkipDraw()` limita el renderizado a un máximo de `30 FPS` (`(sSinceDrawn < 2) || portViBehindSchedule()`), manteniendo la velocidad real de caminado/animaciones a `60 Hz`.
  - En combate (`inBattle`): objetivo de hasta `60 FPS` con `portViBehindScheduleBattle()` (`OSGetTime() > sNextRetrace`, margen completo de 1 frame de deriva para evitar bloqueos artificiales a mitad de tasa cuando hay ligero jitter).
  - Protección de capturas offscreen (`src/driver/dispdrv.c`, `src/driver/imgdrv.c`): `portDispMustDraw()` y `portImgHasPendingCapture()` impiden saltar el dibujado cuando existen texturas/capturas *one-shot* pendientes en `CAMERA_OFFSCREEN`.

#### B. Lote 1 (`paper-cut` + `extern/aurora`): Reducción de envíos en escena/animación, caché de pipelines y sincronización GL
- **Animación (`src/driver/animdrv.c`, `port/src/aurora/gx_compat_aurora.c`)**:
  - Deduplicación de punteros de textura entre sub-draws en `renderProc` (`prevTexPtr` / `prevTexValid`) y envío de arreglos con tamaño explícito (`ANIM_SET_ARRAY_SIZED` -> `portGXSetArrayExplicit` / `portGXSetArraySized`, bajando la subida de arreglos en combate de `102.4 KB[89]` a `32.9 KB[89]`).
- **Mapas e iluminación (`src/driver/mapdrv.c`, `include/driver/mapdrv.h`)**:
  - Caché de luces en vista `sMapLightObjCache` (64 entradas, `mapPrepareLightObj`), caché de matrices de textura de materiales `sMapMatTexCache` (512 entradas) y `mapSetMaterialLightNoFog` para evitar llamadas redundantes a `GXSetFog`.
- **Emisión rápida de vértices de animación (`include/dolphin/gx/GXVert.h`, `lib/dolphin/gx/GXVert.cpp`)**:
  - Rutinas especializadas `GXEmitAnimTriFan16_1Tex` y `GXEmitAnimTriFan16_NTex`.
- **Worker y caché de pipelines (`lib/webgpu/sdl2shim_present.cpp`, `lib/gfx/pipeline_cache.cpp`)**:
  - Espera GPU en servidor mediante `eglWaitSyncKHR` (con fallback `eglClientWaitSyncKHR`) y creación de `eglCreateSyncKHR` antes de `glFlush`.
  - `g_pipelineCacheGeneration` atómico y caché thread-local `FastPipelineEntry` de 512 ranuras en `get_pipeline()`.

#### C. Lote 2 (`extern/aurora`): Caché shadow GX en C, deduplicación de estado en RenderWorker y caché de bind-groups
- **Caché shadow de registros GX en lado C (`lib/dolphin/gx/__gx.h` y `GXManage.cpp`, `GXDispList.cpp`, `GXCull.cpp`, `GXLighting.cpp`, `GXPixel.cpp`, `GXTev.cpp`, `GXGeometry.cpp`, `GXTransform.cpp`)**:
  - Estructura `__GXShadowCache __gxShadow` protegida por `!aurora::gx::fifo::in_display_list()` y `epoch`. Redujo escrituras XF procesadas en combate de `5,570–5,587` (`3.1 ms`) a `3,783` (`2.3 ms`).
- **Fast paths OpenGL ES 3.0 en RenderWorker (`lib/gl/handles.hpp`, `lib/gl/state.hpp`, `lib/gl/state.cpp`, `lib/gl/pass.hpp`, `lib/gl/pass.cpp`, `lib/gx/pipeline.cpp`)**:
  - `BakedState::operator==`, máscaras activas de texturas (`BindingSet::activeTexMask` / `useActiveTexMask` y `Shadow::nonEmptyTexMask` en `bind_texture_set`), `PassEncoder::m_id` monotónico y caché de `SetPipeline` / `SetUniformBuffer0` / `SetBindGroup`.
- **Caché de texturas/bind-groups en Aurora GX (`lib/dolphin/gx/GXFrameBuffer.cpp`, `lib/gx/gx.hpp`, `lib/gx/gx.cpp`, `lib/gx/shader_info.cpp`, `lib/gx/command_processor.cpp`)**:
  - Invalidación selectiva `note_copy_textures_changed()`, `s_fastBindCache`, `s_resolvedDrawBindCache` (512 ranuras sin `TextureHandle`) y `GXState::texMetrics`. El coste de `bg` (`[calls:366]`) cayó de `0.5 ms` a `0.0 ms`, y el `[DRAIN-TIMING-30f]` total en combate bajó de `18.2–19.0 ms` a `16.4 ms`.

---

### 2. Pruebas realizadas en RK3326 (después del Lote 1 y Lote 2)

Binario compilado para pruebas Batch 2: `build-aarch64/ttyd-eu.stripped` (`46M`, `SHA256: d0eb842c5208927bd60a757bb745d8755f22afbcc97b90f7b0cbb333a42ac434`), desplegado en `/roms/ports/papercut/ttyd-eu`.

| Escena / Etapa | Intervalo medio (`last350` / `last300`) | FPS sostenido (`last350` / `last300`) | FPS global (`all_drawn` / `after200`) | `mean_work` (tail) | Notas / Detalle |
|---|---:|---:|---:|---:|---|
| **Combate `hei_00`** — Mixed FPS base | `27.22 ms` (`last350`) | **`36.74 FPS`** | `41.39` / `41.06 FPS` | `9.24 ms` | `p95=32.97ms`, `max=57.91ms` |
| **Combate `hei_00`** — Batch 1 | `27.98 ms` (`last350`) | **`35.74 FPS`** | `40.37` / `39.75 FPS` | `7.76 ms` | `[DRAIN-TIMING-30f]`: `18.2–19.0 ms`, `xf=3.1ms[n:5570..5587]` |
| **Combate `hei_00`** — Batch 2 | **`25.22 ms`** (`last350`) | **`39.64 FPS`** | **`43.95` / `42.51 FPS`** | **`7.26 ms`** | `p95=32.34ms`, `max=37.95ms`, cero spikes GPU en espera |
| **Rogueport `gor_00`** — Mixed FPS base | `46.06 ms` (`last300`) | **`21.71 FPS`** | `26.42` / `24.43 FPS` | `11.44 ms` | `p95=58.59ms`, `max=81.01ms` |
| **Rogueport `gor_00`** — Batch 1 | `45.84 ms` (`last300`) | **`21.81 FPS`** | `24.36` / `23.19 FPS` | `9.90 ms` | `p95=59.43ms`, `max=61.72ms` |
| **Rogueport `gor_00`** — Batch 2 | **`45.38 ms`** (`last300`) | **`22.04 FPS`** | `26.20` / `25.13 FPS` | **`9.61 ms`** | `p95=59.03ms`, `max=64.67ms` |

Desglose de perfilado detallado en **Batch 2 (`hei_00`, `TTYD_TIMING_LOG=1` y `TTYD_TIMING_DETAIL=1`)**:
- **`[WORK-PROF]`**: `draw=20 (disp=5.56-5.91ms main=1.38-1.83ms done=16.03-17.71ms)` (`skip=10`).
- **`[AURORA-SUB]`**: `drain: 8.3-11.8ms (steady ~8.9-9.8ms) | pres: 8.6-10.8ms`.
- **`[PRES-SPIKE]`**: `wait_worker 0.0ms`, `ren 13.8-15.8 qwait 12.4-14.3 cb 1.4-1.6 ops:1 d:469`, `blit_swap 10.4-11.4ms`.
- **`[DRAIN-TIMING-30f]`**: `avg: total 16.4ms | draw_tot 9.1ms (draws:467 hash:1.0 exp:0.6 g_miss:0.7 scan:0.8 pipe_cfg:0.8 pipe_ref:0.0 res:0.8 bg:0.0[calls:366] uni:0.9 push:0.5) | xf 2.3ms[n:3783] | indx 0.0ms | bp 1.1ms | cp 0.1ms | hbytes(fifo:164.8KB arr:32.9KB[89])`.
- **`[REN-DETAIL/470d]`**: `pipe=0.86-0.98ms uni=0.58-0.62ms tex=0.94-1.06ms buf=0.21-0.22ms draw=12.80-14.52ms tot=15.40-17.38ms`.
- **`[PASS-DETAIL/470d]`**: `arr=26(1.3ms) b0=3(0.11ms) bN=441(10.25-15.76ms) pr=288(6.6-11.7ms) fall=0 eab=0 lookup=0.48ms gl=11.6-17.7ms`.
- **Verificación visual Batch 2**: capturas `opt2_bat_2.png`, `opt2_bat_3.png` (`hei_00`) y `opt2_gor_2.png`, `opt2_gor_3.png` (`gor_00`) inspeccionadas y correctas píxel a píxel.
- **Prueba descartada**: `TTYD_TEST_NOBV=1` (desactivar `glDrawElementsBaseVertex`) provocó una regresión de `+1.45 ms`, por lo que `BaseVertex` se mantiene activo.

---

### 3. Diagnóstico raíz actual (por qué `hei_00` está en `39.64 FPS` y no en `48–60 FPS`)

El análisis de código previo al Lote 3 (`GXDispList.cpp`, `mapdrv.c`, `animdrv.c`, `battle_disp.c`, `dispdrv.c`, `command_processor.cpp`, `gl/pass.cpp`) identificó los dos cuellos principales:
1. **`GXCallDisplayList()` invalida `__gxShadow` en exceso (`GXDispList.cpp:63-65`) + faltan caches shadow en GX**:
   - Cada `GXCallDisplayList()` ejecuta `__gx_shadow_invalidate()`, borrando todo el estado shadow **456+ veces por frame** solo en las `228` llamadas de `mapDispMapObj` (`obj->displayList[i]` en `mapdrv.c:2322` y `callPackedLists(mesh)` de geometría pura en `mapdrv.c:2323`).
   - Adicionalmente, `GXSetArray` (`GXGeometry.cpp:218`), `GXLoadTexObj` (`GXTexture.cpp:232`, que emite 37 bytes de metadatos vía `emit_loaded_texobj_metadata`), `GXSetCurrentMtx` (`GXTransform.cpp:97`), `GXLoadPosMtxImm`, `GXLoadNrmMtxImm` y `GXLoadTexMtxImm` (`GXTransform.cpp`) aún carecen de `__gxShadow` en el lado C, manteniendo `3,783` escrituras XF/frame (`2.3 ms`) y reconstrucciones evitables en el FIFO.
2. **Coste por llamada de dibujado en RenderWorker (`470` GL draws/frame = `12.80–14.52 ms` CPU en `libmali` + `10.4–11.4 ms` `blit_swap`)**:
   - `441` llamadas `glDrawElementsBaseVertex` (`10.25–15.76 ms`, `~23.6 µs`/llamada; `288` con primitive restart consumiendo `6.6–11.7 ms`) + `26` `glDrawArrays` (`~1.3 ms`, `~50 µs`/llamada) + `3` `b0` (`0.11 ms`).
   - Superar la barrera hacia **48–60 FPS** en batalla requiere reducir tanto el coste de CPU principal (`disp 5.7 + main 1.5 + drain 9.2 ms`) como el número de draws GL / trabajo de GPU Mali.

---

### 4. Pendiente por realizar y comprobar (Roadmap Lote 3 y cierre)

- [ ] **Optimización Lote 3 (objetivo 48–60 FPS estables en combate)**:
  - Preservar/restaurar selectivamente `__gxShadow` en `GXCallDisplayList()` (evitando invalidar registros no tocados en DLs de geometría pura como `callPackedLists(mesh)`).
  - Extender `__gxShadow` a `GXSetArray`, `GXLoadTexObj` (reduciendo llamadas redundantes a `emit_loaded_texobj_metadata`), `GXSetCurrentMtx` y `GXLoadPosMtxImm`/`NrmMtxImm`/`TexMtxImm`.
  - Reducir el conteo de draw calls GL en `470` draws/frame (agrupación/batching de geometría estática de mapa y UI/partes compatibles, y eliminación de sobrecoste en primitive restart / `glDrawArrays`).
- [ ] **Verificación en RK3326 de rendimiento y fidelidad**:
  - Benchmark en combate `hei_00` buscando **48–60 FPS** sostenidos (incluyendo pruebas activas con movimiento de cámara y animaciones/ataques para confirmar ausencia de caídas).
  - Benchmark de regresión en exploración `gor_00` (`>= 21.8–22.0 FPS` manteniendo lógica `60.00 Hz`) y verificación visual de `640×480` nativa (combate, campo, prólogo/barco y menú).
  - Validación visual comparativa con Dolphin si aplica en escenas con varios enemigos.
- [ ] **Limpieza final y cierre del fork**:
  - Retirar la instrumentación diagnóstica temporal del árbol de trabajo (`TTYD_TIMING_LOG`, `TTYD_TIMING_DETAIL`, `TTYD_EXT_DIAG`, `TTYD_GXARRAY_TRACE`, `TTYD_TEST_SUBDATA`, `TTYD_TEST_NOBV`, `TTYD_FORCE_SWAP0`, `[WORK-PROF]`, `[DISP4-TOP]`, `[CAM]`, `[PRESENT-CAPS]`).
  - Compilar binario `Release` limpio (`ttyd-eu`) y ejecutar prueba smoke E2E en hardware real sin logs de perfilado.
  - Restaurar las copias de seguridad del usuario en la consola RK3326 (`/tmp/backup_settings.txt` y `/tmp/backup_crump_resume.*`).
  - Confirmar commits limpios en `extern/aurora` (`rk3326-gles`) y `paper-cut` (`rk3326-port`), verificar ausencia total de archivos con derechos de autor, hacer push a GitHub y publicar la nueva versión en **Releases**.

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
