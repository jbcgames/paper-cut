Port nativo de Paper Mario: La Puerta Milenaria basado en LLCOOLT/paper-cut y jbcgames/aurora (rk3326-gles), adaptado y optimizado para consolas RK3326 / Mali-G31 MP2 (OpenGL ES 3.0 + KMSDRM) bajo PortMaster.

Aviso legal (sin contenido con derechos de autor)
No incluye game.iso ni ningún recurso extraído del disco original.
El binario ttyd-eu ha sido compilado con -DTTYD_NO_DISC_ASSETS=ON (port/include/no_disc_assets) y auditado con 0 coincidencias respecto a los archivos del disco.
Necesitas aportar tu propia copia legal europea (G8MP01) en /roms/ports/papercut/game.iso.
Novedades en v0.4.3-rk3326
Selector de idioma en PaperCut.sh (GAME_LANGUAGE):
Cabecera configurable en papercut/PaperCut.sh con GAME_LANGUAGE="es" por defecto.
Admite los 5 idiomas de la versión europea (G8MP01): es / 4 (Español), en / 1 (English), de / 2 (Deutsch), fr / 3 (Français), it / 5 (Italiano) y auto.
Exporta TTYD_LANGUAGE y sincroniza language= en saves/settings.txt.
Verificado en RK3326 cambiando entre fr, en y es con textos y guardado confirmados.
Salto con botón durante los créditos iniciales hacia la pantalla principal de bienvenida (SEQ_TITLE):
En src/sequence/seq_logo.c, pulsar cualquier botón (A, B, X, Y, Start, L, R, Z) durante los logos iniciales (case 7..16 / 17), o Start durante la pantalla inicial (case 5..6), omite el resto de logos y la introducción dmo_00 para ir directo a SEQ_TITLE.
En rels/dmo/src/dmo_00.c, si se inicia la historia introductoria dmo_00, pulsar A, B, X, Y o Start salta directamente a "title" (SEQ_TITLE).
Verificado en RK3326 (Start desde arranque en 118 frames y B durante logos en 39 frames, salida limpia RC=0).
Correcciones gráficas, estabilidad y limpieza de menú (incluidas desde v0.4.2-rk3326):
Eliminación de las 3 últimas pestañas añadidas por el port en el menú de pausa en RK3326 (Wardrobe / Vestuario, Online / En línea, Settings / Ajustes) en src/window/win_root.c (c676d8ff).
Corrección de glifos de UI faltantes ("Tiempo 00:04" / "Tiempo 00:06") y de SIGSEGV al iniciar partida en disp_maku mediante buffers dedicados s_mergedVtxBuf/s_mergedIdxBuf y revalidación de cachedEntry en lib/gx/command_processor.cpp (da4c0bed).
Instalación rápida
Descarga papercut-0.4.3-rk3326-portmaster.zip y verifica SHA256SUMS.txt.
Extrae Paper Cut.sh y la carpeta papercut/ dentro de /roms/ports/.
(Opcional) Edita GAME_LANGUAGE="es" en /roms/ports/papercut/PaperCut.sh si deseas otro idioma europeo (en, de, fr, es, it o auto).
Coloca tu copia legal europea (G8MP01) como /roms/ports/papercut/game.iso.
