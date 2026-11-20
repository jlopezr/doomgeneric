/* Visual smoke test for doomgeneric_minigpu.c; it does not link the Doom core. */
#define DOOMGENERIC_RESX 320
#define DOOMGENERIC_RESY 240

#include "doomgeneric.h"

pixel_t frame[DOOMGENERIC_RESX * DOOMGENERIC_RESY];
pixel_t *DG_ScreenBuffer = frame;

int main(void)
{
    unsigned x, y;
    DG_Init();
    for (y = 0; y < DOOMGENERIC_RESY; ++y)
        for (x = 0; x < DOOMGENERIC_RESX; ++x) {
            unsigned r = x < 106 ? 255u : 0u;
            unsigned g = x >= 106 && x < 213 ? 255u : 0u;
            unsigned b = x >= 213 ? 255u : 0u;
            frame[y * DOOMGENERIC_RESX + x] = (r << 16) | (g << 8) | b;
        }
    DG_DrawFrame();
    for (;;) {}
}
