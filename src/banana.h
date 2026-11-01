/* BananaNES additions (GPL-2.0-or-later like the rest): optional instrumentation, off by default.
 *   -DBANANA_PROF   a SIGPROF sampling profiler (BANANANES_PROF=<file> at run time) for devices without perf
 *   -DBANANA_STATS  per-frame workload counters (BANANANES_STATS=<file>): the "fingerprint" used to calibrate the stress ROM
 */
#ifndef BANANA_H
#define BANANA_H
#include <stdint.h>

#ifdef BANANA_STATS
struct banana_stats {
	uint64_t insn, ppu_w[8], ppu_r2002, ppu_r2007, apu_w, mapper_w, sram_w, oamdma, dmc_bytes, irq, nmi,
	         spr_lines, spr_over8_lines, spr_total, spr_max_line, scroll_w;
};
extern struct banana_stats bstat;
#define BST(f) (bstat.f++)
#define BSTN(f, n) (bstat.f += (n))
void banana_stats_frame(void);
#else
#define BST(f) ((void)0)
#define BSTN(f, n) ((void)0)
#endif

#ifdef BANANA_PROF
void banana_prof_start(void);
void banana_prof_stop(void);
#endif
#endif
