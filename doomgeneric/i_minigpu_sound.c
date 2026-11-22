/* PCM SFX backend for the MiniGPU HDMI AUDIO device.
 *
 * Doom stores effects as unsigned mono DMX samples (usually 11025 Hz).  This
 * backend keeps the original data in the embedded WAD, mixes its game
 * channels directly to signed stereo 48 kHz, and feeds the SDRAM ring owned
 * by runtime/audio.c.  Music deliberately stays out: MUS needs a synth.
 */
#include <stdint.h>
#include <stdlib.h>

#include <audio.h>
#include <mmio.h>
#include "i_sound.h"
#include "w_wad.h"
#include "z_zone.h"

#define MINI_CHANNELS 16
#define MINI_RING_FRAMES 8192
#define MINI_TARGET_FRAMES 4096
#define MINI_MIX_FRAMES 256

typedef struct {
    const byte *data;
    uint32_t length;
    uint32_t rate;
} mini_sample_t;

typedef struct {
    mini_sample_t *sample;
    uint32_t position;
    uint32_t step;
    int left;
    int right;
    boolean active;
} mini_voice_t;

static int16_t ring_samples[MINI_RING_FRAMES * AUDIO_CHANNELS];
static int16_t mix_samples[MINI_MIX_FRAMES * AUDIO_CHANNELS];
static audio_ring_t ring;
static mini_voice_t voices[MINI_CHANNELS];
static boolean sound_ready;
static boolean use_sfx_prefix;

/* Opciones que m_config expone cuando FEATURE_SOUND está activo. El backend
 * fijo de la placa no usa libsamplerate, pero conservar los símbolos evita
 * que el menú de configuración quede acoplado a SDL. */
int use_libsamplerate;
float libsamplerate_scale;

static int clamp16(int value) {
    if (value > 32767) return 32767;
    if (value < -32768) return -32768;
    return value;
}

static void sfx_name(sfxinfo_t *sfx, char *name) {
    int i;
    if (sfx->link != NULL) sfx = sfx->link;
    if (use_sfx_prefix) {
        name[0] = 'd'; name[1] = 's';
        for (i = 0; i < 6 && sfx->name[i]; ++i) name[i + 2] = sfx->name[i];
        name[i + 2] = 0;
    } else {
        for (i = 0; i < 8 && sfx->name[i]; ++i) name[i] = sfx->name[i];
        name[i] = 0;
    }
}

static int get_lump_num(sfxinfo_t *sfx) {
    char name[9];
    sfx_name(sfx, name);
    return W_GetNumForName(name);
}

static mini_sample_t *load_sample(sfxinfo_t *sfx) {
    byte *raw;
    unsigned length;
    unsigned total;
    mini_sample_t *sample;
    int lump = sfx->lumpnum;
    if (sfx->link != NULL) sfx = sfx->link;
    if (sfx->driver_data != NULL) return (mini_sample_t *)sfx->driver_data;
    if (lump < 0) lump = get_lump_num(sfx);
    raw = W_CacheLumpNum(lump, PU_STATIC);
    total = W_LumpLength(lump);
    if (total < 40 || raw[0] != 3 || raw[1] != 0) return NULL;
    length = (unsigned)raw[4] | ((unsigned)raw[5] << 8)
           | ((unsigned)raw[6] << 16) | ((unsigned)raw[7] << 24);
    if (length <= 32 || length > total - 8) return NULL;
    sample = malloc(sizeof(*sample));
    if (sample == NULL) return NULL;
    sample->rate = (uint32_t)raw[2] | ((uint32_t)raw[3] << 8);
    sample->data = raw + 24;       /* DMX omite 16 al principio y al final. */
    sample->length = length - 32;
    if (sample->rate == 0 || sample->length == 0) return NULL;
    sfx->driver_data = sample;
    return sample;
}

static void set_params(mini_voice_t *voice, int vol, int sep) {
    voice->left = ((254 - sep) * vol) / 127;
    voice->right = (sep * vol) / 127;
}

static void mix_frames(uint32_t count) {
    uint32_t frame;
    int channel;
    for (frame = 0; frame < count; ++frame) {
        int left = 0, right = 0;
        for (channel = 0; channel < MINI_CHANNELS; ++channel) {
            mini_voice_t *voice = &voices[channel];
            uint32_t index;
            int sample;
            if (!voice->active) continue;
            index = voice->position >> 16;
            if (index >= voice->sample->length) {
                voice->active = false;
                continue;
            }
            sample = (int)voice->sample->data[index] - 128;
            left += sample * voice->left;
            right += sample * voice->right;
            voice->position += voice->step;
        }
        mix_samples[frame * 2] = (int16_t)clamp16(left);
        mix_samples[frame * 2 + 1] = (int16_t)clamp16(right);
    }
}

static boolean mini_init(boolean prefix) {
    int i;
    use_sfx_prefix = prefix;
    for (i = 0; i < MINI_CHANNELS; ++i) voices[i].active = false;
    sound_ready = audio_ring_init(&ring, ring_samples, MINI_RING_FRAMES);
    return sound_ready;
}

static void mini_shutdown(void) {
    if (sound_ready) REG(AUDIO_CTRL) = 0;
    sound_ready = false;
}

static void mini_update(void) {
    uint32_t available;
    uint32_t remaining;
    if (!sound_ready) return;
    available = audio_ring_available(&ring);
    if (available >= MINI_TARGET_FRAMES) return;
    remaining = MINI_TARGET_FRAMES - available;
    while (remaining) {
        uint32_t count = remaining > MINI_MIX_FRAMES ? MINI_MIX_FRAMES : remaining;
        mix_frames(count);
        if (audio_ring_write(&ring, mix_samples, count) != count) return;
        remaining -= count;
    }
}

static void mini_update_params(int channel, int vol, int sep) {
    if (channel >= 0 && channel < MINI_CHANNELS && voices[channel].active)
        set_params(&voices[channel], vol, sep);
}

static int mini_start(sfxinfo_t *sfx, int channel, int vol, int sep) {
    mini_voice_t *voice;
    mini_sample_t *sample;
    if (!sound_ready || channel < 0 || channel >= MINI_CHANNELS) return -1;
    sample = load_sample(sfx);
    if (sample == NULL) return -1;
    voice = &voices[channel];
    voice->sample = sample;
    voice->position = 0;
    voice->step = (sample->rate << 16) / AUDIO_SAMPLE_RATE;
    if (voice->step == 0) voice->step = 1;
    set_params(voice, vol, sep);
    voice->active = true;
    return channel;
}

static void mini_stop(int channel) {
    if (channel >= 0 && channel < MINI_CHANNELS) voices[channel].active = false;
}

static boolean mini_playing(int channel) {
    return channel >= 0 && channel < MINI_CHANNELS && voices[channel].active;
}

static void mini_precache(sfxinfo_t *sounds, int count) {
    (void)sounds; (void)count;
}

static snddevice_t devices[] = { SNDDEVICE_SB };

sound_module_t DG_sound_module = {
    devices, 1, mini_init, mini_shutdown, get_lump_num, mini_update,
    mini_update_params, mini_start, mini_stop, mini_playing, mini_precache
};

/* `-nomusic` keeps this unused, but i_sound.c still names the module when
 * FEATURE_SOUND is enabled. */
music_module_t DG_music_module = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
