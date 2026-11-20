/* Returns the first Doom key event as 0x100 | key for key-down, or key for up. */
#define DOOMGENERIC_RESX 320
#define DOOMGENERIC_RESY 240

#include "doomgeneric.h"

pixel_t *DG_ScreenBuffer;

int main(void)
{
    int pressed;
    unsigned char key;
    DG_Init();
    for (;;) {
        if (DG_GetKey(&pressed, &key))
            return (pressed ? 0x100 : 0) | key;
    }
}
