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

## Actualización `v0.4.2-rk3326`: Objetivo global 30 FPS (lógica 60 Hz), corrección de animación de caminado y optimizaciones GX/GL

### 1. Mejoras implementadas en `v0.4.2-rk3326`

#### A. Límite global a 30 FPS con lógica bloqueada a 60.00 Hz (`paper-cut`)
- **Unificación a 30 FPS en todo el juego (`port/settings/features/toggles.cpp`, `port/src/aurora/battery_aurora.c`, `port/src/aurora/vi_aurora.c`)**:
  - `battery_saver` activo por defecto (`2` = `"Always"`), con override opcional por variables `TTYD_MIXED_FPS` / `TTYD_NO_MIXED_FPS`.
  - `portBatteryFramerate()` mantiene `"60"` Hz de simulación lógica (`marioStMain()`) en todo momento y `portBatteryResolution()` conserva la resolución nativa (`640×480` sin reducir a 0.5x).
  - `portBatterySkipDraw()` limita el renderizado de todas las secuencias (campo, combate, menús y escenas) a un objetivo uniforme de `30 FPS` (`(sSinceDrawn < 2) || portViBehindSchedule()`), manteniendo la velocidad real de juego y controles a `60 Hz`.
  - Protección de capturas offscreen (`src/driver/dispdrv.c`, `src/driver/imgdrv.c`): `portDispMustDraw()` y `portImgHasPendingCapture()` impiden saltar el dibujado cuando existen capturas *one-shot* pendientes en `CAMERA_OFFSCREEN`.

#### B. Corrección del bug visual de vibración de pies al caminar (`src/driver/npcdrv.c`)
- **Causa raíz**: en `npcMain()`, el desplazamiento `dv = entry->position - entry->prevPosition` aplicado por `evtmgrMain()` con el `entry->moveDt` del tick previo se dividía entre el nuevo `entry->moveDt` (o con un tope artificial de `0.020f`), provocando oscilaciones de velocidad aparente `spd` cuando bajaban los FPS o durante ticks de recuperación (*catch-up*). Esto hacía alternar repetidamente entre `stayAnim` (pies juntos, frame 0) y `walkAnimation`/`runAnimation` (reiniciando el ciclo en frame 0).
- **Solución**:
  - Cálculo exacto de `spd` usando `spdDt = (prevMoveDt > 0.0f) ? prevMoveDt : entry->moveDt` sin el tope `0.020f` y protegiendo ticks con `prevMoveDt <= 0.0f && dv == 0`.
  - Histéresis por ranura separada para mundo y batalla (`sAnimDebounce[2][256]` y `sAnimDebStart[2][256]`): entrar en movimiento desde reposo cambia de inmediato (`1` tick), mientras que volver a `stayAnim` o alternar entre caminar/correr exige al menos `4` ticks consecutivos **y** `>= 80 ms` de `gp->renderTime`, preservando la fase del paso entre caminar y correr (`entry->moveAnimState <= 1`).

#### C. Optimizaciones GX/GL (Lotes 1 y 2 en `paper-cut` + `extern/aurora`)
- **Animación y mapas (`src/driver/animdrv.c`, `port/src/aurora/gx_compat_aurora.c`, `src/driver/mapdrv.c`, `include/driver/mapdrv.h`, `src/driver/dispdrv.c`)**:
  - Deduplicación de punteros de textura entre sub-draws en `renderProc` (`prevTexPtr` / `prevTexValid`) y envío de arreglos con tamaño explícito (`ANIM_SET_ARRAY_SIZED` -> `portGXSetArrayExplicit` / `portGXSetArraySized`).
  - Caché de luces en vista `sMapLightObjCache` (64 entradas, `mapPrepareLightObj`), caché de matrices de textura de materiales `sMapMatTexCache` (512 entradas), `mapSetMaterialLightNoFog` y agrupación segura de objetos opacos de mapa en `dispdrv.c`.
- **Emisión rápida de vértices y sincronización EGL (`include/dolphin/gx/GXVert.h`, `lib/dolphin/gx/GXVert.cpp`, `lib/webgpu/sdl2shim_present.cpp`, `lib/gfx/pipeline_cache.cpp`)**:
  - Rutinas especializadas `GXEmitAnimTriFan16_1Tex` y `GXEmitAnimTriFan16_NTex`.
  - Espera GPU en servidor mediante `eglWaitSyncKHR` (con fallback `eglClientWaitSyncKHR`), creación de `eglCreateSyncKHR` antes de `glFlush` y caché thread-local `FastPipelineEntry` (512 ranuras) con `g_pipelineCacheGeneration` atómico.
- **Caché shadow GX en C y fast paths en RenderWorker (`lib/dolphin/gx/__gx.h`, `GX*.cpp`, `lib/gl/*`, `lib/gx/*`)**:
  - `__GXShadowCache __gxShadow` protegido por `!aurora::gx::fifo::in_display_list()` y `epoch`.
  - `BakedState::operator==`, máscaras activas de texturas (`BindingSet::activeTexMask` / `useActiveTexMask` y `Shadow::nonEmptyTexMask`), `PassEncoder::m_id` monotónico, `s_fastBindCache` y `s_resolvedDrawBindCache` (512 ranuras).
  - Descartado y eliminado `coalesce_static_dl_draws` tras confirmar que corrompía texturas/geometría en batalla al fusionar display lists con cambios de estado intermedios.

---

### 2. Verificación multimapa y de combate en RK3326 (`v0.4.2-rk3326`)

Binario Release final limpio (sin hooks de perfilado temporal): `build-aarch64/ttyd-eu.stripped` (`46M`, `SHA256: ea1f5d7c4f73a8f2c641ce38fff25c5c77e0f68c2ba1e96f57f00e2511fa0af9`), desplegado y verificado en `/roms/ports/papercut/ttyd-eu`.

| Ubicación / Escena | Mapa (`story`) | Estado / Salida | Intervalo medio (`mean_iv`) | Mediana (`p50_iv`) | FPS medio | `mean_work` | Verificación visual |
|---|---|---|---:|---:|---:|---:|---|
| **Combate (Prólogo / Goombas)** | `hei_00` (batalla) | `EXIT=0`, `0 errs` | `33.34 ms` (`35.68 ms` con captura) | `32.69–33.34 ms` | **`28.03–29.99 FPS`** | `14.49–16.12 ms` | 100% limpia (`bat_vis_1..3.png`, `final_bat.png`) |
| **Puerto de Rogueport** | `gor_00` (`20`) | `EXIT=0`, `0 errs` | `33.84 ms` | `33.33 ms` | **`29.55 FPS`** | `14.02 ms` | 100% limpia (`map_gor_00.png`) |
| **Villa Petal (Capítulo 1)** | `nok_00` (`50`) | `EXIT=0`, `0 errs` | `33.54 ms` | `33.27 ms` | **`29.81 FPS`** | `18.22 ms` | 100% limpia (`map_nok_00.png`) |
| **Villa Sombría (Capítulo 4)** | `usu_00` (`200`) | `EXIT=0`, `0 errs` | `34.27 ms` | `32.44 ms` | **`29.18 FPS`** | `23.07 ms` | 100% limpia (`map_usu_00.png`) |
| **Vestíbulo Glitz Pit (40+ NPCs)** | `tou_02` (`170`) | `EXIT=0`, `0 errs` | `34.97 ms` | `33.22 ms` | **`28.59 FPS`** | `19.26 ms` | 100% limpia (`final_tou_02.png`) |
| **Plaza de Ciudad Glitz (Cap. 3)** | `tou_01` (`170`) | `EXIT=0`, `0 errs` | `35.13 ms` | `34.29 ms` | **`28.46 FPS`** | `24.74 ms` | Sin fallos ni cierres |
| **Bosque / Gran Árbol (Cap. 2)** | `mri_00` (`110`) | `EXIT=0`, `0 errs` | `35.95 ms` | `34.35 ms` | **`27.82 FPS`** | `21.11 ms` | 100% limpia (`map_mri_00.png`) |
| **Rogueport Este (caminado lento + NPCs)** | `gor_01` (`20`) | `EXIT=0`, `0 errs` | `39.88–42.04 ms` | `36.07–39.39 ms` | **`23.78–25.08 FPS`** | `28.38–29.83 ms` | 100% limpia, zancada fluida (`slow_1..4.png`, `final_gor_01.png`) |
| **Palacio de las Sombras (Cap. 8)** | `las_00` (`380`) | `EXIT=0`, `0 errs` | `42.41 ms` | `37.53 ms` | **`23.58 FPS`** | `32.02 ms` | 100% limpia (`map_las_00.png`) |
| **Subsuelo de Rogueport** | `tik_00` (`20`) | `EXIT=0`, `0 errs` | `47.52 ms` | `41.96 ms` | **`21.05 FPS`** | `33.49 ms` | 100% limpia (`map_tik_00.png`) |

---

## Instalación en PortMaster (desde el paquete Release)

1. Descarga `papercut-0.4.2-rk3326-portmaster.zip` desde la sección **Releases** de este repositorio.
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
