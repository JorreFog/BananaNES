#define _GNU_SOURCE
#include "banana.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef BANANA_STATS
struct banana_stats bstat;
static FILE *sf;
static unsigned long sframe;
void banana_stats_frame(void)
{
	if (!sf) {
		const char *p = getenv("BANANANES_STATS");
		if (!p) { static int once; if (!once) once = 1; return; }
		sf = fopen(p, "w");
		if (!sf) return;
		fprintf(sf, "frame insn ppu2000 ppu2001 ppu2002 ppu2003 ppu2004 ppu2005 ppu2006 ppu2007 r2002 r2007 apu_w mapper_w sram_w oamdma dmc_bytes irq nmi spr_total spr_max_line spr_over8_lines\n");
	}
	fprintf(sf, "%lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu\n", sframe++, (unsigned long)bstat.insn,
	        (unsigned long)bstat.ppu_w[0], (unsigned long)bstat.ppu_w[1], (unsigned long)bstat.ppu_w[2], (unsigned long)bstat.ppu_w[3], (unsigned long)bstat.ppu_w[4],
	        (unsigned long)bstat.ppu_w[5], (unsigned long)bstat.ppu_w[6], (unsigned long)bstat.ppu_w[7], (unsigned long)bstat.ppu_r2002, (unsigned long)bstat.ppu_r2007,
	        (unsigned long)bstat.apu_w, (unsigned long)bstat.mapper_w, (unsigned long)bstat.sram_w, (unsigned long)bstat.oamdma, (unsigned long)bstat.dmc_bytes,
	        (unsigned long)bstat.irq, (unsigned long)bstat.nmi, (unsigned long)bstat.spr_total, (unsigned long)bstat.spr_max_line, (unsigned long)bstat.spr_over8_lines);
	memset(&bstat, 0, sizeof bstat);
}
#endif

#ifdef BANANA_PROF
#include <dlfcn.h>
#include <signal.h>
#include <sys/time.h>
#include <ucontext.h>
#define PROF_MAX 262144
static uintptr_t prof_pc[PROF_MAX];
static volatile unsigned prof_n;
static uintptr_t prof_base;
static char *prof_file;
static void prof_handler(int sig, siginfo_t *si, void *ctx)
{
	ucontext_t *uc = ctx;
	(void)sig; (void)si;
#if defined(__aarch64__)
	uintptr_t pc = uc->uc_mcontext.pc;
#elif defined(__x86_64__)
	uintptr_t pc = uc->uc_mcontext.gregs[REG_RIP];
#else
	uintptr_t pc = 0;
#endif
	if (prof_n < PROF_MAX) prof_pc[prof_n++] = pc;
}
void banana_prof_start(void)
{
	const char *p = getenv("BANANANES_PROF");
	Dl_info di;
	struct sigaction sa;
	struct itimerval it;
	if (!p || prof_file) return;
	if (!dladdr((void *)banana_prof_start, &di)) return;
	prof_base = (uintptr_t)di.dli_fbase;
	prof_file = strdup(p);
	memset(&sa, 0, sizeof sa);
	sa.sa_sigaction = prof_handler;
	sa.sa_flags = SA_SIGINFO | SA_RESTART;
	sigaction(SIGPROF, &sa, NULL);
	it.it_interval.tv_sec = 0; it.it_interval.tv_usec = 1000;   /* 1 kHz of CPU time */
	it.it_value = it.it_interval;
	setitimer(ITIMER_PROF, &it, NULL);
}
void banana_prof_stop(void)
{
	struct itimerval it;
	FILE *f;
	unsigned i;
	if (!prof_file) return;
	memset(&it, 0, sizeof it);
	setitimer(ITIMER_PROF, &it, NULL);
	f = fopen(prof_file, "w");
	if (f) {
		for (i = 0; i < prof_n; i++)
			if (prof_pc[i] >= prof_base && prof_pc[i] < prof_base + (8u << 20)) fprintf(f, "%lx\n", (unsigned long)(prof_pc[i] - prof_base));
		fclose(f);
	}
	free(prof_file); prof_file = NULL;
}
#endif
