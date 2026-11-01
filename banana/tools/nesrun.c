/* nesrun: minimal libretro host for correctness checks on the PC (no display, no audio device).
 *   nesrun <core.so> <rom> <frames> [-o frames.crc] [-d dumpframe.ppm:N] [-v var=value ...] [-s sav-out]
 * Prints: per-1200-frame video CRC lines (to -o if given, per frame), the whole-run video CRC, audio CRC (sum of
 * samples), sample count. Identical CRCs between two cores/builds = identical picture and sound (the exactness gate).
 * GPL-2.0-or-later like the rest of BananaNES. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "libretro.h"

static uint32_t crc_tab[256];
static void crc_init(void) { for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1; crc_tab[i] = c; } }
static uint32_t crc_upd(uint32_t c, const void *p, size_t n) { const uint8_t *b = p; c = ~c; while (n--) c = crc_tab[(c ^ *b++) & 255] ^ (c >> 8); return ~c; }

static enum retro_pixel_format pf = RETRO_PIXEL_FORMAT_0RGB1555;
static uint32_t vcrc, frame_crc, acrc; static uint64_t asamples, vframes;
static FILE *fcrc; static int dumpn = -1; static const char *dumpf; static long fno;
static struct { const char *k, *v; } vars[64]; static int nvars;
static void (*log_cb)(enum retro_log_level, const char *, ...);

static void vlog(enum retro_log_level l, const char *f, ...) { (void)l; (void)f; }
static bool env(unsigned cmd, void *d) {
    switch (cmd) {
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: pf = *(enum retro_pixel_format *)d; return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE: { struct retro_variable *v = d; for (int i = 0; i < nvars; i++) if (!strcmp(vars[i].k, v->key)) { v->value = vars[i].v; return true; } v->value = NULL; return false; }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: *(bool *)d = false; return true;
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: *(const char **)d = "/tmp"; return true;
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: ((struct retro_log_callback *)d)->log = (retro_log_printf_t)vlog; return true;
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION: *(unsigned *)d = 0; return true;
    case RETRO_ENVIRONMENT_SET_VARIABLES: case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS: case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO: case RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS:
    case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS: case RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY: return true;
    case RETRO_ENVIRONMENT_SET_MEMORY_MAPS: if (getenv("NESRUN_MAPS")) { const struct retro_memory_map *m = d; for (unsigned i = 0; i < m->num_descriptors; i++) { const struct retro_memory_descriptor *e = &m->descriptors[i]; printf("map %-7s start %05zx len %05zx flags %x\n", e->addrspace ? e->addrspace : "cpu", e->start, e->len, e->flags); } } return true;
    default: return false;
    }
}
static int fastmode;
static void video(const void *data, unsigned w, unsigned h, size_t pitch) {
    if (!data) return;
    if (fastmode) { vframes++; return; }
    unsigned bpp = pf == RETRO_PIXEL_FORMAT_XRGB8888 ? 4 : 2;
    uint32_t c = 0;
    for (unsigned y = 0; y < h; y++) c = crc_upd(c, (const uint8_t *)data + y * pitch, w * bpp);
    vcrc = crc_upd(vcrc, &c, 4); vframes++;
    if (fcrc) fprintf(fcrc, "%ld %08x\n", fno, c);
    if (fno == dumpn && dumpf) {
        FILE *f = fopen(dumpf, "wb"); if (f) { fprintf(f, "P6\n%u %u\n255\n", w, h);
            for (unsigned y = 0; y < h; y++) for (unsigned x = 0; x < w; x++) { uint8_t px[3];
                if (bpp == 4) { uint32_t v = ((const uint32_t *)((const uint8_t *)data + y * pitch))[x]; px[0] = v >> 16; px[1] = v >> 8; px[2] = v; }
                else { uint16_t v = ((const uint16_t *)((const uint8_t *)data + y * pitch))[x];
                    if (pf == RETRO_PIXEL_FORMAT_RGB565) { px[0] = (v >> 11) << 3; px[1] = ((v >> 5) & 63) << 2; px[2] = (v & 31) << 3; }
                    else { px[0] = ((v >> 10) & 31) << 3; px[1] = ((v >> 5) & 31) << 3; px[2] = (v & 31) << 3; } }
                fwrite(px, 1, 3, f); }
            fclose(f); }
    }
}
static size_t audio_batch(const int16_t *d, size_t n) { if (fastmode) { asamples += n; return n; } acrc = crc_upd(acrc, d, n * 4); asamples += n; return n; }
static void audio_one(int16_t l, int16_t r) { int16_t b[2] = { l, r }; audio_batch(b, 1); }
static void input_poll(void) {}
static int autoplay;
static uint32_t auto_mask(long f) {
    /* scripted pad for homebrew without input files: Start taps early, then a deterministic walk/jump/shoot pattern */
    static uint32_t lfsr = 0, mask;
    if (!lfsr) { const char *sd = getenv("NESRUN_SEED"); lfsr = sd ? (uint32_t)atol(sd) | 1u : 0xACE1u; }
    if (f >= 60 && f < 66) return 1u << 3;
    if (f >= 240 && f < 246) return 1u << 3;
    if (f >= 420 && f < 426) return (1u << 3) | (1u << 8);
    if (f % 18 == 0) { lfsr = (lfsr >> 1) ^ (-(lfsr & 1u) & 0xB400u);
        static const uint32_t pat[8] = { 1u << 7, (1u << 7) | (1u << 8), (1u << 7) | 1u, 1u << 6, 1u << 8, (1u << 7) | (1u << 8), 1u << 4, 0 };
        mask = pat[lfsr & 7]; if (lfsr & 0x100) mask |= 1u; }
    return mask;
}
static int16_t input_state(unsigned p, unsigned d, unsigned i, unsigned id) { (void)i; if (!autoplay || p != 0 || d != RETRO_DEVICE_JOYPAD) return 0; return (auto_mask(fno) >> id) & 1; }

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: nesrun core.so rom frames [-o crc] [-d out.ppm:N] [-v k=v] [-s sav]\n"); return 2; }
    fastmode = getenv("NESRUN_FAST") != NULL;
    autoplay = getenv("NESRUN_AUTOPLAY") != NULL;
    long frames = atol(argv[3]); const char *sav = NULL, *ramout = NULL, *stout = NULL, *wstate = NULL, *lstate = NULL; long wstate_f = -1, lstate_f = -1; (void)log_cb; crc_init();
    for (int i = 4; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc) fcrc = fopen(argv[++i], "w");
        else if (!strcmp(argv[i], "-d") && i + 1 < argc) { char *s = strdup(argv[++i]); char *c = strrchr(s, ':'); if (c) { *c = 0; dumpn = atoi(c + 1); } dumpf = s; }
        else if (!strcmp(argv[i], "-v") && i + 1 < argc && nvars < 64) { char *s = strdup(argv[++i]); char *e = strchr(s, '='); if (e) { *e = 0; vars[nvars].k = s; vars[nvars++].v = e + 1; } }
        else if (!strcmp(argv[i], "-s") && i + 1 < argc) sav = argv[++i];
        else if (!strcmp(argv[i], "-r") && i + 1 < argc) ramout = argv[++i];
        else if (!strcmp(argv[i], "-t") && i + 1 < argc) stout = argv[++i];
        else if (!strcmp(argv[i], "-w") && i + 1 < argc) { char *a = strdup(argv[++i]); char *c = strrchr(a, ':'); if (c) { *c = 0; wstate_f = atol(c + 1); } wstate = a; }
        else if (!strcmp(argv[i], "-l") && i + 1 < argc) { char *a = strdup(argv[++i]); char *c = strrchr(a, ':'); if (c) { *c = 0; lstate_f = atol(c + 1); } lstate = a; }
    }
    #ifdef STATIC_CORE
    void *h = NULL;
#else
    void *h = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL); if (!h) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
#endif
#ifdef STATIC_CORE
#define SYM(n) __typeof__(&n) n##_f = n;
#else
#define SYM(n) __typeof__(&n) n##_f = dlsym(h, #n); if (!n##_f) { fprintf(stderr, "missing " #n "\n"); return 1; }
#endif
    SYM(retro_set_environment) SYM(retro_set_video_refresh) SYM(retro_set_audio_sample) SYM(retro_set_audio_sample_batch)
    SYM(retro_set_input_poll) SYM(retro_set_input_state) SYM(retro_init) SYM(retro_load_game) SYM(retro_run) SYM(retro_get_system_av_info)
    SYM(retro_deinit) SYM(retro_unserialize) SYM(retro_get_memory_data) SYM(retro_get_memory_size) SYM(retro_serialize_size) SYM(retro_serialize) SYM(retro_unload_game)
    retro_set_environment_f(env); retro_init_f();
    retro_set_video_refresh_f(video); retro_set_audio_sample_f(audio_one); retro_set_audio_sample_batch_f(audio_batch);
    retro_set_input_poll_f(input_poll); retro_set_input_state_f(input_state);
    FILE *rf = fopen(argv[2], "rb"); if (!rf) { perror(argv[2]); return 1; }
    fseek(rf, 0, SEEK_END); long rs = ftell(rf); fseek(rf, 0, SEEK_SET); void *rom = malloc(rs); if (fread(rom, 1, rs, rf) != (size_t)rs) return 1; fclose(rf);
    struct retro_game_info gi = { argv[2], rom, rs, NULL };
    if (!retro_load_game_f(&gi)) { fprintf(stderr, "load failed\n"); return 1; }
    struct retro_system_av_info av; retro_get_system_av_info_f(&av);
    struct timespec t0, t1; clock_gettime(CLOCK_MONOTONIC, &t0);
    const char *tr = getenv("NESRUN_TRACE"); long tr0 = tr ? atol(tr) : -1;
    struct { int32_t tcount; uint16_t PC; uint8_t A, X, Y, S, P, mooPI, jammed; } *xs = h ? dlsym(h, "X") : NULL;
    for (fno = 0; fno < frames; fno++) {
        if (lstate && fno == lstate_f) { FILE *f = fopen(lstate, "rb"); size_t n = 0; void *b = NULL; if (f) { fseek(f, 0, SEEK_END); n = (size_t)ftell(f); fseek(f, 0, SEEK_SET); b = calloc(1, n ? n : 1); } /* (the whole file: a state written by another build can be longer than this core's own size) */ if (f && fread(b, 1, n, f) > 0 && retro_unserialize_f(b, n)) fprintf(stderr, "state loaded at %ld\n", fno); else fprintf(stderr, "state load FAILED\n"); if (f) fclose(f); free(b); }
        if (wstate && fno == wstate_f) { size_t n = retro_serialize_size_f(); void *b = calloc(1, n); if (retro_serialize_f(b, n)) { FILE *f = fopen(wstate, "wb"); fwrite(b, 1, n, f); fclose(f); } free(b); }
        retro_run_f(); if (tr0 >= 0 && fno >= tr0 && xs) { uint8_t *rm = retro_get_memory_data_f(RETRO_MEMORY_SYSTEM_RAM); printf("f%ld PC=%04x S=%02x ram0=%02x %02x %02x\n", fno, xs->PC, xs->S, rm[0], rm[1], rm[2]); } }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double sec = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
    uint8_t *ram = retro_get_memory_data_f(RETRO_MEMORY_SAVE_RAM); size_t rn = retro_get_memory_size_f(RETRO_MEMORY_SAVE_RAM);
    uint32_t scrc = ram ? crc_upd(0, ram, rn) : 0;
    if (sav && ram) { FILE *f = fopen(sav, "wb"); if (f) { fwrite(ram, 1, rn, f); fclose(f); } }
    if (ramout) { uint8_t *sr = retro_get_memory_data_f(RETRO_MEMORY_SYSTEM_RAM); size_t sn = retro_get_memory_size_f(RETRO_MEMORY_SYSTEM_RAM); FILE *f = fopen(ramout, "wb"); if (f && sr) { fwrite(sr, 1, sn, f); fclose(f); } }
    size_t ss = retro_serialize_size_f(); uint32_t stcrc = 0;
    if (ss) { void *b = calloc(1, ss); if (retro_serialize_f(b, ss)) { stcrc = crc_upd(0, b, ss); if (stout) { FILE *f = fopen(stout, "wb"); if (f) { fwrite(b, 1, ss, f); fclose(f); } } } free(b); }
    printf("frames=%ld video_crc=%08x frames_out=%lu audio_crc=%08x audio_samples=%lu srm_crc=%08x srm_size=%zu state_crc=%08x state_size=%zu fmt=%d rate=%.0f fps=%.3f host_s=%.2f host_fps=%.0f\n",
           frames, vcrc, (unsigned long)vframes, acrc, (unsigned long)asamples, scrc, rn, stcrc, ss, pf, av.timing.sample_rate, av.timing.fps, sec, frames / sec);
    { struct { int32_t tcount; uint16_t PC; uint8_t A, X, Y, S, P, mooPI, jammed; } *x = h ? dlsym(h, "X") : NULL; if (x) printf("cpu PC=%04x A=%02x X=%02x Y=%02x S=%02x P=%02x jammed=%d\n", x->PC, x->A, x->X, x->Y, x->S, x->P, x->jammed); }
    retro_unload_game_f(); retro_deinit_f(); if (fcrc) fclose(fcrc); return 0;
}
