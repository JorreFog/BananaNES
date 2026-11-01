/* FCE Ultra - NES/Famicom Emulator
 *
 * Copyright notice for this file:
 *  Copyright (C) 2002 Xodnizel
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <string.h>

#include "fceu.h"
#include "fceu-types.h"
#include "x6502.h"
#include "fceu.h"
#include "sound.h"
#include "cart.h"
#include "ppu.h"
#include "banana.h"

X6502 X;
uint8_t encryptOpcodes = 0;
uint8_t encryptOpcodesConfig = 0;

uint32_t timestamp;
uint32_t sound_timestamp;
void FP_FASTAPASS(1) (*MapIRQHook)(int a);

#define _PC        X.PC
#define _A         X.A
#define _X         X.X
#define _Y         X.Y
#define _S         X.S
#define _P         X.P
#define _PI        X.mooPI
#define _DB        X.DB
#define _count     X.count
#define _tcount    X.tcount
#define _IRQlow    X.IRQlow
#define _jammed    X.jammed

#define ADDCYC(x) {									\
	int __x = x;									\
	_tcount += __x;									\
	_count -= __x * 48;								\
	timestamp += __x;  \
	if (!overclocked) sound_timestamp +=  __x; \
}

#ifdef BANANA_STATS
static INLINE void bst_w(uint32_t A) {
	if (A >= 0x8000) BST(mapper_w);
	else if (A >= 0x6000) BST(sram_w);
	else if (A >= 0x4000) { if (A == 0x4014) BST(oamdma); else BST(apu_w); }
	else if (A >= 0x2000) bstat.ppu_w[A & 7]++;
}
static INLINE void bst_r(uint32_t A) {
	if (A >= 0x2000 && A < 0x4000) { if ((A & 7) == 2) BST(ppu_r2002); else if ((A & 7) == 7) BST(ppu_r2007); }
}
#define BST_W(A) bst_w(A)
#define BST_R(A) bst_r(A)
#else
#define BST_W(A) ((void)0)
#define BST_R(A) ((void)0)
#endif

static INLINE uint8_t RdMemNorm(uint32_t A) {
	BST_R(A);
	switch (rdplain[A >> 11]) {
	case 1: return(_DB = RAM[A & 0x7FF]);
	case 2: return(_DB = Page[A >> 11][A]);
	default: return(_DB = ARead[A](A));
	}
}

static INLINE void WrMemNorm(uint32_t A, uint8_t V) {
	BST_W(A);
	BWrite[A](A, V);
}

static INLINE uint8_t RdRAMFast(uint32_t A) {
	return(_DB = RAM[A]);
}

static INLINE void WrRAMFast(uint32_t A, uint8_t V) {
	RAM[A] = V;
}

uint8_t FASTAPASS(1) X6502_DMR(uint32_t A) {
	ADDCYC(1);
	return(X.DB = ARead[A](A));
}

void FASTAPASS(2) X6502_DMW(uint32_t A, uint8_t V) {
	ADDCYC(1);
	BWrite[A](A, V);
}

#define PUSH(V) {									\
	uint8_t VTMP = V;									\
	WrRAM(0x100 + _S, VTMP);						\
	_S--;											\
}

#define POP() RdRAM(0x100 + (++_S))

static uint8_t ZNTable[256];
/* Some of these operations will only make sense if you know what the flag constants are. */

#define X_ZN(zort)  _P &= ~(Z_FLAG | N_FLAG); _P |= ZNTable[zort]
#define X_ZNT(zort) _P |= ZNTable[zort]

#define JR(cond) {									\
	if (cond)										\
	{												\
		uint32_t tmp;									\
		int32_t disp;									\
		disp = (int8_t)RdOp();					\
		_PC++;										\
		ADDCYC(1);									\
		tmp = _PC;									\
		_PC += disp;								\
		if ((tmp ^ _PC) & 0x100)					\
			ADDCYC(1);								\
	} else _PC++;									\
}

#define LDA     _A = x; X_ZN(_A)
#define LDX     _X = x; X_ZN(_X)
#define LDY     _Y = x; X_ZN(_Y)

/* All of the freaky arithmetic operations. */
#define AND     _A &= x; X_ZN(_A)
#define BIT     _P &= ~(Z_FLAG | V_FLAG | N_FLAG); _P |= ZNTable[x & _A] & Z_FLAG; _P |= x & (V_FLAG | N_FLAG)
#define EOR     _A ^= x; X_ZN(_A)
#define ORA     _A |= x; X_ZN(_A)

#define ADC {										\
	uint32_t l = _A + x + (_P & 1);					\
	_P &= ~(Z_FLAG | C_FLAG | N_FLAG | V_FLAG);		\
	_P |= ((((_A ^ x) & 0x80) ^ 0x80) & ((_A ^ l) & 0x80)) >> 1;  \
	_P |= (l >> 8) & C_FLAG;						\
	_A = l;											\
	X_ZNT(_A);										\
}

#define SBC {										\
	uint32_t l = _A - x - ((_P & 1) ^ 1);				\
	_P &= ~(Z_FLAG | C_FLAG | N_FLAG | V_FLAG);		\
	_P |= ((_A ^ l) & (_A ^ x) & 0x80) >> 1;		\
	_P |= ((l >> 8) & C_FLAG) ^ C_FLAG;				\
	_A = l;											\
	X_ZNT(_A);										\
}

#define CMPL(a1, a2) {								\
	uint32_t t = a1 - a2;								\
	X_ZN(t & 0xFF);									\
	_P &= ~C_FLAG;									\
	_P |= ((t >> 8) & C_FLAG) ^ C_FLAG;				\
}

/* Special undocumented operation.  Very similar to CMP. */
#define AXS {										\
	uint32_t t = (_A & _X) - x;						\
	X_ZN(t & 0xFF);									\
	_P &= ~C_FLAG;									\
	_P |= ((t >> 8) & C_FLAG) ^ C_FLAG;				\
	_X = t;											\
}

#define CMP     CMPL(_A, x)
#define CPX     CMPL(_X, x)
#define CPY     CMPL(_Y, x)

/* The following operations modify the byte being worked on. */
#define DEC     x--; X_ZN(x)
#define INC     x++; X_ZN(x)

#define ASL     _P &= ~C_FLAG; _P |= x >> 7; x <<= 1; X_ZN(x)
#define LSR     _P &= ~(C_FLAG | N_FLAG | Z_FLAG); _P |= x & 1; x >>= 1; X_ZNT(x)

/* For undocumented instructions, maybe for other things later... */
#define LSRA    _P &= ~(C_FLAG | N_FLAG | Z_FLAG); _P |= _A & 1; _A >>= 1; X_ZNT(_A)

#define ROL {										\
	uint8_t l = x >> 7;								\
	x <<= 1;										\
	x |= _P & C_FLAG;								\
	_P &= ~(Z_FLAG | N_FLAG | C_FLAG);				\
	_P |= l;										\
	X_ZNT(x);										\
}

#define ROR {										\
	uint8_t l = x & 1;								\
	x >>= 1;										\
	x |= (_P & C_FLAG) << 7;						\
	_P &= ~(Z_FLAG | N_FLAG | C_FLAG);				\
	_P |= l;										\
	X_ZNT(x);										\
}

/* Icky icky thing for some undocumented instructions.  Can easily be
 * broken if names of local variables are changed.
 */

/* Absolute */
#define GetAB(target) {								\
	target = RdOp();							\
	_PC++;											\
	target |= RdOp() << 8;						\
	_PC++;											\
}

/* Absolute Indexed(for reads) */
#define GetABIRD(target, i) {						\
	uint32_t tmp;										\
	GetAB(tmp);										\
	target = tmp;									\
	target += i;									\
	if ((target ^ tmp) & 0x100) {					\
		target &= 0xFFFF;							\
		RdMem(target ^ 0x100);						\
		ADDCYC(1);									\
	}												\
}

/* Absolute Indexed(for writes and rmws) */
#define GetABIWR(target, i) {						\
	uint32_t rt;										\
	GetAB(rt);										\
	target = rt;									\
	target += i;									\
	target &= 0xFFFF;								\
	RdMem((target & 0x00FF) | (rt & 0xFF00));		\
}

/* Zero Page */
#define GetZP(target) {								\
	target = RdOp();							\
	_PC++;											\
}

/* Zero Page Indexed */
#define GetZPI(target, i) {							\
	target = i + RdOp();						\
	_PC++;											\
}

/* Indexed Indirect */
#define GetIX(target) {								\
	uint8_t tmp;										\
	tmp = RdOp();								\
	_PC++;											\
	tmp += _X;										\
	target = RdRAM(tmp);							\
	tmp++;											\
	target |= RdRAM(tmp) << 8;						\
}

/* Indirect Indexed(for reads) */
#define GetIYRD(target) {							\
	uint32_t rt;										\
	uint8_t tmp;										\
	tmp = RdOp();								\
	_PC++;											\
	rt = RdRAM(tmp);								\
	tmp++;											\
	rt |= RdRAM(tmp) << 8;							\
	target = rt;									\
	target += _Y;									\
	if ((target ^ rt) & 0x100) {					\
		target &= 0xFFFF;							\
		RdMem(target ^ 0x100);						\
		ADDCYC(1);									\
	}												\
}

/* Indirect Indexed(for writes and rmws) */
#define GetIYWR(target) {							\
	uint32_t rt;										\
	uint8_t tmp;										\
	tmp = RdOp();								\
	_PC++;											\
	rt = RdRAM(tmp);								\
	tmp++;											\
	rt |= RdRAM(tmp) << 8;							\
	target = rt;									\
	target += _Y;									\
	target &= 0xFFFF;								\
	RdMem((target & 0x00FF) | (rt & 0xFF00));		\
}

/* Now come the macros to wrap up all of the above stuff addressing mode functions
and operation macros.  Note that operation macros will always operate(redundant
redundant) on the variable "x".
*/

#define RMW_A(op)       { uint8_t x = _A; op; _A = x; break; }	/* Meh... */
#define RMW_AB(op)      { uint32_t A; uint8_t x; GetAB(A); x = RdMem(A); WrMem(A, x); op; WrMem(A, x); break; }
#define RMW_ABI(reg, op) { uint32_t A; uint8_t x; GetABIWR(A, reg); x = RdMem(A); WrMem(A, x); op; WrMem(A, x); break; }
#define RMW_ABX(op)     RMW_ABI(_X, op)
#define RMW_ABY(op)     RMW_ABI(_Y, op)
#define RMW_IX(op)      { uint32_t A; uint8_t x; GetIX(A); x = RdMem(A); WrMem(A, x); op; WrMem(A, x); break; }
#define RMW_IY(op)      { uint32_t A; uint8_t x; GetIYWR(A); x = RdMem(A); WrMem(A, x); op; WrMem(A, x); break; }
#define RMW_ZP(op)      { uint8_t A; uint8_t x; GetZP(A); x = RdRAM(A); op; WrRAM(A, x); break; }
#define RMW_ZPX(op)     { uint8_t A; uint8_t x; GetZPI(A, _X); x = RdRAM(A); op; WrRAM(A, x); break; }

#define LD_IM(op)       { uint8_t x; x = RdOp(); _PC++; op; break; }
#define LD_ZP(op)       { uint8_t A; uint8_t x; GetZP(A); x = RdRAM(A); op; break; }
#define LD_ZPX(op)      { uint8_t A; uint8_t x; GetZPI(A, _X); x = RdRAM(A); op; break; }
#define LD_ZPY(op)      { uint8_t A; uint8_t x; GetZPI(A, _Y); x = RdRAM(A); op; break; }
#define LD_AB(op)       { uint32_t A; uint8_t x; GetAB(A); x = RdMem(A); op; break; }
#define LD_ABI(reg, op) { uint32_t A; uint8_t x; GetABIRD(A, reg); x = RdMem(A); op; break; }
#define LD_ABX(op)      LD_ABI(_X, op)
#define LD_ABY(op)      LD_ABI(_Y, op)
#define LD_IX(op)       { uint32_t A; uint8_t x; GetIX(A); x = RdMem(A); op; break; }
#define LD_IY(op)       { uint32_t A; uint8_t x; GetIYRD(A); x = RdMem(A); op; break; }

#define ST_ZP(r)        { uint8_t A; GetZP(A); WrRAM(A, r); break; }
#define ST_ZPX(r)       { uint8_t A; GetZPI(A, _X); WrRAM(A, r); break; }
#define ST_ZPY(r)       { uint8_t A; GetZPI(A, _Y); WrRAM(A, r); break; }
#define ST_AB(r)        { uint32_t A; GetAB(A); WrMem(A, r); break; }
#define ST_ABI(reg, r)  { uint32_t A; GetABIWR(A, reg); WrMem(A, r); break; }
#define ST_ABX(r)       ST_ABI(_X, r)
#define ST_ABY(r)       ST_ABI(_Y, r)
#define ST_IX(r)        { uint32_t A; GetIX(A); WrMem(A, r); break; }
#define ST_IY(r)        { uint32_t A; GetIYWR(A); WrMem(A, r); break; }

static int32_t Cyc48[256];
static const uint8_t CycTable[256] =
{
/*0x00*/ 7, 6, 2, 8, 3, 3, 5, 5, 3, 2, 2, 2, 4, 4, 6, 6,
/*0x10*/ 2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
/*0x20*/ 6, 6, 2, 8, 3, 3, 5, 5, 4, 2, 2, 2, 4, 4, 6, 6,
/*0x30*/ 2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
/*0x40*/ 6, 6, 2, 8, 3, 3, 5, 5, 3, 2, 2, 2, 3, 4, 6, 6,
/*0x50*/ 2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
/*0x60*/ 6, 6, 2, 8, 3, 3, 5, 5, 4, 2, 2, 2, 5, 4, 6, 6,
/*0x70*/ 2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
/*0x80*/ 2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,
/*0x90*/ 2, 6, 2, 6, 4, 4, 4, 4, 2, 5, 2, 5, 5, 5, 5, 5,
/*0xA0*/ 2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,
/*0xB0*/ 2, 5, 2, 5, 4, 4, 4, 4, 2, 4, 2, 4, 4, 4, 4, 4,
/*0xC0*/ 2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,
/*0xD0*/ 2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
/*0xE0*/ 2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,
/*0xF0*/ 2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
};

void FASTAPASS(1) X6502_IRQBegin(int w) {
	_IRQlow |= w;
}

void FASTAPASS(1) X6502_IRQEnd(int w) {
	_IRQlow &= ~w;
}

void TriggerNMI(void) {
	_IRQlow |= FCEU_IQNMI;
}

void TriggerNMI2(void) {
	_IRQlow |= FCEU_IQNMI2;
}

void X6502_Reset(void) {
	_IRQlow = FCEU_IQRESET;
}

void X6502_Init(void) {
	int x;

	memset((void*)&X, 0, sizeof(X));
	for (x = 0; x < 256; x++)
		Cyc48[x] = CycTable[x] * 48;
	for (x = 0; x < 256; x++)
		if (!x)
			ZNTable[x] = Z_FLAG;
		else if (x & 0x80)
			ZNTable[x] = N_FLAG;
		else
			ZNTable[x] = 0;
}

void X6502_Power(void) {
	_count = _tcount = _IRQlow = _PC = _A = _X = _Y = _P = _PI = _DB = _jammed = 0;
	_S = 0xFD;
	timestamp = sound_timestamp = 0;
	X6502_Reset();
}


/* BananaNES: the part of "sync the run's locals out" that needs no local state beyond five numbers, out of line (it is
 * expanded at ~1000 memory-access sites of X6502_Run: 250 KB of code) */
#ifdef BANANA_OUTLINE_SYNC
static __attribute__((noinline)) void cpu_sync_out(int32_t lcount, int32_t cb_sync, int32_t cb_cons, uint8_t ldb)
{
	int32_t lts = (cb_sync - lcount) / 48;
	X.count = lcount;
	X.tcount = (cb_cons - lcount) / 48;
	X.DB = ldb;
	timestamp += lts;
	if (!overclocked) sound_timestamp += lts;
}
#endif
#ifdef BANANA_RUN_O2
__attribute__((optimize("O2")))
#endif
void X6502_Run(int32_t cycles)
{
	#define WrRAM WrRAMFast

	#if (defined(C80x86) && defined(__GNUC__))
	/* Gives a nice little speed boost. */
	register uint16_t pbackus asm ("edi");
	#else
	uint16_t pbackus;
	#endif

	/* BananaNES: the CPU registers and the cycle counters live in locals for the run (only state.c reads X.A..
	 * and never during a run); they go back to X / timestamp / sound_timestamp before anything outside can see
	 * them: a memory handler, a mapper IRQ hook, the APU hook, and the end of the run. */
	uint8_t rA = X.A, rX = X.X, rY = X.Y, rS = X.S, rP = X.P, rPI = X.mooPI;
	/* cycle bookkeeping: only lcount (x48) moves per instruction; everything else is derived from it when needed:
	 *   cb_sync: lcount at the last sync (the cycles since then advance timestamp / sound_timestamp)
	 *   cb_cons: lcount at the last "consumption" point, X.tcount = (cb_cons - lcount) / 48 (the baseline's tcount)
	 *   snd_base: lcount (consumption-aligned) at the last APU hook; the hook is due when lcount <= lsnd_lim */
	int32_t lcount = X.count;
	int32_t cb_sync, cb_cons, snd_base, snd_due = FCEU_SoundHookDue(), lsnd_lim;
	/* per-instruction globals read from locals: they only change inside handlers/hooks (refreshed by SYNC_IN) */
	uint8_t ldb = X.DB, lenc = encryptOpcodes, lrare;
	uint32_t lirq = X.IRQlow;
	void FP_FASTAPASS(1) (*lmap)(int) = MapIRQHook;
	const int lover = overclocked;
	lrare = (MapIRQHook != 0) | (encryptOpcodes != 0);
	/* the code page the PC is in (plain cart ROM): the opcode fetch skips the page-type lookup; dropped whenever a
	 * handler ran (a mapper write may have switched banks) */
	uint32_t cp_page = 0xFFFF;
	const uint8_t *cp_ptr = NULL;

	pbackus = X.PC;

	#undef _PC
	#define _PC pbackus
	#undef _A
	#undef _X
	#undef _Y
	#undef _S
	#undef _P
	#undef _PI
	#undef _count
	#undef _DB
	#undef ADDCYC
	#define _DB ldb
	#define RdRAM(A_) (ldb = RAM[(A_)])
	#define _A rA
	#define _X rX
	#define _Y rY
	#define _S rS
	#define _P rP
	#define _PI rPI
	#define _count lcount
	#define ADDCYC(x) { lcount -= (x) * 48; }
	#ifdef BANANA_OUTLINE_SYNC
	#define SYNC_OUT() cpu_sync_out(lcount, cb_sync, cb_cons, ldb)
	#else
	#define SYNC_OUT() {								\
		int32_t _lts = (cb_sync - lcount) / 48;			\
		X.count = lcount;								\
		X.tcount = (cb_cons - lcount) / 48;				\
		X.DB = ldb;										\
		timestamp += _lts;								\
		if (!overclocked) sound_timestamp += _lts;		\
	}
	#endif
	#define SYNC_IN() { lcount = X.count; cb_sync = lcount; cb_cons = lcount + X.tcount * 48; cp_page = 0xFFFF; ldb = X.DB; lirq = X.IRQlow; lmap = MapIRQHook; lenc = encryptOpcodes; lrare = (lmap != 0) | (lenc != 0); }
	/* the APU hook only counts down between its events: apply what is owed before the APU registers are touched */
	#define SND_OWED() { if (!lover) { int32_t _o = (snd_base - cb_cons) / 48; if (_o) FCEU_SoundCPUHook(_o); snd_base = cb_cons; } }
	#define SND_REFRESH() { snd_due = FCEU_SoundHookDue(); lsnd_lim = snd_base - 48 * snd_due; }
	/* operand byte at the PC: from the cached code page when it is in it */
	#define RdOp() (__builtin_expect((_PC >> 11) == cp_page, 1) ? (ldb = cp_ptr[_PC]) : RdMem(_PC))
	#define RdMem(A_) ({										\
		uint32_t _ma = (A_);								\
		uint8_t _mv;										\
		uint32_t _mk = rdplain[_ma >> 11];					\
		if (__builtin_expect(_mk == 2, 1))					\
			_mv = Page[_ma >> 11][_ma];						\
		else if (__builtin_expect(_mk == 1, 1))				\
			_mv = RAM[_ma & 0x7FF];							\
		else if (__builtin_expect((_ma & 0xE007) == 0x2002, 0) && FCEU_PPUStatusFast(cb_sync - lcount, _ma, &_mv)) { \
			/* PPU status polled in a loop: no handler round trip */	\
		} else {											\
			SYNC_OUT();										\
			if (_ma - 0x4000u < 0x18u) SND_OWED();			\
			BST_R(_ma);										\
			_mv = ARead[_ma](_ma);							\
			SYNC_IN();										\
			if (_ma - 0x4000u < 0x18u) SND_REFRESH();		\
		}													\
		_DB = _mv;											\
		_mv; })
	#define WrMem(A_, V_) {										\
		uint32_t _wa = (A_);								\
		uint8_t _wv = (V_);									\
		if (__builtin_expect(_wa < 0x2000 && wrplain[_wa >> 11], 1)) RAM[_wa & 0x7FF] = _wv;	\
		else {												\
			SYNC_OUT();										\
			if (_wa - 0x4000u < 0x18u) SND_OWED();			\
			BST_W(_wa);										\
			BWrite[_wa](_wa, _wv);							\
			SYNC_IN();										\
			if (_wa - 0x4000u < 0x18u) SND_REFRESH();		\
		}													\
	}

	if (PAL)
		cycles *= 15;	/* 15*4=60 */
	else
		cycles *= 16;	/* 16*4=64 */

	_count += cycles;
	cb_sync = lcount;
	cb_cons = lcount + X.tcount * 48;
	snd_base = cb_cons;
	lsnd_lim = snd_base - 48 * snd_due;

	while (_count > 0) {
		uint8_t b1;

		if (lirq) {
			if (_IRQlow & FCEU_IQRESET) {
				_PC = RdMem(0xFFFC);
				_PC |= RdMem(0xFFFD) << 8;
				_jammed = 0;
				_PI = _P = I_FLAG;
				_IRQlow &= ~FCEU_IQRESET;
			} else if (_IRQlow & FCEU_IQNMI2) {
				_IRQlow &= ~FCEU_IQNMI2;
				_IRQlow |= FCEU_IQNMI;
			} else if (_IRQlow & FCEU_IQNMI) {
				if (!_jammed) {
					BST(nmi);
					ADDCYC(7);
					PUSH(_PC >> 8);
					PUSH(_PC);
					PUSH((_P & ~B_FLAG) | (U_FLAG));
					_P |= I_FLAG;
					_PC = RdMem(0xFFFA);
					_PC |= RdMem(0xFFFB) << 8;
					_IRQlow &= ~FCEU_IQNMI;
				}
			} else {
				if (!(_PI & I_FLAG) && !_jammed) {
					BST(irq);
#ifdef BANANA_STATS
					bstat.irq_line = banana_scanline;
#endif
					ADDCYC(7);
					PUSH(_PC >> 8);
					PUSH(_PC);
					PUSH((_P & ~B_FLAG) | (U_FLAG));
					_P |= I_FLAG;
					_PC = RdMem(0xFFFE);
					_PC |= RdMem(0xFFFF) << 8;
				}
			}
			_IRQlow &= ~(FCEU_IQTEMP);
			lirq = X.IRQlow;
			if (_count <= 0) {
				_PI = _P;
				X.PC = pbackus;
				X.A = rA; X.X = rX; X.Y = rY; X.S = rS; X.P = rP; X.mooPI = rPI;
				SYNC_OUT();
				SND_OWED();
				return;
			}	/* Should increase accuracy without a
				 * major speed hit.
				 */
		}

		_PI = _P;
		BST(insn);
		if (__builtin_expect((_PC >> 11) == cp_page, 1)) {
			b1 = cp_ptr[_PC];
			_DB = b1;
		} else {
			b1 = RdMem(_PC);
			if (rdplain[_PC >> 11] == 2) { cp_page = _PC >> 11; cp_ptr = Page[cp_page]; }
		}

		lcount -= Cyc48[b1];

		if (__builtin_expect(lrare != 0, 0)) {
			if (lmap) {
				int32_t temp = (cb_cons - lcount) / 48;
				cb_cons = lcount;
				SYNC_OUT();
				lmap(temp);
				SYNC_IN();
			} else
				cb_cons = lcount;
			if (lenc) {
				if (encryptOpcodes == 10) b1 = b1 &~0x36 | b1 >>1 &0x12 | b1 <<1 &0x24;
				if (encryptOpcodes == 11) {
					if (encryptOpcodesConfig &0x02) b1 = b1 &~0x30 | b1 >>1 &0x10 | b1 <<1 &0x20;
					if (encryptOpcodesConfig &0x40) b1 = b1 &~0xC6 | b1 >>1 &0x42 | b1 <<1 &0x84;
				}
				if (encryptOpcodes == 12) b1 = b1 &~0xC6 | b1 >>1 &0x42 | b1 <<1 &0x84;
				if (encryptOpcodes == 14) b1 = b1 &~0xC0 | b1 >>1 &0x40 | b1 <<1 &0x80;
			}
		} else
			cb_cons = lcount;
		if (!lover && lcount <= lsnd_lim) {
			int32_t o = (snd_base - cb_cons) / 48;
			SYNC_OUT();
			FCEU_SoundCPUHook(o);
			SYNC_IN();
			snd_base = cb_cons;
			SND_REFRESH();
		}
		_PC++;
		switch (b1) {
			#include "ops.h"
		}
	}

	X.PC = pbackus;
	X.A = rA; X.X = rX; X.Y = rY; X.S = rS; X.P = rP; X.mooPI = rPI;
	SYNC_OUT();
	SND_OWED();

	#undef _PC
	#define _PC X.PC
	#undef _A
	#undef _X
	#undef _Y
	#undef _S
	#undef _P
	#undef _PI
	#undef _count
	#undef _DB
	#undef RdRAM
	#undef ADDCYC
	#undef RdMem
	#undef RdOp
	#undef WrMem
	#define _DB X.DB
	#define _A X.A
	#define _X X.X
	#define _Y X.Y
	#define _S X.S
	#define _P X.P
	#define _PI X.mooPI
	#define _count X.count
	#define _tcount X.tcount
	#define ADDCYC(x) {									\
		int __x = x;									\
		_tcount += __x;									\
		_count -= __x * 48;								\
		timestamp += __x;  \
		if (!overclocked) sound_timestamp +=  __x; \
	}
	#undef WrRAM
}
