#ifndef W_FILE_MINIGPU_H
#define W_FILE_MINIGPU_H

#include "doomtype.h"

/* Select the sole WAD exposed by the MiniGPU port.  `data` normally names an
 * image embedded with mini-cc --data; it remains owned by the caller. */
void W_MiniGPU_SetWad(byte *data, unsigned int length);

#endif
