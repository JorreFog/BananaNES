# BananaNES notes

BananaNES = libretro-fceumm 7a542da (the pin of BananaOS 0.5.0) + the changes below. Branch `banananes`, remote `upstream` =
libretro/libretro-fceumm. `library_name` stays "FCEUmm" on purpose: save files, save states (same serialisation) and core options
are shared with libretro-fceumm, so switching cores in Settings > Other systems loses nothing.

## What differs from upstream (all exact: video + audio + save-RAM + state CRCs identical, see tools/check.sh)
- Build: `banana/Makefile` (A55 / host / Buildroot), `-O3 -flto -fno-semantic-interposition`, `-mcpu=cortex-a55`, XRGB8888.
- `src/x6502.c`: registers, cycle counters, DB, IRQ line and hook pointers in locals during X6502_Run; inline fast paths for
  internal RAM and plain cartridge ROM reads and RAM writes (`rdplain`/`wrplain` kept by SetReadHandler/SetWriteHandler);
  code-page cache for opcode fetches; the APU's per-instruction hook only at its events (`FCEU_SoundHookDue`), owed cycles
  applied before any $4000-$4017 access.
- `src/ppu.c`: BG pair LUT only rebuilt on palette change; sprite-on-line mask (NEON vld4 on aarch64) instead of a 64-entry scan
  per line; NEON/memset passes on the line buffers.
- `src/sound.c`: low-quality mixers add whole runs between channel events per 16-tick bucket.
- `src/drivers/libretro/libretro.c`: NEON palette -> XRGB8888/RGB565 row blit; memory map with merged contiguous windows named
  ram / sram / exram for the companion export.
- `src/banana.[ch]`: optional instrumentation, off by default: `-DBANANA_STATS` (BANANANES_STATS=file: per-frame workload
  counters), `-DBANANA_PROF` (BANANANES_PROF=file: SIGPROF sampler for devices without perf).

## Tools (banana/tools)
nesrun (PC libretro host + CRCs + scripted pad), check.sh (exactness gate on the PC), devcheck.sh (the same on a handheld),
fingerprint.py (workload fingerprint of a ROM), pgo-train.sh. Stress ROM + its A/B scripts: stressrom/.

## Not done / ideas
See ATTEMPTS.md (dropped: $2002 flush skip, not exact). Unoptimised: expansion audio, FDS, MMC5, PAL (upstream code).
