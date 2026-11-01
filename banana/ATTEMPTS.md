# BananaNES optimisation attempts (2026-11-01)

Gate for every change: `banana/tools/check.sh <core>` (video + audio + save-RAM + state CRC on the stress ROM L1/L5/L10 and the
homebrew ROMs, identical to the unmodified upstream core) on the PC, `banana/tools/devcheck.sh` on the handheld (aarch64 paths).
Speed: BananaFront retro_run ms (stressrom/ab-nes.sh, clock pinned 1416 MHz) on the device; "host" = x86 core-only time of
10800 stress frames (a proxy: out-of-order x86 hides some things the in-order A55 does not).
Base = upstream libretro-fceumm 7a542da (BananaOS 0.5.0 pin): host 2.38 s.

| # | Change | Result | Verdict |
|---|---|---|---|
| 1 | `-O3 -flto -fno-semantic-interposition` (was -O2) | Plus: 2.74 -> 2.36 ms/frame (-14 %) | kept |
| 2 | CPU registers + cycle counters in locals; no per-instruction X.PC store | host -2 % | kept |
| 3 | inline RAM / plain-ROM read and RAM write fast paths (rdplain / wrplain pages) | host -6 % | kept |
| 4 | lazy APU hook: FCEU_SoundCPUHook only at DMC / frame-counter events (FCEU_SoundHookDue), owed cycles applied before any $4000-$4017 access | host ~-9 % with 2+3 | kept |
| 5 | BG pair LUT rebuilt only when PALRAM[0..15] changed (was ~500 rebuilds a frame) | host 2.15 -> 1.78 s | kept |
| 6 | LQ APU mixers add whole runs between channel events (per 16-tick bucket) | host 1.78 -> 1.52 s | kept |
| 7 | sprite-on-line bit mask (NEON vld4 on aarch64) instead of scanning 64 OAM entries per line | host neutral; device pending | kept (exact) |
| 8 | NEON palette -> RGB565/XRGB8888 row blit (vqtbl4), scalar fallback for odd rows | device pending | kept (exact) |
| 9 | opcode-fetch code-page cache | host neutral; device pending | tentative |
| 10 | $2002 reads flush the partial line only when a sprite-0 hit can appear (polling games did ~2600 RefreshLine calls a frame) | not exact: video CRC differs by 8 pixels of one line in the scene where rendering is switched on mid-line (stale BG pipeline) | dropped |
