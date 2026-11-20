/* Freestanding entry point: one embedded IWAD, no host filesystem. */
#include <stdlib.h>

#include "doomgeneric.h"
#include "w_file_minigpu.h"

extern unsigned doom_wad[];
extern unsigned doom_wad_end[];

/* 6 MiB starts after the normal code+IWAD image and ends before scanout RAM. */
#define MINIGPU_HEAP_BASE 0x00600000u
#define MINIGPU_HEAP_SIZE 0x00a00000u

int main(void)
{
    static char *argv[] = { "doom", "-iwad", "embedded.wad", "-nosound", 0 };
    void (*heap_init)(void *, size_t) = mini_heap_init;
    void (*doom_create)(int, char **) = doomgeneric_Create;

    /* Indirect calls avoid the ISA's short PC-relative JAL range. */
    heap_init((void *) MINIGPU_HEAP_BASE, MINIGPU_HEAP_SIZE);
    W_MiniGPU_SetWad((byte *) doom_wad,
                     (unsigned int) ((byte *) doom_wad_end - (byte *) doom_wad));
    doom_create(4, argv);
    for (;;) {
        doomgeneric_Tick();
    }
}
