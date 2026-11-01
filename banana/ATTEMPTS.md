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
| 9 | opcode-fetch code-page cache (+ operand fetches) | host neutral / -3 %; device with 11-13 | kept |
| 11 | DB, IRQ line, MapIRQHook, opcode-encryption flag as locals; single cycle counter (cb_sync / cb_cons / snd_base derived from it) | host neutral; device (c8 vs c7) | kept (exact) |
| 12 | PPU tile pipeline (pshift/atlatch/XOffset) in locals during a span | host -2.5 % | kept |
| 13 | emphasis row fill skipped while no game uses the emphasis bits | host neutral | kept (exact incl. irqtest with emphasis cycling) |
| 14 | slow memory paths marked unlikely; X6502_Run is 250 KB of code (1000 call sites inlined): -Os / -O2 no-LTO variants and `-freorder-blocks-and-partition` measured on the device | see findings | |
| 10 | $2002 reads flush the partial line only when a sprite-0 hit can appear (polling games did ~2600 RefreshLine calls a frame) | first try not exact (8 px of one line when rendering is enabled mid-line): cause found = RefreshLine's Fixit1 (vertical increment) runs after the first call reaching pixel 268, so the two pre-fetch tiles see a new fine Y depending on where flushes fell; a read at pixel >= 248 still flushes, earlier reads skip | kept (exact) |
| 15 | X6502_Run with `-Os` (no LTO) to test the I-cache theory (250 KB function) | RG DS pinned 1416: 1.49 ms vs 1.33 (-O3 -flto) | dropped |
| 16 | NEON tile renderer for the plain BG path (vtst bit spread + vqtbl1 palette lookup) | device: 1.33 -> 1.29 ms (-3 %), exact (CRC identical incl. crop / palette / NTSC options on the RG DS) | kept, default on aarch64 |
| 17 | attribute / name-table page cached across tiles (plain BG path) | host neutral, exact | kept |
| 18 | $2002 status polls answered inside X6502_Run (FCEU_PPUStatusFast: no SYNC_OUT/IN, no handler call) when no flush is due | exact on 136 community test ROMs + 7 own ROMs + 13 option sets; device number pending | kept |
| 19 | PGO (-fprofile-use) trained on the RG DS (stress ROM 10800 frames + Nova + Thwaite + 240p), fixed relative object AND include paths, so a build from any directory (Buildroot's) is byte-identical | RG DS pinned 1416: 1.29 -> 1.19 ms (-8 %); the whole .so is byte-identical between the repo and a copy in another directory when GIT_VERSION matches | kept |
| 20 | `-fprofile-partial-training` (code the training never ran - VRC6/MMC5/N163 handlers, HQ sound - compiled for speed instead of size) | stress ROM 1.20 vs 1.19 ms (no change); .so 3.5 -> 4.3 MB | see the 09:xx note in the findings |
| 21 | PRG-RAM ($6000-$7FFF) reads on the inline fast path | exact; not separately measurable (no stress scene polls it) | kept |
| 22 | APU hook `FCEU_SoundHookDue` as a header inline (fhcnt/DMCacc global) | exact, no measurable change | dropped |
| 23 | Register-fuzz ROM (stressrom/fuzz): found that upstream's `VRC2_stateRegs[]` lacks its `{ 0 }` terminator (state of mapper 22 held build-dependent junk) | all 256 mappers x 3 seeds x 3000 frames identical to upstream (85 and 218 are not deterministic in upstream itself) | kept; one-line fix in the core, same fix in the reference build |
