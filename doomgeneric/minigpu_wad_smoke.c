/* Smoke test for the single-blob MiniGPU WAD backend. */
#include "w_file.h"
#include "w_file_minigpu.h"

static byte wad_data[] = {
    'I', 'W', 'A', 'D', 1, 2, 3, 4,
    'D', 'O', 'O', 'M', 5, 6, 7, 8
};

#ifdef MINIGPU_EMBEDDED_WAD
extern unsigned doom_wad[];
extern unsigned doom_wad_end[];
#endif

int main(void)
{
    wad_file_t *wad;
    byte out[6];
    int errors = 0;

#ifdef MINIGPU_EMBEDDED_WAD
    W_MiniGPU_SetWad((byte *) doom_wad,
                     (unsigned int) ((byte *) doom_wad_end - (byte *) doom_wad));
#else
    W_MiniGPU_SetWad(wad_data, sizeof(wad_data));
#endif
    wad = W_OpenFile("embedded.wad");
    if (wad == 0) return 1;
#ifndef MINIGPU_EMBEDDED_WAD
    if (wad->length != sizeof(wad_data) || wad->mapped != wad_data)
        errors |= 1;
#endif
#ifdef MINIGPU_EMBEDDED_WAD
    if (wad->length < 12 || wad->mapped[0] != 'I' || wad->mapped[1] != 'W'
        || wad->mapped[2] != 'A' || wad->mapped[3] != 'D') errors |= 2;
#else
    if (W_Read(wad, 4, out, sizeof(out)) != sizeof(out)) errors |= 2;
    if (out[0] != 1 || out[3] != 4 || out[4] != 'D' || out[5] != 'O') errors |= 4;
    if (W_Read(wad, 15, out, sizeof(out)) != 1 || out[0] != 8) errors |= 8;
    if (W_Read(wad, 16, out, 1) != 0) errors |= 16;
#endif
    W_CloseFile(wad);
    return errors;
}
