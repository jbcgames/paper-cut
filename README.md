# Paper Cut — Fork RK3326 / PortMaster (`v0.5.0-rk3326`)

Adaptación y optimización de [LLCOOLT/paper-cut](https://github.com/LLCOOLT/paper-cut) (descompilación y port nativo de *Paper Mario: The Thousand-Year Door*) junto con el backend OpenGL ES 3.0 del submódulo [jbcgames/aurora](https://github.com/jbcgames/aurora) (rama `rk3326-gles`, basado en [encounter/aurora](https://github.com/encounter/aurora)) para consolas portátiles **RK3326** con GPU **Mali-G31 MP2** bajo **KMSDRM / PortMaster** (verificado en `dArkOS4Clone` / G350 / K36 con la versión europea `G8MP01`).

> **Aviso legal:** Ni este repositorio ni los paquetes de *Release* contienen el juego original, imágenes de disco (`game.iso`), archivos extraídos del disco (`orig/`) ni contenido protegido por derechos de autor. El binario `ttyd-eu` se compila con `-DTTYD_NO_DISC_ASSETS=ON` (usando los *placeholders* de `port/include/no_disc_assets/`). Debes aportar tu propia copia legal europea (`G8MP01`) en `/roms/ports/papercut/game.iso`.

---

## Estado actual y cambios implementados (hasta `v0.5.0-rk3326`)

### 1. Modo unificado a 30 FPS renderizados con lógica bloqueada a 60.00 Hz
- **Objetivo global de `30 FPS` en resolución nativa `640×480`** (`port/src/aurora/battery_aurora.c`, `port/src/aurora/vi_aurora.c`, `port/settings/features/toggles.cpp`):
  - `battery_saver = 2` (*Always*) activo por defecto (`[battery] 30fps on`), manteniendo `marioStMain()` y los controles a `60 Hz` reales mientras `portBatterySkipDraw()` alterna el renderizado (`(sSinceDrawn < 2) || portViBehindSchedule()`).
  - Eliminada la comprobación residual `e->cameraId == CAMERA_OFFSCREEN2` en `portDispMustDraw()` (`src/driver/dispdrv.c`) que obligaba a dibujar cada frame, dejando `portDispForceDrawThisFrame()` únicamente en las capturas *one-shot* necesarias (`sac_deka.c`, `sac_scissor.c`, `sac_zubastar.c`, `unit_party_chuchurina.c`, `battle_stage.c`, `evt_mobj.c`, `win_root.c`, `win_log.c`).
- **Sincronización a 60 Hz de menús, *Action Commands*, Poderes Estelares y efectos**:
  - Se desacopló el avance de estado respecto a los callbacks de render (`disp` → `main` bajo `#ifndef TARGET_PC` / `#ifdef TARGET_PC`) para que ninguna animación o barra avance a mitad de velocidad al saltar frames:
    - Menús de combate (`src/battle/battle_menu_disp.c`): `stepMainMenuState()`, `stepSubMenuState()` (`sSubMenuSkipFirstStep`) y `stepMenuPartyChangeButton()`.
    - *Action Commands* (`include/battle/battle_ac.h`, `src/battle/battle_ac.c` y 19 módulos en `src/battle/ac/`): helpers `BattleAcStepSlideIn`, `BattleAcStepSlideOut`, `BattleAcStepGaugeEase` y `BattleAcStepColorPulse`, corrigiendo el ritmo de aparición de barras y la alineación de indicadores (como los ataques de Koops).
    - Poderes Estelares, HUD y efectos N64 (`sac_scissor.c`, `sac_bakugame.c`, `statuswindow.c`, `eff_sstar_n64.c`, `eff_packn_bfire_n64.c`, `eff_kemuri8_n64.c`).

### 2. Correcciones gráficas y de estabilidad en OpenGL ES 3.0 / Mali-G31
- **Holograma de Peach en sala de Pajarón (`rels/aji/src/aji_17.c`, Cap. 2)**:
  - Aurora omite copias de profundidad EFB (`GX_TF_Z8`, `kEnableDepthCopies = false`), provocando un recuadro negro translúcido alrededor del holograma.
  - Bajo `#ifdef TARGET_PC`, `peach_capture()` habilita `GXSetAlphaUpdate(GX_TRUE)` en los pases `1..2` de `animPoseDrawMtx` y captura la silueta con alfa en `horogram_wp->buf0` (`GX_TF_RGBA8`), mientras `disp_horogram()` usa 1 etapa TEV (`TEXA * RASA`) para recortar al personaje limpiamente.
- **Primitivas de líneas/puntos y efectos N64 (`extern/aurora` + `src/effect/n64/`)**:
  - Soporte en GLES 3.0 / GX para `GX_LINES`, `GX_LINESTRIP` y `GX_POINTS`, consulta de `GL_ALIASED_LINE_WIDTH_RANGE`, `glLineWidth`, `gl_PointSize` y sincronización de registros XF (`xfData + (i + k) * 4` e invalidación `xfRegValid`), corrigiendo el trazado de *Ataque Estelar* (*Art Attack*) y *Torm. Estelar* (*Supernova*).
  - Incorporado `effSetVtxDescN64Sized` (`sizeof(effN64Vtx) == 14`) en los 13 módulos de `src/effect/n64/`.
- **Correcciones acumuladas desde `v0.4.1-rk3326` – `v0.4.5-rk3326`**:
  - Interludio de Peach al final del Acto 1 en `aji_10` (`portDispGroupMapObjects()` en `src/driver/dispdrv.c`).
  - Fondo negro de respaldo cuando falta `.tpl` en lugar de verde neón (`src/driver/bgdrv.c`: `sys_00`, `muj_10`, `muj_20`).
  - Solución a `unknown opcode 0x6A` al cargar partidas (`animdrv.c` + invalidación `lastVtxFmt` en Aurora).
  - Solución a glifos omitidos en UI y `SIGSEGV` en `disp_maku` (`flush_pending_pnmtx_batch` con buffers aislados `s_mergedVtxBuf`/`s_mergedIdxBuf` y revalidación tras rehash).
  - Corrección de vibración de pies al caminar (`src/driver/npcdrv.c` con cálculo exacto de `spd` e histéresis de estado).
  - Corrección 3D→2D→3D del barco (`gor_00` / `ship_pera`), decodificación TLUT/paletas, guardas nulas en `dvdmgr.c`/`battle.c`/`seq_battle.c`, simplificación del menú de pausa (`win_root.c`), flujo directo sin menús extra en título y salto de logos iniciales con botón (`seq_logo.c`, `dmo_00.c`).

### 3. Optimizaciones de rendimiento y gestión de RAM (~895 MiB)
- **Aurora GL/GX (`extern/aurora`)**:
  - Caché persistente de geometría (`GL_EXT_buffer_storage`), promoción multiframe (`NativeGeomSeenInfo`, `minRecurFrames`) para no saturar con geometría efímera de un frame, `kMaxNativeGeomCacheEntries = 65536` con auto-reset acotado, `StaticDlEntry::drawCount` en `u16` y `rehash(0)`.
  - Invalidación de display lists acotada por rango (`invalidate_dl_memory_range`) y poda de texturas no referenciadas (`prune_unreferenced_textures()`).
  - Caché de programas binarios SQLite con tope de precompilación al arranque (`kMaxBootPrecompileRows = 1200`), carga de blobs bajo demanda mediante `g_knownKeys` y liberación en RAM tras el arranque (`{}.swap(g_map)`).
  - `glDrawElementsBaseVertex`, memoización de pipelines (`s_nativePipeMemo`), caché shadow GX/XF, compactación y deduplicación UBO (`s_uniformDedup`) y sincronización GPU mediante `eglWaitSyncKHR`.
- **Motor y escenas (`paper-cut`)**:
  - `smartPreDispSync()` antes de `camDraw()` (`memory.c` / `mariost.c`) para evitar vaciados síncronos en mitad de escena, e invalidación selectiva en `portGXForgetRange()` (`gx_arrays_pc.c`).
  - Culling homogéneo de 6 planos (`mapClipBoxVisible`, `extPointInFrustum`, `effFireMain`), loteo por textura en multitudes (`extDraw`, `ryushi_disp` en `yuu_03`), omisión de Punis inactivos (`mri_puni.c`), captura única en ventanas del Excess Express (`rsh.c`) y optimizaciones en `hom_00.c` / `envdrv.c`.

---

## Pruebas realizadas en hardware real (RK3326 / Mali-G31 MP2)

| Tipo de prueba | Alcance y condiciones | Resultado verificado |
|---|---|---|
| **Barrido completo de escenarios con movimiento** | `271 / 271` escenas jugables de `G8MP01` con desplazamiento continuo y evasión de enemigos (`TTYD_SWEEP_MOVE=1`, Pass 2 en modo 60 Hz sin límite de 30 FPS) | `0` crasheos, `0` timeouts, `0` errores de shader, `21` hojas de contacto limpias; **`263 / 271` escenas `>= 30 FPS`** (`166` `>= 50 FPS`, `97` `30.0–49.9 FPS`, `1` `28.0–29.9 FPS`, `3` `25.0–27.9 FPS`, `4` en `yuu_00..03` a `18.81–22.86 FPS` durante arranque / `24.91–28.43 FPS` en medición estable). |
| **Estabilidad y fugas de memoria RAM (Soak Tests)** | Ejecución prolongada (`TTYD_SWEEP_MOVE=1`, compilación Release) en `las_29` (~240 s), `yuu_03` (~240 s) y bucle multi-escena `gor_01 → yuu_03 → las_29 → win_06` ×3 (~315 s) | `EXIT_CODE=0` en todos los casos. `VmRSS` estabilizado (`las_29`: `583.7 → 585.2 MB`; `yuu_03`: `599.3 MB` sin crecimiento en 165–240 s; bucle multi-escena: `634.9–637.9 MB` con poda activa de `geom-cache`). |
| **Auditoría visual de combate a 30 FPS** | Pruebas por lotes en `[battery] 30fps on` en Dojo de Batalla con inspección directa de capturas | Rueda y submenús fluidos a 60 Hz lógicos; 9 Saltos y 9 Martillos de Mario, 8 Poderes Estelares, técnicas de los 7 compañeros (*Goombella*, *Koops*, *Bobbery*, *Yoshi*, *Claudia*, *Bibiana*, *EMOV/Lupina*), ítems ofensivos y *Guard*/*Superguard* verificados. |
| **Escena `aji_17` (Holograma de Peach)** | Capturas en diálogo y baile + smoke test final en el binario Release desplegado (`[battery] 30fps on`, `TTYD_STORY=113`) | Holograma recortado con transparencia limpia sin recuadro negro; arranque y entrada a `aji_17` con `SMOKE_RC=0`, `[warp] to aji_17` y `0` errores/`FATAL`. |

*(Nota: el barrido completo de 271 escenas, los soak tests de RAM y la auditoría por lotes de ataques en 30 FPS se ejecutaron sobre el código equivalente justo antes del parche final en `rels/aji/src/aji_17.c`, tras el cual se verificó visualmente y con smoke test la escena `aji_17` y la compilación Release final.)*

---

## Rendimiento esperado

- **Comportamiento por defecto (`[battery] 30fps on`)**: el juego renderiza a **`30 FPS` estables en `640×480` nativo** con la simulación lógica, animaciones de interfaz y ventana de *Action Commands* funcionando a **`60 Hz` reales**.
- **Escenarios y batallas**: la gran mayoría del juego (campos, pueblos, interiores, batallas y ~97% de las escenas) mantiene margen suficiente para sostener los `30 FPS`.
- **Zonas de máxima carga**: mapas con gran densidad de partículas, multitudes o geometría (como `yuu_00..03` en la Feria Pianta, `las_29` o `win_06`) pueden oscilar entre **`~25 y 29 FPS`** según el ángulo de cámara, manteniendo la velocidad real de juego a 60 Hz gracias a los *catch-up ticks*.
- **Primer arranque**: la primera visita a zonas nuevas compila shaders para `cache/pipeline_cache.db`; en arranques posteriores solo se precompilan hasta `1200` programas para evitar tirones de inicio y agotar límites del driver Mali-G31.
- **Modo 60 FPS opcional**: si deseas desactivar el objetivo de 30 FPS en escenas ligeras, puedes iniciar con `TTYD_NO_MIXED_FPS=1`.

---

## Forma de ejecutar (PortMaster en RK3326)

### Instalación
1. Descarga `papercut-0.5.0-rk3326-portmaster.zip` y verifica su integridad con `SHA256SUMS.txt` desde la sección **Releases**.
2. Descomprime su contenido dentro de la carpeta de puertos de tu consola (`/roms/ports/`), de modo que queden:
   - `/roms/ports/Paper Cut.sh`
   - `/roms/ports/papercut/PaperCut.sh`
   - `/roms/ports/papercut/ttyd-eu`
   - `/roms/ports/papercut/papercut.gptk`
   - `/roms/ports/papercut/libs.aarch64/` (`libSDL3.so.0`, `libpng16.so.16`, `libstdc++.so.6`)
   - `/roms/ports/papercut/Logo*.jpg`, `README_INSTALACION.txt`, `RK3326_PORT.md`, `saves/settings.txt`
3. Coloca tu copia legal europea (**Paper Mario: The Thousand-Year Door PAL / `G8MP01`**) en:
   ```text
   /roms/ports/papercut/game.iso
   ```
4. Inicia **Paper Cut** desde EmulationStation / PortMaster.
   - **Salir del juego**: combinación `Select + Start` configurada en `papercut/papercut.gptk`.

### Idioma y configuración
- **Idioma por defecto (`es`)**: configurado en `papercut/PaperCut.sh` (`GAME_LANGUAGE="es"`) y `papercut/saves/settings.txt` (`language=4`). Puedes cambiar `GAME_LANGUAGE` al inicio de `PaperCut.sh` a:
  - `"es"` o `"4"` (Español)
  - `"en"` o `"1"` (English)
  - `"de"` o `"2"` (Deutsch)
  - `"fr"` o `"3"` (Français)
  - `"it"` o `"5"` (Italiano)
  - `"auto"` (usa `saves/settings.txt` o el idioma del sistema)
- **Variables de entorno útiles** (usadas internamente por `PaperCut.sh` u opcionales):
  - `TTYD_DISC`: ruta a `game.iso`.
  - `TTYD_USER_DIR`: carpeta de partidas y configuración (por defecto `/roms/ports/papercut/saves`).
  - `TTYD_CACHE_DIR`: carpeta de caché de shaders GLES (`pipeline_cache.db`, por defecto `/roms/ports/papercut/cache`).
  - `TTYD_LANGUAGE`: código de idioma (`es`, `en`, `de`, `fr`, `it`).
  - `TTYD_NO_MIXED_FPS=1`: desactiva el límite de 30 FPS del modo batería.
  - `TTYD_MIXED_FPS=1`: fuerza el modo 30 FPS renderizados / 60 Hz lógicos.

---

## Forma de compilar (Cross-compilation AArch64 / RK3326)

Compilación verificada en **Linux / WSL (x86_64)** generando el ejecutable europeo sin recursos del disco (`TTYD_NO_DISC_ASSETS=ON`).

### Requisitos
- Sysroot AArch64 para RK3326 en `/opt/rk3326-sysroot` (usado por `cmake/toolchains/aarch64-linux-gnu.cmake`).
- `clang-21`, `clang++-21`, herramientas LLVM 21 (`llvm-ar`, `llvm-nm`, `llvm-objcopy`), `cmake` (>= 3.25), `ninja`, `python3` y `aarch64-linux-gnu-strip`.
- Submódulo `extern/aurora` inicializado en la rama `rk3326-gles`.

### Pasos de compilación
```bash
git clone --recursive https://github.com/jbcgames/paper-cut.git
cd paper-cut

cmake -S . -B build-aarch64 -G Ninja \
  --toolchain=cmake/toolchains/aarch64-linux-gnu.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DTTYD_VERSION=G8MP01 \
  -DTTYD_BACKEND=aurora \
  -DTTYD_AURORA_GLES=ON \
  -DTTYD_NO_DISC_ASSETS=ON \
  -DTTYD_PORT_VERSION=0.5.0-rk3326

ninja -C build-aarch64 ttyd-eu

aarch64-linux-gnu-strip -o build-aarch64/ttyd-eu.stripped build-aarch64/ttyd-eu
```

El binario resultante `build-aarch64/ttyd-eu.stripped` sustituye a `/roms/ports/papercut/ttyd-eu` dentro del paquete PortMaster.

---

## Contributors / Contribuidores

- **Jbcgames** ([@jbcgames](https://github.com/jbcgames)) — Creador y mantenedor del fork para RK3326 / PortMaster (`jbcgames/paper-cut`, `jbcgames/aurora`), empaquetado, configuración del entorno KMSDRM/Mali-G31 y dirección de pruebas en hardware.
- **Antigravity** (*AI Coding Assistant*, Google DeepMind) — Contribución técnica en análisis y depuración, optimizaciones de rendimiento GLES 3.0 / GX / caché de geometría, sincronización de lógica a 60 Hz en modo 30 FPS, corrección gráfica de ataques y holograma en `aji_17`, automatización de barridos en hardware y documentación.
- **Proyectos base (Upstream)**:
  - [LLCOOLT/paper-cut](https://github.com/LLCOOLT/paper-cut) — Descompilación de *Paper Mario: The Thousand-Year Door* y port nativo multiplataforma.
  - [encounter/aurora](https://github.com/encounter/aurora) — Capa de plataforma GameCube (`GX`, `VI`, `PAD`, `DVD`, `CARD`).
  - Licencias y avisos de terceros en [docs/THIRD_PARTY.md](docs/THIRD_PARTY.md) y [LICENSE](LICENSE).
