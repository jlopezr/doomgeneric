/* DoomGeneric backend for MiniGPU's 320x240 RGB565 HDMI scanout.
 *
 * Build this file with DOOMGENERIC_RESX=320 and DOOMGENERIC_RESY=240.  The
 * core's normal 320x200 image is then letterboxed by i_video.c, and this
 * backend only converts its final RGBA8888 buffer to the native RGB565 back
 * framebuffer.
 */
#include <stdint.h>

#include "doomgeneric.h"
#include "doomkeys.h"
#include "mmio.h"

#define MINIGPU_WIDTH 320
#define MINIGPU_HEIGHT 240
#define MINIGPU_FB_BYTES (MINIGPU_WIDTH * MINIGPU_HEIGHT * 2u)

#ifndef MINIGPU_FB_FRONT
#define MINIGPU_FB_FRONT 0x01000000u
#endif

#ifndef MINIGPU_FB_BACK
#define MINIGPU_FB_BACK (MINIGPU_FB_FRONT + MINIGPU_FB_BYTES)
#endif

static uint32_t frame_base;
static unsigned short key_queue[16];
static unsigned char key_read;
static unsigned char key_write;
static unsigned char modifiers;

static unsigned char hid_to_doom_key(unsigned char key)
{
    if (key >= 0x04 && key <= 0x1d) return (unsigned char)('a' + key - 0x04);
    if (key >= 0x1e && key <= 0x26) return (unsigned char)('1' + key - 0x1e);
    if (key == 0x27) return '0';
    if (key >= 0x3a && key <= 0x43) return (unsigned char)(KEY_F1 + key - 0x3a);

    switch (key) {
    case 0x28: return KEY_ENTER;
    case 0x29: return KEY_ESCAPE;
    case 0x2a: return KEY_BACKSPACE;
    case 0x2b: return KEY_TAB;
    case 0x2c: return KEY_USE;
    case 0x2d: return KEY_MINUS;
    case 0x2e: return KEY_EQUALS;
    case 0x44: return KEY_F11;
    case 0x45: return KEY_F12;
    case 0x4f: return KEY_RIGHTARROW;
    case 0x50: return KEY_LEFTARROW;
    case 0x51: return KEY_DOWNARROW;
    case 0x52: return KEY_UPARROW;
    case 0xe0: case 0xe4: return KEY_FIRE;
    case 0xe1: case 0xe5: return KEY_RSHIFT;
    case 0xe2: case 0xe6: return KEY_RALT;
    default: return 0;
    }
}

static unsigned char modifier_to_doom_key(unsigned char bit)
{
    switch (bit) {
    case 0: case 4: return KEY_FIRE;    /* left/right Ctrl */
    case 1: case 5: return KEY_RSHIFT;  /* left/right Shift */
    case 2: case 6: return KEY_RALT;    /* left/right Alt */
    default: return 0;                   /* GUI has no Doom binding */
    }
}

static void queue_key(int pressed, unsigned char key)
{
    unsigned char next = (key_write + 1u) & 15u;
    if (next == key_read) return;
    key_queue[key_write] = (unsigned short)((pressed ? 0x100u : 0u) | key);
    key_write = next;
}

static int dequeue_key(int *pressed, unsigned char *doomKey)
{
    unsigned short event;
    if (key_read == key_write) return 0;
    event = key_queue[key_read];
    key_read = (key_read + 1u) & 15u;
    *pressed = event >> 8;
    *doomKey = (unsigned char)event;
    return 1;
}

static uint16_t rgba8888_to_rgb565(uint32_t pixel)
{
    return (uint16_t)(((pixel >> 8) & 0xf800u)
                    | ((pixel >> 5) & 0x07e0u)
                    | ((pixel >> 3) & 0x001fu));
}

void DG_Init(void)
{
    key_read = 0;
    key_write = 0;
    modifiers = 0;
    frame_base = MINIGPU_FB_BACK;
    REG(VIDEO_FB_FRONT) = MINIGPU_FB_FRONT;
    REG(VIDEO_FB_BACK) = MINIGPU_FB_BACK;
    REG(VIDEO_CTRL) = VIDEO_MODE_SCANOUT;
}

void DG_DrawFrame(void)
{
    uint16_t *dst;
    unsigned y, x;

    /* Do not overwrite the buffer the scanout has not swapped away yet. */
    while (REG(VIDEO_SWAP) & 1u) {}

    dst = (uint16_t *)frame_base;
    for (y = 0; y < MINIGPU_HEIGHT; ++y)
        for (x = 0; x < MINIGPU_WIDTH; ++x)
            dst[y * MINIGPU_WIDTH + x] =
                rgba8888_to_rgb565(DG_ScreenBuffer[y * MINIGPU_WIDTH + x]);

    REG(VIDEO_SWAP) = 1;
    frame_base = frame_base == MINIGPU_FB_FRONT ? MINIGPU_FB_BACK : MINIGPU_FB_FRONT;
}

void DG_SleepMs(uint32_t ms)
{
    uint32_t target = DG_GetTicksMs() + ms;
    while ((int32_t)(DG_GetTicksMs() - target) < 0) {}
}

uint32_t DG_GetTicksMs(void)
{
    /* HDMI is 640x480p60.  This is adequate for the platform smoke test;
       the full port can replace it with a monotonic timer peripheral. */
    return REG(MMIO_VIDEO_FRAME_COUNT_ADDR) * 1000u / 60u;
}

int DG_GetKey(int *pressed, unsigned char *doomKey)
{
    if (dequeue_key(pressed, doomKey)) return 1;
    while (REG(MMIO_INPUT_STATUS_ADDR) & MMIO_INPUT_STATUS_COUNT_MASK) {
        unsigned event = REG(MMIO_INPUT_EVENT_DATA_ADDR);
        unsigned char key;
        if (event >> 24) continue;       /* mouse or a future event type */
        if ((unsigned char)event == 0) {
            unsigned char updated = (unsigned char)(event >> 16);
            unsigned char changed = updated ^ modifiers;
            unsigned char bit;
            modifiers = updated;
            for (bit = 0; bit < 8; ++bit)
                if (changed & (1u << bit)) {
                    key = modifier_to_doom_key(bit);
                    if (key) queue_key(updated & (1u << bit), key);
                }
            if (dequeue_key(pressed, doomKey)) return 1;
            continue;
        }
        key = hid_to_doom_key((unsigned char)event);
        if (!key) continue;               /* modifier snapshot or unmapped key */
        *pressed = (event >> 8) & 1u;
        *doomKey = key;
        return 1;
    }
    return 0;
}

void DG_SetWindowTitle(const char *title)
{
    (void)title;
}
