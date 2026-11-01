#!/usr/bin/env python3
"""fingerprint.py <rom> <frames> [label]: runs the BANANA_STATS host core on a ROM (scripted pad) and prints the per-frame
workload fingerprint (mean / p95 / max per counter) as a markdown row set, for CALIBRATION.md."""
import os, subprocess, sys, statistics
rom, frames = sys.argv[1], sys.argv[2]
core = os.path.expanduser('~/banana-emu/BananaNES/build/host-stats/banananes_libretro.so')
csv = '/tmp/claude-1000/fp.csv'
env = dict(os.environ, BANANANES_STATS=csv, NESRUN_AUTOPLAY='1', NESRUN_FAST='1')
subprocess.run([os.path.expanduser('~/banana-emu/bin/nesrun'), core, rom, frames], env=env, capture_output=True)
rows = [l.split() for l in open(csv)]
h, d = rows[0], [list(map(int, r)) for r in rows[1:]]
d = d[30:]
out = []
for i, n in enumerate(h):
    if n == 'frame': continue
    v = sorted(r[i] for r in d)
    out.append((n, statistics.mean(v), v[int(len(v) * .95)], v[-1]))
label = sys.argv[3] if len(sys.argv) > 3 else os.path.basename(rom)
print('| %s | ' % label + ' | '.join('%s %.0f/%d/%d' % (n.replace('ppu', 'p'), m, p, x) for n, m, p, x in out if n in ('insn','ppu2005','ppu2006','ppu2007','r2002','apu_w','mapper_w','oamdma','dmc_bytes','irq','spr_total','spr_max_line','spr_over8_lines')) + ' |')
