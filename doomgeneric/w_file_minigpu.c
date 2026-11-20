/* Read-only WAD backend for MiniGPU.  It deliberately is not a fake POSIX
 * filesystem: Doom's WAD layer needs only open, close and positioned reads. */
#include <string.h>

#include "w_file.h"
#include "w_file_minigpu.h"

typedef struct
{
    wad_file_t wad;
} minigpu_wad_file_t;

static minigpu_wad_file_t embedded_wad;
static byte *embedded_data;
static unsigned int embedded_length;

extern wad_file_class_t minigpu_wad_file;

void W_MiniGPU_SetWad(byte *data, unsigned int length)
{
    embedded_data = data;
    embedded_length = length;
}

static wad_file_t *W_MiniGPU_OpenFile(char *path)
{
    (void)path;
    if (embedded_data == NULL || embedded_length == 0) return NULL;
    embedded_wad.wad.file_class = &minigpu_wad_file;
    embedded_wad.wad.mapped = embedded_data;
    embedded_wad.wad.length = embedded_length;
    return &embedded_wad.wad;
}

static void W_MiniGPU_CloseFile(wad_file_t *wad)
{
    (void)wad;
}

static size_t W_MiniGPU_Read(wad_file_t *wad, unsigned int offset,
                             void *buffer, size_t buffer_len)
{
    size_t count;
    if (offset >= wad->length) return 0;
    count = wad->length - offset;
    if (count > buffer_len) count = buffer_len;
    memcpy(buffer, wad->mapped + offset, count);
    return count;
}

wad_file_class_t minigpu_wad_file =
{
    W_MiniGPU_OpenFile,
    W_MiniGPU_CloseFile,
    W_MiniGPU_Read,
};
