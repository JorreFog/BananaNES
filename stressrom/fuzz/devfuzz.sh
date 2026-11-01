#!/bin/bash
# devfuzz.sh <plus|rgds> <refcore.so> <core.so>... : the register-fuzz ROMs on the real CPU (aarch64 nesrun in /run/nescheck),
# every core compared with the first (base) one. Mappers: DEVFUZZ_MAPPERS (default a spread of the common and the odd ones;
# 85 and 218 are left out: the upstream base core itself is not deterministic on them). Run under locked.sh.
H=$(cd "$(dirname "$0")" && pwd); D=$1; shift
F=${DEVFUZZ_FRAMES:-900}
M=${DEVFUZZ_MAPPERS:-"0 1 2 3 4 5 7 9 10 11 16 18 19 21 22 23 24 25 26 28 30 32 33 34 48 64 65 66 67 68 69 71 73 75 78 80 82 88 90 105 118 119 163 206 210 228"}
dev() { ~/bananaos-0.4.3/dev.sh "$D" "$@"; }
dev 'rm -rf /run/nescheck; mkdir -p /run/nescheck'
put() { dev "cat > /run/nescheck/$2 && chmod +x /run/nescheck/$2" < "$1"; }
put ${NESRUN_A64:-$HOME/banana-emu/bin/nesrun-a64} nesrun
i=0; for c in "$@"; do put $c core$i.so; i=$((i+1)); done
T=$(mktemp -d); trap 'rm -rf "$T"; dev "rm -rf /run/nescheck"' EXIT
n=0; bad=0
for m in $M; do
  python3 "$H/fuzzgen.py" $m 1 "$T/f.nes" 16 16 || continue; put "$T/f.nes" f.nes
  i=0; ref=
  for c in "$@"; do
    out=$(dev "cd /run/nescheck && ./nesrun ./core$i.so f.nes $F" | tr ' ' '\n' | grep -E "^(video_crc|audio_crc|srm_crc|state_crc)=" | tr '\n' ' ')
    [ $i = 0 ] && ref=$out
    [ -n "$out" ] && [ "$out" = "$ref" ] || { echo "DIFF mapper $m core$i"; echo "  ref $ref"; echo "  got $out"; bad=$((bad+1)); }
    i=$((i+1)); n=$((n+1))
  done
done
echo "DEVFUZZ $n runs, $bad different"
