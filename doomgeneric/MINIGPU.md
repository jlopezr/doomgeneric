# Backend MiniGPU: hito 1

[`doomgeneric_minigpu.c`](doomgeneric_minigpu.c) es el backend mínimo de
DoomGeneric para la placa MiniGPU 42. Convierte el framebuffer RGBA8888 de
320×240 a los dos framebuffers RGB565 de 320×240 del scanout HDMI y pide un
swap por frame. `DG_GetKey()` consume la FIFO `INPUT` y traduce HID a Doom:
flechas, letras, números, Enter/Escape/Tab/Backspace, Space, Ctrl, Shift, Alt
y F1–F12. Los snapshots de modificadores `KEY=0` se expanden en una cola local
para que sus down/up lleguen al motor como Ctrl/Shift/Alt. El reloj se deriva
por ahora del contador de frames.

La primera comprobación es [`minigpu_smoke.c`](minigpu_smoke.c), que dibuja
tres barras RGB sin enlazar el motor Doom. Desde la raíz del repositorio:

```text
tools/mini-cc w.doomgeneric/doomgeneric/minigpu_smoke.c \
  w.doomgeneric/doomgeneric/doomgeneric_minigpu.c \
  runtime/src/string.c runtime/src/stdlib.c runtime/src/stdio.c \
  --outdir _build/c -o _build/c/doomgeneric_minigpu_smoke.bin
```

Se carga en la 42 con `run-board --prototype 42 --program
_build/c/doomgeneric_minigpu_smoke.bin`. Las bases de los framebuffers son
`0x01000000` y `0x01025800`, ambas alineadas a 16 bytes y fuera del programa.

El siguiente hito añadirá un reloj monotónico y enlazará el núcleo Doom con
`DOOMGENERIC_RESX=320` y `DOOMGENERIC_RESY=240`.

## Compilación del port

`Makefile.minigpu` fija la lista de fuentes del núcleo y sustituye los
backends X11/stdio por los de MiniGPU. No sintetiza ni programa la FPGA:

```text
make -f Makefile.minigpu
```

Busca el IWAD local en `../Doom1.WAD`; se puede indicar otro sin copiarlo ni
versionarlo con `make -f Makefile.minigpu DOOM_WAD=/ruta/a/doom.wad`.

## Input smoke

[`minigpu_input_smoke.c`](minigpu_input_smoke.c) retorna el primer evento Doom
codificado en `R1`. Con [`minigpu_input.script`](minigpu_input.script), la A
pulsada se traduce a `0x161` (`down | 'a'`):

```text
tools/mini-cc w.doomgeneric/doomgeneric/minigpu_input_smoke.c \
  w.doomgeneric/doomgeneric/doomgeneric_minigpu.c \
  runtime/src/string.c runtime/src/stdlib.c runtime/src/stdio.c \
  --outdir _build/c -o _build/c/doomgeneric_minigpu_input.bin
tools/sim-sys _build/c/doomgeneric_minigpu_input.bin --video --keyboard \
  --input-script w.doomgeneric/doomgeneric/minigpu_input.script
```

## IWAD en memoria

[`w_file_minigpu.c`](w_file_minigpu.c) reemplaza el backend `stdio` al compilar
con `-D MINIGPU`. `W_MiniGPU_SetWad(datos, tamaño)` registra un único blob de
solo lectura (normalmente `extern unsigned doom_wad[]`, incorporado con
`mini-cc --data doom_wad=...`; su longitud se calcula restando ambos símbolos
como punteros a bytes. No emula directorios, descriptores ni un
filesystem POSIX: entrega el blob a la abstracción `wad_file_t` que Doom ya
usa para lecturas posicionadas.

[`minigpu_wad_smoke.c`](minigpu_wad_smoke.c) prueba esa interfaz con un blob
estático. Se compila junto a `w_file.c` y `w_file_minigpu.c`, siempre con
`-D MINIGPU`. Para comprobar el mismo camino con el IWAD local, sin añadirlo a
Git, se usa `--data`, que expone inicio y final del blob:

```text
tools/mini-cc -D MINIGPU -D MINIGPU_EMBEDDED_WAD \
  w.doomgeneric/doomgeneric/minigpu_wad_smoke.c \
  w.doomgeneric/doomgeneric/w_file.c w.doomgeneric/doomgeneric/w_file_minigpu.c \
  runtime/src/string.c runtime/src/stdlib.c runtime/src/stdio.c \
  --data doom_wad=w.doomgeneric/Doom1.WAD \
  --outdir _build/c -o _build/c/doomgeneric_embedded_wad_smoke.bin
tools/sim-sys _build/c/doomgeneric_embedded_wad_smoke.bin
```
