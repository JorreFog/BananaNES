#!/usr/bin/env python3
"""fuzzgen.py <mapper> <seed> <out.nes> [prg16k chr8k] : the register-fuzz ROM (fuzz.s) as a NES file of that mapper."""
import sys, subprocess, os, random
m, seed, out = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3]
prg16 = int(sys.argv[4]) if len(sys.argv) > 4 else 16
chr8 = int(sys.argv[5]) if len(sys.argv) > 5 else 16
here = os.path.dirname(os.path.abspath(__file__))
env = dict(os.environ, PATH=os.path.expanduser("~/.local/opt/cc65/bin") + ":" + os.environ["PATH"])
o = f"/tmp/fuzz-{os.getpid()}"
subprocess.check_call(["ca65", f"-DSEED={seed & 0xFFFF}", "-o", o + ".o", os.path.join(here, "fuzz.s")], env=env)
subprocess.check_call(["ld65", "-C", os.path.join(here, "fuzz.cfg"), "-o", o + ".bin", o + ".o"], env=env)
code = open(o + ".bin", "rb").read()
for e in (".o", ".bin"): os.unlink(o + e)
vec = code[-6:]; code = code[:-6]
assert len(code) < 0x1000, len(code)
rng = random.Random(seed * 7919 + m)
prg = bytearray()
for b in range(prg16 * 2):
    bank = bytearray(code) + bytes(rng.randrange(256) for _ in range(0x2000 - 6 - len(code))) + vec
    prg += bank
hdr = bytearray(b"NES\x1a" + bytes([prg16, chr8, ((m & 15) << 4) | 0x02, m & 0xF0]) + bytes(8))
chr_ = bytes(rng.randrange(256) for _ in range(chr8 * 8192))
open(out, "wb").write(bytes(hdr) + bytes(prg) + chr_)
